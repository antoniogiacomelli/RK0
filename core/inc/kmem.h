#ifndef RK_MEM_H
#define RK_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

RK_ERR kMemInit(RK_MEM* const, VOID *, ULONG const, ULONG);
VOID *kMemAlloc(RK_MEM* const);
RK_ERR kMemFree(RK_MEM* const, VOID *);

#ifdef __cplusplus
}
#endif

#endif
