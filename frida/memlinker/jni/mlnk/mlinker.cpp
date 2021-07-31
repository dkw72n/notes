#include <stdlib.h>
#include "mlinker_debug.h"
#include "mlinker.h"
#include "mlinker_phdr.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unordered_map>
#include "mlinker_sleb128.h"
#include "mlinker_reloc_iterators.h"
#include "mlinker_relocs.h"

extern "C" void* mlnk_dlopen(char* path, int);
extern "C" void* mlnk_dlsym(void*, char*);

int g_ld_debug_verbosity = 999;

static char __linker_dl_err_buf[768];

extern "C" const char* __gnu_basename(const char* path) {
  const char* last_slash = strrchr(path, '/');
  return (last_slash != nullptr) ? last_slash + 1 : path;
}

char* linker_get_error_buffer() {
  return &__linker_dl_err_buf[0];
}

size_t linker_get_error_buffer_size() {
  return sizeof(__linker_dl_err_buf);
}

#if STATS
struct linker_stats_t {
  int count[kRelocMax];
};

static linker_stats_t linker_stats;

void count_relocation(RelocationKind kind) {
  ++linker_stats.count[kind];
}
#else
void count_relocation(RelocationKind) {
}
#endif

static const ElfW(Versym) kVersymNotNeeded = 0;
static const ElfW(Versym) kVersymGlobal = 1;

struct MyRDebug {
  char v[256];
} _r_debug;

struct android_namespace_t {
 public:
  android_namespace_t() : name_(nullptr), is_isolated_(false) {}

  const char* get_name() const { return name_; }
  void set_name(const char* name) { name_ = name; }

  bool is_isolated() const { return is_isolated_; }
  void set_isolated(bool isolated) { is_isolated_ = isolated; }

  const std::vector<std::string>& get_ld_library_paths() const {
    return ld_library_paths_;
  }
  void set_ld_library_paths(std::vector<std::string>&& library_paths) {
    ld_library_paths_ = library_paths;
  }

  const std::vector<std::string>& get_default_library_paths() const {
    return default_library_paths_;
  }
  void set_default_library_paths(std::vector<std::string>&& library_paths) {
    default_library_paths_ = library_paths;
  }

  const std::vector<std::string>& get_permitted_paths() const {
    return permitted_paths_;
  }
  void set_permitted_paths(std::vector<std::string>&& permitted_paths) {
    permitted_paths_ = permitted_paths;
  }

  void add_soinfo(soinfo* si) {
    soinfo_list_.push_back(si);
  }

  void add_soinfos(const soinfo::soinfo_list_t& soinfos) {
    for (auto si : soinfos) {
      add_soinfo(si);
      si->add_secondary_namespace(this);
    }
  }

  void remove_soinfo(soinfo* si) {
    soinfo_list_.remove_if([&](soinfo* candidate) {
      return si == candidate;
    });
  }

  const soinfo::soinfo_list_t& soinfo_list() const { return soinfo_list_; }

  // For isolated namespaces - checks if the file is on the search path;
  // always returns true for not isolated namespace.
  bool is_accessible(const std::string& path);

 private:
  const char* name_;
  bool is_isolated_;
  std::vector<std::string> ld_library_paths_;
  std::vector<std::string> default_library_paths_;
  std::vector<std::string> permitted_paths_;
  soinfo::soinfo_list_t soinfo_list_;

  DISALLOW_COPY_AND_ASSIGN(android_namespace_t);
};

android_namespace_t g_default_namespace;

template<typename T>
class TypeBasedAllocator {
 public:
  static T* alloc() {
    return reinterpret_cast<T*>(::malloc(sizeof(T)));
  }

  static void free(T* ptr) {
    ::free(ptr);
    // FIXME: leak
  }

  void protect_all(int) {}
};

static TypeBasedAllocator<soinfo> g_soinfo_allocator;
static TypeBasedAllocator<LinkedListEntry<soinfo>> g_soinfo_links_allocator;

static std::unordered_map<uintptr_t, soinfo*> g_soinfo_handles_map;

static soinfo* solist;
static soinfo* sonext;
static soinfo* somain; // main process, always the one after libdl_info

LinkedListEntry<soinfo>* SoinfoListAllocator::alloc() {
  return g_soinfo_links_allocator.alloc();
}

void SoinfoListAllocator::free(LinkedListEntry<soinfo>* entry) {
  g_soinfo_links_allocator.free(entry);
}


class ProtectedDataGuard {
 public:
  ProtectedDataGuard() {
    if (ref_count_++ == 0) {
      protect_data(PROT_READ | PROT_WRITE);
    }
  }

  ~ProtectedDataGuard() {
    if (ref_count_ == 0) { // overflow
      fprintf(stderr, "[!] Too many nested calls to dlopen()");
      abort();
    }

    if (--ref_count_ == 0) {
      protect_data(PROT_READ);
    }
  }
 private:
  void protect_data(int protection) {
    g_soinfo_allocator.protect_all(protection);
    g_soinfo_links_allocator.protect_all(protection);
    //g_namespace_allocator.protect_all(protection);
    //g_namespace_list_allocator.protect_all(protection);
  }

  static size_t ref_count_;
};

size_t ProtectedDataGuard::ref_count_ = 0;


class LoadTask {
 public:
  struct deleter_t {
    void operator()(LoadTask* t) {
      t->~LoadTask();
      TypeBasedAllocator<LoadTask>::free(t);
    }
  };

  static deleter_t deleter;

  static LoadTask* create(const char* name, soinfo* needed_by,
                          std::unordered_map<const soinfo*, ElfReader>* readers_map, int hide=0) {
    LoadTask* ptr = TypeBasedAllocator<LoadTask>::alloc();
    return new (ptr) LoadTask(name, needed_by, readers_map, hide);
  }

  const char* get_name() const {
    return name_;
  }

  soinfo* get_needed_by() const {
    return needed_by_;
  }

  soinfo* get_soinfo() const {
    return si_;
  }

  void set_soinfo(soinfo* si) {
    si_ = si;
  }

  off64_t get_file_offset() const {
    return file_offset_;
  }

  void set_file_offset(off64_t offset) {
    file_offset_ = offset;
  }

  int get_fd() const {
    return fd_;
  }

  void set_fd(int fd, bool assume_ownership) {
    fd_ = fd;
    close_fd_ = assume_ownership;
  }

  const android_dlextinfo* get_extinfo() const {
    return extinfo_;
  }

  void set_extinfo(const android_dlextinfo* extinfo) {
    extinfo_ = extinfo;
  }

  bool is_dt_needed() const {
    return is_dt_needed_;
  }

  void set_dt_needed(bool is_dt_needed) {
    is_dt_needed_ = is_dt_needed;
  }

  const ElfReader& get_elf_reader() const {
    CHECK(si_ != nullptr);
    return (*elf_readers_map_)[si_];
  }

  ElfReader& get_elf_reader() {
    CHECK(si_ != nullptr);
    return (*elf_readers_map_)[si_];
  }

  std::unordered_map<const soinfo*, ElfReader>* get_readers_map() {
    return elf_readers_map_;
  }

  bool read(const char* realpath, off64_t file_size) {
    ElfReader& elf_reader = get_elf_reader();
    return elf_reader.Read(realpath, fd_, file_offset_, file_size);
  }

  bool load() {
    ElfReader& elf_reader = get_elf_reader();
    if (!elf_reader.Load(extinfo_, is_hidden)) {
      return false;
    }

    si_->base = elf_reader.load_start();
    si_->size = elf_reader.load_size();
    si_->set_mapped_by_caller(elf_reader.is_mapped_by_caller());
    si_->load_bias = elf_reader.load_bias();
    si_->phnum = elf_reader.phdr_count();
    si_->phdr = elf_reader.loaded_phdr();

    return true;
  }

 private:
  LoadTask(const char* name, soinfo* needed_by,
           std::unordered_map<const soinfo*, ElfReader>* readers_map,
           int hide)
    : name_(name), needed_by_(needed_by), si_(nullptr),
      fd_(-1), close_fd_(false), file_offset_(0), elf_readers_map_(readers_map),
      is_dt_needed_(false), is_hidden(hide) {}

  ~LoadTask() {
    if (fd_ != -1 && close_fd_) {
      close(fd_);
    }
  }

  const char* name_;
  soinfo* needed_by_;
  soinfo* si_;
  const android_dlextinfo* extinfo_;
  int fd_;
  bool close_fd_;
  off64_t file_offset_;
  std::unordered_map<const soinfo*, ElfReader>* elf_readers_map_;
  // TODO(dimitry): needed by workaround for http://b/26394120 (the grey-list)
  bool is_dt_needed_;
  bool is_hidden;
  // END OF WORKAROUND

  DISALLOW_IMPLICIT_CONSTRUCTORS(LoadTask);
};

LoadTask::deleter_t LoadTask::deleter;

template <typename T>
using linked_list_t = LinkedList<T, TypeBasedAllocator<LinkedListEntry<T>>>;

typedef linked_list_t<soinfo> SoinfoLinkedList;
typedef linked_list_t<const char> StringLinkedList;
typedef std::vector<LoadTask*> LoadTaskList;

// This function walks down the tree of soinfo dependencies
// in breadth-first order and
//   * calls action(soinfo* si) for each node, and
//   * terminates walk if action returns false.
//
// walk_dependencies_tree returns false if walk was terminated
// by the action and true otherwise.
template<typename F>
static bool walk_dependencies_tree(soinfo* root_soinfos[], size_t root_soinfos_size, F action) {
  SoinfoLinkedList visit_list;
  SoinfoLinkedList visited;
  int n = 0;
  for (size_t i = 0; i < root_soinfos_size; ++i) {
    // action(root_soinfos[i]);
    // break;
    visit_list.push_back(root_soinfos[i]);
    n++;
  }

  TRACE("visit_list: %d", n);
  soinfo* si;
  while ((si = visit_list.pop_front()) != nullptr) {
    TRACE("pop %p", si);
    if (visited.contains(si)) {
      continue;
    }

    TRACE("walk_dependencies_tree.action");
    if (!action(si)) {
      TRACE("walk_dependencies_tree.action failed");
      return false;
    }

    TRACE("walk_dependencies_tree.action successful");
    visited.push_back(si);

    si->get_children().for_each([&](soinfo* child) {
      visit_list.push_back(child);
    });
  }
  TRACE("walk_dependencies_tree: done");
  return true;
}

static bool is_symbol_global_and_defined(const soinfo* si, const ElfW(Sym)* s) {
  if (ELF_ST_BIND(s->st_info) == STB_GLOBAL ||
      ELF_ST_BIND(s->st_info) == STB_WEAK) {
    return s->st_shndx != SHN_UNDEF;
  } else if (ELF_ST_BIND(s->st_info) != STB_LOCAL) {
    DL_WARN("unexpected ST_BIND value: %d for \"%s\" in \"%s\"",
            ELF_ST_BIND(s->st_info), si->get_string(s->st_name), si->get_realpath());
  }

  return false;
}

static const ElfW(Versym) kVersymHiddenBit = 0x8000;

static inline bool is_versym_hidden(const ElfW(Versym)* versym) {
  // the symbol is hidden if bit 15 of versym is set.
  return versym != nullptr && (*versym & kVersymHiddenBit) != 0;
}

static inline bool check_symbol_version(const ElfW(Versym) verneed,
                                        const ElfW(Versym)* verdef) {
  return verneed == kVersymNotNeeded ||
      verdef == nullptr ||
      verneed == (*verdef & ~kVersymHiddenBit);
}

const ElfW(Versym)* soinfo::get_versym(size_t n) const {
  if (has_min_version(2) && versym_ != nullptr) {
    return versym_ + n;
  }

  return nullptr;
}

ElfW(Addr) soinfo::get_verneed_ptr() const {
  if (has_min_version(2)) {
    return verneed_ptr_;
  }

  return 0;
}

size_t soinfo::get_verneed_cnt() const {
  if (has_min_version(2)) {
    return verneed_cnt_;
  }

  return 0;
}

ElfW(Addr) soinfo::get_verdef_ptr() const {
  if (has_min_version(2)) {
    return verdef_ptr_;
  }

  return 0;
}

size_t soinfo::get_verdef_cnt() const {
  if (has_min_version(2)) {
    return verdef_cnt_;
  }

  return 0;
}

