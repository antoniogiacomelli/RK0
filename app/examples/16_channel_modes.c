/* SPDX-License-Identifier: Apache-2.0 */
/* Channel ownership, copy bounds, completion, cancellation, and lifecycle. */
#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 192U
#define OP_SEND 1U
#define OP_RECV 2U
#define OP_CALL 3U
#define OP_ACCEPT 4U
#define OP_REPLY 5U
#define OP_STALE_REPLY 6U
#define OP_ACCEPT_OTHER 7U
#define FINITE_WAIT RK_MS_TO_TICKS(40UL)

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(senderHandle, WorkerTask, senderStack, STACKSIZE)
RK_DECLARE_TASK(receiverHandle, WorkerTask, receiverStack, STACKSIZE)

typedef struct
{
    UINT request;
    UINT done;
    UINT operation;
    RK_TICK timeout;
    ULONG bytes;
    RK_ERR result;
} WORK;

static volatile WORK work[2];
static UINT workerIds[2] = {0U, 1U};
static RK_CHANNEL channel;
static RK_CHANNEL otherChannel;
static RK_CHANNEL invalidChannel;
static RK_CHANNEL_CALL_DATA accepted;
static RK_CHANNEL_CALL_DATA stale;
static ULONG request[2] = {0x12345678UL, 0xABCD5678UL};
static ULONG answer[2] = {0x87654321UL, 0x8765ABCDUL};
static ULONG reply[2];
static ULONG replyBytes;
static ULONG receivedBytes;
#if (RK_CONF_DYNAMIC_TASK == ON)
static RK_MEM_PARTITION stackPool;
static RK_STACK dynamicStack[STACKSIZE] K_ALIGN(8);
#endif
static struct
{
    ULONG before;
    ULONG payload[2];
    ULONG after;
} received = {0xFEED1234UL, {0UL, 0UL}, 0xFEED5678UL};

static VOID Stop_(VOID)
{
    while (1)
        (void)kSleep(RK_MS_TO_TICKS(1000UL));
}

static VOID Check_(RK_BOOL const ok, CHAR const *const wherePtr)
{
    if (ok == RK_FALSE)
    {
        printf("CH FAIL %s\r\n", wherePtr);
        Stop_();
    }
}

static VOID Expect_(RK_ERR const actual, RK_ERR const expected,
                     CHAR const *const wherePtr)
{
    if (actual != expected)
    {
        printf("CH FAIL %s expected=%d actual=%d\r\n", wherePtr,
               expected, actual);
        Stop_();
    }
}

static VOID Start_(UINT const id, UINT const op, ULONG const bytes,
                    RK_TICK const timeout)
{
    work[id].operation = op;
    work[id].bytes = bytes;
    work[id].timeout = timeout;
    work[id].request++;
}

static VOID Done_(UINT const id, RK_ERR const result)
{
    for (UINT i = 0U; i < 500U; i++)
    {
        if (work[id].done == work[id].request)
        {
            Expect_(work[id].result, result, "worker result");
            return;
        }
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "worker stalled");
}

static VOID Pending_(RK_CHANNEL_STATE const state, RK_BOOL const receiving)
{
    for (UINT i = 0U; i < 500U; i++)
    {
        RK_CR_AREA
        RK_CR_ENTER
        RK_BOOL const ready = (RK_BOOL)((channel.state == state) &&
                                        (channel.receiverWaiting == receiving));
        RK_CR_EXIT
        if (ready)
            return;
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "pending operation missing");
}

static VOID Prio_(RK_TASK_HANDLE const taskHandle, RK_PRIO const priority)
{
    RK_CR_AREA
    RK_CR_ENTER
    taskHandle->prioNominal = priority;
    kTaskUpdateEffectivePrioChain(taskHandle);
    RK_CR_EXIT
}

