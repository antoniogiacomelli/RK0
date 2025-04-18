/******************************************************************************
 *
 *     [[RK0] | [VERSION: 0.4.0]]
 *
 ******************************************************************************
 ******************************************************************************
 * 	In this header:
 * 					o System Tasks and Deferred Handlers function
 * 					  prototypes.
 *
 *****************************************************************************/
#ifndef RK_SYSTASKS_H
#define RK_SYSTASKS_H
#ifdef __cplusplus
extern "C" {
#endif
#include <kmodules.h>
extern RK_TASK_HANDLE timTaskHandle;
extern RK_TASK_HANDLE idleTaskHandle;

void IdleTask(void*);
void TimerHandlerTask(void*);
BOOL kTimerHandler(void*);
#ifdef __cplusplus
 }
#endif
#endif /* KSYSTASKS_H */
