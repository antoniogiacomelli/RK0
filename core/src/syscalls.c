/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.4.1
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <kcommondefs.h>
extern int errno;

__RK_WEAK
caddr_t _sbrk(int incr)
{ 
  /* malloc is not a thing */
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