template<typename F>
static bool for_each_verdef(const soinfo* si, F functor) {
  if (!si->has_min_version(2)) {
    return true;
  }

  uintptr_t verdef_ptr = si->get_verdef_ptr();
  if (verdef_ptr == 0) {
    return true;
  }

  size_t offset = 0;

  size_t verdef_cnt = si->get_verdef_cnt();
  for (size_t i = 0; i<verdef_cnt; ++i) {
    const ElfW(Verdef)* verdef = reinterpret_cast<ElfW(Verdef)*>(verdef_ptr + offset);
    size_t verdaux_offset = offset + verdef->vd_aux;
    offset += verdef->vd_next;

    if (verdef->vd_version != 1) {
      DL_ERR("unsupported verdef[%zd] vd_version: %d (expected 1) library: %s",
          i, verdef->vd_version, si->get_realpath());
      return false;
    }

    if ((verdef->vd_flags & VER_FLG_BASE) != 0) {
      // "this is the version of the file itself.  It must not be used for
      //  matching a symbol. It can be used to match references."
      //
      // http://www.akkadia.org/drepper/symbol-versioning
      continue;
    }

    if (verdef->vd_cnt == 0) {
      DL_ERR("invalid verdef[%zd] vd_cnt == 0 (version without a name)", i);
      return false;
    }

    const ElfW(Verdaux)* verdaux = reinterpret_cast<ElfW(Verdaux)*>(verdef_ptr + verdaux_offset);

    if (functor(i, verdef, verdaux) == true) {
      break;
    }
  }

  return true;
}

bool soinfo::find_verdef_version_index(const version_info* vi, ElfW(Versym)* versym) const {
  if (vi == nullptr) {
    *versym = kVersymNotNeeded;
    return true;
  }

  *versym = kVersymGlobal;

  return for_each_verdef(this,
    [&](size_t, const ElfW(Verdef)* verdef, const ElfW(Verdaux)* verdaux) {
      if (verdef->vd_hash == vi->elf_hash &&
          strcmp(vi->name, get_string(verdaux->vda_name)) == 0) {
        *versym = verdef->vd_ndx;
        return true;
      }

      return false;
    }
  );
}

static uint32_t calculate_elf_hash(const char* name) {
  const uint8_t* name_bytes = reinterpret_cast<const uint8_t*>(name);
  uint32_t h = 0, g;

  while (*name_bytes) {
    h = (h << 4) + *name_bytes++;
    g = h & 0xf0000000;
    h ^= g;
    h ^= g >> 24;
  }

  return h;
}

uint32_t SymbolName::elf_hash() {
  if (!has_elf_hash_) {
    elf_hash_ = calculate_elf_hash(name_);
    has_elf_hash_ = true;
  }

  return elf_hash_;
}

uint32_t SymbolName::gnu_hash() {
  if (!has_gnu_hash_) {
    uint32_t h = 5381;
    const uint8_t* name = reinterpret_cast<const uint8_t*>(name_);
    while (*name != 0) {
      h += (h << 5) + *name++; // h*33 + c = h + h * 32 + c = h + h << 5 + c
    }

    gnu_hash_ =  h;
    has_gnu_hash_ = true;
  }

  return gnu_hash_;
}

bool soinfo::is_gnu_hash() const {
  return (flags_ & FLAG_GNU_HASH) != 0;
}

bool soinfo::gnu_lookup(SymbolName& symbol_name,
                        const version_info* vi,
                        uint32_t* symbol_index) const {
  uint32_t hash = symbol_name.gnu_hash();
  uint32_t h2 = hash >> gnu_shift2_;

  uint32_t bloom_mask_bits = sizeof(ElfW(Addr))*8;
  uint32_t word_num = (hash / bloom_mask_bits) & gnu_maskwords_;
  ElfW(Addr) bloom_word = gnu_bloom_filter_[word_num];

  *symbol_index = 0;

  TRACE_TYPE(LOOKUP, "SEARCH %s in %s@%p (gnu)",
      symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(base));

  // test against bloom filter
  if ((1 & (bloom_word >> (hash % bloom_mask_bits)) & (bloom_word >> (h2 % bloom_mask_bits))) == 0) {
    TRACE_TYPE(LOOKUP, "NOT FOUND %s in %s@%p",
        symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(base));

    return true;
  }

  // bloom test says "probably yes"...
  uint32_t n = gnu_bucket_[hash % gnu_nbucket_];

  if (n == 0) {
    TRACE_TYPE(LOOKUP, "NOT FOUND %s in %s@%p",
        symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(base));

    return true;
  }

  // lookup versym for the version definition in this library
  // note the difference between "version is not requested" (vi == nullptr)
  // and "version not found". In the first case verneed is kVersymNotNeeded
  // which implies that the default version can be accepted; the second case results in
  // verneed = 1 (kVersymGlobal) and implies that we should ignore versioned symbols
  // for this library and consider only *global* ones.
  ElfW(Versym) verneed = 0;
  if (!find_verdef_version_index(vi, &verneed)) {
    return false;
  }

  do {
    ElfW(Sym)* s = symtab_ + n;
    const ElfW(Versym)* verdef = get_versym(n);
    // skip hidden versions when verneed == kVersymNotNeeded (0)
    if (verneed == kVersymNotNeeded && is_versym_hidden(verdef)) {
        continue;
    }
    if (((gnu_chain_[n] ^ hash) >> 1) == 0 &&
        check_symbol_version(verneed, verdef) &&
        strcmp(get_string(s->st_name), symbol_name.get_name()) == 0 &&
        is_symbol_global_and_defined(this, s)) {
      TRACE_TYPE(LOOKUP, "FOUND %s in %s (%p) %zd",
          symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(s->st_value),
          static_cast<size_t>(s->st_size));
      *symbol_index = n;
      return true;
    }
  } while ((gnu_chain_[n++] & 1) == 0);

  TRACE_TYPE(LOOKUP, "NOT FOUND %s in %s@%p",
             symbol_name.get_name(), get_realpath(), reinterpret_cast<void*>(base));

  return true;
}

bool soinfo::elf_lookup(SymbolName& symbol_name,
                        const version_info* vi,
                        uint32_t* symbol_index) const {
  uint32_t hash = symbol_name.elf_hash();

  TRACE_TYPE(LOOKUP, "SEARCH %s in %s@%p h=%x(elf) %zd",
             symbol_name.get_name(), get_realpath(),
             reinterpret_cast<void*>(base), hash, hash % nbucket_);

  ElfW(Versym) verneed = 0;
  if (!find_verdef_version_index(vi, &verneed)) {
    return false;
  }

  for (uint32_t n = bucket_[hash % nbucket_]; n != 0; n = chain_[n]) {
    ElfW(Sym)* s = symtab_ + n;
    const ElfW(Versym)* verdef = get_versym(n);

    // skip hidden versions when verneed == 0
    if (verneed == kVersymNotNeeded && is_versym_hidden(verdef)) {
        continue;
    }

    if (check_symbol_version(verneed, verdef) &&
        strcmp(get_string(s->st_name), symbol_name.get_name()) == 0 &&
        is_symbol_global_and_defined(this, s)) {
      TRACE_TYPE(LOOKUP, "FOUND %s in %s (%p) %zd",
                 symbol_name.get_name(), get_realpath(),
                 reinterpret_cast<void*>(s->st_value),
                 static_cast<size_t>(s->st_size));
      *symbol_index = n;
      return true;
    }
  }

  TRACE_TYPE(LOOKUP, "NOT FOUND %s in %s@%p %x %zd",
             symbol_name.get_name(), get_realpath(),
             reinterpret_cast<void*>(base), hash, hash % nbucket_);

  *symbol_index = 0;
  return true;
}

bool soinfo::find_symbol_by_name(SymbolName& symbol_name,
                                 const version_info* vi,
                                 const ElfW(Sym)** symbol) const {
  uint32_t symbol_index;
  bool success =
      is_gnu_hash() ?
      gnu_lookup(symbol_name, vi, &symbol_index) :
      elf_lookup(symbol_name, vi, &symbol_index);

  if (success) {
    *symbol = symbol_index == 0 ? nullptr : symtab_ + symbol_index;
  }

  return success;
}

static const ElfW(Sym)* dlsym_handle_lookup(soinfo* root, soinfo* skip_until,
                                            soinfo** found, SymbolName& symbol_name,
                                            const version_info* vi) {
  const ElfW(Sym)* result = nullptr;
  bool skip_lookup = skip_until != nullptr;
  TRACE("dlsym_handle_lookup");
  walk_dependencies_tree(&root, 1, [&](soinfo* current_soinfo) {
    if (skip_lookup) {
      skip_lookup = current_soinfo != skip_until;
      return true;
    }

    if (!current_soinfo->find_symbol_by_name(symbol_name, vi, &result)) {
      result = nullptr;
      return false;
    }

    if (result != nullptr) {
      *found = current_soinfo;
      return false;
    }

    return true;
  });

  return result;
}

static const ElfW(Sym)* dlsym_linear_lookup(android_namespace_t* ns,
                                            const char* name,
                                            const version_info* vi,
                                            soinfo** found,
                                            soinfo* caller,
                                            void* handle);

// This is used by dlsym(3).  It performs symbol lookup only within the
// specified soinfo object and its dependencies in breadth first order.
static const ElfW(Sym)* dlsym_handle_lookup(soinfo* si, soinfo** found,
                                            const char* name, const version_info* vi) {
  // According to man dlopen(3) and posix docs in the case when si is handle
  // of the main executable we need to search not only in the executable and its
  // dependencies but also in all libraries loaded with RTLD_GLOBAL.
  //
  // Since RTLD_GLOBAL is always set for the main executable and all dt_needed shared
  // libraries and they are loaded in breath-first (correct) order we can just execute
  // dlsym(RTLD_DEFAULT, ...); instead of doing two stage lookup.
  if (si == somain) {
    return dlsym_linear_lookup(&g_default_namespace, name, vi, found, nullptr, RTLD_DEFAULT);
  }

  SymbolName symbol_name(name);
  return dlsym_handle_lookup(si, nullptr, found, symbol_name, vi);
}

/* This is used by dlsym(3) to performs a global symbol lookup. If the
   start value is null (for RTLD_DEFAULT), the search starts at the
   beginning of the global solist. Otherwise the search starts at the
   specified soinfo (for RTLD_NEXT).
 */
static const ElfW(Sym)* dlsym_linear_lookup(android_namespace_t* ns,
                                            const char* name,
                                            const version_info* vi,
                                            soinfo** found,
                                            soinfo* caller,
                                            void* handle) {
  SymbolName symbol_name(name);

  auto& soinfo_list = ns->soinfo_list();
  auto start = soinfo_list.begin();

  if (handle == RTLD_NEXT) {
    if (caller == nullptr) {
      return nullptr;
    } else {
      auto it = soinfo_list.find(caller);
      CHECK (it != soinfo_list.end());
      start = ++it;
    }
  }

  const ElfW(Sym)* s = nullptr;
  for (auto it = start, end = soinfo_list.end(); it != end; ++it) {
    soinfo* si = *it;
    // Do not skip RTLD_LOCAL libraries in dlsym(RTLD_DEFAULT, ...)
    // if the library is opened by application with target api level <= 22
    // See http://b/21565766
    if ((si->get_rtld_flags() & RTLD_GLOBAL) == 0 && si->get_target_sdk_version() > 22) {
      continue;
    }

    if (!si->find_symbol_by_name(symbol_name, vi, &s)) {
      return nullptr;
    }

    if (s != nullptr) {
      *found = si;
      break;
    }
  }

  // If not found - use dlsym_handle_lookup for caller's
  // local_group unless it is part of the global group in which
  // case we already did it.
  if (s == nullptr && caller != nullptr &&
      (caller->get_rtld_flags() & RTLD_GLOBAL) == 0) {
    return dlsym_handle_lookup(caller->get_local_group_root(),
        (handle == RTLD_NEXT) ? caller : nullptr, found, symbol_name, vi);
  }

  if (s != nullptr) {
    TRACE_TYPE(LOOKUP, "%s s->st_value = %p, found->base = %p",
               name, reinterpret_cast<void*>(s->st_value), reinterpret_cast<void*>((*found)->base));
  }

  return s;
}

soinfo* find_containing_library(const void* p) {
  ElfW(Addr) address = reinterpret_cast<ElfW(Addr)>(p);
  for (soinfo* si = solist; si != nullptr; si = si->next) {
    if (address >= si->base && address - si->base < si->size) {
      return si;
    }
  }
  return nullptr;
}

ElfW(Sym)* soinfo::find_symbol_by_address(const void* addr) {
  return is_gnu_hash() ? gnu_addr_lookup(addr) : elf_addr_lookup(addr);
}

static bool symbol_matches_soaddr(const ElfW(Sym)* sym, ElfW(Addr) soaddr) {
  return sym->st_shndx != SHN_UNDEF &&
      soaddr >= sym->st_value &&
      soaddr < sym->st_value + sym->st_size;
}

