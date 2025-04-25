
#include <kservices.h>
unsigned  __getReadyPrio(unsigned mask)
{
    return (__builtin_ctz(mask));
}