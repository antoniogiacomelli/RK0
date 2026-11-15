/* SPDX-License-Identifier: Apache-2.0 */
/* RK0 - The Embedded Real-Time Kernel '0' */
/* (C) 2026 Antonio Giacomelli <dev@kernel0.org> */

/*
 * One publisher broadcasts HUMIDITY, TEMPERATURE and AIRFLOW using task events.
 * Each subject has an event bit. A task subscribes by selecting its subject mask
 * in subscriptions[] and waiting for those events:
 *   Subscriber 1: HUMIDITY | TEMPERATURE
 *   Subscriber 2: TEMPERATURE | AIRFLOW
 * TEMPERATURE is broadcast to both tasks; the other subjects reach only their
 * interested subscriber. The scheduler stays locked during each fanout so all
 * interested subscribers are notified before any of them can run.
 *
 * Events carry no payload and repeated sets of a bit coalesce. The shared
 * publication therefore remains unchanged until all subscribers to its subject
 * acknowledge it with distinct bits in the publisher's event register. A
 * subject with no subscribers needs no acknowledgements.
 *
 * Build and run:
 * make run PLATFORM=qemu APP=app/examples/publisher_subscriber_using_events.c
 */

#include <kapi.h>
#include <qemu_uart.h>
#include <logger.h>

#define STACKSIZE 256U
#define N_SUBJECTS 3U
#define N_SUBSCRIBERS 2U
#define HUMIDITY RK_EVENT_1
#define TEMPERATURE RK_EVENT_2
#define AIRFLOW RK_EVENT_3
#define SUBSCRIBER1_DONE RK_EVENT_1
#define SUBSCRIBER2_DONE RK_EVENT_2

RK_DECLARE_TASK(publisherHandle, PublisherTask, publisherStack, STACKSIZE)
RK_DECLARE_TASK(subscriber1Handle, SubscriberTask, subscriber1Stack, STACKSIZE)
RK_DECLARE_TASK(subscriber2Handle, SubscriberTask, subscriber2Stack, STACKSIZE)

typedef struct
{
    RK_TASK_EVENT event;
    CHAR const *name;
    ULONG baseValue;
} Subject;

typedef struct
{
    Subject const *subject;
    ULONG sequence;
    ULONG value;
} Publication;

typedef struct
{
    RK_TASK_HANDLE *handlePtr;
    CHAR const *name;
    RK_TASK_EVENT subjects;
    RK_TASK_EVENT doneEvent;
} Subscription;

/* Simulated sensor readings; values vary with the publication sequence. */
static Subject const subjects[N_SUBJECTS] = {
    {HUMIDITY, "HUMIDITY", 50UL},
    {TEMPERATURE, "TEMPERATURE", 20UL},
    {AIRFLOW, "AIRFLOW", 100UL}
};
static volatile Publication publication;
static Subscription subscriptions[N_SUBSCRIBERS] = {
    {&subscriber1Handle, "Subscriber 1", HUMIDITY | TEMPERATURE, SUBSCRIBER1_DONE},
    {&subscriber2Handle, "Subscriber 2", TEMPERATURE | AIRFLOW, SUBSCRIBER2_DONE}
};

static VOID AppCheck_(RK_ERR const err)
{
    if (err != RK_ERR_SUCCESS)
    {
        kPuts("Publisher/subscriber FAIL\r\n");
        while (1)
        {
            kErrHandler(RK_FAULT_APP_CRASH);
        }
    }
}

static VOID Publish_(Subject const *const subject, ULONG const sequence)
{
    publication.subject = subject;
    publication.sequence = sequence;
    publication.value = subject->baseValue + sequence % 10UL;
    logPost("Publisher %s #%lu=%lu", subject->name,
            publication.sequence, publication.value);

    RK_TASK_EVENT doneEvents = 0UL;
    /* Broadcast only to tasks that subscribed to this subject. */
    kSchLock();
    for (UINT i = 0U; i < N_SUBSCRIBERS; i++)
    {
        if ((subscriptions[i].subjects & subject->event) != 0UL)
        {
            doneEvents |= subscriptions[i].doneEvent;
            AppCheck_(kEventSet(*subscriptions[i].handlePtr, subject->event));
        }
    }
    kSchUnlock();

    /* Only notified subscribers must finish before the payload can change. */
    if (doneEvents != 0UL)
    {
        AppCheck_(kEventGet(doneEvents, RK_EVENT_ALL, NULL,
                            RK_WAIT_FOREVER));
    }
}

VOID PublisherTask(VOID *args)
{
    RK_UNUSEARGS
    ULONG sequence = 0UL;

    while (1)
    {
        for (UINT i = 0U; i < N_SUBJECTS; i++)
        {
            Publish_(&subjects[i], ++sequence);
            AppCheck_(kSleep(RK_MS_TO_TICKS(1000UL)));
        }
    }
}

VOID SubscriberTask(VOID *args)
{
    Subscription const *const subscription = (Subscription const *)args;

    while (1)
    {
        AppCheck_(kEventGet(subscription->subjects, RK_EVENT_ANY, NULL,
                            RK_WAIT_FOREVER));
        Publication const received = {
            publication.subject, publication.sequence, publication.value
        };
        logPost("%s %s #%lu=%lu", subscription->name, received.subject->name,
                received.sequence, received.value);
        AppCheck_(kEventSet(publisherHandle, subscription->doneEvent));
    }
}

VOID kApplicationInit(VOID)
{
    AppCheck_(kTaskInit(&publisherHandle, PublisherTask, RK_NO_ARGS, "Publish",
                        publisherStack, STACKSIZE, 2U, RK_PREEMPT));
    AppCheck_(kTaskInit(&subscriber1Handle, SubscriberTask, &subscriptions[0],
                        "Sub1", subscriber1Stack, STACKSIZE, 1U, RK_PREEMPT));
    AppCheck_(kTaskInit(&subscriber2Handle, SubscriberTask, &subscriptions[1],
                        "Sub2", subscriber2Stack, STACKSIZE, 3U, RK_PREEMPT));
    logInit(4U);
}

int main(void)
{
    kCoreInit();
    kInit();

    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}