ElfW(Sym)* soinfo::gnu_addr_lookup(const void* addr) {
  ElfW(Addr) soaddr = reinterpret_cast<ElfW(Addr)>(addr) - load_bias;

  for (size_t i = 0; i < gnu_nbucket_; ++i) {
    uint32_t n = gnu_bucket_[i];

    if (n == 0) {
      continue;
    }

    do {
      ElfW(Sym)* sym = symtab_ + n;
      if (symbol_matches_soaddr(sym, soaddr)) {
        return sym;
      }
    } while ((gnu_chain_[n++] & 1) == 0);
  }

  return nullptr;
}

ElfW(Sym)* soinfo::elf_addr_lookup(const void* addr) {
  ElfW(Addr) soaddr = reinterpret_cast<ElfW(Addr)>(addr) - load_bias;

  // Search the library's symbol table for any defined symbol which
  // contains this address.
  for (size_t i = 0; i < nchain_; ++i) {
    ElfW(Sym)* sym = symtab_ + i;
    if (symbol_matches_soaddr(sym, soaddr)) {
      return sym;
    }
  }

  return nullptr;
}

// TODO: this is slightly unusual way to construct
// the global group for relocation. Not every RTLD_GLOBAL
// library is included in this group for backwards-compatibility
// reasons.
//
// This group consists of the main executable, LD_PRELOADs
// and libraries with the DF_1_GLOBAL flag set.
static soinfo::soinfo_list_t make_global_group(android_namespace_t* ns) {
  soinfo::soinfo_list_t global_group;
  ns->soinfo_list().for_each([&](soinfo* si) {
    if ((si->get_dt_flags_1() & DF_1_GLOBAL) != 0) {
      global_group.push_back(si);
    }
  });

  return global_group;
}

static soinfo* soinfo_alloc(android_namespace_t* ns, const char* name,
                            struct stat* file_stat, off64_t file_offset,
                            uint32_t rtld_flags) {
  if (strlen(name) >= PATH_MAX) {
    DL_ERR("library name \"%s\" too long", name);
    return nullptr;
  }

  soinfo* si = new (g_soinfo_allocator.alloc()) soinfo(ns, name, file_stat,
                                                       file_offset, rtld_flags);

  if (sonext){
    sonext->next = si;
  }
  sonext = si;

  si->generate_handle();
  ns->add_soinfo(si);

  TRACE("name %s: allocated soinfo @ %p", name, si);
  return si;
}

static void soinfo_free(soinfo* si) {
  if (si == nullptr) {
    return;
  }

  if (si->base != 0 && si->size != 0) {
    if (!si->is_mapped_by_caller()) {
      munmap(reinterpret_cast<void*>(si->base), si->size);
    } else {
      // remap the region as PROT_NONE, MAP_ANONYMOUS | MAP_NORESERVE
      mmap(reinterpret_cast<void*>(si->base), si->size, PROT_NONE,
           MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    }
  }

  soinfo *prev = nullptr, *trav;

  TRACE("name %s: freeing soinfo @ %p", si->get_realpath(), si);

  for (trav = solist; trav != nullptr; trav = trav->next) {
    if (trav == si) {
      break;
    }
    prev = trav;
  }

  if (trav == nullptr) {
    // si was not in solist
    DL_ERR("name \"%s\"@%p is not in solist!", si->get_realpath(), si);
    return;
  }

  // clear links to/from si
  si->remove_all_links();

  // prev will never be null, because the first entry in solist is
  // always the static libdl_info.
  prev->next = si->next;
  if (si == sonext) {
    sonext = prev;
  }

  si->~soinfo();
  g_soinfo_allocator.free(si);
}

void soinfo::call_array(const char* array_name __unused, linker_function_t* functions,
                        size_t count, bool reverse) {
  if (functions == nullptr) {
    return;
  }

  TRACE("[ Calling %s (size %zd) @ %p for \"%s\" ]", array_name, count, functions, get_realpath());

  int begin = reverse ? (count - 1) : 0;
  int end = reverse ? -1 : count;
  int step = reverse ? -1 : 1;

  for (int i = begin; i != end; i += step) {
    TRACE("[ %s[%d] == %p ]", array_name, i, functions[i]);
    call_function("function", functions[i]);
  }

  TRACE("[ Done calling %s for \"%s\" ]", array_name, get_realpath());
}

void soinfo::call_function(const char* function_name __unused, linker_function_t function) {
  if (function == nullptr || reinterpret_cast<uintptr_t>(function) == static_cast<uintptr_t>(-1)) {
    return;
  }

  TRACE("[ Calling %s @ %p for \"%s\" ]", function_name, function, get_realpath());
  function();
  TRACE("[ Done calling %s @ %p for \"%s\" ]", function_name, function, get_realpath());
}

void soinfo::call_pre_init_constructors() {
  // DT_PREINIT_ARRAY functions are called before any other constructors for executables,
  // but ignored in a shared library.
  call_array("DT_PREINIT_ARRAY", preinit_array_, preinit_array_count_, false);
}

void soinfo::call_constructors() {
  if (constructors_called) {
    return;
  }

  // We set constructors_called before actually calling the constructors, otherwise it doesn't
  // protect against recursive constructor calls. One simple example of constructor recursion
  // is the libc debug malloc, which is implemented in libc_malloc_debug_leak.so:
  // 1. The program depends on libc, so libc's constructor is called here.
  // 2. The libc constructor calls dlopen() to load libc_malloc_debug_leak.so.
  // 3. dlopen() calls the constructors on the newly created
  //    soinfo for libc_malloc_debug_leak.so.
  // 4. The debug .so depends on libc, so CallConstructors is
  //    called again with the libc soinfo. If it doesn't trigger the early-
  //    out above, the libc constructor will be called again (recursively!).
  constructors_called = true;

  if (!is_main_executable() && preinit_array_ != nullptr) {
    // The GNU dynamic linker silently ignores these, but we warn the developer.
    PRINT("\"%s\": ignoring DT_PREINIT_ARRAY in shared library!", get_realpath());
  }

  get_children().for_each([] (soinfo* si) {
    si->call_constructors();
  });

  TRACE("\"%s\": calling constructors", get_realpath());

  // DT_INIT should be called before DT_INIT_ARRAY if both are present.
  call_function("DT_INIT", init_func_);
  call_array("DT_INIT_ARRAY", init_array_, init_array_count_, false);
}

void soinfo::call_destructors() {
  if (!constructors_called) {
    return;
  }
  TRACE("\"%s\": calling destructors", get_realpath());

  // DT_FINI_ARRAY must be parsed in reverse order.
  call_array("DT_FINI_ARRAY", fini_array_, fini_array_count_, true);

  // DT_FINI should be called after DT_FINI_ARRAY if both are present.
  call_function("DT_FINI", fini_func_);

  // This is needed on second call to dlopen
  // after library has been unloaded with RTLD_NODELETE
  constructors_called = false;
}

dev_t soinfo::get_st_dev() const {
  if (has_min_version(0)) {
    return st_dev_;
  }

  return 0;
};

ino_t soinfo::get_st_ino() const {
  if (has_min_version(0)) {
    return st_ino_;
  }

  return 0;
}

off64_t soinfo::get_file_offset() const {
  if (has_min_version(1)) {
    return file_offset_;
  }

  return 0;
}

uint32_t soinfo::get_rtld_flags() const {
  if (has_min_version(1)) {
    return rtld_flags_;
  }

  return 0;
}

uint32_t soinfo::get_dt_flags_1() const {
  if (has_min_version(1)) {
    return dt_flags_1_;
  }

  return 0;
}

void soinfo::set_dt_flags_1(uint32_t dt_flags_1) {
  if (has_min_version(1)) {
    if ((dt_flags_1 & DF_1_GLOBAL) != 0) {
      rtld_flags_ |= RTLD_GLOBAL;
    }

    if ((dt_flags_1 & DF_1_NODELETE) != 0) {
      rtld_flags_ |= RTLD_NODELETE;
    }

    dt_flags_1_ = dt_flags_1;
  }
}

void soinfo::set_nodelete() {
  rtld_flags_ |= RTLD_NODELETE;
}

const char* soinfo::get_realpath() const {
#if defined(__work_around_b_24465209__)
  if (has_min_version(2)) {
    return realpath_.c_str();
  } else {
    return old_name_;
  }
#else
  return realpath_.c_str();
#endif
}

const char* soinfo::get_string(ElfW(Word) index) const {
  if (has_min_version(1) && (index >= strtab_size_)) {
    DL_ERR("%s: strtab out of bounds error; STRSZ=%zd, name=%d",
        get_realpath(), strtab_size_, index);
    abort();
  }

  return strtab_ + index;
}

void soinfo::set_soname(const char* soname) {
#if defined(__work_around_b_24465209__)
  if (has_min_version(2)) {
    soname_ = soname;
  }
  strlcpy(old_name_, soname_, sizeof(old_name_));
#else
  soname_ = soname;
#endif
}

const char* soinfo::get_soname() const {
#if defined(__work_around_b_24465209__)
  if (has_min_version(2)) {
    return soname_;
  } else {
    return old_name_;
  }
#else
  return soname_;
#endif
}

bool soinfo::is_linked() const {
  return (flags_ & FLAG_LINKED) != 0;
}

bool soinfo::is_main_executable() const {
  return (flags_ & FLAG_EXE) != 0;
}

bool soinfo::is_linker() const {
  return (flags_ & FLAG_LINKER) != 0;
}

void soinfo::set_linked() {
  flags_ |= FLAG_LINKED;
}

void soinfo::set_linker_flag() {
  flags_ |= FLAG_LINKER;
}

void soinfo::set_main_executable() {
  flags_ |= FLAG_EXE;
}

void soinfo::increment_ref_count() {
  local_group_root_->ref_count_++;
}

size_t soinfo::decrement_ref_count() {
  return --local_group_root_->ref_count_;
}

void soinfo::set_mapped_by_caller(bool mapped_by_caller) {
  if (mapped_by_caller) {
    flags_ |= FLAG_MAPPED_BY_CALLER;
  } else {
    flags_ &= ~FLAG_MAPPED_BY_CALLER;
  }
}

bool soinfo::is_mapped_by_caller() const {
  return (flags_ & FLAG_MAPPED_BY_CALLER) != 0;
}

void soinfo::set_dt_runpath(const char* path) {
  if (!has_min_version(3)) {
    return;
  }
  /*
  std::vector<std::string> runpaths;

  split_path(path, ":", &runpaths);

  std::string origin = dirname(get_realpath());
  // FIXME: add $LIB and $PLATFORM.
  std::pair<std::string, std::string> substs[] = {{"ORIGIN", origin}};
  for (auto&& s : runpaths) {
    size_t pos = 0;
    while (pos < s.size()) {
      pos = s.find("$", pos);
      if (pos == std::string::npos) break;
      for (const auto& subst : substs) {
        const std::string& token = subst.first;
        const std::string& replacement = subst.second;
        if (s.substr(pos + 1, token.size()) == token) {
          s.replace(pos, token.size() + 1, replacement);
          // -1 to compensate for the ++pos below.
          pos += replacement.size() - 1;
          break;
        } else if (s.substr(pos + 1, token.size() + 2) == "{" + token + "}") {
          s.replace(pos, token.size() + 3, replacement);
          pos += replacement.size() - 1;
          break;
        }
      }
      // Skip $ in case it did not match any of the known substitutions.
      ++pos;
    }
  }

  resolve_paths(runpaths, &dt_runpath_);
  */
  // FIXME
}

soinfo::soinfo(android_namespace_t* ns, const char* realpath,
               const struct stat* file_stat, off64_t file_offset,
               int rtld_flags) {
  memset(this, 0, sizeof(*this));

  if (realpath != nullptr) {
    realpath_ = realpath;
  }

  flags_ = FLAG_NEW_SOINFO;
  version_ = SOINFO_VERSION;

  if (file_stat != nullptr) {
    this->st_dev_ = file_stat->st_dev;
    this->st_ino_ = file_stat->st_ino;
    this->file_offset_ = file_offset;
  }

  this->rtld_flags_ = rtld_flags;
  this->primary_namespace_ = ns;
}

soinfo::~soinfo() {
  g_soinfo_handles_map.erase(handle_);
}

// This is a return on get_children()/get_parents() if
// 'this->flags' does not have FLAG_NEW_SOINFO set.
static soinfo::soinfo_list_t g_empty_list;

soinfo::soinfo_list_t& soinfo::get_children() {
  if (has_min_version(0)) {
    return children_;
  }

  return g_empty_list;
}

const soinfo::soinfo_list_t& soinfo::get_children() const {
  if (has_min_version(0)) {
    return children_;
  }

  return g_empty_list;
}

soinfo::soinfo_list_t& soinfo::get_parents() {
  if (has_min_version(0)) {
    return parents_;
  }

  return g_empty_list;
}

uintptr_t soinfo::get_handle() const {
  CHECK(has_min_version(3));
  CHECK(handle_ != 0);
  return handle_;
}

void* soinfo::to_handle() {
  if (get_application_target_sdk_version() <= 23 || !has_min_version(3)) {
    return this;
  }

  return reinterpret_cast<void*>(get_handle());
}

void soinfo::generate_handle() {
  CHECK(has_min_version(3));
  CHECK(handle_ == 0); // Make sure this is the first call

  // Make sure the handle is unique and does not collide
  // with special values which are RTLD_DEFAULT and RTLD_NEXT.
  do {
    arc4random_buf(&handle_, sizeof(handle_));
    // the least significant bit for the handle is always 1
    // making it easy to test the type of handle passed to
    // dl* functions.
    handle_ = handle_ | 1;
  } while (handle_ == reinterpret_cast<uintptr_t>(RTLD_DEFAULT) ||
           handle_ == reinterpret_cast<uintptr_t>(RTLD_NEXT) ||
           g_soinfo_handles_map.find(handle_) != g_soinfo_handles_map.end());

  g_soinfo_handles_map[handle_] = this;
}

uint32_t get_application_target_sdk_version(){
  // FIXME:
  return 24;
}

// This function returns api-level at the time of
// dlopen/load. Note that libraries opened by system
// will always have 'current' api level.
uint32_t soinfo::get_target_sdk_version() const {
  /*
  if (!has_min_version(2)) {
    return __ANDROID_API__;
  }

  return local_group_root_->target_sdk_version_;
  */
  return get_application_target_sdk_version();
}


void soinfo::add_child(soinfo* child) {
  if (has_min_version(0)) {
    child->parents_.push_back(this);
    this->children_.push_back(child);
  }
}

static ElfW(Addr) call_ifunc_resolver(ElfW(Addr) resolver_addr) {
  typedef ElfW(Addr) (*ifunc_resolver_t)(void);
  ifunc_resolver_t ifunc_resolver = reinterpret_cast<ifunc_resolver_t>(resolver_addr);
  ElfW(Addr) ifunc_addr = ifunc_resolver();
  TRACE_TYPE(RELO, "Called ifunc_resolver@%p. The result is %p",
      ifunc_resolver, reinterpret_cast<void*>(ifunc_addr));

  return ifunc_addr;
}

bool soinfo::prelink_image() {
  /* Extract dynamic section */
  ElfW(Word) dynamic_flags = 0;
  phdr_table_get_dynamic_section(phdr, phnum, load_bias, &dynamic, &dynamic_flags);

  /* We can't log anything until the linker is relocated */
  bool relocating_linker = (flags_ & FLAG_LINKER) != 0;
  if (!relocating_linker) {
    INFO("[ Linking \"%s\" ]", get_realpath());
    DEBUG("si->base = %p si->flags = 0x%08x", reinterpret_cast<void*>(base), flags_);
  }

  if (dynamic == nullptr) {
    if (!relocating_linker) {
      DL_ERR("missing PT_DYNAMIC in \"%s\"", get_realpath());
    }
    return false;
  } else {
    if (!relocating_linker) {
      DEBUG("dynamic = %p", dynamic);
    }
  }

#if defined(__arm__)
  (void) phdr_table_get_arm_exidx(phdr, phnum, load_bias,
                                  &ARM_exidx, &ARM_exidx_count);
#endif

  // Extract useful information from dynamic section.
  // Note that: "Except for the DT_NULL element at the end of the array,
  // and the relative order of DT_NEEDED elements, entries may appear in any order."
  //
  // source: http://www.sco.com/developers/gabi/1998-04-29/ch5.dynamic.html
  uint32_t needed_count = 0;
  for (ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d) {
    DEBUG("d = %p, d[0](tag) = %p d[1](val) = %p",
          d, reinterpret_cast<void*>(d->d_tag), reinterpret_cast<void*>(d->d_un.d_val));
    switch (d->d_tag) {
      case DT_SONAME:
        // this is parsed after we have strtab initialized (see below).
        break;

      case DT_HASH:
        nbucket_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[0];
        nchain_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[1];
        bucket_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr + 8);
        chain_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr + 8 + nbucket_ * 4);
        break;

      case DT_GNU_HASH:
        gnu_nbucket_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[0];
        // skip symndx
        gnu_maskwords_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[2];
        gnu_shift2_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[3];

        gnu_bloom_filter_ = reinterpret_cast<ElfW(Addr)*>(load_bias + d->d_un.d_ptr + 16);
        gnu_bucket_ = reinterpret_cast<uint32_t*>(gnu_bloom_filter_ + gnu_maskwords_);
        // amend chain for symndx = header[1]
        gnu_chain_ = gnu_bucket_ + gnu_nbucket_ -
            reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[1];

        if (!powerof2(gnu_maskwords_)) {
          DL_ERR("invalid maskwords for gnu_hash = 0x%x, in \"%s\" expecting power to two",
              gnu_maskwords_, get_realpath());
          return false;
        }
        --gnu_maskwords_;

        flags_ |= FLAG_GNU_HASH;
        break;

      case DT_STRTAB:
        strtab_ = reinterpret_cast<const char*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_STRSZ:
        strtab_size_ = d->d_un.d_val;
        break;

      case DT_SYMTAB:
        symtab_ = reinterpret_cast<ElfW(Sym)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_SYMENT:
        if (d->d_un.d_val != sizeof(ElfW(Sym))) {
          DL_ERR("invalid DT_SYMENT: %zd in \"%s\"",
              static_cast<size_t>(d->d_un.d_val), get_realpath());
          return false;
        }
        break;

      case DT_PLTREL:
#if defined(USE_RELA)
        if (d->d_un.d_val != DT_RELA) {
          DL_ERR("unsupported DT_PLTREL in \"%s\"; expected DT_RELA", get_realpath());
          return false;
        }
#else
        if (d->d_un.d_val != DT_REL) {
          DL_ERR("unsupported DT_PLTREL in \"%s\"; expected DT_REL", get_realpath());
          return false;
        }
#endif
        break;

      case DT_JMPREL:
#if defined(USE_RELA)
        plt_rela_ = reinterpret_cast<ElfW(Rela)*>(load_bias + d->d_un.d_ptr);
#else
        plt_rel_ = reinterpret_cast<ElfW(Rel)*>(load_bias + d->d_un.d_ptr);
#endif
        break;

      case DT_PLTRELSZ:
#if defined(USE_RELA)
        plt_rela_count_ = d->d_un.d_val / sizeof(ElfW(Rela));
#else
        plt_rel_count_ = d->d_un.d_val / sizeof(ElfW(Rel));
#endif
        break;

      case DT_PLTGOT:
#if defined(__mips__)
        // Used by mips and mips64.
        plt_got_ = reinterpret_cast<ElfW(Addr)**>(load_bias + d->d_un.d_ptr);
#endif
        // Ignore for other platforms... (because RTLD_LAZY is not supported)
        break;

      case DT_DEBUG:
        // Set the DT_DEBUG entry to the address of _r_debug for GDB
        // if the dynamic table is writable
// FIXME: not working currently for N64
// The flags for the LOAD and DYNAMIC program headers do not agree.
// The LOAD section containing the dynamic table has been mapped as
// read-only, but the DYNAMIC header claims it is writable.
#if !(defined(__mips__) && defined(__LP64__))
        if ((dynamic_flags & PF_W) != 0) {
          d->d_un.d_val = reinterpret_cast<uintptr_t>(&_r_debug);
        }
#endif
        break;
#if defined(USE_RELA)
      case DT_RELA:
        rela_ = reinterpret_cast<ElfW(Rela)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_RELASZ:
        rela_count_ = d->d_un.d_val / sizeof(ElfW(Rela));
        break;

      case DT_ANDROID_RELA:
        android_relocs_ = reinterpret_cast<uint8_t*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_ANDROID_RELASZ:
        android_relocs_size_ = d->d_un.d_val;
        break;

      case DT_ANDROID_REL:
        DL_ERR("unsupported DT_ANDROID_REL in \"%s\"", get_realpath());
        return false;

      case DT_ANDROID_RELSZ:
        DL_ERR("unsupported DT_ANDROID_RELSZ in \"%s\"", get_realpath());
        return false;

      case DT_RELAENT:
        if (d->d_un.d_val != sizeof(ElfW(Rela))) {
          DL_ERR("invalid DT_RELAENT: %zd", static_cast<size_t>(d->d_un.d_val));
          return false;
        }
        break;

      // ignored (see DT_RELCOUNT comments for details)
      case DT_RELACOUNT:
        break;

      case DT_REL:
        DL_ERR("unsupported DT_REL in \"%s\"", get_realpath());
        return false;

      case DT_RELSZ:
        DL_ERR("unsupported DT_RELSZ in \"%s\"", get_realpath());
        return false;

#else
      case DT_REL:
        rel_ = reinterpret_cast<ElfW(Rel)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_RELSZ:
        rel_count_ = d->d_un.d_val / sizeof(ElfW(Rel));
        break;

      case DT_RELENT:
        if (d->d_un.d_val != sizeof(ElfW(Rel))) {
          DL_ERR("invalid DT_RELENT: %zd", static_cast<size_t>(d->d_un.d_val));
          return false;
        }
        break;

      case DT_ANDROID_REL:
        android_relocs_ = reinterpret_cast<uint8_t*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_ANDROID_RELSZ:
        android_relocs_size_ = d->d_un.d_val;
        break;

      case DT_ANDROID_RELA:
        DL_ERR("unsupported DT_ANDROID_RELA in \"%s\"", get_realpath());
        return false;

      case DT_ANDROID_RELASZ:
        DL_ERR("unsupported DT_ANDROID_RELASZ in \"%s\"", get_realpath());
        return false;

      // "Indicates that all RELATIVE relocations have been concatenated together,
      // and specifies the RELATIVE relocation count."
      //
      // TODO: Spec also mentions that this can be used to optimize relocation process;
      // Not currently used by bionic linker - ignored.
      case DT_RELCOUNT:
        break;

      case DT_RELA:
        DL_ERR("unsupported DT_RELA in \"%s\"", get_realpath());
        return false;

      case DT_RELASZ:
        DL_ERR("unsupported DT_RELASZ in \"%s\"", get_realpath());
        return false;

#endif
      case DT_INIT:
        init_func_ = reinterpret_cast<linker_function_t>(load_bias + d->d_un.d_ptr);
        DEBUG("%s constructors (DT_INIT) found at %p", get_realpath(), init_func_);
        break;

      case DT_FINI:
        fini_func_ = reinterpret_cast<linker_function_t>(load_bias + d->d_un.d_ptr);
        DEBUG("%s destructors (DT_FINI) found at %p", get_realpath(), fini_func_);
        break;

      case DT_INIT_ARRAY:
        init_array_ = reinterpret_cast<linker_function_t*>(load_bias + d->d_un.d_ptr);
        DEBUG("%s constructors (DT_INIT_ARRAY) found at %p", get_realpath(), init_array_);
        break;

      case DT_INIT_ARRAYSZ:
        init_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
        break;

      case DT_FINI_ARRAY:
        fini_array_ = reinterpret_cast<linker_function_t*>(load_bias + d->d_un.d_ptr);
        DEBUG("%s destructors (DT_FINI_ARRAY) found at %p", get_realpath(), fini_array_);
        break;

      case DT_FINI_ARRAYSZ:
        fini_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
        break;

      case DT_PREINIT_ARRAY:
        preinit_array_ = reinterpret_cast<linker_function_t*>(load_bias + d->d_un.d_ptr);
        DEBUG("%s constructors (DT_PREINIT_ARRAY) found at %p", get_realpath(), preinit_array_);
        break;

      case DT_PREINIT_ARRAYSZ:
        preinit_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
        break;

      case DT_TEXTREL:
#if defined(__LP64__)
        DL_ERR("text relocations (DT_TEXTREL) found in 64-bit ELF file \"%s\"", get_realpath());
        return false;
#else
        has_text_relocations = true;
        break;
#endif

      case DT_SYMBOLIC:
        has_DT_SYMBOLIC = true;
        break;

      case DT_NEEDED:
        ++needed_count;
        break;

      case DT_FLAGS:
        if (d->d_un.d_val & DF_TEXTREL) {
#if defined(__LP64__)
          DL_ERR("text relocations (DF_TEXTREL) found in 64-bit ELF file \"%s\"", get_realpath());
          return false;
#else
          has_text_relocations = true;
#endif
        }
        if (d->d_un.d_val & DF_SYMBOLIC) {
          has_DT_SYMBOLIC = true;
        }
        break;

      case DT_FLAGS_1:
        set_dt_flags_1(d->d_un.d_val);

        if ((d->d_un.d_val & ~SUPPORTED_DT_FLAGS_1) != 0) {
          DL_WARN("%s: unsupported flags DT_FLAGS_1=%p", get_realpath(), reinterpret_cast<void*>(d->d_un.d_val));
        }
        break;
#if defined(__mips__)
      case DT_MIPS_RLD_MAP:
        // Set the DT_MIPS_RLD_MAP entry to the address of _r_debug for GDB.
        {
          r_debug** dp = reinterpret_cast<r_debug**>(load_bias + d->d_un.d_ptr);
          *dp = &_r_debug;
        }
        break;
      case DT_MIPS_RLD_MAP2:
        // Set the DT_MIPS_RLD_MAP2 entry to the address of _r_debug for GDB.
        {
          r_debug** dp = reinterpret_cast<r_debug**>(
              reinterpret_cast<ElfW(Addr)>(d) + d->d_un.d_val);
          *dp = &_r_debug;
        }
        break;

      case DT_MIPS_RLD_VERSION:
      case DT_MIPS_FLAGS:
      case DT_MIPS_BASE_ADDRESS:
      case DT_MIPS_UNREFEXTNO:
        break;

      case DT_MIPS_SYMTABNO:
        mips_symtabno_ = d->d_un.d_val;
        break;

      case DT_MIPS_LOCAL_GOTNO:
        mips_local_gotno_ = d->d_un.d_val;
        break;

      case DT_MIPS_GOTSYM:
        mips_gotsym_ = d->d_un.d_val;
        break;
#endif
      // Ignored: "Its use has been superseded by the DF_BIND_NOW flag"
      case DT_BIND_NOW:
        break;

      case DT_VERSYM:
        versym_ = reinterpret_cast<ElfW(Versym)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_VERDEF:
        verdef_ptr_ = load_bias + d->d_un.d_ptr;
        break;
      case DT_VERDEFNUM:
        verdef_cnt_ = d->d_un.d_val;
        break;

      case DT_VERNEED:
        verneed_ptr_ = load_bias + d->d_un.d_ptr;
        break;

      case DT_VERNEEDNUM:
        verneed_cnt_ = d->d_un.d_val;
        break;

      case DT_RUNPATH:
        // this is parsed after we have strtab initialized (see below).
        break;

      default:
        if (!relocating_linker) {
          DL_WARN("%s: unused DT entry: type %p arg %p", get_realpath(),
              reinterpret_cast<void*>(d->d_tag), reinterpret_cast<void*>(d->d_un.d_val));
        }
        break;
    }
  }

