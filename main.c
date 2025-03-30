#include "application.h"

int main(void)
{
    /* set systick to 1 ms */
    kCoreSysTickConfig( RK_TICK_1MS);
    RK_TICK_DIS /* disable tick, it is reenabled when the first _system_ task runs */
    /* set interrupt priorities - svc highest priority, pendsv lowest */
    kCoreSetInterruptPriority( RK_CORE_SVC_IRQN, 0x07);
    kCoreSetInterruptPriority( RK_CORE_SYSTICK_IRQN, 0x08);
    kCoreSetInterruptPriority( RK_CORE_PENDSV_IRQN, 0x0A);
    /* Create tasks */
    /* params: task handle, task name, stack addr, stack size, input args, priority, run-to-completion */
    /* task handles and stacks are extern declared in application.h and defined in application.c */
    kCreateTask(&task1Handle, Task1, "Task1", stack1, STACKSIZE, NULL, 1, RK_FALSE);
    kCreateTask(&task2Handle, Task2, "Task2", stack2, STACKSIZE, NULL, 1, RK_FALSE);
    kCreateTask(&task3Handle, Task3, "Task3", stack3, STACKSIZE, NULL, 1, RK_FALSE);
    kInit();
    while(1)
    {   /* not to be here */
        asm volatile("BKPT");
    }
 
}
