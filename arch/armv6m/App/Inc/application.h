#ifndef APPLICATION_H
#define APPLICATION_H
#include <khal.h>
#include <kenv.h>
#include <kapi.h>

/*******************************************************************************
 * TASK STACKS EXTERN DECLARATION
 *******************************************************************************/
#define STACKSIZE 256 /*you can define each stack with a specific
 size*/

extern INT stack1[STACKSIZE];
extern INT stack2[STACKSIZE];
extern INT stack3[STACKSIZE];
extern INT stack4[STACKSIZE];

extern RK_TASK_HANDLE task1Handle;
extern RK_TASK_HANDLE task2Handle;
extern RK_TASK_HANDLE task3Handle;
extern RK_TASK_HANDLE task4Handle;

/*******************************************************************************
 * TASKS ENTRY POINT PROTOTYPES
 *******************************************************************************/


VOID Task1( VOID*);
VOID Task2( VOID*);
VOID Task3( VOID*);
VOID Task4( VOID*);



#endif /* APPLICATION_H */
