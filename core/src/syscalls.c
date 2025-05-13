/* Minimal back-end syscall stubs */
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <kcommondefs.h>
extern int errno;

__RK_WEAK
caddr_t _sbrk(int incr)
{
  extern char _heap_start;  
  extern char _heap_end;      

  static char *heap_curr;    
    if (heap_curr == 0)
        heap_curr = &_heap_start;      

    char *prev = heap_curr;
    char *next = heap_curr + incr;

    if (next < &_heap_start || next > &_heap_end) {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    heap_curr = next;
    return (caddr_t)prev;
}
    
__RK_WEAK
int _close(int file) 
{
    (void)file;
    return -1; 
}
__RK_WEAK
int _fstat(int file, struct stat *st)   
{
  (void)file;
  st->st_mode = S_IFCHR;  
  return 0;
}
__RK_WEAK
int _isatty(int file) 
 { 
    (void)file;
     return 1; 
}
__RK_WEAK
int _lseek(int file, int ptr, int dir)
{ 
    (void)file;
    (void)ptr;
    (void)dir;
    return 0; 
}
__RK_WEAK
int _read(int file, char *ptr, int len)  
{ 
    (void)file;
    (void)ptr;
    (void)len;
    return 0; 
}
__RK_WEAK
int _write(int file, char *ptr, int len)
{
  (void)file;
  (void)ptr;
  return len;
}
