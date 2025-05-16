
#include <kservices.h>
unsigned  __getReadyPrio(unsigned x)
{
    /* count trailing zeroes */
    #define u (0xFF) /*unused position*/
	static char table[64] =
     {
        32,  0, 1, 12, 2,  6, u, 13,  3, u,  7,  u,  u,  u,   u, 14,
        10,  4, u,  u, 8,  u, u, 25,  u, u,  u,  u,  u, 21,  27, 15,
        31, 11, 5,  u, u,  u, u,  u,  9, u,  u, 24,  u,  u,  20, 26,
        30,  u, u,  u, u, 23, u, 19, 29, u, 22, 18, 28,  17, 16, 
        u
    };
      x = (x & -x)*0x0450FBAF;
      return ((unsigned)(table[x >> 26]));
/*Bruijn's algorithm from  Warren, Henry S.. Hacker's Delight (p. 183). Pearson Education. Kindle Edition.  */
}