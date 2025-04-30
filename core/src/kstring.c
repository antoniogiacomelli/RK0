#include <kstring.h>
#include <stddef.h>

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

static void *kmemclr_wrapper(void *dest, size_t len)
{
    return (kmemset(dest, 0, len));
}


void *memset       (void *, int, size_t)  __attribute__((alias("kmemset")));
void *memcpy       (void *, const void *, size_t)  __attribute__((alias("kmemcpy")));
void *__aeabi_memset(void *, int, size_t) __attribute__((alias("kmemset")));
void *__aeabi_memclr(void *, size_t)      __attribute__((alias("kmemclr_wrapper")));
void *__aeabi_memcpy(void *, const void *, size_t)
                                          __attribute__((alias("kmemcpy")));
