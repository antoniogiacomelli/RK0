/* SPDX-License-Identifier: Apache-2.0 */
/* External messaging storage, binding, ownership, tracing, and task reuse. */
#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 192U

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(plainHandle, PlainTask, plainStack, STACKSIZE)
RK_DECLARE_MESG_POOL(messagePool, messageStorage, ULONG, 1U)

static RK_MESG_ENDPOINT endpoint;
static RK_MESG_ENDPOINT otherEndpoint;
#if (RK_CONF_DYNAMIC_TASK == ON)
static RK_MESG_ENDPOINT dynamicEndpoint;
static RK_MEM_PARTITION stackPool;
static RK_STACK dynamicStack[STACKSIZE] K_ALIGN(8);
static volatile RK_BOOL sendNow;
static volatile RK_BOOL sent;
static volatile RK_ERR sendResult;
static VOID DynamicTask(VOID *args);
#endif

static VOID Stop_(VOID)
{
    while (1)
    {
        (VOID)kSleep(RK_MS_TO_TICKS(1000UL));
    }
}

static VOID Check_(RK_BOOL const ok, CHAR const *const wherePtr)
{
    if (ok == RK_FALSE)
    {
        printf("ME FAIL %s\r\n", wherePtr);
        Stop_();
    }
}

static VOID Expect_(RK_ERR const actual, RK_ERR const expected,
                     CHAR const *const wherePtr)
{
    if (actual != expected)
    {
        printf("ME FAIL %s expected=%d actual=%d\r\n", wherePtr,
               expected, actual);
        Stop_();
    }
}

int main(void)
{
    kCoreInit();
    kInit();
    return (0);
}

VOID kApplicationInit(VOID)
{
    Expect_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "MEctrl",
                      controlStack, STACKSIZE, 2U, RK_PREEMPT),
             RK_ERR_SUCCESS, "controller init");
    Expect_(kTaskInit(&plainHandle, PlainTask, RK_NO_ARGS, "MEplain",
                      plainStack, STACKSIZE, 10U, RK_PREEMPT),
             RK_ERR_SUCCESS, "plain task init");
    Expect_(kMesgPoolInit(&messagePool, messageStorage, sizeof(ULONG), 1UL,
                          RK_MESG_PRIO_CEILING_NONE),
             RK_ERR_SUCCESS, "pool init");
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    RK_MESG *messagePtr = NULL;
    RK_MESG *receivedPtr = NULL;
    Check_((RK_BOOL)((controlHandle->mesgEndpointPtr == NULL) &&
                     (plainHandle->mesgEndpointPtr == NULL)), "optional storage");
    Expect_(kMesgAlloc(&messagePool, &messagePtr, RK_NO_WAIT),
             RK_ERR_OBJ_NOT_INIT, "allocation requires endpoint");
    Check_((RK_BOOL)((messagePtr == NULL) && (messagePool.nFreeBlocks == 1UL)),
            "failed allocation preserves pool");
    Expect_(kMesgEndpointInit(&endpoint, controlHandle, 0U),
             RK_ERR_INVALID_PARAM, "invalid mode");
    Expect_(kMesgEndpointInit(&endpoint, controlHandle, RK_MESG_SEND_RECV),
             RK_ERR_SUCCESS, "endpoint init");
    Expect_(kMesgEndpointInit(&endpoint, plainHandle, RK_MESG_SEND_RECV),
             RK_ERR_OBJ_DOUBLE_INIT, "one task per object");
    Expect_(kMesgEndpointInit(&otherEndpoint, controlHandle, RK_MESG_SEND_RECV),
             RK_ERR_OBJ_DOUBLE_INIT, "one object per task");
    Check_((RK_BOOL)((plainHandle->mesgEndpointPtr == NULL) &&
                     (otherEndpoint.init == RK_FALSE) &&
                     (endpoint.task == controlHandle)), "binding atomicity");
    kTraceNameObject(&endpoint, "Inbox");

    Expect_(kMesgAlloc(&messagePool, &messagePtr, RK_NO_WAIT),
             RK_ERR_SUCCESS, "allocate");
    Expect_(kMesgEndpointDestroy(&endpoint), RK_ERR_HAS_OWNER,
             "owned message prevents destruction");
    Expect_(kMesgSend(plainHandle, messagePtr), RK_ERR_OBJ_NOT_INIT,
             "destination needs receive endpoint");
    Check_((RK_BOOL)((messagePtr->owner == controlHandle) &&
                     (endpoint.ownedMesgList.size == 1UL)), "failed send ownership");
    Expect_(kMesgSend(controlHandle, messagePtr), RK_ERR_SUCCESS, "queued send");
    Expect_(kMesgEndpointDestroy(&endpoint), RK_ERR_HAS_OWNER,
             "queued message prevents destruction");
    Expect_(kMesgWait(plainHandle, &receivedPtr, RK_NO_WAIT), RK_ERR_BUFFER_EMPTY,
             "sender filter");
    Check_((RK_BOOL)(endpoint.mesgQueue.size == 1UL), "filter retains message");
#if (RK_CONF_TRACE == ON)
    static RK_TRACE_OBJECT_INFO info[RK_CONF_TRACE_MAX_OBJECTS];
    UINT const count = kTraceMesgSnapshot(info, RK_CONF_TRACE_MAX_OBJECTS);
    RK_BOOL found = RK_FALSE;
    for (UINT i = 0U; i < count; i++)
    {
        if (info[i].objPtr == &endpoint)
        {
            Check_((RK_BOOL)((info[i].objID == RK_MESG_ENDPOINT_KOBJ_ID) &&
                             (info[i].ownerPtr == controlHandle) &&
                             (info[i].buffered == 1UL) &&
                             (info[i].objName[0] == 'I')), "endpoint snapshot");
            found = RK_TRUE;
        }
    }
    Check_(found, "endpoint registered for tracing");
