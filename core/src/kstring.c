/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
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

#include <kenv.h>
#include <kstring.h>
#include <stddef.h>

#ifndef _STRING_H_

void *kmemset(void *dest, int val, size_t len)
{
    unsigned char *d = dest;
    while (len--) *d++ = (unsigned char)val;
    return (dest);
}

void *kmemcpy(void *dest, const void *src, size_t len)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (len--) *d++ = *s++;
    return (dest);
}
void *memset(       void *dest, int val, size_t len )       __attribute__((weak, alias("kmemset")));
void *memcpy(       void *dest, const void *src, size_t len ) __attribute__((weak, alias("kmemcpy")));
void *__aeabi_memset( void *dest, int val, size_t len )     __attribute__((weak, alias("kmemset")));
void *__aeabi_memclr( void *dest, size_t len )              __attribute__((weak, alias("kmemclr_wrapper")));
void *__aeabi_memcpy( void *dest, const void *src, size_t len ) __attribute__((weak, alias("kmemcpy")));


#endif
 