#if defined(__mips__) && !defined(__LP64__)
  if (!mips_check_and_adjust_fp_modes()) {
    return false;
  }
#endif

  DEBUG("si->base = %p, si->strtab = %p, si->symtab = %p",
        reinterpret_cast<void*>(base), strtab_, symtab_);

  TRACE("Sanity begin!");

  // Sanity checks.
  if (relocating_linker && needed_count != 0) {
    DL_ERR("linker cannot have DT_NEEDED dependencies on other libraries");
    return false;
  }
  if (nbucket_ == 0 && gnu_nbucket_ == 0) {
    DL_ERR("empty/missing DT_HASH/DT_GNU_HASH in \"%s\" "
        "(new hash type from the future?)", get_realpath());
    return false;
  }
  if (strtab_ == 0) {
    DL_ERR("empty/missing DT_STRTAB in \"%s\"", get_realpath());
    return false;
  }
  if (symtab_ == 0) {
    DL_ERR("empty/missing DT_SYMTAB in \"%s\"", get_realpath());
    return false;
  }

  TRACE("Sanity passed!");
  // second pass - parse entries relying on strtab
  for (ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d) {
    switch (d->d_tag) {
      case DT_SONAME:
        set_soname(get_string(d->d_un.d_val));
        break;
      case DT_RUNPATH:
        set_dt_runpath(get_string(d->d_un.d_val));
        break;
    }
  }

  TRACE("second pass passed!");
  // Before M release linker was using basename in place of soname.
  // In the case when dt_soname is absent some apps stop working
  // because they can't find dt_needed library by soname.
  // This workaround should keep them working. (applies only
  // for apps targeting sdk version <=22). Make an exception for
  // the main executable and linker; they do not need to have dt_soname
  if (soname_ == nullptr && this != somain && (flags_ & FLAG_LINKER) == 0 &&
      get_application_target_sdk_version() <= 22) {
    soname_ = basename(realpath_.c_str());
    DL_WARN("%s: is missing DT_SONAME will use basename as a replacement: \"%s\"",
        get_realpath(), soname_);
    // Don't call add_dlwarning because a missing DT_SONAME isn't important enough to show in the UI
  }
  TRACE("%s returns true", __FUNCTION__);
  return true;
}

bool soinfo::link_image(const soinfo_list_t& global_group, const soinfo_list_t& local_group,
                        const android_dlextinfo* extinfo) {
  TRACE("link_image: begin");
  local_group_root_ = local_group.front();
  if (local_group_root_ == nullptr) {
    local_group_root_ = this;
  }

  if ((flags_ & FLAG_LINKER) == 0 && local_group_root_ == this) {
    target_sdk_version_ = get_application_target_sdk_version();
  }

  VersionTracker version_tracker;

  if (!version_tracker.init(this)) {
    return false;
  }

#if !defined(__LP64__)
  if (has_text_relocations) {
    // Fail if app is targeting sdk version > 22
    if (get_application_target_sdk_version() > 22) {
      PRINT("%s: has text relocations", get_realpath());
      DL_ERR("%s: has text relocations", get_realpath());
      return false;
    }
    // Make segments writable to allow text relocations to work properly. We will later call
    // phdr_table_protect_segments() after all of them are applied.
    DL_WARN("%s has text relocations. This is wasting memory and prevents "
            "security hardening. Please fix.", get_realpath());
    // add_dlwarning(get_realpath(), "text relocations");
    if (phdr_table_unprotect_segments(phdr, phnum, load_bias) < 0) {
      DL_ERR("can't unprotect loadable segments for \"%s\": %s",
             get_realpath(), strerror(errno));
      return false;
    }
  }
#endif

  if (android_relocs_ != nullptr) {
    // check signature
    if (android_relocs_size_ > 3 &&
        android_relocs_[0] == 'A' &&
        android_relocs_[1] == 'P' &&
        android_relocs_[2] == 'S' &&
        android_relocs_[3] == '2') {
      DEBUG("[ android relocating %s ]", get_realpath());

      bool relocated = false;
      const uint8_t* packed_relocs = android_relocs_ + 4;
      const size_t packed_relocs_size = android_relocs_size_ - 4;

      relocated = relocate(
          version_tracker,
          packed_reloc_iterator<sleb128_decoder>(
            sleb128_decoder(packed_relocs, packed_relocs_size)),
          global_group, local_group);

      if (!relocated) {
        return false;
      }
    } else {
      DL_ERR("bad android relocation header.");
      return false;
    }
  }

#if defined(USE_RELA)
  if (rela_ != nullptr) {
    DEBUG("[ relocating %s ]", get_realpath());
    if (!relocate(version_tracker,
            plain_reloc_iterator(rela_, rela_count_), global_group, local_group)) {
      return false;
    }
  }
  if (plt_rela_ != nullptr) {
    DEBUG("[ relocating %s plt ]", get_realpath());
    if (!relocate(version_tracker,
            plain_reloc_iterator(plt_rela_, plt_rela_count_), global_group, local_group)) {
      return false;
    }
  }
#else
  if (rel_ != nullptr) {
    DEBUG("[ relocating %s ]", get_realpath());
    if (!relocate(version_tracker,
            plain_reloc_iterator(rel_, rel_count_), global_group, local_group)) {
      return false;
    }
  }
  if (plt_rel_ != nullptr) {
    DEBUG("[ relocating %s plt ]", get_realpath());
    if (!relocate(version_tracker,
            plain_reloc_iterator(plt_rel_, plt_rel_count_), global_group, local_group)) {
      return false;
    }
  }
#endif

#if defined(__mips__)
  if (!mips_relocate_got(version_tracker, global_group, local_group)) {
    return false;
  }
#endif

  DEBUG("[ finished linking %s ]", get_realpath());

#if !defined(__LP64__)
  if (has_text_relocations) {
    // All relocations are done, we can protect our segments back to read-only.
    if (phdr_table_protect_segments(phdr, phnum, load_bias) < 0) {
      DL_ERR("can't protect segments for \"%s\": %s",
             get_realpath(), strerror(errno));
      return false;
    }
  }
#endif

  // We can also turn on GNU RELRO protection if we're not linking the dynamic linker
  // itself --- it can't make system calls yet, and will have to call protect_relro later.
  if (!is_linker() && !protect_relro()) {
    return false;
  }

  /* Handle serializing/sharing the RELRO segment */
  if (extinfo && (extinfo->flags & ANDROID_DLEXT_WRITE_RELRO)) {
    if (phdr_table_serialize_gnu_relro(phdr, phnum, load_bias,
                                       extinfo->relro_fd) < 0) {
      DL_ERR("failed serializing GNU RELRO section for \"%s\": %s",
             get_realpath(), strerror(errno));
      return false;
    }
  } else if (extinfo && (extinfo->flags & ANDROID_DLEXT_USE_RELRO)) {
    if (phdr_table_map_gnu_relro(phdr, phnum, load_bias,
                                 extinfo->relro_fd) < 0) {
      DL_ERR("failed mapping GNU RELRO section for \"%s\": %s",
             get_realpath(), strerror(errno));
      return false;
    }
  }

  // notify_gdb_of_load(this);
  TRACE("[+] notify_gdb_of_load %p", this);
  return true;
}

bool soinfo::protect_relro() {
  if (phdr_table_protect_gnu_relro(phdr, phnum, load_bias) < 0) {
    DL_ERR("can't enable GNU RELRO protection for \"%s\": %s",
           get_realpath(), strerror(errno));
    return false;
  }
  return true;
}

ElfW(Addr) soinfo::resolve_symbol_address(const ElfW(Sym)* s) const {
  if (ELF_ST_TYPE(s->st_info) == STT_GNU_IFUNC) {
    return call_ifunc_resolver(s->st_value + load_bias);
  }

  return static_cast<ElfW(Addr)>(s->st_value + load_bias);
}

bool soinfo_do_lookup(soinfo* si_from, const char* name, const version_info* vi,
                      soinfo** si_found_in, const soinfo::soinfo_list_t& global_group,
                      const soinfo::soinfo_list_t& local_group, const ElfW(Sym)** symbol) {
  SymbolName symbol_name(name);
  const ElfW(Sym)* s = nullptr;

  /* "This element's presence in a shared object library alters the dynamic linker's
   * symbol resolution algorithm for references within the library. Instead of starting
   * a symbol search with the executable file, the dynamic linker starts from the shared
   * object itself. If the shared object fails to supply the referenced symbol, the
   * dynamic linker then searches the executable file and other shared objects as usual."
   *
   * http://www.sco.com/developers/gabi/2012-12-31/ch5.dynamic.html
   *
   * Note that this is unlikely since static linker avoids generating
   * relocations for -Bsymbolic linked dynamic executables.
   */
  if (si_from->has_DT_SYMBOLIC) {
    DEBUG("%s: looking up %s in local scope (DT_SYMBOLIC)", si_from->get_realpath(), name);
    if (!si_from->find_symbol_by_name(symbol_name, vi, &s)) {
      return false;
    }

    if (s != nullptr) {
      *si_found_in = si_from;
    }
  }

  // 1. Look for it in global_group
  if (s == nullptr) {
    bool error = false;
    global_group.visit([&](soinfo* global_si) {
      DEBUG("%s: looking up %s in %s (from global group)",
          si_from->get_realpath(), name, global_si->get_realpath());
      if (!global_si->find_symbol_by_name(symbol_name, vi, &s)) {
        error = true;
        return false;
      }

      if (s != nullptr) {
        *si_found_in = global_si;
        return false;
      }

      return true;
    });

    if (error) {
      return false;
    }
  }

  // 2. Look for it in the local group
  if (s == nullptr) {
    bool error = false;
    local_group.visit([&](soinfo* local_si) {
      if (local_si == si_from && si_from->has_DT_SYMBOLIC) {
        // we already did this - skip
        return true;
      }

      DEBUG("%s: looking up %s in %s (from local group)",
          si_from->get_realpath(), name, local_si->get_realpath());
      if (!local_si->find_symbol_by_name(symbol_name, vi, &s)) {
        error = true;
        return false;
      }

      if (s != nullptr) {
        *si_found_in = local_si;
        return false;
      }

      return true;
    });

    if (error) {
      return false;
    }
  }

  if (s != nullptr) {
    TRACE_TYPE(LOOKUP, "si %s sym %s s->st_value = %p, "
               "found in %s, base = %p, load bias = %p",
               si_from->get_realpath(), name, reinterpret_cast<void*>(s->st_value),
               (*si_found_in)->get_realpath(), reinterpret_cast<void*>((*si_found_in)->base),
               reinterpret_cast<void*>((*si_found_in)->load_bias));
  }

  *symbol = s;
  return true;
}

