#include <unistd.h>
#include <jni.h>
#include <stdint.h>
#include <dlfcn.h>
#include <pthread.h>
#include <android/log.h>

#include "callstack.h"
#include "hook.h"
#include "debug.h"

#include <sys/mman.h>
#include <time.h>

#include <mutex>
#include <unordered_set>
extern int fd;

void print_stack_c();

std::mutex g_ptrlog_lock;

int gIsMono = 0;

uintptr_t il2cppbase = 0;

enum Il2CppGCEvent {
	IL2CPP_GC_EVENT_START,
	IL2CPP_GC_EVENT_MARK_START,
	IL2CPP_GC_EVENT_MARK_END,
	IL2CPP_GC_EVENT_RECLAIM_START,
	IL2CPP_GC_EVENT_RECLAIM_END,
	IL2CPP_GC_EVENT_END,
	IL2CPP_GC_EVENT_PRE_STOP_WORLD,
	IL2CPP_GC_EVENT_POST_STOP_WORLD,
	IL2CPP_GC_EVENT_PRE_START_WORLD,
	IL2CPP_GC_EVENT_POST_START_WORLD
};

typedef enum {
	MONO_GC_EVENT_PRE_STOP_WORLD = 6,
	/**
	 * When this event arrives, the GC and suspend locks are acquired.
	 */
	MONO_GC_EVENT_PRE_STOP_WORLD_LOCKED = 10,
	MONO_GC_EVENT_POST_STOP_WORLD = 7,
	MONO_GC_EVENT_START = 0,
	MONO_GC_EVENT_END = 5,
	MONO_GC_EVENT_PRE_START_WORLD = 8,
	/**
	 * When this event arrives, the GC and suspend locks are released.
	 */
	MONO_GC_EVENT_POST_START_WORLD_UNLOCKED = 11,
	MONO_GC_EVENT_POST_START_WORLD = 9,
} MonoProfilerGCEvent;

enum Il2CppProfileFlags {
	IL2CPP_PROFILE_NONE = 0,
	IL2CPP_PROFILE_APPDOMAIN_EVENTS = 1 << 0,
	IL2CPP_PROFILE_ASSEMBLY_EVENTS  = 1 << 1,
	IL2CPP_PROFILE_MODULE_EVENTS    = 1 << 2,
	IL2CPP_PROFILE_CLASS_EVENTS     = 1 << 3,
	IL2CPP_PROFILE_JIT_COMPILATION  = 1 << 4,
	IL2CPP_PROFILE_INLINING         = 1 << 5,
	IL2CPP_PROFILE_EXCEPTIONS       = 1 << 6,
	IL2CPP_PROFILE_ALLOCATIONS      = 1 << 7,
	IL2CPP_PROFILE_GC               = 1 << 8,
	IL2CPP_PROFILE_THREADS          = 1 << 9,
	IL2CPP_PROFILE_REMOTING         = 1 << 10,
	IL2CPP_PROFILE_TRANSITIONS      = 1 << 11,
	IL2CPP_PROFILE_ENTER_LEAVE      = 1 << 12,
	IL2CPP_PROFILE_COVERAGE         = 1 << 13,
	IL2CPP_PROFILE_INS_COVERAGE     = 1 << 14,
	IL2CPP_PROFILE_STATISTICAL      = 1 << 15,
	IL2CPP_PROFILE_METHOD_EVENTS    = 1 << 16,
	IL2CPP_PROFILE_MONITOR_EVENTS   = 1 << 17,
	IL2CPP_PROFILE_IOMAP_EVENTS = 1 << 18, /* this should likely be removed, too */
	IL2CPP_PROFILE_GC_MOVES = 1 << 19
};

struct MethodInfo;

typedef struct Il2CppStackFrameInfo
{
    const MethodInfo *method;
} Il2CppStackFrameInfo;

typedef void Il2CppObject;
typedef void Il2CppClass;
typedef void Il2CppProfiler;
typedef void Il2CppType;

typedef void (*Il2CppProfileAllocFunc) (Il2CppProfiler* prof, Il2CppObject *obj, Il2CppClass *klass);
typedef void (*Il2CppProfileGCFunc) (Il2CppProfiler* prof, enum Il2CppGCEvent event, int generation);
typedef void (*Il2CppProfileGCResizeFunc) (Il2CppProfiler* prof, int64_t new_size);
typedef void (*Il2CppProfileFunc) (Il2CppProfiler* prof);
typedef void (*Il2CppFrameWalkFunc) (const Il2CppStackFrameInfo *info, void *user_data);

size_t (*il2cpp_gc_get_heap_size)() = NULL;
size_t (*il2cpp_gc_get_used_size)() = NULL;
void (*il2cpp_profiler_install)(Il2CppProfiler *prof, Il2CppProfileFunc shutdown_callback) = NULL;
void (*il2cpp_profiler_install_allocation)(Il2CppProfileAllocFunc callback) = NULL;
void (*il2cpp_profiler_install_gc)(Il2CppProfileGCFunc callback, Il2CppProfileGCResizeFunc heap_resize_callback) = NULL;
void (*il2cpp_profiler_set_events)(enum Il2CppProfileFlags events) = NULL;
uint32_t (*il2cpp_object_get_size)(Il2CppObject* obj) = NULL;
const char* (*il2cpp_class_get_name)(Il2CppClass *klass) = NULL;
Il2CppClass* (*il2cpp_object_get_class)(Il2CppObject* obj) = NULL;
const char* (*il2cpp_class_get_namespace)(Il2CppClass *klass) = NULL;
Il2CppType* (*il2cpp_class_get_type)(Il2CppClass *klass) = NULL;
const char* (*il2cpp_type_get_name)(Il2CppType *typ) = NULL;
void (*il2cpp_current_thread_walk_frame_stack)(Il2CppFrameWalkFunc func, void* user_data) = NULL;
const char* (*il2cpp_method_get_name)(const MethodInfo *method) = NULL;

