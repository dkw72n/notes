
const mmap = new NativeFunction(Module.findExportByName(null, "mmap"), 'pointer', ['pointer', 'size_t', 'int', 'int', 'int', 'size_t'])
// void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
const munmap = new NativeFunction(Module.findExportByName(null, "munmap"), 'int', ['pointer', 'size_t'])
// int munmap(void *addr, size_t length);

function Anonymize(lib){
  Process.enumerateRangesSync('---')
    .filter(function(x){
      return (x.file && x.file.path.indexOf(lib) != -1)
    })
    .map(function(x){
      const b = x.base
      const sz = x.size
      const y = Memory.dup(b, sz)
      munmap(b, sz)
      mmap(b, sz, 7, 0x22, -1, 0)
      Memory.copy(b, y, sz)
      Memory.protect(b, sz, x.protection)
      console.log("👻 anonymized:", x)
    })
}


