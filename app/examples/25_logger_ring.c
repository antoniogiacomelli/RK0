/* SPDX-License-Identifier: Apache-2.0 */
/* Public logger regression: no message-queue or direct-message services. */
#include <kapi.h>
#include <logger.h>
#include <stdio.h>
#include <string.h>

#define STACKSIZE 256U
#define SYNC_WAIT 1000UL
#define REQUEST RK_EVENT_1
#define DONE(ID) ((RK_TASK_EVENT)(RK_EVENT_2 << (ID)))
#define ROUNDS 4U
#define WAKE_ROUNDS 8U

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)

static VOID Check_(RK_BOOL const ok, CHAR const *const where)
{
    if (ok == RK_FALSE)
    {
        printf("LR FAIL %s\r\n", where);
        while (1) { }
    }
}

#if (CONF_LOGGER == 1)
RK_DECLARE_TASK(actor1Handle, ActorTask, actor1Stack, STACKSIZE)
RK_DECLARE_TASK(actor2Handle, ActorTask, actor2Stack, STACKSIZE)
static UINT ids[2] = {0U, 1U};
static volatile UINT batch;
static volatile UINT perActor;
static volatile RK_BOOL highPriority;
static volatile RK_BOOL irqDrop;
static volatile RK_BOOL irqDone;
static volatile RK_BOOL irqWasISR;
static RK_TASK_HANDLE loggerHandle;

#if defined(QEMU_MACHINE_LM3S6965EVB)
#define TEST_IRQ_MASK 1UL
VOID GPIO_Handler(VOID)
#elif defined(QEMU_MACHINE_MICROBIT)
#define TEST_IRQ_MASK (1UL << 2)
VOID UART0_Handler(VOID)
#else
#error "This regression requires a supported QEMU target"
#endif
{
    irqWasISR = (kIsISR() != 0U) ? RK_TRUE : RK_FALSE;
    if (irqDrop == RK_TRUE)
        logPost("LRDROP IRQ %u", batch);
    else
        logPost("LRORDER 1");
    irqDone = RK_TRUE;
}

static VOID TriggerIRQ_(VOID)
{
    irqDone = RK_FALSE;
    irqWasISR = RK_FALSE;
    *(volatile ULONG *)0xE000E100UL = TEST_IRQ_MASK;
    *(volatile ULONG *)0xE000E200UL = TEST_IRQ_MASK;
    RK_DSB
    RK_ISB
    for (ULONG spin = 0UL; (irqDone == RK_FALSE) && (spin < 1000000UL); spin++)
        RK_NOP
    *(volatile ULONG *)0xE000E180UL = TEST_IRQ_MASK;
    Check_((RK_BOOL)((irqDone == RK_TRUE) && (irqWasISR == RK_TRUE)),
            "real logger producer interrupt");
}

static VOID WaitIdle_(VOID)
{
    for (RK_TICK tick = 0UL; tick < SYNC_WAIT; tick++)
    {
        if (loggerHandle->status == RK_SLEEPING_EV_FLAG)
            return;
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "logger drained and blocked on its event");
}

static VOID RunActors_(VOID)
{
    Check_((RK_BOOL)(kEventSet(actor1Handle, REQUEST) == RK_ERR_SUCCESS),
            "start actor 1");
    Check_((RK_BOOL)(kEventSet(actor2Handle, REQUEST) == RK_ERR_SUCCESS),
            "start actor 2");
    Check_((RK_BOOL)(kEventGet(DONE(0U) | DONE(1U), RK_EVENT_ALL, NULL,
                              SYNC_WAIT) == RK_ERR_SUCCESS), "actor completion");
    WaitIdle_();
}

