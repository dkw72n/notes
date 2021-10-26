
#include <stdio.h>  
#include <stdlib.h>
#include <asm/ptrace.h>  
#include <sys/ptrace.h>  
#include <sys/wait.h>  
#include <sys/mman.h>  
#include <dlfcn.h>  
#include <dirent.h>  
#include <unistd.h>  
#include <string.h>  
#include <time.h>
#include <elf.h>  
#include <android/log.h>  

#if defined(__aarch64__)
#include <sys/uio.h>
#endif

#if defined(__i386__)  
#define pt_regs         user_regs_struct  
#endif  
#if defined(__i386__)    
#define pt_regs         user_regs_struct    
#elif defined(__aarch64__)
#define pt_regs         user_pt_regs  
#define uregs	regs
#define ARM_pc	pc
#define ARM_sp	sp
#define ARM_cpsr	pstate
#define ARM_lr		regs[30]
#define ARM_r0		regs[0]  
#define PTRACE_GETREGS PTRACE_GETREGSET
#define PTRACE_SETREGS PTRACE_SETREGSET
#endif
#define ENABLE_DEBUG 1  
  
#if ENABLE_DEBUG  
#define  LOG_TAG "inject"  
//#define  LOGD(fmt, args...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG, fmt, ##args)
#define  LOGD(fmt, args...)  printf(fmt, ##args)
#define DEBUG_PRINT(format,args...) \
    LOGD(format "\n", ##args)  
#else  
#define DEBUG_PRINT(format,args...)
#endif  
  
#define CPSR_T_MASK     ( 1u << 5 )  

typedef void* laddr_t;
typedef unsigned long raddr_t;
#define LADDR(x) ((laddr_t)(x))
#define RADDR(x) ((raddr_t)(x))

struct Config{
  int stealth;
  int verbose;
} config;

int ptrace_setregs(pid_t pid, struct pt_regs * regs);
int ptrace_continue(pid_t pid);

int ptrace_readdata(pid_t pid, raddr_t src, laddr_t buf, size_t size)  
{  
    uint32_t i, j, remain;  
    uint8_t *laddr;  
  
    union u {  
        long val;  
        char chars[sizeof(long)];  
    } d;  
  
    j = size / 4;  
    remain = size % 4;  
  
    laddr = buf;  
  
    for (i = 0; i < j; i ++) {  
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);  
        memcpy(laddr, d.chars, 4);  
        src += 4;  
        laddr += 4;  
    }  
  
    if (remain > 0) {  
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);  
        memcpy(laddr, d.chars, remain);  
    }  
  
    return 0;  
}  
  
int ptrace_writedata(pid_t pid, raddr_t dest, laddr_t data, size_t size)  
{  
    uint32_t i, j, remain;  
    const uint8_t *laddr;  
  
    union u {  
        long val;  
        char chars[sizeof(long)];  
    } d;  
  
    j = size / 4;  
    remain = size % 4;  
  
    laddr = data;  
  
    for (i = 0; i < j; i ++) {  
        memcpy(d.chars, laddr, 4);  
        ptrace(PTRACE_POKETEXT, pid, dest, d.val);  
  
        dest  += 4;  
        laddr += 4;  
    }  
  
    if (remain > 0) {  
        d.val = ptrace(PTRACE_PEEKTEXT, pid, dest, 0);  
        for (i = 0; i < remain; i ++) {  
            d.chars[i] = *laddr ++;  
        }  
  
        ptrace(PTRACE_POKETEXT, pid, dest, d.val);  
    }  
  
    return 0;  
}  
  
