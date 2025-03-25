/******************************************************************************
 *
 *     [[RK0] | [VERSION: 0.4.0]]
 *
 ******************************************************************************
 ******************************************************************************
 *  In this header:
 *                  o High-level scheduler kernel module.
 *
 *****************************************************************************/
#ifndef RK_SCH_H
#define RK_SCH_H
#ifdef __cplusplus
{
#endif

/* shared data */
extern RK_TCB* runPtr; /* Pointer to the running TCB */
extern RK_TCB tcbs[RK_NTHREADS]; /* Pool of TCBs */
extern volatile RK_FAULT faultID; /* Fault ID */
extern INT idleStack[RK_CONF_IDLE_STACKSIZE]; /* Stack for idle task */
extern INT timerHandlerStack[RK_CONF_TIMHANDLER_STACKSIZE];
extern RK_TCBQ readyQueue[RK_CONF_MIN_PRIO + 2]; /* Table of ready queues */
extern RK_TCBQ timeOutQueue;
extern volatile BOOL lockScheduler;
extern volatile ULONG idleTicks;
BOOL kSchNeedReschedule(RK_TCB*);
VOID kSchSwtch(VOID);
UINT kEnterCR(VOID);
VOID kExitCR(UINT);
VOID kInit(VOID);
VOID kYield(VOID);
VOID kApplicationInit(VOID);
extern unsigned __getReadyPrio(unsigned);
RK_ERR kTCBQInit(RK_LIST* const, CHAR*);
RK_ERR kTCBQEnq(RK_LIST* const, RK_TCB* const);
RK_ERR kTCBQDeq(RK_TCBQ* const, RK_TCB** const);
RK_ERR kTCBQRem(RK_TCBQ* const, RK_TCB** const);
RK_ERR kReadyCtxtSwtch(RK_TCB* const);
RK_ERR kReadyQDeq(RK_TCB** const, RK_PRIO);
RK_TCB* kTCBQPeek(RK_TCBQ* const);
RK_ERR kTCBQEnqByPrio(RK_TCBQ* const, RK_TCB* const);

/* Doubly Linked List ADT */

#define RK_LIST_GET_TCB_NODE(nodePtr, containerType) \
    RK_GET_CONTAINER_ADDR(nodePtr, containerType, tcbNode)

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

__attribute__((always_inline)) static inline RK_ERR kListInit(RK_LIST* const kobj,
        CHAR* listName)
{
    if (kobj == NULL)
    {
        return (RK_ERR_OBJ_NULL);
    }
    kobj->listDummy.nextPtr = & (kobj->listDummy);
    kobj->listDummy.prevPtr = & (kobj->listDummy);
    kobj->listName = listName;
    kobj->size = 0U;
    kobj->init=RK_TRUE;
    _RK_DMB/*guarantee data is updated before going*/
    return (RK_SUCCESS);
}
__attribute__((always_inline))    static inline RK_ERR kListInsertAfter(
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

    _RK_DMB
    return (RK_SUCCESS);
}
__attribute__((always_inline))    static inline RK_ERR kListRemove(RK_LIST* const kobj,
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
    _RK_DMB
    return (RK_SUCCESS);
}

__attribute__((always_inline))    static inline RK_ERR kListRemoveHead(
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
    _RK_DMB
    return (RK_SUCCESS);
}
__attribute__((always_inline))    static inline RK_ERR kListAddTail(
        RK_LIST* const kobj, RK_NODE* const newNodePtr)
{
    return (kListInsertAfter(kobj, kobj->listDummy.prevPtr, newNodePtr));
}

__attribute__((always_inline))    static inline RK_ERR kListAddHead(
        RK_LIST* const kobj, RK_NODE* const newNodePtr)
{

    return (kListInsertAfter(kobj, &kobj->listDummy, newNodePtr));
}

__attribute__((always_inline))

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
        _RK_DMB
        return (RK_SUCCESS);
    }
    return (RK_ERR_OBJ_NULL);
}

#ifdef __cplusplus
}
#endif

#endif /* KSCH_H */