static VOID SendCases_(VOID)
{
    Expect_(kChannelSend(&channel, request, sizeof(request), RK_NO_WAIT),
             RK_ERR_NOT_OWNER, "unbound sender");
    Expect_(kChannelRecv(&channel, received.payload, sizeof(request), NULL,
                         RK_NO_WAIT), RK_ERR_NOT_OWNER, "unbound receiver");
    Start_(0U, OP_CALL, sizeof(request), RK_WAIT_FOREVER);
    Done_(0U, RK_ERR_INVALID_PARAM);
    Start_(0U, OP_SEND, sizeof(request), RK_NO_WAIT);
    Done_(0U, RK_ERR_NOWAIT);
    Start_(1U, OP_RECV, sizeof(request), RK_NO_WAIT);
    Done_(1U, RK_ERR_BUFFER_EMPTY);

    Start_(0U, OP_SEND, sizeof(request), RK_WAIT_FOREVER);
    Pending_(RK_CHANNEL_QUEUED, RK_FALSE);
    Check_((RK_BOOL)(receiverHandle->priority == senderHandle->priority),
           "queued send donation");
    Expect_(kChannelDestroy(&channel), RK_ERR_CHANNEL_BUSY, "queued destroy");
    Start_(1U, OP_RECV, sizeof(ULONG), RK_NO_WAIT);
    Done_(1U, RK_ERR_INVALID_MSG_SIZE);
    Check_((RK_BOOL)(channel.state == RK_CHANNEL_QUEUED), "oversize retained");
    Start_(1U, OP_RECV, sizeof(request), RK_NO_WAIT);
    Done_(1U, RK_ERR_SUCCESS);
    Done_(0U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)((receivedBytes == sizeof(request)) &&
                     (received.payload[0] == request[0]) &&
                     (received.payload[1] == request[1]) &&
                     (receiverHandle->priority == receiverHandle->prioNominal)),
           "queued copy and priority restore");

    Start_(1U, OP_RECV, sizeof(ULONG), RK_WAIT_FOREVER);
    Pending_(RK_CHANNEL_IDLE, RK_TRUE);
    Expect_(kChannelDestroy(&channel), RK_ERR_CHANNEL_BUSY, "receiver destroy");
    Start_(0U, OP_SEND, sizeof(request), RK_NO_WAIT);
    Done_(0U, RK_ERR_INVALID_MSG_SIZE);
    Check_(channel.receiverWaiting, "oversize kept receiver waiting");
    Start_(0U, OP_SEND, sizeof(ULONG), RK_NO_WAIT);
    Done_(0U, RK_ERR_SUCCESS);
    Done_(1U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)(receivedBytes == sizeof(ULONG)), "direct copy length");

    Start_(0U, OP_SEND, sizeof(request), FINITE_WAIT);
    Done_(0U, RK_ERR_TIMEOUT);
    Check_((RK_BOOL)((channel.state == RK_CHANNEL_IDLE) &&
                     (channel.requestPtr == NULL) &&
                     (senderHandle->waitingChannelPtr == NULL) &&
                     (receiverHandle->priority == receiverHandle->prioNominal)),
           "send timeout cleanup");
    Start_(1U, OP_RECV, sizeof(request), FINITE_WAIT);
    Done_(1U, RK_ERR_TIMEOUT);
    Check_((RK_BOOL)((channel.receiverWaiting == RK_FALSE) &&
                     (channel.data.send.recvPtr == NULL) &&
                     (receiverHandle->waitingChannelPtr == NULL)),
           "receive timeout cleanup");
    Check_((RK_BOOL)((received.before == 0xFEED1234UL) &&
                     (received.after == 0xFEED5678UL)), "buffer guards");
}

