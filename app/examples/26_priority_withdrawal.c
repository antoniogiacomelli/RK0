/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Independent withdrawal of message-pool ceilings and mutex inheritance.
 * A starts at nominal 20, owns a ceiling-8 buffer, and inherits 4 from H.
 * Changing nominal to 12 must preserve effective 4. Test both release orders:
 *   mutex first:  4 -> 8 -> 12
 *   buffer first: 4 -> 4 -> 12
 *
 * Peers at 6 and 10 expose incorrect ready scheduling and priority wait order.
 * Additional H-timeout cases withdraw inheritance while A is already READY or
 * waiting on a priority semaphore, exercising relocation in both queue types.
 * Run: make PLATFORM=qemu ARCH=armv7m run-priority-withdrawal-regression
 */
#include <kapi.h>
#include <ksch.h>
#include <stdio.h>

#define STACKSIZE 256U
#define A_NOMINAL 20U
#define A_NEW_NOMINAL 12U
#define BUFFER_CEILING 8U
#define H_PRIO 4U
#define PEER6_PRIO 6U
#define PEER10_PRIO 10U
#define DONOR_TIMEOUT RK_MS_TO_TICKS(80UL)
#define SETUP_TIMEOUT RK_MS_TO_TICKS(1000UL)

#define A_ID 0U
#define PEER6_ID 1U
#define PEER10_ID 2U
#define N_WORKERS 3U
#define CMD_ACQUIRE 1U
#define CMD_UNLOCK 2U
#define CMD_FREE 3U
#define CMD_READY 4U
#define CMD_WAIT 5U
#define CMD_UNLOCK_READY 6U
#define CMD_FREE_READY 7U

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(aHandle, ATask, aStack, STACKSIZE)
RK_DECLARE_TASK(hHandle, HTask, hStack, STACKSIZE)
RK_DECLARE_TASK(peer6Handle, PeerTask, peer6Stack, STACKSIZE)
RK_DECLARE_TASK(peer10Handle, PeerTask, peer10Stack, STACKSIZE)
RK_DECLARE_MESG_POOL(bufferPool, bufferStorage, ULONG, 1U)

static RK_MUTEX mutex;
static RK_MESG_ENDPOINT aEndpoint;
static RK_MESG *bufferPtr;
static RK_SEMAPHORE gates[N_WORKERS];
static RK_SEMAPHORE hGate;
static RK_SEMAPHORE completed;
static RK_SEMAPHORE orderGate;
static RK_TASK_HANDLE workers[N_WORKERS];
static UINT peerIds[2] = {PEER6_ID, PEER10_ID};
static volatile UINT commands[N_WORKERS];
static volatile UINT events[N_WORKERS];
static volatile UINT eventCount;
static volatile UINT handoffs;
static volatile UINT timeouts;
static volatile RK_TICK hTimeout;

static VOID Check_(RK_BOOL const ok, CHAR const *const wherePtr)
{
    if (ok == RK_FALSE)
    {
        printf("IW FAIL %s\r\n", wherePtr);
        while (1)
            (void)kSleep(RK_MS_TO_TICKS(1000UL));
    }
}

static VOID CheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
        printf("IW ERR %s err=%d\r\n", wherePtr, err);
    Check_((RK_BOOL)(err == RK_ERR_SUCCESS), wherePtr);
}

static VOID ExpectPrio_(RK_PRIO const expected)
{
    RK_PRIO const actual = RK_TASK_PRIO(aHandle);
    if (actual != expected)
        printf("IW PRIO expected=%u actual=%u nominal=%u\r\n",
               (UINT)expected, (UINT)actual, (UINT)aHandle->prioNominal);
    Check_((RK_BOOL)(actual == expected), "A effective priority");
}

static VOID WaitOn_(RK_TASK_HANDLE const task, RK_LIST const *const queue)
{
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), SETUP_TIMEOUT);
    while ((task->status != RK_BLOCKED) || (task->waitingQueuePtr != queue))
    {
        Check_((RK_BOOL)(K_TICK_EXPIRED(deadline) == 0U), "task did not park");
        CheckErr_(kSleep(1UL), "wait for task");
    }
}

/* There is no public nominal-priority setter. Use the kernel recalculation
 * path, as other regression benches do, without writing effective priority. */
static VOID SetNominal_(RK_PRIO const priority)
{
    RK_CR_AREA
    RK_CR_ENTER
    aHandle->prioNominal = priority;
    kTaskUpdateEffectivePrioChain(aHandle);
    RK_CR_EXIT
}

static VOID Issue_(UINT const id, UINT const command)
{
    commands[id] = command;
    CheckErr_(kSemaphorePost(&gates[id]), "issue worker command");
}

