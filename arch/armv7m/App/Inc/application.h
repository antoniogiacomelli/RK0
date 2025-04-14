#ifndef APPLICATION_H
#define APPLICATION_H
#include <khal.h>
#include <kenv.h>
#include <kapi.h>
/*******************************************************************************
 * TASK STACKS EXTERN DECLARATION
 *******************************************************************************/
#define STACKSIZE 256 /*you can define each stack with a specific
 size - this value is in WORDS - make it a multiple of 8 */

extern INT stack1[STACKSIZE];
extern INT stack2[STACKSIZE];
extern INT stack3[STACKSIZE];
 
extern RK_TASK_HANDLE task1Handle;
extern RK_TASK_HANDLE task2Handle;
extern RK_TASK_HANDLE task3Handle;
 

/*******************************************************************************
 * TASKS ENTRY POINT PROTOTYPES
 *******************************************************************************/


VOID Task1( VOID*);
VOID Task2( VOID*);
VOID Task3( VOID*);
 


#endif /* APPLICATION_H */
