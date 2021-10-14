#include <android/log.h>
extern "C"  void* mlnk_dlopen(char* path, int);
extern "C"  void* mlnk_dlsym(void*, char*);

extern "C" __attribute__((visibility("default"))) void* dmlnk_dlopen(char* p, int h) {
    return mlnk_dlopen(p, h);
}

extern "C" __attribute__((visibility("default"))) void* dmlnk_dlsym(void* h, char* s) {
    return mlnk_dlsym(h, s);
}

extern "C" __attribute__((visibility("default"))) void* start_entry(char* p) {
    __android_log_print(ANDROID_LOG_INFO, "MLNK", "Start Entry!");
    void* h = mlnk_dlopen(p, 1);
    return p;
}

extern "C" {

  #include <sys/mman.h>
  typedef void* (*ty_mmap)(void*, size_t, int, int, int, off_t);
  typedef int (*ty_munmap)(void*, size_t);
  typedef int (*ty_mprotect)(void*, size_t, int);
  typedef void* (*ty_memcpy)(void*, void*, size_t);
  struct Range{
    void* start;
    int len;
    int prot;
  };

  struct Ctx{
    ty_mmap fn_mmap;
    ty_munmap fn_munmap;
    ty_mprotect fn_mprotect;
    ty_memcpy fn_memcpy;
    struct Range r[20];
    int nrange;
  };

  static void do_hide(struct Ctx* ctx){
    int i = 0;
    int max_len = 0;
    void* buf;
    for(i = 0; i < ctx->nrange; ++i){
      if (ctx->r[i].len > max_len) max_len = ctx->r[i].len;
    }
    buf = ctx->fn_mmap(NULL, max_len, 3, 0x22, -1, 0);
    for(i = 0; i < ctx->nrange; ++i){
      ctx->fn_memcpy(buf, ctx->r[i].start, ctx->r[i].len);
      ctx->fn_munmap(ctx->r[i].start, ctx->r[i].len);
      ctx->fn_mmap(ctx->r[i].start, ctx->r[i].len, 3, 0x22, -1, 0);
      ctx->fn_memcpy(ctx->r[i].start, buf, ctx->r[i].len);
      ctx->fn_mprotect(ctx->r[i].start, ctx->r[i].len, ctx->r[i].prot);
    }
  }

  static void  __attribute__((constructor)) ctor0(){
  }
}
