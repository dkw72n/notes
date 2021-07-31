#include <stdio.h>

extern "C" void* mlnk_dlopen(char* path, int);
extern "C" void* mlnk_dlsym(void*, char*);

int main(int argc, char** argv){
    void* ret = mlnk_dlopen(argv[1], 1);
    printf("[1st] dlopen(%s) => %p\n", argv[1], ret);
    if (ret) {
        void* func = mlnk_dlsym(ret, argv[2]);
        printf("      dlsym(%p) => %p\n", ret, func);
        if (func){
            ((int (*)())func)();
        }
    }
    ret = mlnk_dlopen(argv[1], 0);
    printf("[2nd] dlopen(%s) => %p\n", argv[1], ret);
    //getchar();
    return 0;
}