#if defined(__arm__) || defined(__aarch64__)
int ptrace_call(pid_t pid, uintptr_t addr, uintptr_t *params, int num_params, struct pt_regs* regs)  
{  
    int i;
#if defined(__arm__) 
    int num_param_registers = 4;
#elif defined(__aarch64__) 
    int num_param_registers = 8;
#endif

    for (i = 0; i < num_params && i < num_param_registers; i ++) {  
        regs->uregs[i] = params[i];  
    }  
  
    //  
    // push remained params onto stack  
    //  
    if (i < num_params) {  
        regs->ARM_sp -= (num_params - i) * sizeof(void*) ;  
        ptrace_writedata(pid, (void *)regs->ARM_sp, (uint8_t *)&params[i], (num_params - i) * sizeof(long));  
    }  
  
    regs->ARM_pc = addr;  
    if (regs->ARM_pc & 1) {  
        /* thumb */  
        regs->ARM_pc &= (~1u);  
        regs->ARM_cpsr |= CPSR_T_MASK;  
    } else {  
        /* arm */  
        regs->ARM_cpsr &= ~CPSR_T_MASK;  
    }  
  
    regs->ARM_lr = 0;      
  
    if (ptrace_setregs(pid, regs) == -1   
            || ptrace_continue(pid) == -1) {  
        printf("error\n");  
        return -1;  
    }  
  
  int stat = 0;
  waitpid(pid, &stat, WUNTRACED);
  while (stat != 0xb7f) {
    if (ptrace_continue(pid) == -1) {
      printf("error\n");
      return -1;
    }
    waitpid(pid, &stat, WUNTRACED);
  }
  
    return 0;  
}  
  
#elif defined(__i386__)  
static int syscall_flag = 0;

long ptrace_call(pid_t pid, uintptr_t addr, long *params, uint32_t num_params, struct user_regs_struct * regs)  
{  
  regs->esp -= (num_params) * sizeof(long) ;  
  ptrace_writedata(pid, (void *)regs->esp, (uint8_t *)params, (num_params) * sizeof(long));  

  long tmp_addr = 0x00;  
  regs->esp -= sizeof(long);  
  ptrace_writedata(pid, (void*)regs->esp, (uint8_t *)&tmp_addr, sizeof(tmp_addr));   

  if (syscall_flag){
    regs->eip = addr + 2;
  } else {
    regs->eip = addr;
  }

  if (ptrace_setregs(pid, regs) == -1   
          || ptrace_continue( pid) == -1) {  
    printf("error\n");  
    return -1;  
  }  
  
  
  int stat = 0;
  waitpid(pid, &stat, WUNTRACED);
  while (stat != 0xb7f) {
  printf("waitpid: %x\n", stat);
  if (ptrace_continue(pid) == -1) {
    printf("error\n");
    return -1;
  }
    waitpid(pid, &stat, WUNTRACED);
  }
  
  syscall_flag = 0; // clear the flag after ptrace_continue
  return 0;  
}  
#else   
#error "Not supported"  
#endif  
  
int ptrace_getregs(pid_t pid, struct pt_regs * regs)  
{  
#if defined(__aarch64__)
    struct iovec iovec = { regs, sizeof(*regs) };
    if (ptrace(PTRACE_GETREGSET, pid, (void *)NT_PRSTATUS, &iovec) < 0){
        perror("ptrace_getregs: Can not get register values");  
        return -1;  
    }
#else
    if (ptrace(PTRACE_GETREGS, pid, NULL, regs) < 0) {  
        perror("ptrace_getregs: Can not get register values");  
        return -1;  
    }  
#endif 
    return 0;  
}  
  
int ptrace_setregs(pid_t pid, struct pt_regs * regs)  
{  
#if defined(__aarch64__)
    struct iovec iovec = { regs, sizeof(*regs) };
    if (ptrace(PTRACE_SETREGSET, pid, (void *)NT_PRSTATUS, &iovec) < 0){
        perror("ptrace_setregs: Can not set register values");  
        return -1;  
    }
#else
    if (ptrace(PTRACE_SETREGS, pid, NULL, regs) < 0) {  
        perror("ptrace_setregs: Can not set register values");  
        return -1;  
    }  
#endif
    return 0;  
}  
  
