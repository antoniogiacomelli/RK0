#include <application.h>

#define RK_SYSTEMCORECLOCK (5000000)
#define RK_CORE_INIT() \
do { \
kCoreSysTickConfig( RK_SYSTEMCORECLOCK/100); \
RK_TICK_DIS \
kCoreSetInterruptPriority( RK_CORE_SVC_IRQN, 0x07); \
kCoreSetInterruptPriority( RK_CORE_SYSTICK_IRQN, 0x08); \
kCoreSetInterruptPriority( RK_CORE_PENDSV_IRQN, 0x0A); \
} while(0)

int main(void)
{
    
    /* initialise systick and needed core interrupts */
    RK_CORE_INIT();
    
    /* Create tasks */
    /* task handles and stacks are extern declared in application.h and defined in application.c */
    /* params: task handle, task name, stack addr, stack size, input args, priority, run-to-completion */

    kCreateTask(&task1Handle, Task1, "Task1", stack1, STACKSIZE, NULL, 1, FALSE);
    kCreateTask(&task2Handle, Task2, "Task2", stack2, STACKSIZE, NULL, 1, FALSE);
    kCreateTask(&task3Handle, Task3, "Task3", stack3, STACKSIZE, NULL, 1, FALSE);
    kInit();
    while(1)
    {   /* not to be here */
        asm volatile("BKPT");
    }
 
}
