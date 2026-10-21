/* SPDX-License-Identifier: Apache-2.0 */
/* Time a native send that changes a blocked task's message ceiling and
 * propagates through an existing PIP chain. Each wait queue has a peer to
 * overtake; the terminal owner is READY. No kernel state is assigned here.
 */
#include "bench.h"
#include <ksch.h>

#ifndef EXP_CHAIN_DEPTH
#define EXP_CHAIN_DEPTH 2U
#endif
#ifndef EXP_CHAIN_CEILING
#define EXP_CHAIN_CEILING 3U
#endif
#if (EXP_CHAIN_DEPTH < 1U) || (EXP_CHAIN_DEPTH > 3U)
#error "EXP_CHAIN_DEPTH must be 1..3"
#endif
#if (EXP_CHAIN_CEILING != 0U) && (EXP_CHAIN_CEILING != 3U)
#error "EXP_CHAIN_CEILING must be 0 (control) or 3"
#endif
#if (RK_CONF_N_USRTASKS_MAX < (3U + 2U * EXP_CHAIN_DEPTH))
#error "Chain fixture needs controller, sender, source, owners, and peers"
#endif

#define OWNER_LOCKED RK_EVENT_1
#define SENT RK_EVENT_2
#define SOURCE_DONE RK_EVENT_3
#define LINK RK_EVENT_2
#define CHAIN_STACK 128U
#define SOURCE_PRIO 8U
#define PEER_PRIO 6U
#define SENDER_PRIO 4U
#define BOOST_PRIO 3U
#define NO_RANK ((UINT)0xFFFFFFFFU)

typedef struct { ULONG generation; } Payload;
RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, 320U)
RK_DECLARE_TASK(senderHandle, SenderTask, senderStack, EXP_STACK)
RK_DECLARE_TASK(sourceHandle, SourceTask, sourceStack, CHAIN_STACK)
RK_DECLARE_MESG_POOL(pool, poolBuffer, Payload, 1U)
static RK_MESG_ENDPOINT senderEndpoint;
static RK_MESG_ENDPOINT sourceEndpoint;
static RK_MUTEX mutexes[EXP_CHAIN_DEPTH];
static RK_TASK_HANDLE owners[EXP_CHAIN_DEPTH];
static RK_TASK_HANDLE peers[EXP_CHAIN_DEPTH];
static RK_STACK ownerStacks[EXP_CHAIN_DEPTH][CHAIN_STACK] K_ALIGN(8);
static RK_STACK peerStacks[EXP_CHAIN_DEPTH][CHAIN_STACK] K_ALIGN(8);
static UINT identities[EXP_CHAIN_DEPTH];
static volatile ULONG generation;
static volatile ULONG sendCounts;
static volatile RK_PRIO senderBefore;
static volatile RK_PRIO senderAfter;
static RK_PRIO beforePrio[EXP_CHAIN_DEPTH + 1U];
static UINT beforeRank[EXP_CHAIN_DEPTH];

static VOID OwnerTask(VOID *args);
static VOID PeerTask(VOID *args);

static RK_PRIO OwnerNominal_(UINT const index)
{
    return ((RK_PRIO)(12U + 2U * index));
}

static RK_TASK_EVENT OwnerDone_(UINT const index)
{
    return (1UL << (3U + index));
}

static RK_TASK_EVENT PeerDone_(UINT const index)
{
    return (1UL << (3U + EXP_CHAIN_DEPTH + index));
}

static RK_TASK_HANDLE Waiter_(UINT const index)
{
    return ((index == 0U) ? sourceHandle : owners[index - 1U]);
}

/* Read-only queue inspection, outside the timed interval. Bound traversal
 * and check both directions so a corrupt link cannot masquerade as a move.
 */
