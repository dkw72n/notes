#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(){
  void* addr = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  printf("to view my pss: adb shell cat /proc/%d/smaps | grep -A16 %lx\n", getpid(), addr);
  // snoopy.c: https://gist.github.com/FergusInLondon/fec6aebabc3c9e61e284983618f40730#file-snoopy-c
  printf("try snoopy me: adb shell /data/local/tmp/snoopy %d %p 4096\n", getpid(), addr);
  while(1){
    char flag = 0;
    if (mincore(addr, 4096, &flag) == 0 && flag){
      break;
    }
    sleep(1);
  }
  printf("gotcha~\n");
  return 0;
}
