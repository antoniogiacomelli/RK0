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
    (void)incr;
    errno = ENOMEM;
    return (caddr_t)-1;
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