int ptrace_continue(pid_t pid)  
{  
    if (ptrace(PTRACE_CONT, pid, NULL, 0) < 0) {  
        perror("ptrace_cont");  
        return -1;  
    }  
  
    return 0;  
}  
  
int ptrace_attach(pid_t pid)  
{  
    if (ptrace(PTRACE_ATTACH, pid, NULL, 0) < 0) {  
        perror("ptrace_attach");  
        return -1;  
    }  
  
    int status = 0;  
    waitpid(pid, &status , WUNTRACED);  
  
    return 0;  
}  
  
int ptrace_detach(pid_t pid)  
{  
    if (ptrace(PTRACE_DETACH, pid, NULL, 0) < 0) {  
        perror("ptrace_detach");  
        return -1;  
    }  
  
    return 0;  
}  
  
void* get_module_base(pid_t pid, const char* module_name)  
{  
    FILE *fp;  
    long addr = 0;  
    char *pch;  
    char filename[32];  
    char line[1024];  
  
    if (pid < 0) {  
        /* self process */  
        snprintf(filename, sizeof(filename), "/proc/self/maps");  
    } else {  
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);  
    }  
  
    fp = fopen(filename, "r");  
  
    if (fp != NULL) {  
        while (fgets(line, sizeof(line), fp)) {  
            if (strstr(line, module_name)) {  
                pch = strtok( line, "-" );  
                addr = strtoul( pch, NULL, 16 );  
  
                if (addr == 0x8000)  
                    addr = 0;  
  
                break;  
            }  
        }  
  
        fclose(fp) ;  
    }  
  
    return (void *)addr;  
}


void* get_module_offset(pid_t pid, const char* module_name, uintptr_t offset)  
{  
    FILE *fp;  
    long addr = 0;  
    char *pch;  
    char filename[32];  
    char line[1024]; 
    char perm[64];
    void* ret = NULL;

    if (pid < 0) {  
        /* self process */  
        snprintf(filename, sizeof(filename), "/proc/self/maps");  
    } else {  
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);  
    }  

 	DEBUG_PRINT("[-] %s %s(%s, %p)", __FUNCTION__, filename, module_name, offset);
    fp = fopen(filename, "r");  
  
    if (fp != NULL) {  
        while (fgets(line, sizeof(line), fp)) {  
          uintptr_t start, end, offs;
	  int pos, t;

	  if (!strstr(line, module_name)) continue;

          if ((t = sscanf(line, "%lx-%lx %s%lx%*s%*s%n", &start, &end, perm, &offs, &pos)) == 4){
          	DEBUG_PRINT("[m] %s",line);
		if (!strstr(perm, "xp"))continue;
		  if (offset >= offs && offset < offs + end - start){
			  ret = (void*)(start + offset - offs);
			  break;
		  }
  
          }  
        }  
  
        fclose(fp) ;  
    }  
  
    return ret;  
}

int get_module_name(void* local_addr, uintptr_t* module_offset, char* module_name, size_t name_size){
  static char s_line[2048];
  FILE* fp = fopen("/proc/self/maps", "rb");
  if (!fp){
          DEBUG_PRINT("[!] cannot open /proc/self/maps");
          return -1;
  }
  while(fgets(s_line, sizeof(s_line), fp)){
          uintptr_t start, end, offset;
	  int pos, t;
          // DEBUG_PRINT("[ ] %s",s_line);
          if ((t = sscanf(s_line, "%lx-%lx %*s%lx%*s%*s%n", &start, &end, &offset, &pos)) == 3){
                  while (s_line[pos] == ' ') pos += 1;
	    	  // DEBUG_PRINT("[-] map line: %08llx to %08llx", start, end);
                  if (start <= (uintptr_t)local_addr && end > (uintptr_t)local_addr){
                          int l;
                          strncpy(module_name, s_line + pos, name_size);
                          l = strlen(module_name);
                          while (l){
                                  if(module_name[l - 1] == '\r' || module_name[l-1] == '\n' || module_name[l-1] == ' '){
                                          module_name[--l] = 0;
                                  } else {
                                          break;
                                  }
                          }
                          module_name[l] = 0;
			  *module_offset = offset + local_addr - start;
			  DEBUG_PRINT("[+] found local entry %lx-%lx %lx %s", start, end, offset, module_name);
                          return 0;
                  }
          } else {
		  DEBUG_PRINT("[-] sscanf failed: %d", t);
	  }
  }
  DEBUG_PRINT("[!] get no module name for addresss %p", local_addr);
  fclose(fp);
  exit(-1);
  return -1;
}
  