typedef int (*TyMonoStackWalk)(void *method, int native_offset, int il_offset, int managed, void *data);
int (*mono_stack_walk_no_il)(TyMonoStackWalk, void*) = NULL;
const char* (*mono_method_full_name)(void*, int) = NULL;

void (*GC_dump_named)(const char* name) = NULL;

void* il2cpp_mem_worker(void* _){
  while (1){
    __android_log_print(ANDROID_LOG_INFO,"IL2CPP", "[%ld] mono mem: %zu/%zu", time(NULL), il2cpp_gc_get_used_size(), il2cpp_gc_get_heap_size());
    sleep(1);
  }
}

#define MAX_PTR_LOG 10000

struct MonoPtr {
  uint32_t hdr;
  uint32_t ptr;
};

typedef struct PtrLog{
  int cnt;
  struct MonoPtr ptrs[MAX_PTR_LOG];
  // uint8_t marks[MAX_PTR_LOG];
} PtrLog;

std::unordered_set<uintptr_t> AllObjs;
int64_t current_heap_size = 0;

struct my_il2cpp_obj{
    void* kind;
    void* monitor;
};

#define PTR_MASK ((uint32_t)0xdeadbeef)
void ptrlog_add(PtrLog* logger, uint32_t ptr){
  const std::lock_guard<std::mutex> l(g_ptrlog_lock);
  int i;
  my_il2cpp_obj * r = (my_il2cpp_obj*)ptr;
  if (!r->kind) return;
  for(i = 0; i < logger->cnt; ++i){ // check duplication
    if (logger->ptrs[i].ptr == (PTR_MASK ^ ptr))
      return;
  }
  __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [+] checkin object: %p Kind=%p, monitor=%p", r, r->kind, r->monitor);
  if (logger->cnt + 1 < MAX_PTR_LOG){
    // logger->ptrs[logger->cnt++].hdr = *(uint32_t*)ptr;
    logger->ptrs[logger->cnt].ptr = ptr ^ PTR_MASK;
    logger->cnt++;
  }
}

int (*_s_is_object_marked)(uint32_t) = NULL;

int is_object_marked(struct MonoPtr* mp){
  if (_s_is_object_marked){
    return _s_is_object_marked(mp->ptr ^ PTR_MASK);
  }
  return 0;
}

void ptrlog_pop_idx(PtrLog* logger, uint32_t idx){
  // TODO: check idx in range
  logger->ptrs[idx] = logger->ptrs[--logger->cnt];
}

#define MAX_BUF 128
const char* dump_bytes(void* _p){
  static char r[MAX_BUF + 1];
  char* p = (char*)_p;
  int i = 0;
  for(;i < MAX_BUF; ++i){
    if(isprint(p[i])){
      r[i] = p[i];
    } else {
      r[i] = '.';
    }
  }
  r[MAX_BUF] = 0;
  return r;
}


const char* UNKNOWN = "<unknown>";
int ptrlog_reclaim(PtrLog* logger){
  const std::lock_guard<std::mutex> l(g_ptrlog_lock);
  int idx = 0;
  while (idx < logger->cnt){
    uint32_t obj = logger->ptrs[idx].ptr ^ PTR_MASK;
    my_il2cpp_obj * r = (my_il2cpp_obj*)obj;
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [-] checkout object: %p Kind=%p, monitor=%p", r, r->kind, r->monitor);
#if IL2CPP_RUNTIME_DUMP
    uint32_t size = il2cpp_object_get_size((Il2CppObject*)obj);
    Il2CppClass* klass = il2cpp_object_get_class((Il2CppObject*)obj);
    const char* name = il2cpp_class_get_name(klass);
    if (!name) name = UNKNOWN;
#endif
    if (!is_object_marked(&logger->ptrs[idx])){
#if IL2CPP_RUNTIME_DUMP
      __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [-] %s@%p is dead, size = %d", name, obj, size);
#else
      __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [-] %p is dead", obj);
#endif
      ptrlog_pop_idx(logger, idx); // cnt--
    } else {
#if IL2CPP_RUNTIME_DUMP
      const char* ns = il2cpp_class_get_namespace(klass);
      if (!ns) ns = UNKNOWN;
      __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [-] %s.%s@%p is alive, size = %d", ns, name, obj, size);
#else
      __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [-] %p is alive", obj);
#endif
      idx++;
    }
  }
  return logger->cnt;
}

void ptrlog_assert_mark_cleared(PtrLog* logger){
  int idx;
  for (idx = 0; idx < logger->cnt; idx++){
    uint32_t obj = logger->ptrs[idx].ptr ^ PTR_MASK;
    if (is_object_marked(&logger->ptrs[idx])){
      __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "[!] !!!!!! %p is marked before mark stage", obj);
    }
  }
}

int ptrlog_get_marks(PtrLog* logger, uint8_t* buf, int len){
  int i;
  for(i = 0; i < len && i < logger->cnt; ++i){
    // uint32_t obj = logger->ptrs[i].ptr ^ PTR_MASK;
    buf[i] = is_object_marked(&logger->ptrs[i]);
  }
  return i;
}

PtrLog p2523488 = {0};
PtrLog p2523482 = {0};
PtrLog aaa = {0};

static int LogMonoStack(void *method, int native_offset, int il_offset, int managed, void *data){
  int* layer = (int*)data;
  __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "#%02d %s", *layer, mono_method_full_name(method, 1));
  (*layer)++;
  return 0;
}

static void MyFrameFunc (const Il2CppStackFrameInfo *info, void *user_data){
    int* layer = (int*)user_data;
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP"," [STACK] #%02d %p", *layer, info->method/*il2cpp_method_get_name(info->method)*/);
    (*layer)++;
}
static void print_stack(){
  int64_t cur = now_us();
  if(gIsMono && mono_stack_walk_no_il){
    int layer = 0;
    mono_stack_walk_no_il(LogMonoStack, &layer);
  } else {
    // print_stack_c();
    if (il2cpp_current_thread_walk_frame_stack && il2cpp_method_get_name){
        int layer = 0;
        il2cpp_current_thread_walk_frame_stack(MyFrameFunc, &layer);
    }
  }
  __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [+] print_stack cost %lld us", now_us() - cur);
}