static UINT Rank_(RK_TCBQ const *const queue, RK_TASK_HANDLE const task)
{
    RK_NODE const *node = queue->listDummy.nextPtr;
    UINT rank = NO_RANK;
    for (ULONG i = 0UL; i < queue->size; i++)
    {
        BenchCheck_((RK_BOOL)((node != &queue->listDummy) && (node != NULL)),
                    "queue traversal");
        BenchCheck_((RK_BOOL)((node->nextPtr->prevPtr == node) &&
                              (node->prevPtr->nextPtr == node)), "queue links");
        if (K_GET_TCB_ADDR(node) == task)
        {
            BenchCheck_((RK_BOOL)(rank == NO_RANK), "duplicate queue member");
            rank = (UINT)i;
        }
        node = node->nextPtr;
    }
    BenchCheck_((RK_BOOL)(node == &queue->listDummy), "queue size");
    return (rank);
}

static VOID WaitDependency_(RK_TASK_HANDLE const task, RK_MUTEX *const mutex)
{
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), EXP_LIMIT);
    while (task->waitingForMutexPtr != mutex)
    {
        BenchCheck_((RK_BOOL)(K_TICK_EXPIRED(deadline) == 0U), "chain dependency");
        kSleep(1UL);
    }
    BenchCheck_((RK_BOOL)((task->status == RK_BLOCKED) &&
                          (task->waitingQueuePtr == &mutex->waitingQueue) &&
                          (task->waitingQueueOrder == RK_WAIT_PRIORITY)),
                "priority-ordered mutex wait");
}

static VOID Prepare_(VOID)
{
    /* All owners first park on private events while holding their mutex.
     * Peers enter before the chain links so equal-priority FIFO order puts
     * the upstream chain task behind its competing peer at every link.
     */
    for (UINT j = EXP_CHAIN_DEPTH; j > 0U; j--)
    {
        BenchSignal_(owners[j - 1U], EXP_GO);
        BenchWait_(OWNER_LOCKED);
    }
    for (UINT i = 0U; i < EXP_CHAIN_DEPTH; i++)
    {
        BenchSignal_(peers[i], EXP_GO);
        WaitDependency_(peers[i], &mutexes[i]);
    }
    for (UINT j = EXP_CHAIN_DEPTH; j > 1U; j--)
    {
        UINT const i = j - 2U;
        BenchSignal_(owners[i], LINK);
        WaitDependency_(owners[i], &mutexes[i + 1U]);
    }
    BenchSignal_(sourceHandle, EXP_GO);
    WaitDependency_(sourceHandle, &mutexes[0]);
    beforePrio[0] = sourceHandle->priority;
    BenchCheck_((RK_BOOL)(beforePrio[0] == SOURCE_PRIO), "source baseline");
    for (UINT i = 0U; i < EXP_CHAIN_DEPTH; i++)
    {
        beforePrio[i + 1U] = owners[i]->priority;
        beforeRank[i] = Rank_(&mutexes[i].waitingQueue, Waiter_(i));
        BenchCheck_((RK_BOOL)((mutexes[i].ownerPtr == owners[i]) &&
                              (mutexes[i].waitingQueue.size == 2UL) &&
                              (beforeRank[i] == 1U) &&
                              (Rank_(&mutexes[i].waitingQueue, peers[i]) == 0U) &&
                              (beforePrio[i + 1U] == PEER_PRIO)),
                    "chain baseline and peer order");
    }
    BenchCheck_((RK_BOOL)((pool.nFreeBlocks == 1UL) &&
                          (sourceEndpoint.ownedMesgList.size == 0UL) &&
                          (sourceEndpoint.mesgQueue.size == 0UL)), "empty source endpoint");

    /* Root is readied but cannot dispatch ahead of controller (1) or sender
     * (4). It will unlock only after measurement and observation complete.
     */
    RK_TASK_HANDLE const root = owners[EXP_CHAIN_DEPTH - 1U];
    BenchSignal_(root, LINK);
    BenchCheck_((RK_BOOL)((root->status == RK_READY) &&
                          (Rank_(&RK_gReadyQueue[PEER_PRIO], root) == 0U) &&
                          (RK_gReadyQueue[BOOST_PRIO].size == 0UL) &&
                          ((RK_gReadyBitmask & (1UL << PEER_PRIO)) != 0UL)),
                "terminal owner ready before send");
}

