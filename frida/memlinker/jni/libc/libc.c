
/* Find the .ARM.exidx section (which in the case of a static executable
 * can be identified through its start and end symbols), and return its
 * beginning and numbe of entries to the caller.  Note that for static
 * executables we do not need to use the value of the PC to find the
 * EXIDX section.
 */
#ifdef __arm__
typedef long unsigned int* _Unwind_Ptr;
extern long unsigned int __exidx_end;
extern long unsigned int __exidx_start;

_Unwind_Ptr __gnu_Unwind_Find_exidx(_Unwind_Ptr pc,
                                    int *pcount)
{
    *pcount = (__exidx_end-__exidx_start)/8;
    return __exidx_start;
}
#endif

char* strchr(const char* s, int c){
  const char *p = s;
  while(*p != 0){
    if (*p == c) return (char*)p;
    p++;
  }
  return (char*)0;
}
