/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RK_CHANNEL_H
#define RK_CHANNEL_H

#include <kenv.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (RK_CONF_SYNCH_MESG == ON)
RK_ERR kChannelInit(RK_CHANNEL *const,
                    RK_TASK_HANDLE const *const, RK_OPTION const);
RK_ERR kChannelDestroy(RK_CHANNEL *const);
RK_ERR kChannelSend(RK_CHANNEL *const, VOID const *const,
                    ULONG const, RK_TICK const);
RK_ERR kChannelRecv(RK_CHANNEL *const, VOID *const, ULONG const,
                    ULONG *const, RK_TICK const);
RK_ERR kChannelCall(RK_CHANNEL *const, RK_CHANNEL_ATTR const *const,
                    RK_TICK const);
RK_ERR kChannelAccept(RK_CHANNEL *const, RK_CHANNEL_CALL_DATA *const,
                      VOID *const, ULONG const, ULONG *const, RK_TICK const);
RK_ERR kChannelReply(RK_CHANNEL_CALL_DATA const *const,
                     VOID const *const, ULONG const);

/* Kernel hooks; caller holds the kernel critical section. */
VOID kChannelTimeout(RK_TCB *const);
RK_PRIO kChannelTaskBasePrio(RK_TCB const *const, RK_PRIO const);
RK_PRIO kChannelTaskWaiterPrio(RK_TCB const *const, RK_PRIO const);
#endif

#ifdef __cplusplus
}
#endif
#endif /* RK_CHANNEL_H */