static VOID Observe_(UINT const sample)
{
    RK_BOOL const boost = (RK_BOOL)(EXP_CHAIN_CEILING != 0U);
    RK_PRIO const expectedSource = (boost == RK_TRUE) ? BOOST_PRIO : SOURCE_PRIO;
    RK_PRIO const expectedOwner = (boost == RK_TRUE) ? BOOST_PRIO : PEER_PRIO;
    UINT changed = (sourceHandle->priority != beforePrio[0]) ? 1U : 0U;
    UINT moved = 0U;
    BenchCheck_((RK_BOOL)((sourceHandle->priority == expectedSource) &&
                          (sourceHandle->status == RK_BLOCKED) &&
                          (sourceEndpoint.ownedMesgList.size == 1UL) &&
                          (sourceEndpoint.mesgQueue.size == 1UL) &&
                          (senderEndpoint.ownedMesgList.size == 0UL) &&
                          (pool.nFreeBlocks == 0UL)), "send ownership and source priority");
    for (UINT i = 0U; i < EXP_CHAIN_DEPTH; i++)
    {
        UINT const rank = Rank_(&mutexes[i].waitingQueue, Waiter_(i));
        if (owners[i]->priority != beforePrio[i + 1U]) { changed++; }
        if (rank != beforeRank[i]) { moved++; }
        BenchCheck_((RK_BOOL)((owners[i]->priority == expectedOwner) &&
                              (peers[i]->priority == PEER_PRIO) &&
                              (Waiter_(i)->status == RK_BLOCKED) &&
                              (Waiter_(i)->waitingForMutexPtr == &mutexes[i]) &&
                              (Waiter_(i)->waitingQueuePtr == &mutexes[i].waitingQueue) &&
                              (peers[i]->status == RK_BLOCKED) &&
                              (peers[i]->waitingForMutexPtr == &mutexes[i]) &&
                              (peers[i]->waitingQueuePtr == &mutexes[i].waitingQueue) &&
                              (mutexes[i].ownerPtr == owners[i]) &&
                              (mutexes[i].waitingQueue.size == 2UL) &&
                              (rank == ((boost == RK_TRUE) ? 0U : 1U)) &&
                              (Rank_(&mutexes[i].waitingQueue, peers[i]) ==
                               ((boost == RK_TRUE) ? 1U : 0U))), "propagated queue repositioning");
    }
    RK_TASK_HANDLE const root = owners[EXP_CHAIN_DEPTH - 1U];
    UINT const readyMoved = (root->priority != beforePrio[EXP_CHAIN_DEPTH]) ? 1U : 0U;
    BenchCheck_((RK_BOOL)((root->status == RK_READY) &&
                          (Rank_(&RK_gReadyQueue[expectedOwner], root) == 0U) &&
                          ((RK_gReadyBitmask & (1UL << expectedOwner)) != 0UL)),
                "terminal ready queue after send");
    if (boost == RK_TRUE)
    {
        BenchCheck_((RK_BOOL)((Rank_(&RK_gReadyQueue[PEER_PRIO], root) == NO_RANK) &&
                              ((RK_gReadyBitmask & (1UL << PEER_PRIO)) == 0UL) &&
                              (changed == EXP_CHAIN_DEPTH + 1U) &&
                              (moved == EXP_CHAIN_DEPTH) && (readyMoved == 1U)),
                    "all priority changes and queue moves observed");
    }
    else
    {
        BenchCheck_((RK_BOOL)((changed == 0U) && (moved == 0U) && (readyMoved == 0U) &&
                              (RK_gReadyQueue[BOOST_PRIO].size == 0UL)),
                    "control send leaves chain unchanged");
    }
    BenchCheck_((RK_BOOL)((senderBefore == ((boost == RK_TRUE) ? BOOST_PRIO : SENDER_PRIO)) &&
                          (senderAfter == SENDER_PRIO)), "sender ownership restoration");
    if (sample != 0U)
    {
        printf("EXP RESULT experiment=E3 path=chain_change ceiling=%u depth=%u "
               "sample=%u send_counts=%lu changed_tasks=%u wait_queue_moves=%u "
               "ready_queue_moves=%u queue_size=2 source_before_prio=%u "
               "source_after_prio=%u owner_before_prio=%u owner_after_prio=%u "
               "sender_before_prio=%u sender_after_prio=%u\r\n",
               (UINT)EXP_CHAIN_CEILING, (UINT)EXP_CHAIN_DEPTH, sample, sendCounts,
               changed, moved, readyMoved, (UINT)beforePrio[0],
               (UINT)sourceHandle->priority, (UINT)beforePrio[EXP_CHAIN_DEPTH],
               (UINT)root->priority, (UINT)senderBefore, (UINT)senderAfter);
    }
}