void* get_remote_addr(pid_t target_pid, void* local_addr)  
{  
	void* local_handle, *remote_handle;
	uintptr_t offset;
	char module_name[256];
	if (get_module_name(local_addr, &offset, module_name, 256) != 0){
		return NULL;
	}


	void* ret_addr = get_module_offset(target_pid, module_name, offset);  


	DEBUG_PRINT("[+] get_remote_addr: local[%p], remote[%p]", local_addr, ret_addr);  


#if defined(__i386__)  && 0
	if (!strcmp(module_name, libc_path)) {  
		ret_addr += 2;  
	}  
#endif  
	return ret_addr;  
}  
  
int find_pid_of(const char *process_name)  
{  
    int id;  
    pid_t pid = -1;  
    DIR* dir;  
    FILE *fp;  
    char filename[32];  
    char cmdline[256];  
  
    struct dirent * entry;  
  
    if (process_name == NULL)  
        return -1;  
  
    dir = opendir("/proc");  
    if (dir == NULL)  
        return -1;  
  
    while((entry = readdir(dir)) != NULL) {  
        id = atoi(entry->d_name);  
        if (id != 0) {  
            sprintf(filename, "/proc/%d/cmdline", id);  
            fp = fopen(filename, "r");  
            if (fp) {  
                fgets(cmdline, sizeof(cmdline), fp);  
                fclose(fp);  
  
                if (strcmp(process_name, cmdline) == 0) {  
                    /* process found */  
                    pid = id;  
                    break;  
                }  
            }  
        }  
    }  
  
    closedir(dir);  
    return pid;  
}  
  
static char libc[1024];
static char linker[1024];

uintptr_t ptrace_retval(struct pt_regs * regs)  
{  
#if defined(__arm__) || defined(__aarch64__)
    return regs->ARM_r0;  
#elif defined(__i386__)  
    return regs->eax;  
#else  
#error "Not supported"  
#endif  
}  
  
uintptr_t ptrace_ip(struct pt_regs * regs)  
{  
#if defined(__arm__) || defined(__aarch64__)
    return regs->ARM_pc;  
#elif defined(__i386__)  
    return regs->eip;  
#else  
#error "Not supported"  
#endif  
}  
  
int ptrace_call_wrapper(pid_t target_pid, const char * func_name, void * func_addr, uintptr_t * parameters, int param_num, struct pt_regs * regs)   
{ 
    char log_msg[512];
    int pos = 0;
    uintptr_t ret=0x1337, pc=0x1337;
    int succ = 0;
    pos += sprintf(log_msg + pos, "ptrace_call %s(", func_name);
    for (int i = 0; i < param_num; ++i){
      if (i != 0) log_msg[pos++] = ',';
      pos += sprintf(log_msg + pos, "%p", parameters[i]);  
    }
    log_msg[pos++] = ')';
    if ((succ = ptrace_call(target_pid, (uintptr_t)func_addr, parameters, param_num, regs)) == -1)  
        goto exit0;
    if ((succ = ptrace_getregs(target_pid, regs)) == -1)  
        goto exit0;
    ret = ptrace_retval(regs);
    pc = ptrace_ip(regs);
exit0:
    pos += sprintf(log_msg + pos, " = %p // pc=%p", ret, pc); 
    log_msg[pos] = 0;
    DEBUG_PRINT("[+] [%s] %s", succ == -1? "FAIL":"SUCC", log_msg);
    return succ;  
}

