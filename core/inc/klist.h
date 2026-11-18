/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.92.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#ifndef RK_LIST_H
#define RK_LIST_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Inlined functions for the Doubly Linked List ADT */

#ifndef K_GET_TCB_ADDR
#define K_GET_TCB_ADDR(nodePtr) K_GET_CONTAINER_ADDR(nodePtr, RK_TCB, tcbNode)
#endif

#ifndef KLISTNODEDEL
#define KLISTNODEDEL(nodePtr)\
        do\
        {\
            nodePtr->nextPtr->prevPtr = nodePtr->prevPtr;\
            nodePtr->prevPtr->nextPtr = nodePtr->nextPtr;\
            nodePtr->prevPtr = NULL;\
            nodePtr->nextPtr = NULL;\
        } while (0U)
#endif

RK_ERR kListInit(RK_LIST *const kobj);
RK_ERR kListInsertAfter(RK_LIST *const kobj, RK_NODE *const refNodePtr,
                        RK_NODE *const newNodePtr);
RK_ERR kListRemove(RK_LIST *const kobj, RK_NODE *const remNodePtr);
RK_ERR kListRemoveHead(RK_LIST *const kobj, RK_NODE **const remNodePPtr);
RK_ERR kListAddTail(RK_LIST *const kobj, RK_NODE *const newNodePtr);
RK_ERR kListAddHead(RK_LIST *const kobj, RK_NODE *const newNodePtr);
RK_ERR kListRemoveTail(RK_LIST *const kobj, RK_NODE **remNodePPtr);

/*
Convenience macros: 
lp list pointer
ndp node pointer
rndp ref node pointer
remndpp pointer to pointer to store address of removed node
remndp pointer to node to remove

*/

#ifndef RK_DECLARE_LIST
#define RK_DECLARE_LIST(ln) RK_LIST ln;
#endif

#ifndef RK_DECLARE_NODE
#define RK_DECLARE_NODE(node) RK_NODE node;
#endif

#ifndef K_LIST_INIT
#define K_LIST_INIT(lp) kListInit((lp))
#endif
#ifndef K_LIST_ADD_TO_TAIL
#define K_LIST_ADD_TO_TAIL(lp, ndp) kListAddTail((lp), (ndp))
#endif
#ifndef K_LIST_ADD_TO_HEAD
#define K_LIST_ADD_TO_HEAD(lp, ndp) kListAddHead((lp), (ndp))
#endif
#ifndef K_LIST_INSERT_AFTER
#define K_LIST_INSERT_AFTER(lp, rndp, ndp) kListInsertAfter((lp), (rndp), (ndp))
#endif
#ifndef K_LIST_REMOVE
#define K_LIST_REMOVE(lp, remndp) kListRemove((lp), (remndp))
#endif
#ifndef K_LIST_REMOVE_HEAD
#define K_LIST_REMOVE_HEAD(lp, remndpp) kListRemoveHead((lp), (remndpp))
#endif
#ifndef K_LIST_REMOVE_TAIL
#define K_LIST_REMOVE_TAIL(lp, remndpp) kListRemoveTail((lp), (remndpp))
#endif






#ifdef __cplusplus
}
#endif

#endif