static void objectAlloction(Il2CppProfiler* prof, Il2CppObject *obj, Il2CppClass *klass){
  uint32_t size = il2cpp_object_get_size(obj);
  const char* name = il2cpp_class_get_name(klass);
  
  // wired_il2cpp_alloc((void*)name, (void*)obj, size);
  /*
  if (size == 2523488){
    ptrlog_add(&p2523488, (uint32_t)obj);
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "[+] %s %p %d", name, obj, size);
  }
  if (size == 2523482){
    ptrlog_add(&p2523482, (uint32_t)obj);
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "[+] %s %p %d", name, obj, size);
  }
  */
  /*
  if (strcmp(name, "Slot[]") == 0 && size == 23188 && 0){
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [+] %s %p %d", name, obj, size);
    print_stack();
  }
  */
  //if (strcmp(name, "Entry[]") == 0 && /*(size == 14720 || size == 280320) && 0*/ size == 30912){
  //  __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [+] %s %p %d", name, obj, size);
  //  print_stack();
  //}
  /*
  if (strcmp(name, "TeachV2") == 0){
    uint32_t gc_desc = *(uint32_t*)((uint8_t*)(klass) + 4);
    uint8_t has_ref = *((uint8_t*)klass + 189) & 0x20;
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "TeachV2: has_ref = %d, gc_desc = %d", has_ref, gc_desc);
    // print_stack();
  }
  */
  // if (size >= 2523482){
  if (size >= 2000000){
    ptrlog_add(&aaa, (uint32_t)obj);
    // print_stack();
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP", " [+] %s %p %d", name, obj, size);
  }
}

static void garbageCollected(Il2CppProfiler* prof, enum Il2CppGCEvent event, int generation){
  static int flag = 0;
  // __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "garbageCollected %d", event);
  switch(event){
    case IL2CPP_GC_EVENT_PRE_START_WORLD:
    case IL2CPP_GC_EVENT_RECLAIM_START: if (flag == 0){
        int cnt0 = aaa.cnt;
        int cnt = ptrlog_reclaim(&aaa);
        __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "[IL2CPP_GC_EVENT_RECLAIM_START] objects: %d -> %d", cnt0, cnt);
        
        /*
        if (GC_dump_named){
          GC_dump_named(NULL);
        }
        */
        flag = 1;
      }
      break;
    
    case IL2CPP_GC_EVENT_MARK_START: 
    case IL2CPP_GC_EVENT_POST_STOP_WORLD: if (flag == 1){
      ptrlog_assert_mark_cleared(&aaa);
      flag = 0;
    }
    break;
  }
}

static void heapResized(Il2CppProfiler* prof, int64_t new_size){
  __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "[IL2CPP_GC_HEAP_RESIZED] new size: %lld", new_size);
  current_heap_size = new_size;
}

static void vmShutdown(Il2CppProfiler* prof){
  __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "[!] vm shutdown");
}

static void myPrint(const char* fmt, ...){
  va_list ap;
  va_start(ap, fmt);
  __android_log_vprint(ANDROID_LOG_INFO, "IL2CPP", fmt, ap);
  va_end(ap);
}

/*
6594705: 04057780   284 FUNC    LOCAL  HIDDEN    12 GC_dump_named
6594706: 050d7eb0     4 OBJECT  LOCAL  HIDDEN    23 GC_dump_regularly
6594875: 04054354   280 FUNC    LOCAL  HIDDEN    12 GC_printf
6594661: 05138b14 0x1f270 OBJECT  LOCAL  HIDDEN    23 GC_arrays
6594810: 0405af58  2252 FUNC    LOCAL  HIDDEN    12 GC_mark_from
*/

#define PRINT_VERBOSE 2
#define PRINT_INTO 1

uint32_t BI_OFFSET_IN_U32 = 0;
uint32_t GC_arrays = 0;
uint32_t* GC_mark_state = NULL;
void* (*GC_base)(void*) = NULL;

union word_ptr_ao_u {
  uint32_t w;
  int32_t sw;
  void *vp;
};

typedef struct GC_ms_entry {
    void* mse_start;    /* First word of object, word aligned.  */
    union word_ptr_ao_u mse_descr;
                        /* Descriptor; low order two bits are tags,     */
                        /* as described in gc_mark.h.                   */
} mse;

