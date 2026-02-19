/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.9.19                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#ifndef RK_LIST_H
#define RK_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

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
    } while (0U)
#endif

RK_ERR kListInit(RK_LIST *const kobj);
RK_ERR kListInsertAfter(RK_LIST *const kobj, RK_NODE *const refNodePtr,  RK_NODE *const newNodePtr);
RK_ERR kListRemove(RK_LIST *const kobj, RK_NODE *const remNodePtr);
RK_ERR kListRemoveHead(RK_LIST *const kobj,                                     RK_NODE **const remNodePPtr);
RK_ERR kListAddTail(RK_LIST *const kobj, RK_NODE *const newNodePtr);
RK_ERR kListAddHead(RK_LIST *const kobj, RK_NODE *const newNodePtr);
RK_ERR kListRemoveTail(RK_LIST *const kobj, RK_NODE **remNodePPtr);

#ifdef __cplusplus
}
#endif

#endif