#endif
    Expect_(kMesgWait(RK_ANY_TASK, &receivedPtr, RK_NO_WAIT), RK_ERR_SUCCESS,
             "receive queued message");
    Check_((RK_BOOL)((receivedPtr == messagePtr) &&
                     (messagePtr->state == RK_MESG_STATE_RECEIVED) &&
                     (kMesgGetSenderHandle(messagePtr) == controlHandle)),
            "received ownership and identity");
    Expect_(kMesgFree(receivedPtr), RK_ERR_SUCCESS, "free received message");
    Expect_(kMesgEndpointDestroy(&endpoint), RK_ERR_SUCCESS, "idle destroy");
    Check_((RK_BOOL)((controlHandle->mesgEndpointPtr == NULL) &&
                     (endpoint.task == NULL) && (endpoint.init == RK_FALSE)),
            "detached endpoint");
    Expect_(kMesgEndpointDestroy(&endpoint), RK_ERR_OBJ_NOT_INIT,
             "double destroy");

    Expect_(kMesgEndpointInit(&endpoint, controlHandle, RK_MESG_SEND_ONLY),
             RK_ERR_SUCCESS, "send-only reuse");
    Expect_(kMesgAlloc(&messagePool, &messagePtr, RK_NO_WAIT), RK_ERR_SUCCESS,
             "send-only allocation");
    Expect_(kMesgSend(controlHandle, messagePtr), RK_ERR_OBJ_NOT_INIT,
             "send-only rejects incoming delivery");
    Expect_(kMesgWait(RK_ANY_TASK, &receivedPtr, RK_NO_WAIT), RK_ERR_OBJ_NOT_INIT,
             "send-only rejects receive");
    Expect_(kMesgFree(messagePtr), RK_ERR_SUCCESS, "send-only free");
    Expect_(kMesgEndpointDestroy(&endpoint), RK_ERR_SUCCESS, "send-only destroy");

#if (RK_CONF_DYNAMIC_TASK == ON)
    RK_TASK_HANDLE dynamicHandle = NULL;
    RK_DYNAMIC_TASK_ATTR attr = {DynamicTask, RK_NO_ARGS, "MEdyn", 12U,
                                 RK_PREEMPT, &stackPool};
    Expect_(kMesgEndpointInit(&endpoint, controlHandle, RK_MESG_SEND_RECV),
             RK_ERR_SUCCESS, "controller receive rebind");
    Expect_(kMemPartitionInit(&stackPool, dynamicStack, sizeof(dynamicStack), 1UL),
             RK_ERR_SUCCESS, "dynamic stack pool");
    Expect_(kTaskSpawn(&attr, &dynamicHandle), RK_ERR_SUCCESS, "spawn sender");
    Expect_(kMesgEndpointInit(&dynamicEndpoint, dynamicHandle, RK_MESG_SEND_ONLY),
             RK_ERR_SUCCESS, "dynamic endpoint bind");
    sendNow = RK_TRUE;
    for (UINT i = 0U; (sent == RK_FALSE) && (i < 100U); i++)
    {
        (VOID)kSleep(1UL);
    }
    Check_(sent, "dynamic sender completed");
    Expect_(sendResult, RK_ERR_SUCCESS, "dynamic send");
    Expect_(kTaskTerminate(&dynamicHandle), RK_ERR_TASK_INVALID_ST,
             "queued sender reference prevents termination");
    Expect_(kMesgWait(dynamicHandle, &receivedPtr, RK_NO_WAIT), RK_ERR_SUCCESS,
             "receive from dynamic sender");
    Expect_(kMesgFree(receivedPtr), RK_ERR_SUCCESS, "free dynamic message");
    Expect_(kTaskTerminate(&dynamicHandle), RK_ERR_SUCCESS, "terminate sender");
    Check_((RK_BOOL)((dynamicHandle == NULL) &&
                     (dynamicEndpoint.task == NULL) &&
                     (dynamicEndpoint.init == RK_FALSE)), "termination detaches storage");
    sendNow = RK_FALSE;
    Expect_(kTaskSpawn(&attr, &dynamicHandle), RK_ERR_SUCCESS, "reuse task slot");
    Check_((RK_BOOL)(dynamicHandle->mesgEndpointPtr == NULL), "reused slot has no endpoint");
    Expect_(kMesgEndpointInit(&dynamicEndpoint, dynamicHandle, RK_MESG_SEND_ONLY),
             RK_ERR_SUCCESS, "reuse endpoint storage");
    Expect_(kTaskTerminate(&dynamicHandle), RK_ERR_SUCCESS, "terminate reused task");
    Expect_(kMesgEndpointDestroy(&endpoint), RK_ERR_SUCCESS, "final detach");
#endif
    printf("ME PASS external message endpoint lifecycle\r\n");
    Stop_();
}

VOID PlainTask(VOID *args)
{
    RK_UNUSEARGS
    Stop_();
}

#if (RK_CONF_DYNAMIC_TASK == ON)
static VOID DynamicTask(VOID *args)
{
    RK_UNUSEARGS
    while (sendNow == RK_FALSE)
    {
        (VOID)kSleep(1UL);
    }
    RK_MESG *messagePtr = NULL;
    sendResult = kMesgAlloc(&messagePool, &messagePtr, RK_NO_WAIT);
    if (sendResult == RK_ERR_SUCCESS)
    {
        sendResult = kMesgSend(controlHandle, messagePtr);
    }
    sent = RK_TRUE;
    Stop_();
}
#endif