const version_info* VersionTracker::get_version_info(ElfW(Versym) source_symver) const {
  if (source_symver < 2 ||
      source_symver >= version_infos.size() ||
      version_infos[source_symver].name == nullptr) {
    return nullptr;
  }

  return &version_infos[source_symver];
}

bool soinfo::lookup_version_info(const VersionTracker& version_tracker, ElfW(Word) sym,
                                 const char* sym_name, const version_info** vi) {
  const ElfW(Versym)* sym_ver_ptr = get_versym(sym);
  ElfW(Versym) sym_ver = sym_ver_ptr == nullptr ? 0 : *sym_ver_ptr;

  if (sym_ver != VER_NDX_LOCAL && sym_ver != VER_NDX_GLOBAL) {
    *vi = version_tracker.get_version_info(sym_ver);

    if (*vi == nullptr) {
      DL_ERR("cannot find verneed/verdef for version index=%d "
          "referenced by symbol \"%s\" at \"%s\"", sym_ver, sym_name, get_realpath());
      return false;
    }
  } else {
    // there is no version info
    *vi = nullptr;
  }

  return true;
}

#if !defined(__mips__)
#if defined(USE_RELA)
static ElfW(Addr) get_addend(ElfW(Rela)* rela, ElfW(Addr) reloc_addr __unused) {
  return rela->r_addend;
}
#else
static ElfW(Addr) get_addend(ElfW(Rel)* rel, ElfW(Addr) reloc_addr) {
  if (ELFW(R_TYPE)(rel->r_info) == R_GENERIC_RELATIVE ||
      ELFW(R_TYPE)(rel->r_info) == R_GENERIC_IRELATIVE) {
    return *reinterpret_cast<ElfW(Addr)*>(reloc_addr);
  }
  return 0;
}
#endif

template<typename ElfRelIteratorT>
bool soinfo::relocate(const VersionTracker& version_tracker, ElfRelIteratorT&& rel_iterator,
                      const soinfo_list_t& global_group, const soinfo_list_t& local_group) {
  for (size_t idx = 0; rel_iterator.has_next(); ++idx) {
    const auto rel = rel_iterator.next();
    if (rel == nullptr) {
      return false;
    }

    ElfW(Word) type = ELFW(R_TYPE)(rel->r_info);
    ElfW(Word) sym = ELFW(R_SYM)(rel->r_info);

    ElfW(Addr) reloc = static_cast<ElfW(Addr)>(rel->r_offset + load_bias);
    ElfW(Addr) sym_addr = 0;
    const char* sym_name = nullptr;
    ElfW(Addr) addend = get_addend(rel, reloc);

    DEBUG("Processing \"%s\" relocation at index %zd", get_realpath(), idx);
    if (type == R_GENERIC_NONE) {
      continue;
    }

    const ElfW(Sym)* s = nullptr;
    soinfo* lsi = nullptr;
    void* dlsym_ret = nullptr;

    if (sym != 0) {
      sym_name = get_string(symtab_[sym].st_name);
      const version_info* vi = nullptr;

      /*
      if (!lookup_version_info(version_tracker, sym, sym_name, &vi)) {
        return false;
      }
      */
      TRACE("skip version lookup for %s", sym_name);

      
      TRACE("soinfo_do_lookup for %s", sym_name);
      if (!soinfo_do_lookup(this, sym_name, vi, &lsi, global_group, local_group, &s)) {
        return false;
      }
      
      if (s == nullptr){
        TRACE("fall back to dlsym: %s", sym_name);
        dlsym_ret = (ElfW(Sym)*)dlsym(NULL, sym_name);
      }

      TRACE("%s => %p", sym_name, dlsym_ret);
      if (s == nullptr && dlsym_ret == nullptr) {
        // We only allow an undefined symbol if this is a weak reference...
        s = &symtab_[sym];
        if (ELF_ST_BIND(s->st_info) != STB_WEAK) {
          DL_ERR("cannot locate symbol \"%s\" referenced by \"%s\"...", sym_name, get_realpath());
          return false;
        }

        /* IHI0044C AAELF 4.5.1.1:

           Libraries are not searched to resolve weak references.
           It is not an error for a weak reference to remain unsatisfied.

           During linking, the value of an undefined weak reference is:
           - Zero if the relocation type is absolute
           - The address of the place if the relocation is pc-relative
           - The address of nominal base address if the relocation
             type is base-relative.
         */

        switch (type) {
          case R_GENERIC_JUMP_SLOT:
          case R_GENERIC_GLOB_DAT:
          case R_GENERIC_RELATIVE:
          case R_GENERIC_IRELATIVE:
#if defined(__aarch64__)
          case R_AARCH64_ABS64:
          case R_AARCH64_ABS32:
          case R_AARCH64_ABS16:
#elif defined(__x86_64__)
          case R_X86_64_32:
          case R_X86_64_64:
#elif defined(__arm__)
          case R_ARM_ABS32:
#elif defined(__i386__)
          case R_386_32:
#endif
            /*
             * The sym_addr was initialized to be zero above, or the relocation
             * code below does not care about value of sym_addr.
             * No need to do anything.
             */
            break;
#if defined(__x86_64__)
          case R_X86_64_PC32:
            sym_addr = reloc;
            break;
#elif defined(__i386__)
          case R_386_PC32:
            sym_addr = reloc;
            break;
#endif
          default:
            DL_ERR("unknown weak reloc type %d @ %p (%zu)", type, rel, idx);
            return false;
        }
      } else { // We got a definition.
#if !defined(__LP64__)
        // When relocating dso with text_relocation .text segment is
        // not executable. We need to restore elf flags before resolving
        // STT_GNU_IFUNC symbol.
        bool protect_segments = has_text_relocations &&
                                lsi == this &&
                                ELF_ST_TYPE(s->st_info) == STT_GNU_IFUNC;
        if (protect_segments) {
          if (phdr_table_protect_segments(phdr, phnum, load_bias) < 0) {
            DL_ERR("can't protect segments for \"%s\": %s",
                   get_realpath(), strerror(errno));
            return false;
          }
        }
#endif
        if (dlsym_ret){
          sym_addr = (ElfW(Addr))dlsym_ret;
        } else {
          sym_addr = lsi->resolve_symbol_address(s);
        }
#if !defined(__LP64__)
        if (protect_segments) {
          if (phdr_table_unprotect_segments(phdr, phnum, load_bias) < 0) {
            DL_ERR("can't unprotect loadable segments for \"%s\": %s",
                   get_realpath(), strerror(errno));
            return false;
          }
        }
#endif
      }
      count_relocation(kRelocSymbol);
    }

    switch (type) {
      case R_GENERIC_JUMP_SLOT:
        count_relocation(kRelocAbsolute);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO JMP_SLOT %16p <- %16p %s\n",
                   reinterpret_cast<void*>(reloc),
                   reinterpret_cast<void*>(sym_addr + addend), sym_name);

        *reinterpret_cast<ElfW(Addr)*>(reloc) = (sym_addr + addend);
        break;
      case R_GENERIC_GLOB_DAT:
        count_relocation(kRelocAbsolute);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO GLOB_DAT %16p <- %16p %s\n",
                   reinterpret_cast<void*>(reloc),
                   reinterpret_cast<void*>(sym_addr + addend), sym_name);
        *reinterpret_cast<ElfW(Addr)*>(reloc) = (sym_addr + addend);
        break;
      case R_GENERIC_RELATIVE:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO RELATIVE %16p <- %16p\n",
                   reinterpret_cast<void*>(reloc),
                   reinterpret_cast<void*>(load_bias + addend));
        *reinterpret_cast<ElfW(Addr)*>(reloc) = (load_bias + addend);
        break;
      case R_GENERIC_IRELATIVE:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO IRELATIVE %16p <- %16p\n",
                    reinterpret_cast<void*>(reloc),
                    reinterpret_cast<void*>(load_bias + addend));
        {
#if !defined(__LP64__)
          // When relocating dso with text_relocation .text segment is
          // not executable. We need to restore elf flags for this
          // particular call.
          if (has_text_relocations) {
            if (phdr_table_protect_segments(phdr, phnum, load_bias) < 0) {
              DL_ERR("can't protect segments for \"%s\": %s",
                     get_realpath(), strerror(errno));
              return false;
            }
          }
#endif
          ElfW(Addr) ifunc_addr = call_ifunc_resolver(load_bias + addend);
#if !defined(__LP64__)
          // Unprotect it afterwards...
          if (has_text_relocations) {
            if (phdr_table_unprotect_segments(phdr, phnum, load_bias) < 0) {
              DL_ERR("can't unprotect loadable segments for \"%s\": %s",
                     get_realpath(), strerror(errno));
              return false;
            }
          }
#endif
          *reinterpret_cast<ElfW(Addr)*>(reloc) = ifunc_addr;
        }
        break;

#if defined(__aarch64__)
      case R_AARCH64_ABS64:
        count_relocation(kRelocAbsolute);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO ABS64 %16llx <- %16llx %s\n",
                   reloc, sym_addr + addend, sym_name);
        *reinterpret_cast<ElfW(Addr)*>(reloc) = sym_addr + addend;
        break;
      case R_AARCH64_ABS32:
        count_relocation(kRelocAbsolute);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO ABS32 %16llx <- %16llx %s\n",
                   reloc, sym_addr + addend, sym_name);
        {
          const ElfW(Addr) min_value = static_cast<ElfW(Addr)>(INT32_MIN);
          const ElfW(Addr) max_value = static_cast<ElfW(Addr)>(UINT32_MAX);
          if ((min_value <= (sym_addr + addend)) &&
              ((sym_addr + addend) <= max_value)) {
            *reinterpret_cast<ElfW(Addr)*>(reloc) = sym_addr + addend;
          } else {
            DL_ERR("0x%016llx out of range 0x%016llx to 0x%016llx",
                   sym_addr + addend, min_value, max_value);
            return false;
          }
        }
        break;
      case R_AARCH64_ABS16:
        count_relocation(kRelocAbsolute);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO ABS16 %16llx <- %16llx %s\n",
                   reloc, sym_addr + addend, sym_name);
        {
          const ElfW(Addr) min_value = static_cast<ElfW(Addr)>(INT16_MIN);
          const ElfW(Addr) max_value = static_cast<ElfW(Addr)>(UINT16_MAX);
          if ((min_value <= (sym_addr + addend)) &&
              ((sym_addr + addend) <= max_value)) {
            *reinterpret_cast<ElfW(Addr)*>(reloc) = (sym_addr + addend);
          } else {
            DL_ERR("0x%016llx out of range 0x%016llx to 0x%016llx",
                   sym_addr + addend, min_value, max_value);
            return false;
          }
        }
        break;
      case R_AARCH64_PREL64:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO REL64 %16llx <- %16llx - %16llx %s\n",
                   reloc, sym_addr + addend, rel->r_offset, sym_name);
        *reinterpret_cast<ElfW(Addr)*>(reloc) = sym_addr + addend - rel->r_offset;
        break;
      case R_AARCH64_PREL32:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO REL32 %16llx <- %16llx - %16llx %s\n",
                   reloc, sym_addr + addend, rel->r_offset, sym_name);
        {
          const ElfW(Addr) min_value = static_cast<ElfW(Addr)>(INT32_MIN);
          const ElfW(Addr) max_value = static_cast<ElfW(Addr)>(UINT32_MAX);
          if ((min_value <= (sym_addr + addend - rel->r_offset)) &&
              ((sym_addr + addend - rel->r_offset) <= max_value)) {
            *reinterpret_cast<ElfW(Addr)*>(reloc) = sym_addr + addend - rel->r_offset;
          } else {
            DL_ERR("0x%016llx out of range 0x%016llx to 0x%016llx",
                   sym_addr + addend - rel->r_offset, min_value, max_value);
            return false;
          }
        }
        break;
      case R_AARCH64_PREL16:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO REL16 %16llx <- %16llx - %16llx %s\n",
                   reloc, sym_addr + addend, rel->r_offset, sym_name);
        {
          const ElfW(Addr) min_value = static_cast<ElfW(Addr)>(INT16_MIN);
          const ElfW(Addr) max_value = static_cast<ElfW(Addr)>(UINT16_MAX);
          if ((min_value <= (sym_addr + addend - rel->r_offset)) &&
              ((sym_addr + addend - rel->r_offset) <= max_value)) {
            *reinterpret_cast<ElfW(Addr)*>(reloc) = sym_addr + addend - rel->r_offset;
          } else {
            DL_ERR("0x%016llx out of range 0x%016llx to 0x%016llx",
                   sym_addr + addend - rel->r_offset, min_value, max_value);
            return false;
          }
        }
        break;

      case R_AARCH64_COPY:
        /*
         * ET_EXEC is not supported so this should not happen.
         *
         * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0056b/IHI0056B_aaelf64.pdf
         *
         * Section 4.6.11 "Dynamic relocations"
         * R_AARCH64_COPY may only appear in executable objects where e_type is
         * set to ET_EXEC.
         */
        DL_ERR("%s R_AARCH64_COPY relocations are not supported", get_realpath());
        return false;
      case R_AARCH64_TLS_TPREL64:
        TRACE_TYPE(RELO, "RELO TLS_TPREL64 *** %16llx <- %16llx - %16llx\n",
                   reloc, (sym_addr + addend), rel->r_offset);
        break;
      case R_AARCH64_TLS_DTPREL32:
        TRACE_TYPE(RELO, "RELO TLS_DTPREL32 *** %16llx <- %16llx - %16llx\n",
                   reloc, (sym_addr + addend), rel->r_offset);
        break;