void* (*GC_mark_from)(mse*, mse*, mse*);
void* (*old_GC_mark_from)(mse*, mse*, mse*);
void* new_GC_mark_from1(mse* v1, mse* v2, mse* v3){
  uint8_t marks_before[64];
  uint8_t marks_after[64];
  mse backup = *v1;
  int count = ptrlog_get_marks(&aaa, marks_before, 64);
  void* ret = old_GC_mark_from(v1, v2, v3);
  if (ptrlog_get_marks(&aaa, marks_after, 64) != count){
    myPrint("ASSERT Failed: ptrlog_get_marks(&aaa, marks_after, 64) == count");
    return ret;
  }
  __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "Count = %d", count);
  while(--count >= 0){
    if (marks_before[count] == 0 && marks_after[count] != 0){
      myPrint("[%02d]obj(%p) marked by (%p, %p, %p) => %p mse_start=%p descr=%p, tag=%p", *GC_mark_state, aaa.ptrs[count].ptr ^ PTR_MASK, v1, v2, v3, ret, backup.mse_start, backup.mse_descr.vp, (backup.mse_descr.w & 0xFFFFFE03));
    }
  }
  //__android_log_print(ANDROID_LOG_INFO, "IL2CPP", "void* new_GC_mark_from(mse_descr.w=%d, %p): %p=>%p", backup.mse_descr.w, backup.mse_start, v1, ret);
  return ret;
}
void* new_GC_mark_from(mse* v1, mse* v2, mse* v3){
  uint8_t marks_before[64];
  uint8_t marks_after[64];
  int count = ptrlog_get_marks(&aaa, marks_before, 64);
  mse backup = *v1;
  void* ret = old_GC_mark_from(v1, v2, v3);
  if (ptrlog_get_marks(&aaa, marks_after, 64) != count){
    myPrint("ASSERT Failed: ptrlog_get_marks(&aaa, marks_after, 64) == count");
    return ret;
  }
  while(--count >= 0){
    if (marks_before[count] == 0 && marks_after[count] != 0){
      myPrint("[%02d]obj(%p) marked by (%p, %p, %p) => %p mse_start=%p descr=%p, tag=%p v1->mse_start=%p", *GC_mark_state, aaa.ptrs[count].ptr ^ PTR_MASK, v1, v2, v3, ret, backup.mse_start, backup.mse_descr.vp, (backup.mse_descr.w & 0xFFFFFE03), v1->mse_start);
      if ((backup.mse_descr.w & 0xFFFFFE03) && (backup.mse_descr.w & 0x3) == 0 && (mse*)ret >= v1){
        myPrint("    case A");
        mse* _ret = (mse*)v1;
        if ((uint32_t)(_ret->mse_start) > (uint32_t)(backup.mse_start)){
          uint32_t* found = NULL;
          uint32_t size = il2cpp_object_get_size((Il2CppObject*)(aaa.ptrs[count].ptr ^ PTR_MASK));
          for(uint32_t* cur = (uint32_t*)(backup.mse_start); cur < (uint32_t*)(_ret->mse_start); ++cur){
            
            if (*cur >=  (aaa.ptrs[count].ptr ^ PTR_MASK) && *cur < (aaa.ptrs[count].ptr ^ PTR_MASK) + size){
              found = cur;
            }
            // myPrint("[Splitted] testing %p", *cur);
          }
          void* base = found ? GC_base(found) : 0;
          
          if (base){
            int offset = *found - (aaa.ptrs[count].ptr ^ PTR_MASK);
            // offset 大于 0, 通常就是假引用
            myPrint("    splitted at %p, found ref at %p, val=%p, offset=%d, base=%p # ..", _ret->mse_start, found, *found, offset, base/*, dump_bytes(*((char**)found + 2))*/);
            ptrlog_add(&aaa, (uint32_t)base);
          } else {
            myPrint("    splitted at %p, found = %p, base = null", _ret->mse_start, found);
          }
        }
      }else if ((backup.mse_descr.w & 0xFFFFFE03) == 0){ // /* Small object with length descriptor */
        myPrint("    case B");
        uint32_t* found = NULL;
        uint32_t size = il2cpp_object_get_size((Il2CppObject*)(aaa.ptrs[count].ptr ^ PTR_MASK));
        for(uint32_t* cur = (uint32_t*)(backup.mse_start); cur < (uint32_t*)((uint32_t)backup.mse_start + backup.mse_descr.w); ++cur){
          
          if (*cur >=  (aaa.ptrs[count].ptr ^ PTR_MASK) && *cur < (aaa.ptrs[count].ptr ^ PTR_MASK) + size){
            found = cur;
          }
          // myPrint("[Splitted] testing %p", *cur);
        }
        void* base = found ? GC_base(found) : 0;
        
        if (base){
          // offset 大于 0, 通常就是假引用
          myPrint("    smallobj %p, found ref at %p, val=%p, offset=%d, base=%p # ..", backup.mse_start, found, *found, *found - (aaa.ptrs[count].ptr ^ PTR_MASK), base/*, dump_bytes(*((char**)found + 2))*/);
          ptrlog_add(&aaa, (uint32_t)base);
        } else {
          myPrint("    smallobj %p, found = %p, base = null", backup.mse_start, found);
        }
      }else if ((backup.mse_descr.w & 0x3) == 0x3){ // GC_DS_PER_OBJECT
        myPrint("    case C");
        void* base = backup.mse_start ? GC_base(backup.mse_start) : 0;
        if (base){
          my_il2cpp_obj* r = (my_il2cpp_obj*)base;
#if IL2CPP_RUNTIME_DUMP
          Il2CppClass* klass = il2cpp_object_get_class(base);
          Il2CppType* typ = klass ? il2cpp_class_get_type(klass): NULL;
          myPrint("    refby %p %s %s", base, klass?il2cpp_class_get_name(klass):"???", typ?il2cpp_type_get_name(typ):"???");
#else
          myPrint("    refby %p kind=%p, monitor=%p", base, r->kind, r->monitor);
#endif
          
          ptrlog_add(&aaa, (uint32_t)base);
        }
      }else{
        myPrint("    case D");
      }
    }
  }
  return ret;
}

typedef struct MonoImage{
  char reserve1[8];
  void* data; // 8
  uint32_t len; // 12
  int reserve2;// 16
  const char* name;
} MonoImage;
  
typedef void* (*ty_do_mono_image_load)(MonoImage* image, void* status, int, int);
void* (*do_mono_image_load)(MonoImage* image, void* status, int, int);
void* (*old_do_mono_image_load)(MonoImage* image, void* status, int, int);
void* new_do_mono_image_load(MonoImage* image, void* status, int unk0, int unk1){
  if (image->name){
    // p = strrchr(name, '/');
    myPrint("mono load image: %s", image->name);
    /*
    if (strstr(image->name, "mscorlib.dll")){
      FILE* fp = fopen("/sdcard/mscorlib_ljj.dll", "r");
      if (fp){
        myPrint("found /sdcard/mscorlib_ljj.dll: %p", fp);
        
        int len;
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        char* buf = (char*)malloc(len);
        fread(buf, len, 1, fp);
        myPrint("found /sdcard/mscorlib_ljj.dll: buf=%p, len=%d", buf, len);
        
        
        image->data = (void*)buf;
        image->len = len;
        
        fclose(fp);
      }
    }
    */
  }
  /*
  char path[500];
  char rppath[500];
  const char* p = NULL;
  char* my_data;
  uint32_t my_len;
  char *data = (char*)image->data;
  uint32_t data_len = image->len;
  const char *name = image->name;
  
  uid_t uid = getuid();
  if (image->name){
    p = strrchr(name, '/');
  }
  
  if (p){
    snprintf(path, 500, "/sdcard/wzry_orig%s", p);
    snprintf(rppath, 500, "/sdcard/hook_%d%s", uid, p);
  } else {
    snprintf(path, 500, "/sdcard/wzry_orig/unknow_%d.dll", image->len);
    snprintf(rppath, 500, "/sdcard/hook_%d/unknow_%d.dll", uid, image->len);
  }
  write_to_file(path, data, data_len);
  */
  
  /*
  if (load_managed_dll(rppath, &my_data, &my_len) == 0){
    DEBUG_PRINT("try_load %s successful.", rppath);
    data = my_data;
    data_len = my_len;
  }
  */
  return old_do_mono_image_load(image, status, unk0, unk1);
}
  