VOID kApplicationInit(VOID)
{
    BenchOK_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "E3ctrl",
                      controlStack, 320U, 1U, RK_PREEMPT), "control init");
    BenchOK_(kTaskInit(&senderHandle, SenderTask, RK_NO_ARGS, "E3send",
                      senderStack, EXP_STACK, SENDER_PRIO, RK_PREEMPT), "sender init");
    BenchOK_(kTaskInit(&sourceHandle, SourceTask, RK_NO_ARGS, "E3src",
                      sourceStack, CHAIN_STACK, SOURCE_PRIO, RK_PREEMPT), "source init");
    BenchOK_(kMesgEndpointInit(&senderEndpoint, senderHandle, RK_MESG_SEND_ONLY),
             "sender endpoint");
    BenchOK_(kMesgEndpointInit(&sourceEndpoint, sourceHandle, RK_MESG_SEND_RECV),
             "source endpoint");
    BenchOK_(kMesgPoolInit(&pool, poolBuffer, sizeof(Payload), 1U,
                         (EXP_CHAIN_CEILING == 0U) ? RK_MESG_PRIO_CEILING_NONE
                                                  : BOOST_PRIO), "pool init");
    for (UINT i = 0U; i < EXP_CHAIN_DEPTH; i++)
    {
        identities[i] = i;
        BenchOK_(kMutexInit(&mutexes[i], RK_PRIO_INHERITANCE, RK_NO_CEILING), "mutex init");
        BenchOK_(kTaskInit(&owners[i], OwnerTask, &identities[i], "E3owner",
                          ownerStacks[i], CHAIN_STACK, OwnerNominal_(i), RK_PREEMPT), "owner init");
        BenchOK_(kTaskInit(&peers[i], PeerTask, &identities[i], "E3peer",
                          peerStacks[i], CHAIN_STACK, PEER_PRIO, RK_PREEMPT), "peer init");
    }
}

static VOID OwnerTask(VOID *args)
{
    UINT const i = *(UINT const *)args;
    for (;;)
    {
        BenchGo_();
        BenchOK_(kMutexLock(&mutexes[i], RK_NO_WAIT), "owner initial lock");
        BenchSignal_(controlHandle, OWNER_LOCKED);
        BenchWait_(LINK);
        if (i + 1U < EXP_CHAIN_DEPTH)
        {
            BenchOK_(kMutexLock(&mutexes[i + 1U], EXP_LIMIT), "owner chain lock");
            BenchOK_(kMutexUnlock(&mutexes[i + 1U]), "owner chain unlock");
        }
        BenchOK_(kMutexUnlock(&mutexes[i]), "owner initial unlock");
        BenchSignal_(controlHandle, OwnerDone_(i));
    }
}

