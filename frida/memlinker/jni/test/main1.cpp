#include <stdio.h>
#include <dlfcn.h>

typedef void* (*fn_dlopen)(char* path, int);
typedef void* (*fn_dlsym)(void*, char*);


int main(int argc, char** argv){
    void* h = dlopen("/data/local/tmp/libdmlnk.so", 2);
    if (!h){
      printf("[L0] dlopen:%s\n", dlerror());
      return -1;
    }
    fn_dlopen mlnk_dlopen = (fn_dlopen)dlsym(h, "dmlnk_dlopen");
    fn_dlsym mlnk_dlsym = (fn_dlsym)dlsym(h, "dmlnk_dlsym");
    printf("mlnk_dlopen = %p\n", mlnk_dlopen);
    printf("mlnk_dlsym = %p\n", mlnk_dlsym);

    void* hh = mlnk_dlopen("/data/local/tmp/libdmlnk.so", 1);
    if (!hh){
      printf("[L1] dlopen failed\n");
      return -1;
    }
    fn_dlopen mlnk_dlopen_1 = (fn_dlopen)mlnk_dlsym(hh, "dmlnk_dlopen");
    fn_dlsym mlnk_dlsym_1 = (fn_dlsym)mlnk_dlsym(hh, "dmlnk_dlsym");
    printf("mlnk_dlopen_1 = %p\n", mlnk_dlopen_1);
    printf("mlnk_dlsym_1 = %p\n", mlnk_dlsym_1);
    
    dlclose(h);

    void* ret = mlnk_dlopen_1(argv[1], 1);
    printf("[1st] dlopen(%s) => %p\n", argv[1], ret);
    if (ret) {
        void* func = mlnk_dlsym_1(ret, argv[2]);
        printf("      dlsym(%p) => %p\n", ret, func);
        if (func){
            ((int (*)())func)();
        }
    }
    ret = mlnk_dlopen_1(argv[1], 0);
    printf("[2nd] dlopen(%s) => %p\n", argv[1], ret);
    getchar();
    return 0;
}