int il2cpp_is_object_marked(uint32_t v17){
  // __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "checking %p", v17);
  char* v18 = (char *)GC_arrays + 4 * (v17 >> 22);
  uint32_t v20 = *((uint32_t *)v18 + BI_OFFSET_IN_U32);
  uint32_t v21 = (v17 >> 8) & 0xF;
  return (*(uint32_t *)(*(uint32_t *)(v20 + 4 * ((v17 >> 12) & 0x3FF)) + 4 * v21 + 32) & (1 << ((unsigned char)v17 >> 3))) != 0;
}

#define JX3M 1
#define SSSJ 0

#define ADDR_OF_IL2CPP_GC_GET_HEAP_SIZE 0x04CB6374
#define ADDR_OF_GC_DUMP_NAMED 0x04cef6c8
#define ADDR_OF_GC_PRINTF 0x04cec29c
#define ADDR_OF_GC_LOG_PRINTF 0x04ce9eb8
#define ADDR_OF_GC_QUIET 0x06120174
#define ADDR_OF_GC_PRINT_STATS 0x0611fd14
#define ADDR_OF_GC_MARK_FROM 0x04cf2ea0
#define ADDR_OF_GC_ARRAYS 0x06194564
#define ADDR_OF_GC_MARK_STATE 0x06120068
#define ADDR_OF_GC_BASE 0x04ce98a0
#define ADDR_OF_MOV_R0_4K 0x04CF2F04
#
void il2cpp_setup(void* lib){
  pthread_t t;
  il2cpp_gc_get_heap_size = (size_t (*)())dlsym(lib, "il2cpp_gc_get_heap_size");
  il2cpp_gc_get_used_size = (size_t (*)())dlsym(lib, "il2cpp_gc_get_used_size");
  il2cpp_object_get_class = (Il2CppClass* (*)(Il2CppObject* obj))dlsym(lib, "il2cpp_object_get_class");
  il2cpp_profiler_install_allocation = (void(*)(Il2CppProfileAllocFunc callback))dlsym(lib, "il2cpp_profiler_install_allocation");
  il2cpp_profiler_install_gc = (void (*)(Il2CppProfileGCFunc callback, Il2CppProfileGCResizeFunc heap_resize_callback))dlsym(lib, "il2cpp_profiler_install_gc");
  il2cpp_profiler_set_events = (void (*)(enum Il2CppProfileFlags events))dlsym(lib, "il2cpp_profiler_set_events");
  il2cpp_object_get_size = (uint32_t (*)(Il2CppObject* obj))dlsym(lib, "il2cpp_object_get_size");
  il2cpp_profiler_install = (void (*)(Il2CppProfiler *prof, Il2CppProfileFunc shutdown_callback))dlsym(lib, "il2cpp_profiler_install");
  il2cpp_class_get_name = (const char* (*)(Il2CppClass *klass))dlsym(lib, "il2cpp_class_get_name");
  il2cpp_class_get_namespace = (const char* (*)(Il2CppClass *klass))dlsym(lib, "il2cpp_class_get_namespace");
  il2cpp_class_get_type = (Il2CppType* (*)(Il2CppClass *))dlsym(lib, "il2cpp_class_get_type");
  il2cpp_type_get_name = (const char* (*)(Il2CppType *))dlsym(lib, "il2cpp_type_get_name");
  il2cpp_current_thread_walk_frame_stack = (void (*)(Il2CppFrameWalkFunc, void*))dlsym(lib, "il2cpp_current_thread_walk_frame_stack");
  il2cpp_method_get_name = (const char* (*)(const MethodInfo *))dlsym(lib, "il2cpp_method_get_name");
  
  if (il2cpp_gc_get_heap_size && il2cpp_gc_get_used_size){
    // pthread_create(&t, NULL, il2cpp_mem_worker, NULL);
  }
  
  #if JX3M
  // 版本相关, 不可复用
  if ((((uintptr_t)il2cpp_gc_get_heap_size ^ ADDR_OF_IL2CPP_GC_GET_HEAP_SIZE) & 0xfff) == 0){
    
    il2cppbase = (uintptr_t)il2cpp_gc_get_heap_size - ADDR_OF_IL2CPP_GC_GET_HEAP_SIZE;
    GC_dump_named = (void (*)(const char*))(il2cppbase + ADDR_OF_GC_DUMP_NAMED);
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "il2cpp base = %p", il2cppbase);
    uintptr_t GC_printf = il2cppbase + ADDR_OF_GC_PRINTF, _dummy;
    uintptr_t GC_log_printf = il2cppbase + ADDR_OF_GC_LOG_PRINTF;
    uintptr_t GC_quiet = il2cppbase + ADDR_OF_GC_QUIET;
    uintptr_t GC_print_stats = il2cppbase + ADDR_OF_GC_PRINT_STATS;
    
    GC_mark_from = (void* (*)(mse*,mse*,mse*))(il2cppbase + ADDR_OF_GC_MARK_FROM);
    GC_arrays = il2cppbase + ADDR_OF_GC_ARRAYS;
    GC_mark_state = (uint32_t*)(il2cppbase + ADDR_OF_GC_MARK_STATE);
    GC_base = (void*(*)(void*))(il2cppbase + ADDR_OF_GC_BASE);
    // 6499120: 0542c824 0x1f270 OBJECT  LOCAL  HIDDEN    23 GC_arrays
    BI_OFFSET_IN_U32 = 30876; 
    
    _s_is_object_marked = il2cpp_is_object_marked;

    tpmm::hook::inline_hook((uintptr_t)GC_printf, (uintptr_t)myPrint, _dummy);
    ((void (*)(const char*, ...))GC_printf)("GC_printf hooked: %p", _dummy);
    tpmm::hook::inline_hook((uintptr_t)GC_log_printf, (uintptr_t)myPrint, _dummy);
    ((void (*)(const char*, ...))GC_printf)("GC_printf hooked: %p", _dummy);
    tpmm::hook::inline_hook(GC_mark_from, new_GC_mark_from, old_GC_mark_from);
    ((void (*)(const char*, ...))GC_printf)("GC_mark_from hooked: %p", old_GC_mark_from);

    uint32_t target = il2cppbase + ADDR_OF_MOV_R0_4K;
    mprotect((void *) (target & (~4095)), 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
    if (*(uint32_t*)target == 0xe3a00a01) // MOV             R0, #0x1000
    {
      *(uint32_t*)target = 0xE3A00008; // mov R0, #0x8
      myPrint("modified credit: 0x1000 -> 0x8");
    }
    // *(uint32_t*)(GC_quiet) = 0;
    // *(uint32_t*)(GC_print_stats) = PRINT_VERBOSE;
  } else {
      myPrint("params are out-dated. saiyonara~");
  }
  #endif
  
  #if SSSJ
  if ((((uintptr_t)il2cpp_gc_get_heap_size ^ 0x007ea100) & 0xfff) == 0){ 
    
    il2cppbase = (uintptr_t)il2cpp_gc_get_heap_size - 0x007ea100;
    GC_dump_named = (void (*)(const char*))(il2cppbase + 0x0082238c);
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "il2cpp base = %p", il2cppbase);
    uintptr_t GC_printf = il2cppbase + 0x0081ef5c, _dummy;
    uintptr_t GC_log_printf = il2cppbase + 0x0081cb90;
    uintptr_t GC_quiet = il2cppbase + 0x034260cc;
    uintptr_t GC_print_stats = il2cppbase + 0x03425c6c;
    
    GC_mark_from = (void* (*)(mse*,mse*,mse*))(il2cppbase + 0x00825bb0);
    GC_arrays = il2cppbase + 0x0342c584;
    GC_mark_state = (uint32_t*)(il2cppbase + 0x03425fc0);
    GC_base = (void*(*)(void*))(il2cppbase + 0x0081c574);
    
    BI_OFFSET_IN_U32 = 122012;
    
    _s_is_object_marked = il2cpp_is_object_marked;

    if (registerInlineHook(GC_printf, (uint32_t)myPrint, (uint32_t**)&_dummy) == ELE7EN_OK){
      inlineHook(GC_printf);
      ((void (*)(const char*, ...))GC_printf)("GC_printf hooked: %p", _dummy);
    }
    if (registerInlineHook(GC_log_printf, (uint32_t)myPrint, (uint32_t**)&_dummy) == ELE7EN_OK){
      inlineHook(GC_log_printf);
      ((void (*)(const char*, ...))GC_printf)("GC_log_printf hooked: %p", _dummy);
    }
    if (registerInlineHook(GC_mark_from, (uint32_t)new_GC_mark_from, (uint32_t**)&old_GC_mark_from) == ELE7EN_OK){
      inlineHook(GC_mark_from);
      ((void (*)(const char*, ...))GC_printf)("GC_mark_from hooked: %p", old_GC_mark_from);
    }
    
    
    uint32_t target = il2cppbase + 0x00825C14; // in GC_mark_from
    mprotect((void *) (target & (~4095)), 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
    if (*(uint32_t*)target == 0xe3a00a01) // MOV             R0, #0x1000
    {
      *(uint32_t*)target = 0xE3A00008; // mov R0, #0x8
      myPrint("modified credit: 0x1000 -> 0x8");
    }
    // *(uint32_t*)(GC_quiet) = 0;
    // *(uint32_t*)(GC_print_stats) = PRINT_VERBOSE;
  }
  #endif
  myPrint("build at %s %s", __DATE__, __TIME__);
  
  __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "[%s] %p %p %p %p %p %p %p",
    __TIME__,
    il2cpp_profiler_install_allocation, 
    il2cpp_profiler_install_gc, 
    il2cpp_profiler_set_events, 
    il2cpp_object_get_size, 
    il2cpp_profiler_install, 
    il2cpp_class_get_name,
    il2cpp_object_get_class
    );
    
  #if 1
  if (il2cpp_profiler_install_allocation && il2cpp_profiler_install_gc && il2cpp_profiler_set_events && il2cpp_object_get_size && il2cpp_profiler_install && il2cpp_class_get_name && il2cpp_object_get_class){
    enum Il2CppProfileFlags events;
    int flags = IL2CPP_PROFILE_GC|IL2CPP_PROFILE_ALLOCATIONS;
    __android_log_print(ANDROID_LOG_INFO, "IL2CPP", "API READY");
    il2cpp_profiler_install(NULL, vmShutdown);
    il2cpp_profiler_set_events((Il2CppProfileFlags)flags);
    il2cpp_profiler_install_allocation(objectAlloction);
    il2cpp_profiler_install_gc(garbageCollected, heapResized);
  }
  #endif
}

