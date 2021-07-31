#include <stdio.h>

const char * ro_string = "ro_string";

extern "C" int __attribute__((visibility("default"))) print_hello(){
  static int static_int = 0;
  const char* p = ro_string;
  printf("\n-----------------------\n");
  printf("hello from exported function\n");
  printf("\n-----------------------\n");
  printf("addr_of(static_int) = %p\n", &static_int);
  printf("\n-----------------------\n");
  printf("addr_of(%s) = %p\n", __FUNCTION__, print_hello);
  printf("\n-----------------------\n");
  printf("ro_string(%p) = %s\n", p, p);
  printf("\n-----------------------\n");
  printf("\n-----------------------\n");
  return 1;
}


static void __attribute__((constructor)) my_ctor() {
  printf("\n\n\n\n--------------- \n hello from ctor \n --------------------- \n");
}

static void __attribute__((constructor)) my_ctor2() {
  printf("\n\n\n\n--------------- \n hello from ctor2 \n --------------------- \n");
}