static VOID PeerTask(VOID *args)
{
    UINT const i = *(UINT const *)args;
    for (;;)
    {
        BenchGo_();
        BenchOK_(kMutexLock(&mutexes[i], EXP_LIMIT), "peer lock");
        BenchOK_(kMutexUnlock(&mutexes[i]), "peer unlock");
        BenchSignal_(controlHandle, PeerDone_(i));
    }
}

VOID SourceTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        BenchGo_();
        BenchOK_(kMutexLock(&mutexes[0], EXP_LIMIT), "source lock");
        BenchOK_(kMutexUnlock(&mutexes[0]), "source unlock");
        RK_MESG *message = NULL;
        BenchOK_(kMesgWait(RK_ANY_TASK, &message, RK_NO_WAIT), "source receive");
        Payload const *const payload = kMesgPayloadConst(message);
        BenchCheck_((RK_BOOL)(payload->generation == generation), "message generation");
        BenchOK_(kMesgFree(message), "source free");
        BenchSignal_(controlHandle, SOURCE_DONE);
    }
}

VOID SenderTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        BenchGo_();
        RK_MESG *message = NULL;
        BenchOK_(kMesgAlloc(&pool, &message, RK_NO_WAIT), "sender allocation");
        Payload *const payload = kMesgPayload(message);
        payload->generation = generation;
        senderBefore = senderHandle->priority;
        /* Scheduler locking is outside timing. Recalculation, wait-queue and
         * READY-queue moves, bitmask updates, and reschedule decisions remain
         * native; only dispatch is deferred until after the ending timestamp.
         */
        kSchLock();
        ULONG const start = BenchNow_();
        RK_ERR const err = kMesgSend(sourceHandle, message);
        sendCounts = BenchNow_() - start;
        senderAfter = senderHandle->priority;
        BenchOK_(err, "timed native send");
        BenchSignal_(controlHandle, SENT);
        kSchUnlock();
    }
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    BenchHeader_("E3");
    RK_TASK_EVENT done = SOURCE_DONE;
    for (UINT i = 0U; i < EXP_CHAIN_DEPTH; i++) { done |= OwnerDone_(i) | PeerDone_(i); }
    for (UINT sample = 0U; sample <= EXP_SAMPLES; sample++)
    {
        generation++;
        Prepare_();
        BenchSignal_(senderHandle, EXP_GO);
        BenchWait_(SENT);
        Observe_(sample);
        BenchWait_(done);
        BenchCheck_((RK_BOOL)((pool.nFreeBlocks == 1UL) &&
                              (pool.waitingQueue.size == 0UL) &&
                              (sourceEndpoint.ownedMesgList.size == 0UL) &&
                              (sourceEndpoint.mesgQueue.size == 0UL) &&
                              (sourceHandle->priority == SOURCE_PRIO) &&
                              (sourceHandle->waitingForMutexPtr == NULL) &&
                              (sourceHandle->ownedMutexList.size == 0UL) &&
                              (senderHandle->priority == SENDER_PRIO)), "chain message cleanup");
        for (UINT i = 0U; i < EXP_CHAIN_DEPTH; i++)
        {
            BenchCheck_((RK_BOOL)((mutexes[i].ownerPtr == NULL) &&
                                  (mutexes[i].waitingQueue.size == 0UL) &&
                                  (owners[i]->priority == OwnerNominal_(i)) &&
                                  (peers[i]->priority == PEER_PRIO) &&
                                  (owners[i]->ownedMutexList.size == 0UL) &&
                                  (peers[i]->ownedMutexList.size == 0UL) &&
                                  (owners[i]->waitingForMutexPtr == NULL) &&
                                  (peers[i]->waitingForMutexPtr == NULL)), "chain mutex cleanup");
        }
    }
    printf("EXP PASS E3\r\n");
    BenchStop_();
}