static VOID CallCases_(VOID)
{
    Start_(0U, OP_SEND, sizeof(request), RK_NO_WAIT);
    Done_(0U, RK_ERR_INVALID_PARAM);
    Start_(0U, OP_CALL, sizeof(request), RK_NO_WAIT);
    Done_(0U, RK_ERR_INVALID_TIMEOUT);
    Start_(1U, OP_ACCEPT, sizeof(request), RK_NO_WAIT);
    Done_(1U, RK_ERR_BUFFER_EMPTY);
    Start_(1U, OP_ACCEPT, sizeof(request), FINITE_WAIT);
    Done_(1U, RK_ERR_TIMEOUT);

    Start_(0U, OP_CALL, sizeof(request), RK_WAIT_FOREVER);
    Pending_(RK_CHANNEL_QUEUED, RK_FALSE);
    Prio_(senderHandle, 2U);
    Check_((RK_BOOL)(receiverHandle->priority == 2U), "updated caller donation");
    Prio_(senderHandle, 5U);
    Start_(1U, OP_ACCEPT, sizeof(ULONG), RK_NO_WAIT);
    Done_(1U, RK_ERR_INVALID_MSG_SIZE);
    Start_(1U, OP_ACCEPT, sizeof(request), RK_NO_WAIT);
    Done_(1U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)((receivedBytes == sizeof(request)) &&
                     (receiverHandle->priority == 5U)), "accept lowered server");
    stale = accepted;
    Expect_(kChannelReply(&accepted, answer, sizeof(answer)),
             RK_ERR_NOT_OWNER, "unbound reply");
    Start_(1U, OP_ACCEPT_OTHER, sizeof(request), RK_NO_WAIT);
    Done_(1U, RK_ERR_SYNCH_CALL_BUSY);
    Start_(1U, OP_REPLY, sizeof(answer) + 1UL, RK_NO_WAIT);
    Done_(1U, RK_ERR_INVALID_MSG_SIZE);
    Expect_(kChannelDestroy(&channel), RK_ERR_CHANNEL_BUSY, "active destroy");
    Start_(1U, OP_REPLY, sizeof(answer), RK_NO_WAIT);
    Done_(1U, RK_ERR_SUCCESS);
    Done_(0U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)((replyBytes == sizeof(answer)) &&
                     (reply[0] == answer[0]) && (reply[1] == answer[1]) &&
                     (receiverHandle->priority == 3U)), "reply committed");
    Start_(1U, OP_STALE_REPLY, sizeof(answer), RK_NO_WAIT);
    Done_(1U, RK_ERR_SYNCH_CALL_NOT_ACTIVE);

    Start_(1U, OP_ACCEPT, sizeof(request), RK_WAIT_FOREVER);
    Pending_(RK_CHANNEL_IDLE, RK_TRUE);
    Start_(0U, OP_CALL, sizeof(request), RK_WAIT_FOREVER);
    Done_(1U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)(accepted.callId != stale.callId), "new call identity");
    Start_(1U, OP_STALE_REPLY, sizeof(answer), RK_NO_WAIT);
    Done_(1U, RK_ERR_SYNCH_CALL_NOT_ACTIVE);
    Start_(1U, OP_REPLY, 0UL, RK_NO_WAIT);
    Done_(1U, RK_ERR_SUCCESS);
    Done_(0U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)(replyBytes == 0UL), "empty reply");

    Start_(0U, OP_CALL, sizeof(request), FINITE_WAIT);
    Done_(0U, RK_ERR_TIMEOUT);
    Check_((RK_BOOL)((channel.state == RK_CHANNEL_IDLE) &&
                     (channel.data.invocation.replyPtr == NULL) &&
                     (receiverHandle->priority == 3U)), "queued call timeout");

    reply[0] = 0xDEADBEEFUL;
    reply[1] = 0xDEADBEEFUL;
    Start_(1U, OP_ACCEPT, sizeof(request), RK_WAIT_FOREVER);
    Pending_(RK_CHANNEL_IDLE, RK_TRUE);
    Start_(0U, OP_CALL, sizeof(request), FINITE_WAIT);
    Done_(1U, RK_ERR_SUCCESS);
    Done_(0U, RK_ERR_TIMEOUT);
    Check_((RK_BOOL)((channel.state == RK_CHANNEL_ABANDONED) &&
                     (channel.data.invocation.replyPtr == NULL) &&
                     (receiverHandle->priority == 3U)), "active call abandoned");
    Expect_(kChannelDestroy(&channel), RK_ERR_CHANNEL_BUSY, "abandoned destroy");
    Start_(0U, OP_CALL, sizeof(request), RK_WAIT_FOREVER);
    Done_(0U, RK_ERR_CHANNEL_BUSY);
    Start_(1U, OP_REPLY, sizeof(answer), RK_NO_WAIT);
    Done_(1U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)((channel.state == RK_CHANNEL_IDLE) &&
                     (reply[0] == 0xDEADBEEFUL) &&
                     (reply[1] == 0xDEADBEEFUL) && (replyBytes == 0UL)),
           "abandoned reply did not copy");

    /* Rebinding the same object must not revive a previously accepted token. */
    stale = accepted;
    Expect_(kChannelDestroy(&channel), RK_ERR_SUCCESS, "unbind invocation");
    Expect_(kChannelInit(&channel, &senderHandle, &receiverHandle,
                         SYNCH_INVOCATION), RK_ERR_SUCCESS, "rebind invocation");
    Start_(0U, OP_CALL, sizeof(request), RK_WAIT_FOREVER);
    Pending_(RK_CHANNEL_QUEUED, RK_FALSE);
    Start_(1U, OP_ACCEPT, sizeof(request), RK_NO_WAIT);
    Done_(1U, RK_ERR_SUCCESS);
    Start_(1U, OP_STALE_REPLY, sizeof(answer), RK_NO_WAIT);
    Done_(1U, RK_ERR_SYNCH_CALL_NOT_ACTIVE);
    Start_(1U, OP_REPLY, sizeof(answer), RK_NO_WAIT);
    Done_(1U, RK_ERR_SUCCESS);
    Done_(0U, RK_ERR_SUCCESS);
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    SendCases_();
    Expect_(kChannelDestroy(&channel), RK_ERR_SUCCESS, "unbind send");
    Check_((RK_BOOL)((senderHandle->channelList.size == 0UL) &&
                     (receiverHandle->channelList.size == 0UL)), "removed bindings");
    Prio_(receiverHandle, 3U);
    Expect_(kChannelInit(&channel, &senderHandle, &receiverHandle,
                         SYNCH_INVOCATION), RK_ERR_SUCCESS, "invocation bind");
    Expect_(kChannelInit(&otherChannel, &controlHandle, &receiverHandle,
                         SYNCH_INVOCATION), RK_ERR_SUCCESS, "second caller bind");
    CallCases_();
    Expect_(kChannelDestroy(&channel), RK_ERR_SUCCESS, "destroy invocation");
    Expect_(kChannelDestroy(&otherChannel), RK_ERR_SUCCESS, "destroy other");
    Check_((RK_BOOL)((controlHandle->channelList.size == 0UL) &&
                     (senderHandle->channelList.size == 0UL) &&
                     (receiverHandle->channelList.size == 0UL)), "all bindings removed");
