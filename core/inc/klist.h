/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.2                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_LIST_H
#define RK_LIST_H

/* Inlined functions for the Doubly Linked List ADT */

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

RK_FORCE_INLINE
static inline RK_ERR kListInit(RK_LIST *const kobj)
{
    K_ASSERT(kobj != NULL);
    kobj->listDummy.nextPtr = &(kobj->listDummy);
    kobj->listDummy.prevPtr = &(kobj->listDummy);
    kobj->size = 0U;
    return (RK_ERR_SUCCESS);
}
RK_FORCE_INLINE
static inline RK_ERR kListInsertAfter(RK_LIST *const kobj, RK_NODE *const refNodePtr,
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
static inline RK_ERR kListRemove(RK_LIST *const kobj, RK_NODE *const remNodePtr)
{
    K_ASSERT(kobj != NULL && remNodePtr != NULL);
    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }
    KLISTNODEDEL(remNodePtr);
    kobj->size -= 1U;
    return (RK_ERR_SUCCESS);
}

RK_FORCE_INLINE
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
    return (RK_ERR_SUCCESS);
}
RK_FORCE_INLINE
static inline RK_ERR kListAddTail(RK_LIST *const kobj, RK_NODE *const newNodePtr)
{
    return (kListInsertAfter(kobj, kobj->listDummy.prevPtr, newNodePtr));
}

RK_FORCE_INLINE
static inline RK_ERR kListAddHead(RK_LIST *const kobj, RK_NODE *const newNodePtr)
{
    return (kListInsertAfter(kobj, &kobj->listDummy, newNodePtr));
}

RK_FORCE_INLINE
static inline RK_ERR kListRemoveTail(RK_LIST *const kobj, RK_NODE **remNodePPtr)
{
    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }

    RK_NODE *currTailPtr = kobj->listDummy.prevPtr;
    K_ASSERT(currTailPtr != NULL);
    *remNodePPtr = currTailPtr;
    KLISTNODEDEL(currTailPtr);
    kobj->size -= 1U;
    return (RK_ERR_SUCCESS);
}

/******************************************************************************/
/* MUTEX LIST                                                                 */
/******************************************************************************/
#if (RK_CONF_MUTEX==ON)
RK_FORCE_INLINE
static inline RK_ERR kMutexListAdd(struct RK_OBJ_LIST *ownedMutexList,
                            struct RK_OBJ_LIST_NODE *mutexNode)
{
    return kListAddTail(ownedMutexList, mutexNode);
}

RK_FORCE_INLINE
static inline RK_ERR kMutexListRem(struct RK_OBJ_LIST *ownedMutexList,
                            struct RK_OBJ_LIST_NODE *mutexNode)
{
    return kListRemove(ownedMutexList, mutexNode);
}
#endif
#endif
