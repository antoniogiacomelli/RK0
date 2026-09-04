/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.73.1                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#include <kapi.h>
/* Configure the application logger facility here */
#include <logger.h>
#include <qemu_uart.h>
#include <stdio.h>

int main(void)
{
    /*
     * Boot sequence:
     * 1. kCoreInit() starts the CPU/board layer.
     * 2. kInit() starts RK0 and calls kApplicationInit().
     */
    kCoreInit();

    kInit();

    while (1)
    {
        /* Returning from kInit() is treated as an application fault. */
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}


#define STACKSIZ 256U

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZ)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZ)

typedef struct
{
    VOID *mesgPtr;
    RK_SEMAPHORE full;
    RK_SEMAPHORE empty;
} Mbox_t;

static Mbox_t mailBox;
static ULONG mailContent;

static VOID mboxInit(Mbox_t *const mboxPtr)
{
    RK_ERR err;

    mboxPtr->mesgPtr = NULL;

    err = kSemaBinInit(&mboxPtr->full, 0U);
    K_ERR_CHECK(err);
    err = kSemaBinInit(&mboxPtr->empty, 1U);
    K_ERR_CHECK(err);
}

VOID kApplicationInit(VOID)
{
    RK_ERR err = -1;

    mboxInit(&mailBox);

    err = kTaskInit(&task1Handle, Task1, RK_NO_ARGS, "Tsk1", stack1,
                    STACKSIZ, 2U, RK_PREEMPT);
    K_ERR_CHECK(err);

    err = kTaskInit(&task2Handle, Task2, RK_NO_ARGS, "Tsk2", stack2,
                    STACKSIZ, 1U, RK_PREEMPT);
    K_ERR_CHECK(err);
}

static RK_ERR postMbox(Mbox_t *const mboxPtr, VOID *const mesgPtr,
                       RK_TICK const timeout)
{
    if ((mboxPtr == NULL) || (mesgPtr == NULL))
    {
        return (RK_ERR_OBJ_NULL);
    }

    RK_ERR err = kP(&mboxPtr->empty, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    mboxPtr->mesgPtr = mesgPtr;
    RK_NOP
    err = kV(&mboxPtr->full);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    return (RK_ERR_SUCCESS);
}

static RK_ERR pendMbox(Mbox_t *const mboxPtr, VOID **const mesgPtrPtr,
                       RK_TICK const timeout)
{
    if ((mboxPtr == NULL) || (mesgPtrPtr == NULL))
    {
        return (RK_ERR_OBJ_NULL);
    }

    RK_ERR err = kP(&mboxPtr->full, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    *mesgPtrPtr = mboxPtr->mesgPtr;
    mboxPtr->mesgPtr = NULL;
    RK_NOP
    err = kV(&mboxPtr->empty);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    return (RK_ERR_SUCCESS);
}

VOID Task1(VOID *args)
{
    RK_UNUSEARGS

    mailContent = 0UL;

    while (1)
    {
        mailContent++;

        RK_ERR const err = postMbox(&mailBox, &mailContent, RK_WAIT_FOREVER);
        K_ERR_CHECK(err);
        kPuts("Task1 posted\r\n");

        kSleep(1);
    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        VOID *msgPtr = NULL;
        RK_ERR const err = pendMbox(&mailBox, &msgPtr, RK_WAIT_FOREVER);
        K_ERR_CHECK(err);

        ULONG const readMesg = *(ULONG const *)msgPtr;
        K_ASSERT(readMesg != 0UL);
        printf("Task2 received ULONG: %lu\r\n", readMesg);
        kBusyDelay(5);

        kSleep(1);
    }
}
