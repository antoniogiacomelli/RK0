/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.6
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

#ifndef RK_LIST_H
#define RK_LIST_H

/* Doubly Linked List ADT */


#ifndef K_GET_TCB_ADDR
#define K_GET_TCB_ADDR(nodePtr) \
    K_GET_CONTAINER_ADDR(nodePtr, RK_TCB, tcbNode)
#endif

#ifndef KLISTNODEDEL
#define KLISTNODEDEL(nodePtr)                         \
    do                                                \
                                                      \
    {                                                 \
        nodePtr->nextPtr->prevPtr = nodePtr->prevPtr; \
        nodePtr->prevPtr->nextPtr = nodePtr->nextPtr; \
        nodePtr->prevPtr = NULL;                      \
        nodePtr->nextPtr = NULL;                      \
                                                      \
    } while (0U)
#endif

__RK_INLINE
static inline RK_ERR kListInit(RK_LIST *const kobj)
{
    kassert(kobj != NULL);
    kobj->listDummy.nextPtr = &(kobj->listDummy);
    kobj->listDummy.prevPtr = &(kobj->listDummy);
    kobj->size = 0U;
    return (RK_SUCCESS);
}
__RK_INLINE
static inline RK_ERR kListInsertAfter(RK_LIST *const kobj, RK_NODE *const refNodePtr,
                                      RK_NODE *const newNodePtr)
{
    kassert(kobj != NULL && newNodePtr != NULL && refNodePtr != NULL);
    newNodePtr->nextPtr = refNodePtr->nextPtr;
    refNodePtr->nextPtr->prevPtr = newNodePtr;
    newNodePtr->prevPtr = refNodePtr;
    refNodePtr->nextPtr = newNodePtr;
    kobj->size += 1U;

    return (RK_SUCCESS);
}
__RK_INLINE
static inline RK_ERR kListRemove(RK_LIST *const kobj, RK_NODE *const remNodePtr)
{
    kassert(kobj != NULL && remNodePtr != NULL);
    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }
    KLISTNODEDEL(remNodePtr);
    kobj->size -= 1U;
    return (RK_SUCCESS);
}

__RK_INLINE
static inline RK_ERR kListRemoveHead(RK_LIST *const kobj,
                                     RK_NODE **const remNodePPtr)
{

    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }

    RK_NODE *currHeadPtr = kobj->listDummy.nextPtr;
    *remNodePPtr = currHeadPtr;
    KLISTNODEDEL(currHeadPtr);
    kobj->size -= 1U;
    return (RK_SUCCESS);
}
__RK_INLINE
static inline RK_ERR kListAddTail(RK_LIST *const kobj, RK_NODE *const newNodePtr)
{
    return (kListInsertAfter(kobj, kobj->listDummy.prevPtr, newNodePtr));
}

__RK_INLINE
static inline RK_ERR kListAddHead(RK_LIST *const kobj, RK_NODE *const newNodePtr)
{
    return (kListInsertAfter(kobj, &kobj->listDummy, newNodePtr));
}

__RK_INLINE
static inline RK_ERR kListRemoveTail(RK_LIST *const kobj, RK_NODE **remNodePPtr)
{
    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }

    RK_NODE *currTailPtr = kobj->listDummy.prevPtr;
    kassert(currTailPtr != NULL);
    *remNodePPtr = currTailPtr;
    KLISTNODEDEL(currTailPtr);
    kobj->size -= 1U;
    return (RK_SUCCESS);
}

/*******************************************************************************
 TASK QUEUE MANAGEMENT
 ******************************************************************************/
__RK_INLINE
static inline RK_ERR kTCBQInit(RK_TCBQ *const kobj)
{
    kassert(kobj != NULL);
    return (kListInit(kobj));
}

