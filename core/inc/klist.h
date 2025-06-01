/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
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
#include <kdefs.h>
#include <kstring.h>
/* Doubly Linked List ADT */

#define KLISTNODEDEL(nodePtr) \
    do \
    \
    { \
    nodePtr->nextPtr->prevPtr = nodePtr->prevPtr; \
    nodePtr->prevPtr->nextPtr = nodePtr->nextPtr; \
    nodePtr->prevPtr=NULL; \
    nodePtr->nextPtr=NULL; \
    \
    }while(0U)

__RK_INLINE static inline RK_ERR kListInit(RK_LIST* const kobj)
{
    if (kobj == NULL)
    {
        return (RK_ERR_OBJ_NULL);
    }
    kobj->listDummy.nextPtr = & (kobj->listDummy);
    kobj->listDummy.prevPtr = & (kobj->listDummy);
    kobj->size = 0U;
    kobj->init=TRUE;
     return (RK_SUCCESS);
}
__RK_INLINE   static inline RK_ERR kListInsertAfter(
        RK_LIST* const kobj, RK_NODE* const refNodePtr,
        RK_NODE* const newNodePtr)
{
    if (kobj == NULL || newNodePtr == NULL || refNodePtr == NULL)
    {
        return (RK_ERR_OBJ_NULL);
    }
    newNodePtr->nextPtr = refNodePtr->nextPtr;
    refNodePtr->nextPtr->prevPtr = newNodePtr;
    newNodePtr->prevPtr = refNodePtr;
    refNodePtr->nextPtr = newNodePtr;
    kobj->size += 1U;

     return (RK_SUCCESS);
}
__RK_INLINE   static inline RK_ERR kListRemove(RK_LIST* const kobj,
        RK_NODE* const remNodePtr)
{
    if (kobj == NULL || remNodePtr == NULL)
    {
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }
    KLISTNODEDEL(remNodePtr);
    kobj->size -= 1U;
     return (RK_SUCCESS);
}

__RK_INLINE   static inline RK_ERR kListRemoveHead(
        RK_LIST* const kobj, RK_NODE** const remNodePPtr)
{

    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }

    RK_NODE* currHeadPtr = kobj->listDummy.nextPtr;
    *remNodePPtr = currHeadPtr;
    KLISTNODEDEL(currHeadPtr);
    kobj->size -= 1U;
     return (RK_SUCCESS);
}
__RK_INLINE   static inline RK_ERR kListAddTail(
        RK_LIST* const kobj, RK_NODE* const newNodePtr)
{
    return (kListInsertAfter(kobj, kobj->listDummy.prevPtr, newNodePtr));
}

__RK_INLINE   static inline RK_ERR kListAddHead(
        RK_LIST* const kobj, RK_NODE* const newNodePtr)
{

    return (kListInsertAfter(kobj, &kobj->listDummy, newNodePtr));
}

__RK_INLINE
static inline RK_ERR kListRemoveTail(RK_LIST* const kobj, RK_NODE** remNodePPtr)
{
    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }

    RK_NODE* currTailPtr = kobj->listDummy.prevPtr;
    if (currTailPtr != NULL)
    {
        *remNodePPtr = currTailPtr;
        KLISTNODEDEL(currTailPtr);
        kobj->size -= 1U;
         return (RK_SUCCESS);
    }
    return (RK_ERR_OBJ_NULL);
}

#endif