/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RK_MUTEX_PRIVATE_H
#define RK_MUTEX_PRIVATE_H

#include <ktimer.h>
#include <ksch.h>
#include <ktrace.h>
#include <kmutex.h>

#if (RK_CONF_MUTEX == ON)

/******************************************************************************/
/* MUTEX LIST                                                                 */
/******************************************************************************/
static inline RK_ERR kMutexListAdd(struct RK_STRUCT_LIST *ownedMutexList,
                                   struct RK_STRUCT_LIST_NODE *mutexNode)
{
    RK_DSB
    return kListAddTail(ownedMutexList, mutexNode);
}

static inline RK_ERR kMutexListRem(struct RK_STRUCT_LIST *ownedMutexList,
                                   struct RK_STRUCT_LIST_NODE *mutexNode)
{
    RK_DSB
    return kListRemove(ownedMutexList, mutexNode);
}

/******************************************************************************/
/* PRIORITY INHERITANCE                                                       */
/******************************************************************************/
static RK_PRIO kMutexOwnedPipPrio_(RK_TCB const *const ownerTcb,
                                   RK_PRIO const currentPrio)
{
    RK_PRIO newPrio = currentPrio;
    RK_NODE *node = ownerTcb->ownedMutexList.listDummy.nextPtr;

    while (node != &ownerTcb->ownedMutexList.listDummy)
    {
        RK_MUTEX *mtxPtr = K_GET_CONTAINER_ADDR(node, RK_MUTEX, mutexNode);

        if ((mtxPtr->protocol == RK_PRIO_INHERITANCE) &&
            (mtxPtr->waitingQueue.size > 0UL))
        {
            RK_TCB const *waiterPtr = kTCBQPeek(&mtxPtr->waitingQueue);
            if ((waiterPtr != NULL) && (waiterPtr->priority < newPrio))
            {
                newPrio = waiterPtr->priority;
            }
        }

        node = node->nextPtr;
        RK_BARRIER
    }

    return (newPrio);
}

static VOID kMutexSetTaskPrio_(RK_TCB *const tcbPtr,
                               RK_PRIO const newPrio)
{
    if (tcbPtr->priority == newPrio)
    {
        return;
    }

    RK_PRIO const oldPrio = tcbPtr->priority;

    if (tcbPtr->status == RK_READY)
    {
        RK_TCB *remPtr = tcbPtr;
        RK_ERR err = kTCBQRem(&RK_gReadyQueue[oldPrio], &remPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);

        tcbPtr->priority = newPrio;
        kTraceRecordTaskPrio(tcbPtr, oldPrio, newPrio);

        err = kTCBQEnq(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);

        kReschedTask(tcbPtr);
    }
    else
    {
        tcbPtr->priority = newPrio;
        kTraceRecordTaskPrio(tcbPtr, oldPrio, newPrio);
        if (tcbPtr->status == RK_RUNNING)
        {
            kReschedRunning();
        }
    }
}

static VOID kMutexUpdateOwnerPrio_(RK_TCB *ownerTcb)
{
    RK_DSB

    RK_TCB *currTcbPtr = ownerTcb;

    while (currTcbPtr != NULL)
    {
        RK_PRIO const newPrio =
            kMutexOwnedPipPrio_(currTcbPtr, currTcbPtr->prioNominal);

        if (currTcbPtr->priority == newPrio)
        {
            break;
        }

        kMutexSetTaskPrio_(currTcbPtr, newPrio);
        RK_BARRIER

        if ((currTcbPtr->status != RK_BLOCKED) ||
            (currTcbPtr->waitingForMutexPtr == NULL))
        {
            break;
        }

        RK_LIST *waitQueuePtr = currTcbPtr->timeoutNode.waitingQueuePtr;
        if ((waitQueuePtr != NULL) && (waitQueuePtr->size > 1UL))
        {
            RK_TCB *requeuePtr = currTcbPtr;
            RK_ERR err = kTCBQRem(waitQueuePtr, &requeuePtr);
            K_ASSERT(err == RK_ERR_SUCCESS);

            err = kTCBQEnqByPrio(waitQueuePtr, requeuePtr);
            K_ASSERT(err == RK_ERR_SUCCESS);
        }

        RK_MUTEX *const waitMtxPtr = currTcbPtr->waitingForMutexPtr;
        if ((waitMtxPtr->protocol != RK_PRIO_INHERITANCE) ||
            (waitMtxPtr->ownerPtr == NULL))
        {
            break;
        }

        currTcbPtr = waitMtxPtr->ownerPtr;
    }

    RK_ISB
}

#endif /* RK_CONF_MUTEX */

#endif /* RK_MUTEX_PRIVATE_H */