static void dump_bytes(const char* tag, uint8_t *addr, size_t size){
  int i = 0;
  printf("%s: ", tag);
  for(;i < size; ++i){
    printf(" %02x", addr[i]);
  }
  printf("\n");
}

static void ptrace_dump(const char* tag, pid_t pid, uint8_t *addr, size_t size){
  uint8_t* buf = malloc(size);
  if (ptrace_readdata(pid, addr, buf, size) != -1){
    dump_bytes(tag, buf, size);
  } else {
    DEBUG_PRINT("dump failed: ptrace_readdata returns -1");
  }
  free(buf);
}

int inject_remote_process(pid_t target_pid, const char *library_path, const char *function_name, const char *param, size_t param_size)  
{  
    int ret = -1;  
    void *mmap_addr, *munmap_addr, *dlopen_addr, *dlsym_addr, *dlclose_addr, *dlerror_addr, *memcpy_addr;  
    void *local_handle, *remote_handle, *dlhandle;  
    uint8_t *map_base = 0;  
    uint8_t *dlopen_param1_ptr, *dlsym_param2_ptr, *saved_r0_pc_ptr, *inject_param_ptr, *remote_code_ptr, *local_code_ptr;  
    int status = 0;
    struct pt_regs regs, original_regs;  
    extern uint32_t _dlopen_addr_s, _dlopen_param1_s, _dlopen_param2_s, _dlsym_addr_s, \
        _dlsym_param2_s, _dlclose_addr_s, _inject_start_s, _inject_end_s, _inject_function_param_s, \
        _saved_cpsr_s, _saved_r0_pc_s;
  
    uint32_t code_length;  
    uintptr_t parameters[10];  
    int lsys = 0;
    DEBUG_PRINT("[+] Injecting process: %d", target_pid);  
  
    if (ptrace_attach(target_pid) == -1)  
        goto exit;  
    
    DEBUG_PRINT("[+] get reg... ");
    if (ptrace_getregs(target_pid, &regs) == -1)  
        goto exit;
    
#if defined(__i386__)
    unsigned short ins;
    DEBUG_PRINT("[+] eip = %lx", regs.eip);
    ptrace_dump("eip - 2", target_pid, (uint8_t*)(regs.eip - 2), 20);
    if (ptrace_readdata(target_pid, (uint8_t*)(regs.eip - 2), &ins, sizeof(ins)) == -1)
      goto exit;
    if (ins == 0x80cd){
      syscall_flag = 1;
      lsys = 1;
    }
    // dump_bytes("EIP", (uint8_t*)((uint32_t)(regs.eip) - 2), 50);
#endif

/*
    // waitpid(target_pid, status, WUNTRACED);
    ptrace(PTRACE_SYSCALL, target_pid, NULL, NULL);
    DEBUG_PRINT("[+] waiting syscall enter... ");
    wait(&status);
#if defined(__i386__)
    DEBUG_PRINT("[+] syscall(%d) => %x", regs.orig_eax, regs.eax);
#endif
    ptrace(PTRACE_SYSCALL, target_pid, NULL, NULL);
    DEBUG_PRINT("[+] waiting syscall exit... ");
    wait(&status);
#if defined(__i386__)
    DEBUG_PRINT("[+] syscall(%d) => %x", regs.orig_eax, regs.eax);
#endif
    DEBUG_PRINT("[+] get reg... ");
    if (ptrace_getregs(target_pid, &regs) == -1)  
        goto exit;
    
#if defined(__i386__)
    DEBUG_PRINT("[+] eip = %x", regs.eip);
    ptrace_dump("eip - 2", target_pid, regs.eip - 2, 20);
    // dump_bytes("EIP", (uint8_t*)((uint32_t)(regs.eip) - 2), 50);
#endif
*/
    /* save original registers */  
    memcpy(&original_regs, &regs, sizeof(regs));  
  
    mmap_addr = get_remote_addr(target_pid, (void *)mmap);  
    DEBUG_PRINT("[+] Remote mmap address: %p", mmap_addr);  
  
    munmap_addr = get_remote_addr(target_pid, (void *)munmap);  
    DEBUG_PRINT("[+] Remote munmap address: %p", munmap_addr);
    
    memcpy_addr = get_remote_addr(target_pid, (void*)memcpy);
    DEBUG_PRINT("[+] Remote memcpy addr: %p", memcpy_addr);

    /* call mmap */  
    parameters[0] = 0;  // addr  
    parameters[1] = 0x4000; // size  
    parameters[2] = PROT_READ | PROT_WRITE | PROT_EXEC;  // prot  
    parameters[3] =  MAP_ANONYMOUS | MAP_PRIVATE; // flags  
    parameters[4] = 0; //fd  
    parameters[5] = 0; //offset  
  
    if (ptrace_call_wrapper(target_pid, "mmap", mmap_addr, parameters, 6, &regs) == -1)  
        goto exit;  
  
    map_base = ptrace_retval(&regs);  
    
    if (map_base == 0){
      DEBUG_PRINT("[!] Failed to mmap\n"); 
      goto exit;
    }
    dlopen_addr = get_remote_addr( target_pid, (void *)dlopen );  
    dlsym_addr = get_remote_addr( target_pid, (void *)dlsym );  
    dlclose_addr = get_remote_addr( target_pid, (void *)dlclose );  
    dlerror_addr = get_remote_addr( target_pid, (void *)dlerror );  
  
    DEBUG_PRINT("[+] Get imports: dlopen: %p, dlsym: %p, dlclose: %p, dlerror: %p",  
            dlopen_addr, dlsym_addr, dlclose_addr, dlerror_addr);  
  
    DEBUG_PRINT("[+] library path = %s\n", library_path);  
    ptrace_writedata(target_pid, map_base, library_path, strlen(library_path) + 1);  
  
    parameters[0] = (long)map_base;     
    parameters[1] = RTLD_NOW| RTLD_GLOBAL;   
  
    if (ptrace_call_wrapper(target_pid, "dlopen", dlopen_addr, parameters, 2, &regs) == -1)  
        goto exit;  
  
    void * sohandle = ptrace_retval(&regs);  
 
#if 0
    {
      char filename[50];
      char line[1024];
      struct range {
        uintptr_t begin, end;
        int prot;
      } r[20];
      int nRange = 0;
      snprintf(filename, sizeof(filename), "/proc/%d/maps", target_pid);

      FILE* fp = fopen(filename, "r");

      if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, library_path)) {
                unsigned long a = 0,b = 0;
                char p[5];
                sscanf(line, "%lx-%lx %s", &a, &b, p);
                r[nRange].begin = a;
                r[nRange].end = b;
                r[nRange].prot = 0;
                if (p[0] == 'r') r[nRange].prot |= PROT_READ;
                if (p[1] == 'w') r[nRange].prot |= PROT_WRITE;
                if (p[2] == 'x') r[nRange].prot |= PROT_EXEC;
                nRange++;
            }
        }
        fclose(fp) ;
        fp = NULL;
      }
      if (nRange == 0){
        DEBUG_PRINT("[+] no map entry found containing `%s`", library_path);
        goto fail0;
      }
      uintptr_t max_len = 0; 
      for (int i = 0; i < nRange; ++i){
        uintptr_t tmp;
        DEBUG_PRINT("%lx-%lx %x", r[i].begin, r[i].end, r[i].prot);
        if ((tmp = r[i].end - r[i].begin) > max_len){
          max_len = tmp;
        }
      }
      
      /* call mmap */
      parameters[0] = 0;  // addr
      parameters[1] = max_len; // size
      parameters[2] = PROT_READ | PROT_WRITE | PROT_EXEC;  // prot
      parameters[3] =  MAP_ANONYMOUS | MAP_PRIVATE; // flags
      parameters[4] = (long)-1; //fd
      parameters[5] = 0; //offset

      if (ptrace_call_wrapper(target_pid, "mmap", mmap_addr, parameters, 6, &regs) == -1)
        goto fail0;

      void* tmp_buf = ptrace_retval(&regs);
      DEBUG_PRINT("[+] tmp_buf = %p", tmp_buf);
      for (int i = 0; i < nRange; ++i){
        parameters[0] = (long)tmp_buf;
        parameters[1] = (long)r[i].begin;
        parameters[2] = r[i].end - r[i].begin;
        if (ptrace_call_wrapper(target_pid, "memcpy", memcpy_addr, parameters, 3, &regs) == -1)
          goto fail0;
        
        DEBUG_PRINT("[>] memcpy %p => %p", parameters[1], parameters[0]);

        parameters[0] = r[i].begin;
        parameters[1] = r[i].end - r[i].begin;
        if (ptrace_call_wrapper(target_pid, "munmap", munmap_addr, parameters, 2, &regs) == -1)
          goto fail0;
        DEBUG_PRINT("munmap");

        /* call mmap */
        parameters[0] = r[i].begin;  // addr
        parameters[1] = r[i].end - r[i].begin; // size
        parameters[2] = r[i].prot | PROT_WRITE;  // prot
        parameters[3] =  MAP_ANONYMOUS | MAP_PRIVATE; // flags
        parameters[4] = (long)-1; //fd
        parameters[5] = 0; //offset

        if (ptrace_call_wrapper(target_pid, "mmap", mmap_addr, parameters, 6, &regs) == -1)
          goto fail0;
        void* addr = ptrace_retval(&regs);
        DEBUG_PRINT("mmap %p => %p", r[i].begin, addr);

        if ((long)addr != r[i].begin) goto fail0;

        parameters[0] = (long)r[i].begin;
        parameters[1] = (long)tmp_buf;
        parameters[2] = r[i].end - r[i].begin;
        if (ptrace_call_wrapper(target_pid, "memcpy", memcpy_addr, parameters, 3, &regs) == -1)
          goto fail0;
        
        DEBUG_PRINT("[<] memcpy %p => %p", parameters[1], parameters[0]);

      }
      parameters[0] = (long)tmp_buf;
      parameters[1] = max_len;
      if (ptrace_call_wrapper(target_pid, "munmap", munmap_addr, parameters, 2, &regs) == -1)
        goto fail0;

      fail0:
      ;
    }
