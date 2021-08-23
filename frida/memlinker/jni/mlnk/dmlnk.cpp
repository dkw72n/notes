extern "C"  void* mlnk_dlopen(char* path, int);
extern "C"  void* mlnk_dlsym(void*, char*);

extern "C" __attribute__((visibility("default"))) void* dmlnk_dlopen(char* p, int h) {
    return mlnk_dlopen(p, h);
}

extern "C" __attribute__((visibility("default"))) void* dmlnk_dlsym(void* h, char* s) {
    return mlnk_dlsym(h, s);
}

extern "C" __attribute__((visibility("default"))) void* start_entry(char* p) {
    void* h = mlnk_dlopen(p, 1);
    return h;
}
