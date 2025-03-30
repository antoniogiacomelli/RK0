#include "application.h"

int main(void)
{
    RK_CORE_SETUP(RK_TICK_1MS);
    /* Create tasks */
    /* params: task handle, task name, stack addr, stack size, input args, priority, run-to-completion */
    /* task handles and stacks are extern declared in application.h and defined in application.c */
    kCreateTask(&task1Handle, Task1, "Task1", stack1, STACKSIZE, NULL, 1, FALSE);
    kCreateTask(&task2Handle, Task2, "Task2", stack2, STACKSIZE, NULL, 1, FALSE);
    kCreateTask(&task3Handle, Task3, "Task3", stack3, STACKSIZE, NULL, 1, FALSE);
    kInit();
    while(1)
    {   /* not to be here */
        asm volatile("BKPT");
    }
 
}