static VOID RunA_(UINT const command)
{
    Issue_(A_ID, command);
    CheckErr_(kSemaphorePend(&completed, RK_WAIT_FOREVER), "A completion");
    WaitOn_(aHandle, &gates[A_ID].waitingQueue);
}

static VOID Record_(UINT const id)
{
    RK_CR_AREA
    RK_CR_ENTER
    Check_((RK_BOOL)(eventCount < N_WORKERS), "extra execution event");
    events[eventCount++] = id;
    RK_CR_EXIT
}

/* Fixed expected sequences, independent of the scheduler's calculated values. */
static UINT Expected_(RK_PRIO const priority, UINT const index)
{
    static UINT const at4[N_WORKERS] = {A_ID, PEER6_ID, PEER10_ID};
    static UINT const at8[N_WORKERS] = {PEER6_ID, A_ID, PEER10_ID};
    static UINT const at12[N_WORKERS] = {PEER6_ID, PEER10_ID, A_ID};
    if (priority == H_PRIO)
        return (at4[index]);
    if (priority == BUFFER_CEILING)
        return (at8[index]);
    Check_((RK_BOOL)(priority == A_NEW_NOMINAL), "invalid expected priority");
    return (at12[index]);
}

static VOID QueueReady_(UINT const aCommand)
{
    eventCount = 0U;
    for (UINT id = 0U; id < N_WORKERS; id++)
    {
        Issue_(id, (id == A_ID) ? aCommand : CMD_READY);
        Check_((RK_BOOL)(workers[id]->status == RK_READY), "probe not READY");
    }
    /* Control at priority 1 keeps all three queued until it waits below. */
    Check_((RK_BOOL)(eventCount == 0U), "probe ran before release");
}

static VOID DrainReady_(RK_PRIO const expected)
{
    for (UINT i = 0U; i < N_WORKERS; i++)
        CheckErr_(kSemaphorePend(&completed, RK_WAIT_FOREVER), "ready completion");
    Check_((RK_BOOL)(eventCount == N_WORKERS), "ready event count");
    for (UINT i = 0U; i < N_WORKERS; i++)
    {
        Check_((RK_BOOL)(events[i] == Expected_(expected, i)), "ready execution order");
        WaitOn_(workers[i], &gates[i].waitingQueue);
    }
    ExpectPrio_(expected);
}

static VOID QueueWaiters_(VOID)
{
    static UINT const arrival[N_WORKERS] = {A_ID, PEER10_ID, PEER6_ID};
    eventCount = 0U;
    for (UINT i = 0U; i < N_WORKERS; i++)
    {
        UINT const id = arrival[i];
        Issue_(id, CMD_WAIT);
        WaitOn_(workers[id], &orderGate.waitingQueue);
    }
}

static VOID ExpectWaitOrder_(RK_PRIO const expected)
{
    Check_((RK_BOOL)(orderGate.waitingQueue.size == N_WORKERS), "wait queue size");
    RK_NODE const *node = orderGate.waitingQueue.listDummy.nextPtr;
    for (UINT i = 0U; i < N_WORKERS; i++)
    {
        Check_((RK_BOOL)(node == &workers[Expected_(expected, i)]->tcbNode),
               "priority wait queue order");
        node = node->nextPtr;
    }
}

static VOID DrainWaiters_(RK_PRIO const expected)
{
    ExpectPrio_(expected);
    ExpectWaitOrder_(expected);
    for (UINT i = 0U; i < N_WORKERS; i++)
    {
        /* Release one waiter at a time so ready scheduling cannot hide an
         * incorrect semaphore dequeue order. Verify who actually executed. */
        CheckErr_(kSemaphorePost(&orderGate), "release priority waiter");
        CheckErr_(kSemaphorePend(&completed, RK_WAIT_FOREVER), "wait completion");
        Check_((RK_BOOL)((eventCount == i + 1U) &&
                         (events[i] == Expected_(expected, i))),
               "waiter execution order");
        UINT const id = Expected_(expected, i);
        WaitOn_(workers[id], &gates[id].waitingQueue);
    }
}

static VOID ProbeRelease_(UINT const command, RK_PRIO const expected)
{
    /* Peers are READY before A releases anything. Record A only after its
     * release returns, so an omitted immediate deboost/preemption also fails. */
    QueueReady_(command);
    DrainReady_(expected);
    QueueWaiters_();
    DrainWaiters_(expected);
    printf("IW effective=%u ready/wait execution order verified\r\n", (UINT)expected);
}

