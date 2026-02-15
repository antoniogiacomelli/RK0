/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.17                                              */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_KSTRING_H
#define RK_KSTRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

 

#if defined(__GNUC__) || defined(__clang__)
  #define RK_MEMSET  __builtin_memset
  #define RK_MEMCPY  __builtin_memcpy
  #define RK_MEMMOVE __builtin_memmove
  #define RK_STRCPY  __builtin_strcpy
  #if defined(__has_builtin) && __has_builtin(__builtin_memcpy_inline)
    #define RK_MEMCPY_INLINE(d,s,n) __builtin_memcpy_inline((d),(s),(size_t)(n))
  #else
    #define RK_MEMCPY_INLINE(d,s,n) __builtin_memcpy((d),(s),(size_t)(n))
  #endif
#else
  #if defined(RK_USE_LIBC) || (__STDC_HOSTED__+0 == 1)
    #include <string.h>
    #define RK_MEMSET  memset
    #define RK_MEMCPY  memcpy
    #define RK_MEMMOVE memmove
    #define RK_STRCPY  strcpy
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif
