/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.4.1
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

#ifndef APPLICATION_H
#define APPLICATION_H
#include <kapi.h>
/*******************************************************************************
 * TASK STACKS EXTERN DECLARATION
 *******************************************************************************/
/* using this large because printf botches*/
#define STACKSIZE 128 /*you can define each stack with a specific size */
/* this value is in WORDS - make it a multiple of 8 */

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