static VOID StartCase_(RK_TICK const timeout)
{
    WaitOn_(aHandle, &gates[A_ID].waitingQueue);
    WaitOn_(hHandle, &hGate.waitingQueue);
    SetNominal_(A_NOMINAL);
    ExpectPrio_(A_NOMINAL);
    RunA_(CMD_ACQUIRE);
    ExpectPrio_(BUFFER_CEILING);
    hTimeout = timeout;
    CheckErr_(kSemaphorePost(&hGate), "start H");
    WaitOn_(hHandle, &mutex.waitingQueue);
    Check_((RK_BOOL)((mutex.ownerPtr == aHandle) &&
                     (aEndpoint.ownedMesgList.size == 1UL)), "A owns both resources");
    ExpectPrio_(H_PRIO);
}

static VOID CheckNominalChange_(VOID)
{
    SetNominal_(A_NEW_NOMINAL);
    Check_((RK_BOOL)(aHandle->prioNominal == A_NEW_NOMINAL), "new nominal stored");
    ExpectPrio_(H_PRIO);
    printf("IW nominal 20->12 effective=4 retains both contributions\r\n");
}

static VOID RunReleaseOrder_(UINT const order)
{
    printf("IW order=%u start nominal=20 ceiling=8 inheritance=4\r\n", order);
    StartCase_(RK_WAIT_FOREVER);
    if (order == 1U)
    {
        QueueReady_(CMD_READY);
        CheckNominalChange_(); /* A is already READY when nominal changes. */
        DrainReady_(H_PRIO);
        QueueWaiters_();
        DrainWaiters_(H_PRIO);
        ProbeRelease_(CMD_UNLOCK_READY, BUFFER_CEILING);
        Check_((RK_BOOL)(handoffs == order), "mutex handed to H");
        ProbeRelease_(CMD_FREE_READY, A_NEW_NOMINAL);
    }
    else
    {
        QueueWaiters_();
        CheckNominalChange_(); /* A is already in a priority wait queue. */
        DrainWaiters_(H_PRIO);
        QueueReady_(CMD_READY);
        DrainReady_(H_PRIO);
        ProbeRelease_(CMD_FREE_READY, H_PRIO);
        ProbeRelease_(CMD_UNLOCK_READY, A_NEW_NOMINAL);
        Check_((RK_BOOL)(handoffs == order), "mutex handed to H");
    }
    Check_((RK_BOOL)((aHandle->prioNominal == A_NEW_NOMINAL) &&
                     (aEndpoint.ownedMesgList.size == 0UL) &&
                     (aHandle->ownedMutexList.size == 0UL)), "final obligations cleared");
    printf("IW order=%u PASS\r\n", order);
}

static VOID WaitForDonorTimeout_(VOID)
{
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), SETUP_TIMEOUT);
    /* Keep Control running while SysTick removes H. A must stay in the queue
     * it occupied before the timeout and be relocated there by the kernel. */
    while (mutex.waitingQueue.size != 0UL)
        Check_((RK_BOOL)(K_TICK_EXPIRED(deadline) == 0U), "H did not time out");
    ExpectPrio_(BUFFER_CEILING);
}

static VOID RunQueuedWithdrawal_(RK_BOOL const ready)
{
    StartCase_(DONOR_TIMEOUT);
    if (ready)
        QueueReady_(CMD_READY);
    else
        QueueWaiters_();
    CheckNominalChange_();
    if (!ready)
        ExpectWaitOrder_(H_PRIO);
    WaitForDonorTimeout_();
    if (ready)
        DrainReady_(BUFFER_CEILING);
    else
        DrainWaiters_(BUFFER_CEILING);
    WaitOn_(hHandle, &hGate.waitingQueue);
    Check_((RK_BOOL)(timeouts == (ready ? 1U : 2U)), "H timeout returned");
    Check_((RK_BOOL)((mutex.ownerPtr == aHandle) &&
                     (aEndpoint.ownedMesgList.size == 1UL)), "timeout preserves ownership");
    RunA_(CMD_UNLOCK);
    ExpectPrio_(BUFFER_CEILING);
    RunA_(CMD_FREE);
    ExpectPrio_(A_NEW_NOMINAL);
    printf("IW timeout while %s PASS 4->8->12\r\n", ready ? "READY" : "WAITING");
}

int main(void)
{
    kCoreInit();
    kInit();
    Check_(RK_FALSE, "kernel returned");
}