#if (RK_CONF_DYNAMIC_TASK == ON)
    RK_TASK_HANDLE dynamicHandle = NULL;
    RK_DYNAMIC_TASK_ATTR attr = {WorkerTask, &workerIds[0], "Dynamic", 10U,
                                 RK_PREEMPT, &stackPool};
    Expect_(kMemPartitionInit(&stackPool, dynamicStack, sizeof(dynamicStack), 1UL),
             RK_ERR_SUCCESS, "dynamic stack pool");
    Expect_(kTaskSpawn(&attr, &dynamicHandle), RK_ERR_SUCCESS, "spawn endpoint");
    Expect_(kChannelInit(&channel, &dynamicHandle, &receiverHandle, SYNCH_SEND),
             RK_ERR_SUCCESS, "dynamic endpoint bind");
    Expect_(kTaskTerminate(&dynamicHandle), RK_ERR_TASK_INVALID_ST,
             "bound endpoint termination");
    Expect_(kChannelDestroy(&channel), RK_ERR_SUCCESS, "dynamic endpoint unbind");
    Expect_(kTaskTerminate(&dynamicHandle), RK_ERR_SUCCESS, "terminate endpoint");
    Check_((RK_BOOL)(dynamicHandle == NULL), "dynamic handle cleared");
#endif
#if (RK_CONF_ASYNCH_MESG == ON)
    Expect_(kMesgEndpointInit(senderHandle), RK_ERR_SUCCESS, "async endpoint init");
    Expect_(kChannelInit(&channel, &senderHandle, &receiverHandle, SYNCH_SEND),
             RK_ERR_HAS_OWNER, "async endpoint conflict");
