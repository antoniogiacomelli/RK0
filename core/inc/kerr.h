/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.1
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

#ifndef RK_ERR_H
#define RK_ERR_H
#include <kcommondefs.h>
#ifdef __cplusplus
extern "C" {
#endif
extern volatile RK_FAULT faultID;
struct traceItem 
{
    RK_FAULT code;
    RK_TICK tick;
    INT sp;
    CHAR* task;
    BYTE taskID;
    UINT lr;
}__K_ALIGN(4);

__attribute__((section(".noinit")))
extern volatile struct traceItem traceInfo;
VOID kErrHandler(RK_FAULT);
#ifdef __cplusplus
}
#endif
#endif /* RK_ERR_H*/