#endif

#define FUNCTION_NAME_ADDR_OFFSET       0x100  
    ptrace_writedata(target_pid, (long)map_base + FUNCTION_NAME_ADDR_OFFSET, function_name, strlen(function_name) + 1);  
    parameters[0] = sohandle;     
    parameters[1] = (long)map_base + FUNCTION_NAME_ADDR_OFFSET;   
  
    if (ptrace_call_wrapper(target_pid, "dlsym", dlsym_addr, parameters, 2, &regs) == -1)  
        goto exit;  
  
    void * hook_entry_addr = ptrace_retval(&regs);  
    DEBUG_PRINT("%s_addr = %p\n", function_name, hook_entry_addr);  
  
#define FUNCTION_PARAM_ADDR_OFFSET      0x200  
    ptrace_writedata(target_pid, (long)map_base + FUNCTION_PARAM_ADDR_OFFSET, (const uint8_t*)param, strlen(param) + 1);  
    parameters[0] = (long)map_base + FUNCTION_PARAM_ADDR_OFFSET;    
 
    if (ptrace_call_wrapper(target_pid, function_name, hook_entry_addr, parameters, 1, &regs) == -1)  
        goto exit;  	
  
#ifdef USE_ORIGINAL_VERSION
    printf("Press enter to dlclose and detach\n");  
    getchar();  
    parameters[0] = sohandle;     
  
    if (ptrace_call_wrapper(target_pid, "dlclose", dlclose, parameters, 1, &regs) == -1)  
        goto exit;  