#elif defined(__x86_64__)
      case R_X86_64_32:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO R_X86_64_32 %08zx <- +%08zx %s", static_cast<size_t>(reloc),
                   static_cast<size_t>(sym_addr), sym_name);
        *reinterpret_cast<Elf32_Addr*>(reloc) = sym_addr + addend;
        break;
      case R_X86_64_64:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO R_X86_64_64 %08zx <- +%08zx %s", static_cast<size_t>(reloc),
                   static_cast<size_t>(sym_addr), sym_name);
        *reinterpret_cast<Elf64_Addr*>(reloc) = sym_addr + addend;
        break;
      case R_X86_64_PC32:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO R_X86_64_PC32 %08zx <- +%08zx (%08zx - %08zx) %s",
                   static_cast<size_t>(reloc), static_cast<size_t>(sym_addr - reloc),
                   static_cast<size_t>(sym_addr), static_cast<size_t>(reloc), sym_name);
        *reinterpret_cast<Elf32_Addr*>(reloc) = sym_addr + addend - reloc;
        break;
#elif defined(__arm__)
      case R_ARM_ABS32:
        count_relocation(kRelocAbsolute);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO ABS %08x <- %08x %s", reloc, sym_addr, sym_name);
        *reinterpret_cast<ElfW(Addr)*>(reloc) += sym_addr;
        break;
      case R_ARM_REL32:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO REL32 %08x <- %08x - %08x %s",
                   reloc, sym_addr, rel->r_offset, sym_name);
        *reinterpret_cast<ElfW(Addr)*>(reloc) += sym_addr - rel->r_offset;
        break;
      case R_ARM_COPY:
        /*
         * ET_EXEC is not supported so this should not happen.
         *
         * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0044d/IHI0044D_aaelf.pdf
         *
         * Section 4.6.1.10 "Dynamic relocations"
         * R_ARM_COPY may only appear in executable objects where e_type is
         * set to ET_EXEC.
         */
        DL_ERR("%s R_ARM_COPY relocations are not supported", get_realpath());
        return false;
#elif defined(__i386__)
      case R_386_32:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO R_386_32 %08x <- +%08x %s", reloc, sym_addr, sym_name);
        *reinterpret_cast<ElfW(Addr)*>(reloc) += sym_addr;
        break;
      case R_386_PC32:
        count_relocation(kRelocRelative);
        MARK(rel->r_offset);
        TRACE_TYPE(RELO, "RELO R_386_PC32 %08x <- +%08x (%08x - %08x) %s",
                   reloc, (sym_addr - reloc), sym_addr, reloc, sym_name);
        *reinterpret_cast<ElfW(Addr)*>(reloc) += (sym_addr - reloc);
        break;
#endif
      default:
        DL_ERR("unknown reloc type %d @ %p (%zu)", type, rel, idx);
        return false;
    }
  }
  TRACE("soinfo::relocate returns true");
  return true;
}
#endif  // !defined(__mips__)


static const char* fix_dt_needed(const char* dt_needed, const char* sopath __unused) {
#if !defined(__LP64__)
  // Work around incorrect DT_NEEDED entries for old apps: http://b/21364029
  if (get_application_target_sdk_version() <= 22) {
    const char* bname = basename(dt_needed);
    if (bname != dt_needed) {
      DL_WARN("library \"%s\" has invalid DT_NEEDED entry \"%s\"", sopath, dt_needed);
      // add_dlwarning(sopath, "invalid DT_NEEDED entry",  dt_needed);
    }

    return bname;
  }
#endif
  return dt_needed;
}

template<typename F>
static void for_each_dt_needed(const soinfo* si, F action) {
  for (const ElfW(Dyn)* d = si->dynamic; d->d_tag != DT_NULL; ++d) {
    if (d->d_tag == DT_NEEDED) {
      action(fix_dt_needed(si->get_string(d->d_un.d_val), si->get_realpath()));
    }
  }
}

template<typename F>
static void for_each_dt_needed(const ElfReader& elf_reader, F action) {
  for (const ElfW(Dyn)* d = elf_reader.dynamic(); d->d_tag != DT_NULL; ++d) {
    if (d->d_tag == DT_NEEDED) {
      action(fix_dt_needed(elf_reader.get_string(d->d_un.d_val), elf_reader.name()));
    }
  }
}

static bool load_library(android_namespace_t* ns,
                         LoadTask* task,
                         LoadTaskList* load_tasks,
                         int rtld_flags) {
  off64_t file_offset = task->get_file_offset();
  const char* name = task->get_name();
  const android_dlextinfo* extinfo = task->get_extinfo();

  char rpath[260];
  if (name[0] != '/'){
    sprintf(rpath, "/system/lib64/%s", name);
  }
  int fd = open(name, O_RDONLY);
  if (fd == -1){
    fd = open(rpath, O_RDONLY);
  }
  task->set_fd(fd, true);

  TRACE("open fd = %d", fd);
  if ((file_offset % PAGE_SIZE) != 0) {
    DL_ERR("file offset for the library \"%s\" is not page-aligned: %" PRId64, name, file_offset);
    return false;
  }
  if (file_offset < 0) {
    DL_ERR("file offset for the library \"%s\" is negative: %" PRId64, name, file_offset);
    return false;
  }

  struct stat file_stat;
  if (TEMP_FAILURE_RETRY(fstat(task->get_fd(), &file_stat)) != 0) {
    DL_ERR("unable to stat file for the library \"%s\": %s", name, strerror(errno));
    return false;
  }
  if (file_offset >= file_stat.st_size) {
    DL_ERR("file offset for the library \"%s\" >= file size: %" PRId64 " >= %" PRId64,
        name, file_offset, file_stat.st_size);
    return false;
  }

  // Check for symlink and other situations where
  // file can have different names, unless ANDROID_DLEXT_FORCE_LOAD is set
  if (extinfo == nullptr || (extinfo->flags & ANDROID_DLEXT_FORCE_LOAD) == 0) {
    auto predicate = [&](soinfo* si) {
      return si->get_st_dev() != 0 &&
             si->get_st_ino() != 0 &&
             si->get_st_dev() == file_stat.st_dev &&
             si->get_st_ino() == file_stat.st_ino &&
             si->get_file_offset() == file_offset;
    };

    soinfo* si = ns->soinfo_list().find_if(predicate);

    // check public namespace
    /*
    if (si == nullptr) {
      si = g_public_namespace.find_if(predicate);
      if (si != nullptr) {
        ns->add_soinfo(si);
      }
    }
    */

    if (si != nullptr) {
      TRACE("library \"%s\" is already loaded under different name/path \"%s\" - "
            "will return existing soinfo", name, si->get_realpath());
      task->set_soinfo(si);
      return true;
    }
  }

  TRACE("duplication check passed!");

  if ((rtld_flags & RTLD_NOLOAD) != 0) {
    DL_ERR("library \"%s\" wasn't loaded and RTLD_NOLOAD prevented it", name);
    return false;
  }

  /*
  if (!ns->is_accessible(realpath)) {
    // TODO(dimitry): workaround for http://b/26394120 - the grey-list
    const soinfo* needed_by = task->is_dt_needed() ? task->get_needed_by() : nullptr;
    if (is_greylisted(name, needed_by)) {
      // print warning only if needed by non-system library
      if (needed_by == nullptr || !is_system_library(needed_by->get_realpath())) {
        const soinfo* needed_or_dlopened_by = task->get_needed_by();
        const char* sopath = needed_or_dlopened_by == nullptr ? "(unknown)" :
                                                      needed_or_dlopened_by->get_realpath();
        DL_WARN("library \"%s\" (\"%s\") needed or dlopened by \"%s\" is not accessible for the namespace \"%s\""
                " - the access is temporarily granted as a workaround for http://b/26394120, note that the access"
                " will be removed in future releases of Android.",
                name, realpath.c_str(), sopath, ns->get_name());
        add_dlwarning(sopath, "unauthorized access to",  name);
      }
    } else {
      // do not load libraries if they are not accessible for the specified namespace.
      const char* needed_or_dlopened_by = task->get_needed_by() == nullptr ?
                                          "(unknown)" :
                                          task->get_needed_by()->get_realpath();

      DL_ERR("library \"%s\" needed or dlopened by \"%s\" is not accessible for the namespace \"%s\"",
             name, needed_or_dlopened_by, ns->get_name());

      PRINT("library \"%s\" (\"%s\") needed or dlopened by \"%s\" is not accessible for the"
            " namespace: [name=\"%s\", ld_library_paths=\"%s\", default_library_paths=\"%s\","
            " permitted_paths=\"%s\"]",
            name, realpath.c_str(),
            needed_or_dlopened_by,
            ns->get_name(),
            android::base::Join(ns->get_ld_library_paths(), ':').c_str(),
            android::base::Join(ns->get_default_library_paths(), ':').c_str(),
            android::base::Join(ns->get_permitted_paths(), ':').c_str());
      return false;
    }
    return false;
  }
  */

  soinfo* si = soinfo_alloc(ns, name, &file_stat, file_offset, rtld_flags);
  if (si == nullptr) {
    return false;
  }

  task->set_soinfo(si);
  
  TRACE("task->read begin:");
  // Read the ELF header and some of the segments.
  if (!task->read(name, file_stat.st_size)) {
    soinfo_free(si);
    task->set_soinfo(nullptr);
    return false;
  }
  
  TRACE("task->read finished!");
  // find and set DT_RUNPATH and dt_soname
  // Note that these field values are temporary and are
  // going to be overwritten on soinfo::prelink_image
  // with values from PT_LOAD segments.
  const ElfReader& elf_reader = task->get_elf_reader();
  for (const ElfW(Dyn)* d = elf_reader.dynamic(); d->d_tag != DT_NULL; ++d) {
    if (d->d_tag == DT_RUNPATH) {
      si->set_dt_runpath(elf_reader.get_string(d->d_un.d_val));
    }
    if (d->d_tag == DT_SONAME) {
      si->set_soname(elf_reader.get_string(d->d_un.d_val));
    }
  }

  for_each_dt_needed(task->get_elf_reader(), [&](const char* name) {
    void* p = dlopen(name, RTLD_NOW);
    TRACE("skip push needed: %s(%p)", name, p);
    // load_tasks->push_back(LoadTask::create(name, si, task->get_readers_map()));
  });

  return true;
}

bool VersionTracker::init(const soinfo* si_from) {
  /*
  if (!si_from->has_min_version(2)) {
    return true;
  }

  return init_verneed(si_from) && init_verdef(si_from);
  */
  return true;
}

// Returns true if library was found and false in 2 cases
// 1. (for default namespace only) The library was found but loaded under different
//    target_sdk_version (*candidate != nullptr)
// 2. The library was not found by soname (*candidate is nullptr)
static bool find_loaded_library_by_soname(android_namespace_t* ns,
                                          const char* name, soinfo** candidate) {
  *candidate = nullptr;

  // Ignore filename with path.
  if (strchr(name, '/') != nullptr) {
    return false;
  }

  uint32_t target_sdk_version = get_application_target_sdk_version();

  return !ns->soinfo_list().visit([&](soinfo* si) {
    const char* soname = si->get_soname();
    if (soname != nullptr && (strcmp(name, soname) == 0)) {
      // If the library was opened under different target sdk version
      // skip this step and try to reopen it. The exceptions are
      // "libdl.so" and global group. There is no point in skipping
      // them because relocation process is going to use them
      // in any case.
      bool is_libdl = si == solist;
      if (is_libdl || (si->get_dt_flags_1() & DF_1_GLOBAL) != 0 ||
          !si->is_linked() || si->get_target_sdk_version() == target_sdk_version ||
          ns != &g_default_namespace) {
        *candidate = si;
        return false;
      } else if (*candidate == nullptr) {
        // for the different sdk version in the default namespace
        // remember the first library.
        *candidate = si;
      }
    }

    return true;
  });
}

