#include <application.h>

/* Task handles */
RK_TASK_HANDLE task1Handle;
RK_TASK_HANDLE task2Handle;
RK_TASK_HANDLE task3Handle;

#define RK_SYSTEMCORECLOCK (40000000)
#define RK_CORE_INIT() \
do { \
kCoreSysTickConfig( RK_SYSTEMCORECLOCK/100); \
RK_TICK_DIS \
kCoreSetInterruptPriority( RK_CORE_SVC_IRQN, 0x01); \
kCoreSetInterruptPriority( RK_CORE_SYSTICK_IRQN, 0x02); \
kCoreSetInterruptPriority( RK_CORE_PENDSV_IRQN, 0x02); \
} while(0)


/* Main function - entry point */
int main(void) {



    kCoreSysTickConfig( RK_SYSTEMCORECLOCK/100); 
    RK_TICK_DIS 
    kCoreSetInterruptPriority( RK_CORE_SVC_IRQN, 0x01); 
    kCoreSetInterruptPriority( RK_CORE_SYSTICK_IRQN, 0x02); 
    kCoreSetInterruptPriority( RK_CORE_PENDSV_IRQN, 0x02); 
    
    /* Create tasks */
  
    kCreateTask(&task1Handle, Task1, "Task1", stack1, STACKSIZE, NULL, 1, FALSE);
    kCreateTask(&task2Handle, Task2, "Task2", stack2, STACKSIZE, NULL, 1, FALSE);
    kCreateTask(&task3Handle, Task3, "Task3", stack3, STACKSIZE, NULL, 1, FALSE);
     
    /* Initialize and start the kernel */
    kInit();
    
    while(1)
    {
        asm volatile("bkpt\n\r");
    }
}