__RK_INLINE
static inline RK_ERR kTCBQEnq(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{

    kassert(kobj != NULL && tcbPtr != NULL);
    RK_ERR err = kListAddTail(kobj, &(tcbPtr->tcbNode));
    if (kobj == &readyQueue[tcbPtr->priority])
        readyQBitMask |= 1 << tcbPtr->priority;
    return (err);
}
__RK_INLINE
static inline RK_ERR kTCBQJam(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{
    kassert(kobj != NULL && tcbPtr != NULL);
    RK_ERR err = kListAddHead(kobj, &(tcbPtr->tcbNode));
    if (kobj == &readyQueue[tcbPtr->priority])
        readyQBitMask |= 1 << tcbPtr->priority;
    return (err);
}

__RK_INLINE
static inline RK_ERR kTCBQDeq(RK_TCBQ *const kobj, RK_TCB **const tcbPPtr)
{
    RK_NODE *dequeuedNodePtr = NULL;
    RK_ERR err = kListRemoveHead(kobj, &dequeuedNodePtr);
    *tcbPPtr = K_GET_TCB_ADDR(dequeuedNodePtr);
    kassert(*tcbPPtr != NULL);
    RK_TCB const *tcbPtr_ = *tcbPPtr;
    RK_PRIO prio_ = tcbPtr_->priority;
    if ((kobj == &readyQueue[prio_]) && (kobj->size == 0))
        readyQBitMask &= ~(1U << prio_);
    return (err);
}
__RK_INLINE
static inline RK_ERR kTCBQRem(RK_TCBQ *const kobj, RK_TCB **const tcbPPtr)
{
    kassert(kobj != NULL);
    RK_NODE *dequeuedNodePtr = &((*tcbPPtr)->tcbNode);
    kListRemove(kobj, dequeuedNodePtr);
    *tcbPPtr = K_GET_TCB_ADDR(dequeuedNodePtr);
    kassert(*tcbPPtr != NULL);
    RK_TCB const *tcbPtr_ = *tcbPPtr;
    RK_PRIO prio_ = tcbPtr_->priority;
    if ((kobj == &readyQueue[prio_]) && (kobj->size == 0))
        readyQBitMask &= ~(1U << prio_);
    return (RK_SUCCESS);
}

__RK_INLINE
static inline RK_TCB *kTCBQPeek(RK_TCBQ *const kobj)
{
    kassert(kobj != NULL);
    RK_NODE *nodePtr = kobj->listDummy.nextPtr;
    return (K_GET_CONTAINER_ADDR(nodePtr, RK_TCB, tcbNode));
}

__RK_INLINE
static inline RK_ERR kTCBQEnqByPrio(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{
    RK_CR_AREA
    RK_CR_ENTER
    kassert(kobj != NULL && tcbPtr != NULL);
    RK_NODE *currNodePtr = &(kobj->listDummy);

    /*  Traverse the list from head to tail */
    while (currNodePtr->nextPtr != &(kobj->listDummy))
    {
        RK_TCB const *currTcbPtr = K_GET_TCB_ADDR(currNodePtr->nextPtr);
        if (currTcbPtr->priority > tcbPtr->priority)
        {
            break;
        }
        currNodePtr = currNodePtr->nextPtr;
    }

    RK_CR_EXIT

    return kListInsertAfter(kobj, currNodePtr, &(tcbPtr->tcbNode));
}


/* this function enq ready and pend ctxt swtch if ready > running */
__RK_INLINE
static inline RK_ERR kReadyCtxtSwtch(RK_TCB *const tcbPtr)
{

    kassert(tcbPtr != NULL);
    RK_ERR err = -1;
    if (tcbPtr->pid == RK_TIMHANDLER_ID)
    {
        /* timer handle task has 'negative' priority */
        err = kTCBQJam(&readyQueue[tcbPtr->priority], tcbPtr);
    }
    else
    {   
        err = kTCBQEnq(&readyQueue[tcbPtr->priority], tcbPtr);
    }
    kassert(err == RK_SUCCESS);
    tcbPtr->status = RK_READY;
    if (runPtr->priority > tcbPtr->priority && runPtr->preempt == 1UL)
    {
        if (runPtr->schLock == 0UL)
        {
            RK_PEND_CTXTSWTCH
        }
        else
        {
            isPendingCtxtSwtch = 1;
        }
    }
    return (RK_SUCCESS);
}

/******************************************************************************/
/* MUTEX LIST                                                                 */
/******************************************************************************/
#if (RK_CONF_MUTEX==ON)
__RK_INLINE
static inline RK_ERR kMQEnq(struct kList *ownedMutexList,
                            struct kListNode *mutexNode)
{
    return kListAddTail(ownedMutexList, mutexNode);
}

__RK_INLINE
static inline RK_ERR kMQRem(struct kList *ownedMutexList,
                            struct kListNode *mutexNode)
{
    return kListRemove(ownedMutexList, mutexNode);
}
#endif


/******************************************************************************/
/* EVENT GROUP POST-PROCESSING LIST                                           */
/******************************************************************************/
#if (RK_CONF_SLEEPQ_GROUP==ON)
__RK_INLINE
static inline RK_ERR kEvGroupQEnq(struct kList *evGroupList,
                            struct kListNode *evGroupNode)
{
    return kListAddTail(evGroupList, evGroupNode);
}

__RK_INLINE
static inline RK_EVENT_GROUP* kEvGroupQRem(struct kList *evGroupList)
{
    RK_NODE* nodePtr = NULL;
    RK_ERR err = kListRemoveHead(evGroupList, &nodePtr);
    kassert(err==0);
    RK_EVENT_GROUP* evgPtr = K_GET_CONTAINER_ADDR(nodePtr, RK_EVENT_GROUP, evGroupNode);
    return (evgPtr);
}
#endif


#endif