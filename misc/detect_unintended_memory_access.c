#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/prctl.h>

int check1(void* addr){
    char flag = 1;
    return mincore(addr, 4096, &flag) && flag;
}

int check2(void* addr){
    static int fd = -1;
    if (fd == -1){
        fd = open("/proc/self/pagemap", O_RDONLY);
    }
    off_t off = lseek(fd, (uintptr_t)addr / 4096 * 8, SEEK_SET);
    uint64_t buf;
    if (read(fd, &buf, sizeof(buf)) == 8){
/*
    * Bits 0-54  page frame number (PFN) if present
    * Bits 0-4   swap type if swapped
    * Bits 5-54  swap offset if swapped
    * Bit  55    pte is soft-dirty (see Documentation/vm/soft-dirty.txt)
    * Bit  56    page exclusively mapped (since 4.2)
    * Bits 57-60 zero
    * Bit  61    page is file-page or shared-anon (since 3.5)
    * Bit  62    page swapped
    * Bit  63    page present
*/
        return buf >> 63;
    }
    return 0;
}

int main(){
  void* addr = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, addr, 4096, "libc_malloc");
  printf("to view my pss: adb shell cat /proc/%d/smaps | grep -A16 %lx\n", getpid(), addr);
  // snoopy.c: https://gist.github.com/FergusInLondon/fec6aebabc3c9e61e284983618f40730#file-snoopy-c
  printf("try snoopy me: adb shell /data/local/tmp/snoopy %d %p 4096\n", getpid(), addr);
  while(1){
    if (check2(addr)){
      break;
    }
    sleep(1);
  }
  printf("gotcha~\n");
  return 0;
}