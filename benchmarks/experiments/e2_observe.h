/* SPDX-License-Identifier: Apache-2.0 */
#ifndef E2_OBSERVE_H
#define E2_OBSERVE_H
#include <kapi.h>
VOID E2ObserveGrant(VOID *block, RK_TASK_HANDLE task);
VOID E2ObserveRelease(VOID *block);
VOID E2ObserveAvailable(VOID *block);
VOID E2ObserveSend(VOID *block, RK_TASK_HANDLE receiver);
VOID E2ObserveDirect(VOID *block, RK_TASK_HANDLE receiver);
VOID E2ObserveQueueSend(RK_MESG_QUEUE *queue, VOID const *pointer,
                        RK_TASK_HANDLE receiver);
VOID E2ObserveQueueRecv(RK_MESG_QUEUE *queue, VOID const *pointer);
VOID E2ObservePriority(RK_TASK_HANDLE task, RK_PRIO oldPriority, RK_BOOL queuedBefore);
#endif