static bool find_library_internal(android_namespace_t* ns,
                                  LoadTask* task,
                                  LoadTaskList* load_tasks,
                                  int rtld_flags) {
  soinfo* candidate;

  if (find_loaded_library_by_soname(ns, task->get_name(), &candidate)) {
    task->set_soinfo(candidate);
    return true;
  }

  if (ns != &g_default_namespace) {
    // check public namespace
    fprintf(stderr, "FIXME:(%s:%d)", __FILE__, __LINE__);
    abort();
    /*
    candidate = g_public_namespace.find_if([&](soinfo* si) {
      return strcmp(task->get_name(), si->get_soname()) == 0;
    });

    if (candidate != nullptr) {
      ns->add_soinfo(candidate);
      task->set_soinfo(candidate);
      return true;
    }
    */
  }

  // Library might still be loaded, the accurate detection
  // of this fact is done by load_library.
  TRACE("[ \"%s\" find_loaded_library_by_soname failed (*candidate=%s@%p). Trying harder...]",
      task->get_name(), candidate == nullptr ? "n/a" : candidate->get_realpath(), candidate);

  if (load_library(ns, task, load_tasks, rtld_flags)) {
    return true;
  } else {
    // In case we were unable to load the library but there
    // is a candidate loaded under the same soname but different
    // sdk level - return it anyways.
    if (candidate != nullptr) {
      task->set_soinfo(candidate);
      return true;
    }
  }

  return false;
}

static void soinfo_unload(soinfo* soinfos[], size_t count) {
  // FIXME:
}
// add_as_children - add first-level loaded libraries (i.e. library_names[], but
// not their transitive dependencies) as children of the start_with library.
// This is false when find_libraries is called for dlopen(), when newly loaded
// libraries must form a disjoint tree.
static bool find_libraries(android_namespace_t* ns,
                           soinfo* start_with,
                           const char* const library_names[],
                           size_t library_names_count, soinfo* soinfos[],
                           std::vector<soinfo*>* ld_preloads,
                           size_t ld_preloads_count, int rtld_flags,
                           const android_dlextinfo* extinfo,
                           bool add_as_children,
                           int hide
                           ) {
  // Step 0: prepare.
  LoadTaskList load_tasks;
  std::unordered_map<const soinfo*, ElfReader> readers_map;

  for (size_t i = 0; i < library_names_count; ++i) {
    const char* name = library_names[i];
    load_tasks.push_back(LoadTask::create(name, start_with, &readers_map, hide));
  }

  // Construct global_group.
  soinfo::soinfo_list_t global_group = make_global_group(ns);

  // If soinfos array is null allocate one on stack.
  // The array is needed in case of failure; for example
  // when library_names[] = {libone.so, libtwo.so} and libone.so
  // is loaded correctly but libtwo.so failed for some reason.
  // In this case libone.so should be unloaded on return.
  // See also implementation of failure_guard below.

  if (soinfos == nullptr) {
    size_t soinfos_size = sizeof(soinfo*)*library_names_count;
    soinfos = reinterpret_cast<soinfo**>(alloca(soinfos_size));
    memset(soinfos, 0, soinfos_size);
  }

  // list of libraries to link - see step 2.
  size_t soinfos_count = 0;

  auto scope_guard = make_scope_guard([&]() {
    for (LoadTask* t : load_tasks) {
      LoadTask::deleter(t);
    }
  });

  auto failure_guard = make_scope_guard([&]() {
    // Housekeeping
    soinfo_unload(soinfos, soinfos_count);
  });

  // ZipArchiveCache zip_archive_cache;

  // Step 1: expand the list of load_tasks to include
  // all DT_NEEDED libraries (do not load them just yet)
  for (size_t i = 0; i<load_tasks.size(); ++i) {
    LoadTask* task = load_tasks[i];
    soinfo* needed_by = task->get_needed_by();

    bool is_dt_needed = needed_by != nullptr && (needed_by != start_with || add_as_children);
    task->set_extinfo(is_dt_needed ? nullptr : extinfo);
    task->set_dt_needed(is_dt_needed);

    if(!find_library_internal(ns, task, &load_tasks, rtld_flags)) {
      return false;
    }

    soinfo* si = task->get_soinfo();

    if (is_dt_needed) {
      needed_by->add_child(si);
    }

    if (si->is_linked()) {
      si->increment_ref_count();
    }

    // When ld_preloads is not null, the first
    // ld_preloads_count libs are in fact ld_preloads.
    if (ld_preloads != nullptr && soinfos_count < ld_preloads_count) {
      ld_preloads->push_back(si);
    }

    if (soinfos_count < library_names_count) {
      soinfos[soinfos_count++] = si;
    }
  }

  // Step 2: Load libraries in random order (see b/24047022)
  LoadTaskList load_list;
  for (auto&& task : load_tasks) {
    soinfo* si = task->get_soinfo();
    auto pred = [&](const LoadTask* t) {
      return t->get_soinfo() == si;
    };

    if (!si->is_linked() &&
        std::find_if(load_list.begin(), load_list.end(), pred) == load_list.end() ) {
      load_list.push_back(task);
    }
  }

  // shuffle(&load_list);

  for (auto&& task : load_list) {
    if (!task->load()) {
      return false;
    }
  }

  TRACE("Step 3: pre-link all DT_NEEDED libraries in breadth first order.");
  // Step 3: pre-link all DT_NEEDED libraries in breadth first order.
  for (auto&& task : load_tasks) {
    soinfo* si = task->get_soinfo();
    if (!si->is_linked() && !si->prelink_image()) {
      return false;
    }
  }


  TRACE("Step 4: Add LD_PRELOADed libraries to the global group...");
  // Step 4: Add LD_PRELOADed libraries to the global group for
  // future runs. There is no need to explicitly add them to
  // the global group for this run because they are going to
  // appear in the local group in the correct order.
  if (ld_preloads != nullptr) {
    for (auto&& si : *ld_preloads) {
      si->set_dt_flags_1(si->get_dt_flags_1() | DF_1_GLOBAL);
    }
  }


  TRACE("Step 5: link libraries.");
  // Step 5: link libraries.
  
  soinfo::soinfo_list_t local_group;
  
  walk_dependencies_tree(
      (start_with != nullptr && add_as_children) ? &start_with : soinfos,
      (start_with != nullptr && add_as_children) ? 1 : soinfos_count,
      [&] (soinfo* si) {
    local_group.push_back(si);
    return true;
  });
  
  TRACE("walk_dependencies_tree finished.");
  
  // We need to increment ref_count in case
  // the root of the local group was not linked.
  bool was_local_group_root_linked = local_group.front()->is_linked();

  bool linked = local_group.visit([&](soinfo* si) {
    if (!si->is_linked()) {
      if (!si->link_image(global_group, local_group, extinfo)) {
        return false;
      }
    }

    return true;
  });

  
  if (linked) {
    local_group.for_each([](soinfo* si) {
      if (!si->is_linked()) {
        si->set_linked();
      }
    });

    failure_guard.disable();
  }

  if (!was_local_group_root_linked) {
    local_group.front()->increment_ref_count();
  }
  
  
  TRACE("%s return %d", __FUNCTION__);
  return linked;
}

static soinfo* find_library(android_namespace_t* ns,
                            const char* name, int rtld_flags,
                            const android_dlextinfo* extinfo,
                            soinfo* needed_by,
                            int hide
                            ) {
  soinfo* si;

  if (name == nullptr) {
    si = somain;
  } else if (!find_libraries(ns, needed_by, &name, 1, &si, nullptr, 0, rtld_flags,
                             extinfo, /* add_as_children */ false, hide)) {
    return nullptr;
  }

  return si;
}

static soinfo* soinfo_from_handle(void* handle) {
  if ((reinterpret_cast<uintptr_t>(handle) & 1) != 0) {
    auto it = g_soinfo_handles_map.find(reinterpret_cast<uintptr_t>(handle));
    if (it == g_soinfo_handles_map.end()) {
      return nullptr;
    } else {
      return it->second;
    }
  }

  return static_cast<soinfo*>(handle);
}

static void* do_dlopen(const char* name, int flags, const android_dlextinfo* extinfo, int hide) {

  if ((flags & ~(RTLD_NOW|RTLD_LAZY|RTLD_LOCAL|RTLD_GLOBAL|RTLD_NODELETE|RTLD_NOLOAD)) != 0) {
    DL_ERR("invalid flags to dlopen: %x", flags);
    return nullptr;
  }

  android_namespace_t* ns = &g_default_namespace;

  if (extinfo != nullptr) {
    if ((extinfo->flags & ~(ANDROID_DLEXT_VALID_FLAG_BITS)) != 0) {
      DL_ERR("invalid extended flags to android_dlopen_ext: 0x%" PRIx64, extinfo->flags);
      return nullptr;
    }

    if ((extinfo->flags & ANDROID_DLEXT_USE_LIBRARY_FD) == 0 &&
        (extinfo->flags & ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET) != 0) {
      DL_ERR("invalid extended flag combination (ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET without "
          "ANDROID_DLEXT_USE_LIBRARY_FD): 0x%" PRIx64, extinfo->flags);
      return nullptr;
    }

    if ((extinfo->flags & ANDROID_DLEXT_LOAD_AT_FIXED_ADDRESS) != 0 &&
        (extinfo->flags & (ANDROID_DLEXT_RESERVED_ADDRESS | ANDROID_DLEXT_RESERVED_ADDRESS_HINT)) != 0) {
      DL_ERR("invalid extended flag combination: ANDROID_DLEXT_LOAD_AT_FIXED_ADDRESS is not "
             "compatible with ANDROID_DLEXT_RESERVED_ADDRESS/ANDROID_DLEXT_RESERVED_ADDRESS_HINT");
      return nullptr;
    }

    if ((extinfo->flags & ANDROID_DLEXT_USE_NAMESPACE) != 0) {
      if (extinfo->library_namespace == nullptr) {
        DL_ERR("ANDROID_DLEXT_USE_NAMESPACE is set but extinfo->library_namespace is null");
        return nullptr;
      }
      ns = extinfo->library_namespace;
    }
  }

  ProtectedDataGuard guard;
  soinfo* si = find_library(ns, name, flags, extinfo, nullptr, hide);
  if (si != nullptr) {
    si->call_constructors();
    return si->to_handle();
  }

  return nullptr;
}

bool do_dlsym(void* handle, const char* sym_name, void** symbol) {
#if !defined(__LP64__)
  if (handle == nullptr) {
    DL_ERR("dlsym failed: library handle is null");
    return false;
  }
#endif

  if (sym_name == nullptr) {
    DL_ERR("dlsym failed: symbol name is null");
    return false;
  }

  soinfo* found = nullptr;
  const ElfW(Sym)* sym = nullptr;
  android_namespace_t* ns = &g_default_namespace;

  version_info vi_instance;
  version_info* vi = nullptr;

  if (handle == RTLD_DEFAULT || handle == RTLD_NEXT) {
    DL_WARN("dlsym'ing on virtual handle is not supported");
    return false;
  } else {
    soinfo* si = soinfo_from_handle(handle);
    if (si == nullptr) {
      DL_ERR("dlsym failed: invalid handle: %p", handle);
      return false;
    }
    sym = dlsym_handle_lookup(si, &found, sym_name, vi);
  }

  if (sym != nullptr) {
    uint32_t bind = ELF_ST_BIND(sym->st_info);

    if ((bind == STB_GLOBAL || bind == STB_WEAK) && sym->st_shndx != 0) {
      *symbol = reinterpret_cast<void*>(found->resolve_symbol_address(sym));
      return true;
    }

    DL_ERR("symbol \"%s\" found but not global", sym_name);/*symbol_display_name(sym_name, sym_ver).c_str()*/
    return false;
  }

  DL_ERR("undefined symbol: %s", sym_name);/*symbol_display_name(sym_name, sym_ver).c_str()*/
  return false;
}

void* mlnk_dlopen(char* path, int hide){
    /*
    ElfReader elf_reader;
    int fd = open(path, O_RDONLY);
    if (fd != -1){
      void* base = mmap(nullptr, 4096, PROT_READ|PROT_EXEC, MAP_PRIVATE, fd, 0);
      printf("[+] base = %p\n", base);
      off_t l = lseek(fd, 0, SEEK_END);
      lseek(fd, 0, SEEK_SET);
      bool ret = elf_reader.Read("test", fd, 0, l);
      printf("[-] elf_reader.Read -> %d\n", ret);
      printf("[-] adb shell cat /proc/%d/maps\n", getpid());
      elf_reader.Load(nullptr);
      close(fd);
    }
    */
    return do_dlopen(path, 2, nullptr, hide);
}

void* mlnk_dlsym(void* handle , char* name){
    void* sym = nullptr;
    if (do_dlsym(handle, name, &sym)){
      return sym;
    }
    return NULL;
}