VOID kApplicationInit(VOID)
{
    CheckErr_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "Control",
                        controlStack, STACKSIZE, 1U, RK_PREEMPT), "task Control");
    CheckErr_(kTaskInit(&aHandle, ATask, RK_NO_ARGS, "A", aStack,
                        STACKSIZE, A_NOMINAL, RK_PREEMPT), "task A");
    CheckErr_(kTaskInit(&hHandle, HTask, RK_NO_ARGS, "H", hStack,
                        STACKSIZE, H_PRIO, RK_PREEMPT), "task H");
    CheckErr_(kTaskInit(&peer6Handle, PeerTask, &peerIds[0], "Peer6", peer6Stack,
                        STACKSIZE, PEER6_PRIO, RK_PREEMPT), "task Peer6");
    CheckErr_(kTaskInit(&peer10Handle, PeerTask, &peerIds[1], "Peer10", peer10Stack,
                        STACKSIZE, PEER10_PRIO, RK_PREEMPT), "task Peer10");
    workers[A_ID] = aHandle;
    workers[PEER6_ID] = peer6Handle;
    workers[PEER10_ID] = peer10Handle;
    for (UINT i = 0U; i < N_WORKERS; i++)
        CheckErr_(kSemaBinInit(&gates[i], 0U, RK_WAIT_PRIORITY), "worker gate");
    CheckErr_(kSemaBinInit(&hGate, 0U, RK_WAIT_PRIORITY), "H gate");
    CheckErr_(kSemaCountInit(&completed, 0U, RK_WAIT_PRIORITY), "completion gate");
    CheckErr_(kSemaCountInit(&orderGate, 0U, RK_WAIT_PRIORITY), "priority gate");
    CheckErr_(kMutexInit(&mutex, RK_PRIO_INHERITANCE, RK_NO_CEILING), "mutex");
    CheckErr_(kMesgEndpointInit(&aEndpoint, aHandle, RK_MESG_SEND_ONLY), "A endpoint");
    CheckErr_(kMesgPoolInit(&bufferPool, bufferStorage, sizeof(ULONG), 1UL,
                           BUFFER_CEILING), "ceiling pool");
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    for (UINT i = 0U; i < N_WORKERS; i++)
        WaitOn_(workers[i], &gates[i].waitingQueue);
    RunReleaseOrder_(1U);
    RunReleaseOrder_(2U);
    RunQueuedWithdrawal_(RK_TRUE);
    RunQueuedWithdrawal_(RK_FALSE);
    printf("IW PASS independent priority withdrawal and scheduling\r\n");
    while (1)
        (void)kSleep(RK_MS_TO_TICKS(1000UL));
}

VOID ATask(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        CheckErr_(kSemaphorePend(&gates[A_ID], RK_WAIT_FOREVER), "A command");
        switch (commands[A_ID])
        {
            case CMD_ACQUIRE:
                CheckErr_(kMutexLock(&mutex, RK_WAIT_FOREVER), "A lock");
                CheckErr_(kMesgAlloc(&bufferPool, &bufferPtr, RK_NO_WAIT), "A alloc");
                break;
            case CMD_UNLOCK:
            case CMD_UNLOCK_READY:
                CheckErr_(kMutexUnlock(&mutex), "A unlock");
                if (commands[A_ID] == CMD_UNLOCK_READY)
                    Record_(A_ID);
                break;
            case CMD_FREE:
            case CMD_FREE_READY:
                CheckErr_(kMesgFree(bufferPtr), "A free");
                bufferPtr = NULL;
                if (commands[A_ID] == CMD_FREE_READY)
                    Record_(A_ID);
                break;
            case CMD_WAIT:
                CheckErr_(kSemaphorePend(&orderGate, RK_WAIT_FOREVER), "A priority wait");
                Record_(A_ID);
                break;
            case CMD_READY:
                Record_(A_ID);
                break;
            default:
                Check_(RK_FALSE, "unknown A command");
                break;
        }
        CheckErr_(kSemaphorePost(&completed), "A completed");
    }
}

VOID HTask(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        CheckErr_(kSemaphorePend(&hGate, RK_WAIT_FOREVER), "H command");
        RK_ERR const err = kMutexLock(&mutex, hTimeout);
        if (hTimeout == RK_WAIT_FOREVER)
        {
            CheckErr_(err, "H lock after handoff");
            handoffs++;
            CheckErr_(kMutexUnlock(&mutex), "H unlock");
        }
        else
        {
            Check_((RK_BOOL)(err == RK_ERR_TIMEOUT), "H expected timeout");
            timeouts++;
        }
    }
}

VOID PeerTask(VOID *args)
{
    UINT const id = *(UINT const *)args;
    while (1)
    {
        CheckErr_(kSemaphorePend(&gates[id], RK_WAIT_FOREVER), "peer command");
        if (commands[id] == CMD_WAIT)
            CheckErr_(kSemaphorePend(&orderGate, RK_WAIT_FOREVER), "peer priority wait");
        else
            Check_((RK_BOOL)(commands[id] == CMD_READY), "unknown peer command");
        Record_(id);
        CheckErr_(kSemaphorePost(&completed), "peer completed");
    }
}