VOID ActorTask(VOID *args)
{
    UINT const id = *(UINT *)args;
    while (1)
    {
        Check_((RK_BOOL)(kEventGet(REQUEST, RK_EVENT_ANY, NULL,
                                  RK_WAIT_FOREVER) == RK_ERR_SUCCESS), "actor request");
        for (UINT i = 0U; i < perActor; i++)
        {
            if (highPriority == RK_TRUE)
                logPost("LRHIGH %u %u", id, i);
            else
                logPost("LRMULTI %u %u %u", batch, id, i);
        }
        Check_((RK_BOOL)(kEventSet(controlHandle, DONE(id)) == RK_ERR_SUCCESS),
                "actor done");
    }
}
#endif

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
#if (CONF_LOGGER == 1)
    for (UINT i = 0U; i < RK_NTHREADS; i++)
        if ((RK_gTcbs[i].init == RK_TRUE) &&
            (strcmp(RK_gTcbs[i].taskName, "LogTsk") == 0))
            loggerHandle = &RK_gTcbs[i];
    Check_((RK_BOOL)(loggerHandle != NULL), "dedicated logger task exists");
    WaitIdle_();

    /* Logger priority 8 cannot run until the priority-1 producer yields.
     * Exactly one pool's worth must survive each burst; both overflows drop.
     * Repeating the burst verifies pool return and circular-buffer wraparound.
     */
    for (batch = 0U; batch < ROUNDS; batch++)
    {
        for (UINT i = 0U; i < LOGPOOLSIZ; i++)
            logPost("LRREC %u %u", batch, i);
        logPost("LRDROP task %u", batch);
        irqDrop = RK_TRUE;
        TriggerIRQ_();
        WaitIdle_();
    }
    perActor = LOGPOOLSIZ / 2U;
    highPriority = RK_FALSE;
    for (batch = 0U; batch < ROUNDS; batch++)
        RunActors_();

    irqDrop = RK_FALSE;
    logPost("LRORDER 0");
    TriggerIRQ_();
    logPost("LRORDER 2");
    WaitIdle_();
    for (UINT i = 0U; i < WAKE_ROUNDS; i++)
    {
        logPost("LRWAKE %u", i);
        WaitIdle_();
    }

    /* A higher-priority consumer can print/free a record before logPost()
     * returns. Neither producer may access its published pointer afterward.
     */
    RK_CR_AREA
    RK_CR_ENTER
    loggerHandle->prioNominal = 2U;
    kTaskUpdateEffectivePrioChain(loggerHandle);
    RK_CR_EXIT
    perActor = 2U * LOGPOOLSIZ;
    highPriority = RK_TRUE;
    RunActors_();

    CHAR text[LOGLEN + 16U];
    for (UINT i = 0U; i < sizeof(text) - 1U; i++)
        text[i] = 'X';
    text[sizeof(text) - 1U] = '\0';
    logPost("LRTRUNC:%s", text);
    WaitIdle_();
    printf("LR PASS pointer ring, pool reuse, task/ISR producers and wakeups\r\n");
#else
    logPost("LROFF normal");
    logError("LROFF fault");
    printf("LR PASS disabled logger\r\n");
#endif
    while (1)
        (void)kSleep(1000UL);
}

VOID kApplicationInit(VOID)
{
    Check_((RK_BOOL)(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "LRctrl",
                              controlStack, STACKSIZE, 1U, RK_PREEMPT) ==
                      RK_ERR_SUCCESS), "control init");
    logInit(8U);
#if (CONF_LOGGER == 1)
    Check_((RK_BOOL)(kTaskInit(&actor1Handle, ActorTask, &ids[0], "LRact1",
                              actor1Stack, STACKSIZE, 3U, RK_PREEMPT) ==
                      RK_ERR_SUCCESS), "actor1 init");
    Check_((RK_BOOL)(kTaskInit(&actor2Handle, ActorTask, &ids[1], "LRact2",
                              actor2Stack, STACKSIZE, 4U, RK_PREEMPT) ==
                      RK_ERR_SUCCESS), "actor2 init");
#endif
}

int main(void)
{
    kCoreInit();
    kInit();
    return (0);
}