void mono_setup(void* lib){
  pthread_t t;
  gIsMono = 1;
  il2cpp_gc_get_heap_size = (size_t (*)())dlsym(lib, "mono_gc_get_heap_size");
  il2cpp_gc_get_used_size = (size_t (*)())dlsym(lib, "mono_gc_get_used_size");
  il2cpp_object_get_class = (Il2CppClass* (*)(Il2CppObject* obj))dlsym(lib, "mono_object_get_class");
  il2cpp_profiler_install_allocation = (void(*)(Il2CppProfileAllocFunc callback))dlsym(lib, "mono_profiler_install_allocation");
  il2cpp_profiler_install_gc = (void (*)(Il2CppProfileGCFunc callback, Il2CppProfileGCResizeFunc heap_resize_callback))dlsym(lib, "mono_profiler_install_gc");
  il2cpp_profiler_set_events = (void (*)(enum Il2CppProfileFlags events))dlsym(lib, "mono_profiler_set_events");
  il2cpp_object_get_size = (uint32_t (*)(Il2CppObject* obj))dlsym(lib, "mono_object_get_size");
  il2cpp_profiler_install = (void (*)(Il2CppProfiler *prof, Il2CppProfileFunc shutdown_callback))dlsym(lib, "mono_profiler_install");
  il2cpp_class_get_name = (const char* (*)(Il2CppClass *klass))dlsym(lib, "mono_class_get_name");
  il2cpp_class_get_namespace = (const char* (*)(Il2CppClass *klass))dlsym(lib, "mono_class_get_namespace");
  il2cpp_class_get_type = (Il2CppType* (*)(Il2CppClass *))dlsym(lib, "mono_class_get_type");
  il2cpp_type_get_name = (const char* (*)(Il2CppType *))dlsym(lib, "mono_type_get_name");
  
  if (il2cpp_gc_get_heap_size && il2cpp_gc_get_used_size){
    // pthread_create(&t, NULL, il2cpp_mem_worker, NULL);
  }

  #if SSSJ
  if (1){
    // uintptr_t GC_printf = (uintptr_t)dlsym(lib, "GC_printf");
    il2cppbase = (uintptr_t)dlsym(lib, "mono_image_open_from_data_with_name") - 0x002CDC4C;
    GC_mark_from = (void* (*)(mse*,mse*,mse*))(dlsym(lib, "GC_mark_from"));
    GC_mark_state = (uint32_t*)(dlsym(lib, "GC_mark_state"));
    GC_base = (void* (*)(void*))(dlsym(lib, "GC_base"));
    mono_stack_walk_no_il = (void(*)(TyMonoStackWalk, void*))dlsym(lib, "mono_stack_walk_no_il");
    mono_method_full_name = (const char*(*)(void*, int))dlsym(lib, "mono_method_full_name");
    _s_is_object_marked = (int (*)(uint32_t))(dlsym(lib, "GC_is_marked"));
    do_mono_image_load = (ty_do_mono_image_load)(il2cppbase + 0x002CCF7C);
    
    uint32_t target = (uintptr_t)GC_mark_from - 0x00432CE8 + 0x00432D08; // in GC_mark_from
    mprotect((void *) (target & (~4095)), 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
    if (*(uint32_t*)target == 0xE3A03A01) // MOV             R3, #0x1000
    {
      *(uint32_t*)target = 0xE3A03008; // mov R3, #0x8
      myPrint("modified credit: 0x1000 -> 0x8");
    }
    
    if (registerInlineHook(GC_mark_from, (uint32_t)new_GC_mark_from, (uint32_t**)&old_GC_mark_from) == ELE7EN_OK){
      inlineHook(GC_mark_from);
      myPrint("GC_mark_from hooked: %p", old_GC_mark_from);
    }
    
    if (registerInlineHook(do_mono_image_load, (uint32_t)new_do_mono_image_load, (uint32_t**)&old_do_mono_image_load) == ELE7EN_OK){
      inlineHook(do_mono_image_load);
      myPrint("do_mono_image_load hooked: %p", old_do_mono_image_load);
    }
  }
  #endif
  __android_log_print(ANDROID_LOG_INFO, "tpmm-il2cpp", "[%s] %p %p %p %p %p %p %p",
    __TIME__,
    il2cpp_profiler_install_allocation, 
    il2cpp_profiler_install_gc, 
    il2cpp_profiler_set_events, 
    il2cpp_object_get_size, 
    il2cpp_profiler_install, 
    il2cpp_class_get_name,
    il2cpp_object_get_class
    );
    
  #if 1
  if (il2cpp_profiler_install_allocation && il2cpp_profiler_install_gc && il2cpp_profiler_set_events && il2cpp_object_get_size && il2cpp_profiler_install && il2cpp_class_get_name && il2cpp_object_get_class){
    enum Il2CppProfileFlags events;
    int flags = IL2CPP_PROFILE_GC|IL2CPP_PROFILE_ALLOCATIONS;
    __android_log_print(ANDROID_LOG_INFO, "tpmm-il2cpp", "API READY");
    il2cpp_profiler_install(NULL, vmShutdown);
    il2cpp_profiler_set_events((Il2CppProfileFlags)flags);
    il2cpp_profiler_install_allocation(objectAlloction);
    il2cpp_profiler_install_gc(garbageCollected, heapResized);
  }
  #endif
}

enum DLVersion{
    edl_unknown = 0,
    edl_v0,
    edl_v19,
    edl_v24,
    edl_v26
};


static void* (*old_dlopen_for_all)(const char *name, int flags, const void *extinfo, void *caller_addr) = NULL;
static void* dlopen_for_all(const char *name, int flags, const void *extinfo, void *caller_addr){
    void* ret = old_dlopen_for_all(name, flags, extinfo, caller_addr);
    // myPrint("dlopen(%s) => %p", name, ret);
    if (strstr(name, "libil2cpp")){
        static int n = 0;
        if (n++ == 0){
            myPrint("dlopen(%s) => %p", name, ret);
            il2cpp_setup(ret);
        }
    }
    return ret;
}

static bool hook_dlopen(){
    void* dlopen_addr = NULL;
    DLVersion v = edl_unknown;
    const char* linker_name = sizeof(void*) == 4 ? "/bin/linker": "/bin/linker64";
    do {
        tpmm::hook::find_symbol(linker_name, "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv", dlopen_addr);
        if (dlopen_addr){
            v = edl_v26; 
            break;
        }
        tpmm::hook::find_symbol(linker_name, "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv", dlopen_addr);
        if (dlopen_addr){
            v = edl_v24; 
            break;
        }
        tpmm::hook::find_symbol(linker_name, "__dl__Z9do_dlopenPKciPK17android_dlextinfo", dlopen_addr);
        if (dlopen_addr){
            v = edl_v19; 
            break;
        }
        tpmm::hook::find_symbol(linker_name, "__dl_dlopen", dlopen_addr);
        if (dlopen_addr){
            v = edl_v0; 
            break;
        }
    } while (0);
    LOGI("dlopen = %p, version = %d", dlopen_addr, v);
    
    if (v != edl_unknown){
        *((void**)&old_dlopen_for_all) = dlopen_addr;
        tpmm::hook::inline_hook(old_dlopen_for_all, dlopen_for_all, old_dlopen_for_all);
        return true;
    }
    return false;
}

static void inspector_setup(){
    tpmm::tls::setup();
    hook_dlopen();
}

JNIEXPORT jint __attribute__((visibility("default"))) JNI_OnLoad(JavaVM* vm, void* reserved)
{
    LOGI("JNI_OnLoad %p %p", vm, reserved);
    if (!vm){
        return 0;
    }
    JNIEnv* env = NULL;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    inspector_setup();
    LOGI("hello jni~");
    return JNI_VERSION_1_6;
}


/* GUILD

IDA
.text:04CB6374                 EXPORT il2cpp_gc_get_heap_size
.text:04CB6374 il2cpp_gc_get_heap_size

$ readelf -s --wide libil2cpp.so | grep GC_dump
8255412: 04cef6c8   284 FUNC    LOCAL  HIDDEN    12 GC_dump_named

$ readelf -s --wide libil2cpp.so | grep GC_printf
8255582: 04cec29c   280 FUNC    LOCAL  HIDDEN    12 GC_printf

$ readelf -s --wide libil2cpp.so | grep GC_log_printf
8255509: 04ce9eb8   256 FUNC    LOCAL  HIDDEN    12 GC_log_printf

$ readelf -s --wide libil2cpp.so | grep GC_quiet
8255608: 06120174     4 OBJECT  LOCAL  HIDDEN    23 GC_quiet

$ readelf -s --wide libil2cpp.so | grep GC_print_stats
8255581: 0611fd14     4 OBJECT  LOCAL  HIDDEN    23 GC_print_stats

$ readelf -s --wide libil2cpp.so | grep GC_mark_from
8255517: 04cf2ea0  2252 FUNC    LOCAL  HIDDEN    12 GC_mark_from

$ readelf -s --wide libil2cpp.so | grep GC_arrays
8255368: 06194564 0x1f270 OBJECT  LOCAL  HIDDEN    23 GC_arrays

$ readelf -s --wide libil2cpp.so | grep GC_mark_state
8255521: 06120068     4 OBJECT  LOCAL  HIDDEN    23 GC_mark_state

$ readelf -s --wide libil2cpp.so | grep GC_base
8255370: 04ce98a0   236 FUNC    LOCAL  HIDDEN    12 GC_base

$ readelf -s --wide libil2cpp.so | grep GC_finalize
8255429: 04cef8fc  2188 FUNC    LOCAL  HIDDEN    12 GC_finalize

反编译 GC_finalize
        v17 = ~*(_DWORD *)i;
        v18 = (char *)&unk_6194564 + 4 * (v17 >> 22);
        v20 = *((_DWORD *)v18 + 30876);
        v19 = v18 + 123504;
        v21 = (v17 >> 8) & 0xF;
        if ( !(*(_DWORD *)(*(_DWORD *)(v20 + 4 * ((v17 >> 12) & 0x3FF)) + 4 * v21 + 32) & (1 << ((unsigned __int8)v17 >> 3))) )
        {
30876 就是 BI_OFFSET_IN_U32, 顺着 v20 找.


反编译 GC_make_from, 找到 `MOV             R0, #0x1000` 的地址
.text:04CF2EA0 ; ---------------------------------------------------------------------------
.text:04CF2EA0                 STMFD           SP!, {R4-R11,LR}
.text:04CF2EA4                 ADD             R11, SP, #0x1C
.text:04CF2EA8                 SUB             SP, SP, #0x84
.text:04CF2EAC                 MOV             LR, R1
.text:04CF2EB0                 MOV             R8, R0
.text:04CF2EB4                 LDR             R0, =(unk_6023E00 - 0x4CF2ED4)
.text:04CF2EB8                 VMOV.I32        Q8, #0
.text:04CF2EBC                 LDR             R1, =(unk_611FFD0 - 0x4CF2EDC)
.text:04CF2EC0                 MOV             R10, #1
.text:04CF2EC4                 STR             R2, [SP,#0x2C]
.text:04CF2EC8                 ADD             R2, SP, #0x40
.text:04CF2ECC                 LDR             R9, [PC,R0] ; unk_6023E00
.text:04CF2ED0                 LDR             R0, =(unk_6120080 - 0x4CF2EEC)
.text:04CF2ED4                 LDR             R12, [PC,R1] ; unk_611FFD0
.text:04CF2ED8                 ADD             R1, R2, #0x30
.text:04CF2EDC                 VST1.64         {D16-D17}, [R1]
.text:04CF2EE0                 ADD             R1, R2, #0x20
.text:04CF2EE4                 STR             R10, [PC,R0] ; unk_6120080
.text:04CF2EE8                 MOV             R0, R2
.text:04CF2EEC                 VST1.64         {D16-D17}, [R1]
.text:04CF2EF0                 VST1.64         {D16-D17}, [R0]!
.text:04CF2EF4                 VST1.64         {D16-D17}, [R0]
.text:04CF2EF8                 SUB             R0, R8, LR
.text:04CF2EFC                 CMP             R0, #0
.text:04CF2F00                 BLT             unk_4CF36E4
.text:04CF2F04                 MOV             R0, #0x1000
.text:04CF2F08                 STR             R9, [SP,#0x30]
.text:04CF2F0C                 STR             R0, [SP,#0x3C]
.text:04CF2F10                 LDR             R0, =(aMarkStackOverf - 0x4CF2F20)
.text:04CF2F14                 STR             R12, [SP,#0x38]
.text:04CF2F18                 ADD             R0, PC, R0 ; "Mark stack overflow; current size = %lu"...
.text:04CF2F1C                 STR             R0, [SP,#4]

*/