#endif
    parameters[0] = (long)map_base;
    parameters[1] = 0x4000;
    
    ptrace_dump("[r]mmap", target_pid, mmap_addr, 50);
    // dump_bytes("[l]mmap", (uint8_t*)mmap, 50);
    ptrace_dump("[r]munmap", target_pid, munmap_addr, 50);
    // dump_bytes("[l]munmap", (uint8_t*)munmap, 50);
    
    if (ptrace_call_wrapper(target_pid, "munmap", munmap_addr, parameters, 2, &regs) == -1)  
        goto exit;

    /* restore */ 
#if defined(__i386__) && 0
    if (lsys == 1 && syscall_flag == 0){
      original_regs.eip -= 2;
    }
#endif    
    ptrace_setregs(target_pid, &original_regs);  
    ptrace_detach(target_pid);  
    DEBUG_PRINT("detach\n");
    ret = 0;
exit:
    DEBUG_PRINT("exit\n");
    return ret;  
}  

int main(int argc, char** argv) {
	pid_t target_pid;

	DEBUG_PRINT("[+] ARCH: "
#if defined(__aarch64__)
  "ARM64"
#elif defined(__arm__)
  "ARM"
#elif defined(__i386__)
  "X86"
#else
  "? ARCH not supported"
#endif
  ""
  );
 
	 int depth = 0;
	 DEBUG_PRINT("[+] main:%p", main);	
	 //_Unwind_Backtrace(trace_callback, &depth);  


	if(argc != 4){
		DEBUG_PRINT("./inject appname sopath param\n");
		DEBUG_PRINT("such as:./inject APP_OR_PID /data/data/libperformance.so BLAHBLAH");
		return 0;
	}

	DEBUG_PRINT("[+] cmd: %s %s %s %s", argv[0], argv[1], argv[2], argv[3]);
  DEBUG_PRINT("Sizeof(long) = %d\n", sizeof(long));
	int begin_ts = time(NULL);
	
  target_pid = strtol(argv[1], NULL, 10);
  if (target_pid != 0){
    DEBUG_PRINT("[+] use pid from argc: %d\n", target_pid);
  } else {
    do{
      target_pid = find_pid_of(argv[1]);
    
      DEBUG_PRINT("[+] find_pid: %d", target_pid);

      if(target_pid == -1)
        usleep(50);

      if(time(NULL) - begin_ts > 10){
        DEBUG_PRINT("[!] oops~ timeout!");
        exit(0);
      }
    }while(target_pid == -1);
  }

	DEBUG_PRINT("[+] argv1:%s", argv[1]);
	DEBUG_PRINT("[+] argv2:%s",argv[2]);
	DEBUG_PRINT("[+] argv3:%s length:%u",argv[3], strlen(argv[3]));
	
	return 	inject_remote_process( target_pid, argv[2], "start_entry", argv[3], strlen(argv[3]));
}

// #ref
//  [1] http://bbs.pediy.com/showthread.php?t=141355 - 发个Android平台上的注入代码
//  [2] https://blog.csdn.net/jinzhuojun/article/details/9900105 - Android中的so注入(inject)和挂钩(hook) - For both x86 and arm
//  [3] https://bbs.pediy.com/thread-189475.htm - Android Libinject X86平台EIP-2的分析


// todo: https://github.com/DynamoRIO/dynamorio/blob/master/core/unix/injector.c
