#include "application.h"

int main(void)
{
    
    /* initialise systick and needed core interrupts */
    RK_CORE_INIT();
    
    /* Create tasks */
    /* task handles and stacks are extern declared in application.h and defined in application.c */
    /* params: task handle, task name, stack addr, stack size, input args, priority, run-to-completion */

    kCreateTask(&task1Handle, Task1, "Task1", stack1, STACKSIZE, NULL, 2, FALSE);
    kCreateTask(&task2Handle, Task2, "Task2", stack2, STACKSIZE, NULL, 1, FALSE);
    kCreateTask(&task3Handle, Task3, "Task3", stack3, STACKSIZE, NULL, 3, FALSE);
    kInit();
    while(1)
    {   /* not to be here */
        asm volatile("BKPT");
    }
 
}
