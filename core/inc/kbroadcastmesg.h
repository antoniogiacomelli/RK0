/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RK_BROADCAST_MESG_H
#define RK_BROADCAST_MESG_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (RK_CONF_BROADCAST_MESG == ON)
/** BroadcastMesg owns one copied frame, not a queue of messages.
 * bufferPtr supplies 1, 2, 4 or 8 ULONG words and must outlive the object.
 * Send targets receivers already blocked in Recv. A later receiver waits for
 * the next send. The frame stays occupied until all selected tasks consume it.
 */
RK_ERR kBroadcastMesgInit(RK_BROADCAST_MESG * , VOID * ,
                          ULONG  );
RK_ERR kBroadcastMesgInitCeiling(RK_BROADCAST_MESG * , VOID * ,
                                 ULONG  , RK_PRIO  );
/** Explicit members retain the ceiling across receives until Leave. Recv
 * automatically joins non-members for the duration of a blocking receive.
 * A task can join at most one ceiling-enabled BroadcastMesg. Task-only.
 */
RK_ERR kBroadcastMesgJoin(RK_BROADCAST_MESG * );
RK_ERR kBroadcastMesgLeave(RK_BROADCAST_MESG * );
/** Send is nonblocking. No waiters: BUFFER_EMPTY; occupied frame: BUFFER_FULL.
 * Task fanout runs inline under scheduler protection, with interrupts enabled
 * between atomic receiver transitions. Only multi-receiver ISR fanout uses
 * PostProc. Failure to reserve an ISR job returns NOWAIT without committing.
 * nRecvPtr is optional; it is cleared on failure and counts committed receivers.
 */
RK_ERR kBroadcastMesgSend(RK_BROADCAST_MESG * , VOID const * ,
                          UINT *   );
/** Wait for a receiver threshold and an available frame. Finite retries retain
 * one deadline. ISR callers must use RK_NO_WAIT. Sender priority is unchanged.
 */
RK_ERR kBroadcastMesgSendWaitN(RK_BROADCAST_MESG * , VOID const * ,
                               UINT  , RK_TICK  ,
                               UINT * );
/** Receive a copy into mesgWords of destination storage. NO_WAIT reports empty:
 * a BroadcastMesg has no buffered message available to a late receiver.
 * Explicit membership survives success/timeout; lazy membership is released.
 */
RK_ERR kBroadcastMesgRecv(RK_BROADCAST_MESG * , VOID * ,
                          RK_TICK  );
/** Snapshot of uncommitted parked receivers, threshold senders, and outstanding
 * recipients of the current frame (including deferred/ready recipients).
 * At least one output is required.
 */
RK_ERR kBroadcastMesgQuery(RK_BROADCAST_MESG const * , UINT * ,
                           UINT * , UINT * );
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
/** Fixed-pool allocation; caller owns the frame storage. Requires
 * kObjPartitionsInit(). Destroy rejects waiters, retained calls, in-flight
 * frames and explicit/lazy ceiling memberships, and clears the handle on success.
 */
RK_ERR kBroadcastMesgCreate(RK_BROADCAST_MESG_HANDLE * , VOID * ,
                            ULONG  , RK_PRIO  );
RK_ERR kBroadcastMesgDestroy(RK_BROADCAST_MESG_HANDLE * );
#endif

/* Kernel PostProc entry point, not a second send operation. */
RK_ERR kBroadcastMesgWake(RK_BROADCAST_MESG *, UINT  );

#ifndef RK_BROADCAST_MESG_SIZE
#define RK_BROADCAST_MESG_SIZE(TYPE) RK_TYPE_SIZE_POW2_WORDS(TYPE)
#endif
#ifndef RK_DECLARE_BROADCAST_MESG
#define RK_DECLARE_BROADCAST_MESG(NAME, BUFFER, TYPE) \
    ULONG BUFFER[RK_BROADCAST_MESG_SIZE(TYPE)] K_ALIGN(4); \
    RK_BROADCAST_MESG NAME;
#endif
#endif /* RK_CONF_BROADCAST_MESG */

#ifdef __cplusplus
}
#endif
#endif
