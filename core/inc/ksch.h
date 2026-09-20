/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.82.0                                                         */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
#ifndef RK_SCH_H
#define RK_SCH_H
#ifdef __cplusplus
{
#endif
#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>
#include <klist.h>
#include <kstring.h>
/* Globals */

extern RK_TCB* RK_gRunPtr; /* Pointer to the running TCB */
extern RK_TCB RK_gTcbs[RK_NTHREADS]; /* Pool of RK_gTcbs */
extern volatile RK_FAULT RK_gFaultID; /* Fault ID */
extern UINT RK_gIdleStack[RK_CONF_IDLE_STACKSIZE]; /* Stack for idle task */
extern UINT RK_gPostProcStack[RK_CONF_POSTPROC_STACKSIZE];
extern RK_TCBQ RK_gReadyQueue[RK_RDYQSIZ]; /* Table of ready queues */
extern volatile ULONG RK_gReadyBitmask;
extern volatile ULONG RK_gReadyPos;
extern volatile UINT RK_gPendingCtxtSwtch;
extern volatile UINT RK_gSchLock;

/* internal return values */
#ifndef RK_ERR_RESCHED_PENDING
#define RK_ERR_RESCHED_PENDING                ((RK_ERR)900)
#endif
#ifndef RK_ERR_RESCHED_NOT_NEEDED
#define RK_ERR_RESCHED_NOT_NEEDED            ((RK_ERR)901)
#endif
#ifndef RK_CONF_MIN_PRIO
#define RK_CONF_MIN_PRIO 31
#endif

VOID kSchLock(VOID);
VOID kSchUnlock(VOID);
VOID kPendCtxSwtch(VOID);
VOID kSwtch(VOID);
VOID kInit(VOID);
VOID kYield(VOID);
#ifndef kPreemptEnable
#define kPreemptEnable kSchUnlock
#endif
#ifndef kPreemptDisable
#define kPreemptDisable kSchLock
#endif

/* Task queue management */
RK_ERR kTCBQInit(RK_TCBQ *const);
RK_ERR kTCBQEnq(RK_TCBQ *const, RK_TCB *const);
RK_ERR kTCBQJam(RK_TCBQ *const, RK_TCB *const);
RK_ERR kTCBQDeq(RK_TCBQ *const, RK_TCB **const);
RK_ERR kTCBQRem(RK_TCBQ *const, RK_TCB **const);
RK_TCB *kTCBQPeek(RK_TCBQ *const);
RK_ERR kTCBQEnqByPrio(RK_TCBQ *const, RK_TCB *const);
RK_ERR kWaitQEnqByPrio(RK_TCBQ *const, RK_TCB *const);
RK_ERR kWaitQEnqTail(RK_TCBQ *const, RK_TCB *const);
RK_ERR kWaitQRemove(RK_TCBQ *const, RK_TCB *const);
RK_ERR kWaitQDeq(RK_TCBQ *const, RK_TCB **const);
RK_ERR kReschedTask(RK_TCB *);
RK_ERR kReschedRunning(VOID);
RK_BOOL kTaskUpdateEffectivePrio(RK_TCB *const);
VOID kTaskUpdateEffectivePrioChain(RK_TCB *const);
RK_ERR kReadySwtch(RK_TCB *const);
RK_ERR kReadyNoSwtch(RK_TCB *const);

#if (RK_CONF_INLINE_SCHEDULER == ON) && !defined(RK_SCH_SOURCE_CODE)

#if defined(RK_DMB)
#define RK_TASK_QUEUE_INLINE_DMB_ RK_DMB
#else
#define RK_TASK_QUEUE_INLINE_DMB_ RK_ASM volatile("DMB" :: : "memory");
#endif

RK_FORCE_INLINE
static inline RK_ERR kTCBQInsertAfterInline_(RK_TCBQ *const kobj,
                                             RK_NODE *const refNodePtr,
                                             RK_NODE *const newNodePtr)
{
    K_ASSERT(kobj != NULL && newNodePtr != NULL && refNodePtr != NULL);
    newNodePtr->nextPtr = refNodePtr->nextPtr;
    refNodePtr->nextPtr->prevPtr = newNodePtr;
    newNodePtr->prevPtr = refNodePtr;
    refNodePtr->nextPtr = newNodePtr;
    kobj->size += 1U;
    return (RK_ERR_SUCCESS);
}

RK_FORCE_INLINE
static inline RK_ERR kTCBQInitInline_(RK_TCBQ *const kobj)
{
    K_ASSERT(kobj != NULL);
    kobj->listDummy.nextPtr = &kobj->listDummy;
    kobj->listDummy.prevPtr = &kobj->listDummy;
    kobj->size = 0U;
    return (RK_ERR_SUCCESS);
}

RK_FORCE_INLINE
static inline RK_ERR kTCBQEnqInline_(RK_TCBQ *const kobj,
                                     RK_TCB *const tcbPtr)
{
    RK_ERR const err = kTCBQInsertAfterInline_(
        kobj, kobj->listDummy.prevPtr, &tcbPtr->tcbNode);
    if ((kobj == &RK_gReadyQueue[tcbPtr->priority]) &&
        (tcbPtr->priority < (sizeof(ULONG) * 8U)))
    {
        RK_gReadyBitmask |= 1UL << tcbPtr->priority;
    }
    return (err);
}

RK_FORCE_INLINE
static inline RK_ERR kTCBQJamInline_(RK_TCBQ *const kobj,
                                     RK_TCB *const tcbPtr)
{
    RK_ERR const err =
        kTCBQInsertAfterInline_(kobj, &kobj->listDummy, &tcbPtr->tcbNode);
    if ((kobj == &RK_gReadyQueue[tcbPtr->priority]) &&
        (tcbPtr->priority < (sizeof(ULONG) * 8U)))
    {
        RK_gReadyBitmask |= 1UL << tcbPtr->priority;
        RK_TASK_QUEUE_INLINE_DMB_
    }
    return (err);
}

RK_FORCE_INLINE
static inline RK_ERR kTCBQRemoveNodeInline_(RK_TCBQ *const kobj,
                                            RK_NODE *const nodePtr)
{
    K_ASSERT(kobj != NULL && nodePtr != NULL);
    K_ASSERT(nodePtr != &kobj->listDummy);
    if (kobj->size == 0U)
    {
        return (RK_ERR_LIST_EMPTY);
    }
    KLISTNODEDEL(nodePtr);
    kobj->size -= 1U;
    return (RK_ERR_SUCCESS);
}

RK_FORCE_INLINE
static inline RK_ERR kTCBQDeqInline_(RK_TCBQ *const kobj,
                                     RK_TCB **const tcbPPtr)
{
    RK_ERR err;
    RK_NODE *nodePtr;

    K_ASSERT(kobj != NULL && tcbPPtr != NULL);
    if (kobj->size == 0U)
    {
        return (RK_ERR_LIST_EMPTY);
    }
    nodePtr = kobj->listDummy.nextPtr;
    err = kTCBQRemoveNodeInline_(kobj, nodePtr);
    *tcbPPtr = K_GET_TCB_ADDR(nodePtr);
    K_ASSERT(*tcbPPtr != NULL);
    if ((kobj == &RK_gReadyQueue[(*tcbPPtr)->priority]) &&
        ((*tcbPPtr)->priority < (sizeof(ULONG) * 8U)) &&
        (kobj->size == 0U))
    {
        RK_gReadyBitmask &= ~(1UL << (*tcbPPtr)->priority);
        RK_TASK_QUEUE_INLINE_DMB_
    }
    return (err);
}

RK_FORCE_INLINE
static inline RK_ERR kTCBQRemInline_(RK_TCBQ *const kobj,
                                     RK_TCB **const tcbPPtr)
{
    RK_NODE *const nodePtr = &(*tcbPPtr)->tcbNode;

    (VOID)kTCBQRemoveNodeInline_(kobj, nodePtr);
    *tcbPPtr = K_GET_TCB_ADDR(nodePtr);
    if ((kobj == &RK_gReadyQueue[(*tcbPPtr)->priority]) &&
        ((*tcbPPtr)->priority < (sizeof(ULONG) * 8U)) &&
        (kobj->size == 0U))
    {
        RK_gReadyBitmask &= ~(1UL << (*tcbPPtr)->priority);
        RK_TASK_QUEUE_INLINE_DMB_
    }
    return (RK_ERR_SUCCESS);
}

RK_FORCE_INLINE
static inline RK_TCB *kTCBQPeekInline_(RK_TCBQ *const kobj)
{
    return (K_GET_TCB_ADDR(kobj->listDummy.nextPtr));
}

RK_FORCE_INLINE
static inline RK_ERR kTCBQEnqByPrioInline_(RK_TCBQ *const kobj,
                                           RK_TCB *const tcbPtr)
{
    RK_NODE *nodePtr = &kobj->listDummy;

    while (nodePtr->nextPtr != &kobj->listDummy)
    {
        RK_TCB const *const queuedPtr = K_GET_TCB_ADDR(nodePtr->nextPtr);
        if (queuedPtr->priority > tcbPtr->priority)
        {
            break;
        }
        nodePtr = nodePtr->nextPtr;
    }
    return (kTCBQInsertAfterInline_(kobj, nodePtr, &tcbPtr->tcbNode));
}

RK_FORCE_INLINE
static inline RK_ERR kWaitQEnqByPrioInline_(RK_TCBQ *const kobj,
                                            RK_TCB *const tcbPtr)
{
    K_ASSERT(kobj != NULL && tcbPtr != NULL);
    K_ASSERT(tcbPtr->waitingQueuePtr == NULL);
    RK_ERR const err = kTCBQEnqByPrioInline_(kobj, tcbPtr);
    if (err == RK_ERR_SUCCESS)
    {
        tcbPtr->waitingQueuePtr = kobj;
    }
    return (err);
}

RK_FORCE_INLINE
static inline RK_ERR kWaitQEnqTailInline_(RK_TCBQ *const kobj,
                                          RK_TCB *const tcbPtr)
{
    K_ASSERT(kobj != NULL && tcbPtr != NULL);
    K_ASSERT(tcbPtr->waitingQueuePtr == NULL);
    RK_ERR const err = kTCBQEnqInline_(kobj, tcbPtr);
    if (err == RK_ERR_SUCCESS)
    {
        tcbPtr->waitingQueuePtr = kobj;
    }
    return (err);
}

RK_FORCE_INLINE
static inline RK_ERR kWaitQRemoveInline_(RK_TCBQ *const kobj,
                                         RK_TCB *const tcbPtr)
{
    RK_TCB *removedPtr = tcbPtr;

    K_ASSERT(kobj != NULL && tcbPtr != NULL);
    K_ASSERT(tcbPtr->waitingQueuePtr == kobj);
    RK_ERR const err = kTCBQRemInline_(kobj, &removedPtr);
    if (err == RK_ERR_SUCCESS)
    {
        removedPtr->waitingQueuePtr = NULL;
    }
    return (err);
}

RK_FORCE_INLINE
static inline RK_ERR kWaitQDeqInline_(RK_TCBQ *const kobj,
                                      RK_TCB **const tcbPPtr)
{
    K_ASSERT(kobj != NULL && tcbPPtr != NULL);
    RK_ERR const err = kTCBQDeqInline_(kobj, tcbPPtr);
    if ((err == RK_ERR_SUCCESS) && (*tcbPPtr != NULL))
    {
        K_ASSERT((*tcbPPtr)->waitingQueuePtr == kobj);
        (*tcbPPtr)->waitingQueuePtr = NULL;
    }
    return (err);
}

#define kTCBQInit(kobj) kTCBQInitInline_((kobj))
#define kTCBQEnq(kobj, tcbPtr) kTCBQEnqInline_((kobj), (tcbPtr))
#define kTCBQJam(kobj, tcbPtr) kTCBQJamInline_((kobj), (tcbPtr))
#define kTCBQDeq(kobj, tcbPPtr) kTCBQDeqInline_((kobj), (tcbPPtr))
#define kTCBQRem(kobj, tcbPPtr) kTCBQRemInline_((kobj), (tcbPPtr))
#define kTCBQPeek(kobj) kTCBQPeekInline_((kobj))
#define kTCBQEnqByPrio(kobj, tcbPtr) \
    kTCBQEnqByPrioInline_((kobj), (tcbPtr))
#define kWaitQEnqByPrio(kobj, tcbPtr) \
    kWaitQEnqByPrioInline_((kobj), (tcbPtr))
#define kWaitQEnqTail(kobj, tcbPtr) \
    kWaitQEnqTailInline_((kobj), (tcbPtr))
#define kWaitQRemove(kobj, tcbPtr) kWaitQRemoveInline_((kobj), (tcbPtr))
#define kWaitQDeq(kobj, tcbPPtr) kWaitQDeqInline_((kobj), (tcbPPtr))

#undef RK_TASK_QUEUE_INLINE_DMB_

#if defined(RK_DMB)
#define RK_CTX_SWITCH_INLINE_DMB_ RK_DMB
#else
#define RK_CTX_SWITCH_INLINE_DMB_ RK_ASM volatile("DMB" :: : "memory");
#endif

#if defined(RK_DSB)
#define RK_CTX_SWITCH_INLINE_DSB_ RK_DSB
#else
#define RK_CTX_SWITCH_INLINE_DSB_ RK_ASM volatile("DSB" :: : "memory");
#endif

#if defined(RK_ISB)
#define RK_CTX_SWITCH_INLINE_ISB_ RK_ISB
#else
#define RK_CTX_SWITCH_INLINE_ISB_ RK_ASM volatile("ISB" :: : "memory");
#endif

#if defined(RK_PEND_CTXTSWTCH)
#define RK_CTX_SWITCH_INLINE_PEND_ RK_PEND_CTXTSWTCH
#else
#define RK_CTX_SWITCH_INLINE_PEND_                                      \
    do                                                                  \
    {                                                                   \
        *((volatile unsigned long *)(0xE000ED04UL)) |= (1UL << 28);     \
    } while (0);
#endif

RK_FORCE_INLINE
static inline VOID kPendCtxSwtchNowInline_(VOID)
{
    RK_gPendingCtxtSwtch = 0U;
    RK_CTX_SWITCH_INLINE_DSB_
    RK_CTX_SWITCH_INLINE_PEND_
    RK_CTX_SWITCH_INLINE_ISB_
}

RK_FORCE_INLINE
static inline VOID kDeferCtxSwtchInline_(VOID)
{
    RK_gPendingCtxtSwtch = 1U;
    RK_BARRIER
}

RK_FORCE_INLINE
static inline VOID kPendCtxSwtchInline_(VOID)
{
    if ((RK_gRunPtr != NULL) && (RK_gRunPtr->status == RK_READY) &&
        (RK_gSchLock > 0U))
    {
        kDeferCtxSwtchInline_();
    }
    else if ((RK_gRunPtr != NULL) && (RK_gRunPtr->status != RK_RUNNING))
    {
        kPendCtxSwtchNowInline_();
    }
    else if (RK_gSchLock == 0U)
    {
        kPendCtxSwtchNowInline_();
    }
    else
    {
        kDeferCtxSwtchInline_();
    }
}

RK_FORCE_INLINE
static inline RK_ERR kReadySwtchInline_(RK_TCB *const tcbPtr)
{
    RK_ERR err = -1;

    if (tcbPtr->tid == RK_POSTPROC_TASK_ID)
    {
        err = kTCBQJam(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    else
    {
        err = kTCBQEnq(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }

    if (err == RK_ERR_SUCCESS)
    {
        tcbPtr->status = RK_READY;
        return (kReschedTask(tcbPtr));
    }

    return (err);
}

RK_FORCE_INLINE
static inline RK_ERR kReadyNoSwtchInline_(RK_TCB *const tcbPtr)
{
    RK_ERR err = -1;

    K_ASSERT(tcbPtr != NULL);

    if (tcbPtr->tid == RK_POSTPROC_TASK_ID)
    {
        err = kTCBQJam(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    else
    {
        err = kTCBQEnq(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }

    K_ASSERT(err == RK_ERR_SUCCESS);

    tcbPtr->status = RK_READY;

    RK_CTX_SWITCH_INLINE_DMB_

    return (RK_ERR_SUCCESS);
}

#ifndef kPendCtxSwtch
#define kPendCtxSwtch() kPendCtxSwtchInline_()
#endif

#ifndef kReadySwtch
#define kReadySwtch(tcbPtr) kReadySwtchInline_((tcbPtr))
#endif

#ifndef kReadyNoSwtch
#define kReadyNoSwtch(tcbPtr) kReadyNoSwtchInline_((tcbPtr))
#endif

#undef RK_CTX_SWITCH_INLINE_DMB_
#undef RK_CTX_SWITCH_INLINE_DSB_
#undef RK_CTX_SWITCH_INLINE_ISB_
#undef RK_CTX_SWITCH_INLINE_PEND_

#endif /* RK_CONF_INLINE_SCHEDULER */

RK_ERR kTaskInit(RK_TASK_HANDLE *,
                   const RK_TASKENTRY, VOID *,
                   CHAR *const, RK_STACK *const,
                   const ULONG, const RK_PRIO,
                   const RK_OPTION);
#if (RK_CONF_DYNAMIC_TASK == ON)
RK_ERR kTaskSpawn(RK_DYNAMIC_TASK_ATTR const * ,
                  RK_TASK_HANDLE * );

RK_ERR kTaskTerminate(RK_TASK_HANDLE *);
RK_ERR kTaskTerminateSelf(VOID);
#endif

RK_TASK_HANDLE kTaskGetRunningHandle(VOID);
RK_TID kTaskGetID(RK_TASK_HANDLE taskHandle);
RK_ERR kTaskGetName(RK_TASK_HANDLE taskHandle, CHAR *buf);
RK_PRIO kTaskGetPrio(RK_TASK_HANDLE taskHandle);



#ifdef __cplusplus
}
#endif

#endif /* KSCH_H */
