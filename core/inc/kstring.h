
#ifndef RK_STRING_H
#define RK_STRING_H

#include <stddef.h>

void *kmemset(void *dest, int val, size_t len);
void *kmemcpy(void *dest, const void *src, size_t len);

#define RK_MEMSET kmemset
#define RK_MEMCPY kmemcpy

#endif