#endif
    printf("CH PASS channel modes, copy bounds, timeouts and lifecycle\r\n");
    Stop_();
}

VOID WorkerTask(VOID *args)
{
    UINT const id = *(UINT *)args;
    UINT seen = 0U;
    while (1)
    {
        if (work[id].request == seen)
        {
            (void)kSleep(1UL);
            continue;
        }
        seen = work[id].request;
        RK_ERR result = RK_ERR_ERROR;
        switch (work[id].operation)
        {
            case OP_SEND:
                result = kChannelSend(&channel, request, work[id].bytes,
                                       work[id].timeout);
                break;
            case OP_RECV:
                result = kChannelRecv(&channel, received.payload,
                                       work[id].bytes, &receivedBytes,
                                       work[id].timeout);
                break;
            case OP_CALL:
            {
                RK_CHANNEL_ATTR attr = {request, work[id].bytes, reply,
                                         sizeof(reply), &replyBytes};
                result = kChannelCall(&channel, &attr, work[id].timeout);
                break;
            }
            case OP_ACCEPT:
            case OP_ACCEPT_OTHER:
                result = kChannelAccept(
                    (work[id].operation == OP_ACCEPT) ? &channel : &otherChannel,
                    &accepted, received.payload, work[id].bytes,
                    &receivedBytes, work[id].timeout);
                break;
            case OP_REPLY:
            case OP_STALE_REPLY:
                result = kChannelReply(
                    (work[id].operation == OP_REPLY) ? &accepted : &stale,
                    (work[id].bytes == 0UL) ? NULL : answer, work[id].bytes);
                break;
            default:
                break;
        }
        work[id].result = result;
        work[id].done = seen;
    }
}

VOID kApplicationInit(VOID)
{
    Expect_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "Control",
                      controlStack, STACKSIZE, 1U, RK_PREEMPT),
             RK_ERR_SUCCESS, "control init");
    Expect_(kTaskInit(&senderHandle, WorkerTask, &workerIds[0], "Sender",
                      senderStack, STACKSIZE, 5U, RK_PREEMPT),
             RK_ERR_SUCCESS, "sender init");
    Expect_(kTaskInit(&receiverHandle, WorkerTask, &workerIds[1], "Receiver",
                      receiverStack, STACKSIZE, 7U, RK_PREEMPT),
             RK_ERR_SUCCESS, "receiver init");
    Expect_(kChannelInit(&channel, &senderHandle, &receiverHandle,
                         SYNCH_SEND | SYNCH_INVOCATION),
             RK_ERR_INVALID_PARAM, "combined modes");
    Expect_(kChannelInit(&channel, &senderHandle, &senderHandle, SYNCH_SEND),
             RK_ERR_INVALID_PARAM, "self channel");
    Expect_(kChannelInit(&channel, NULL, &receiverHandle, SYNCH_SEND),
             RK_ERR_OBJ_NULL, "null handle");
    Expect_(kChannelInit(&channel, &senderHandle, &receiverHandle, SYNCH_SEND),
             RK_ERR_SUCCESS, "send bind");
    Expect_(kChannelInit(&channel, &senderHandle, &receiverHandle, SYNCH_SEND),
             RK_ERR_OBJ_DOUBLE_INIT, "double init");
    Expect_(kChannelInit(&invalidChannel, &controlHandle, &receiverHandle,
                         SYNCH_INVOCATION), RK_ERR_HAS_OWNER, "receiver mode conflict");
    Expect_(kChannelInit(&invalidChannel, &senderHandle, &controlHandle,
                         SYNCH_INVOCATION), RK_ERR_HAS_OWNER, "sender mode conflict");
    Check_((RK_BOOL)((controlHandle->channelList.size == 0UL) &&
                     (senderHandle->channelList.size == 1UL) &&
                     (receiverHandle->channelList.size == 1UL)), "failed init atomicity");
#if (RK_CONF_ASYNCH_MESG == ON)
    Expect_(kMesgEndpointInit(senderHandle), RK_ERR_HAS_OWNER, "channel endpoint conflict");
#endif
}

int main(void)
{
    kCoreInit();
    kInit();
    return (0);
}
