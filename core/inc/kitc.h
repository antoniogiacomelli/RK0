/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
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
/***
 * About DMB usage on ITC
 * 
 * Orders memory transactions (LDR/STR) so that anything before 
 * the DMB is visible before anything after it is issued. 
 * It does not stall the processor until the transfers complete, 
 * it only prevents reordering.
 * 
 * ARMv7-M in-order alone does *not* guarantee bus ordering
 * in every possible use-case (e.g., DMA, multi-master)
 * 
 * Design Choice:
 * - Acquire‐fence (after locking/pend/availability test): 
 *   prevent LOADs and subsequent STOREs to go ahead the check.
 * 
 * - Release‐fence (before unlock/post/signalling availability): 
 *   no consumer will see the availability until all data writes
 *   have reached the buss
 * 
 * (Prone to further inspection.)
 ***/

#ifndef RK_ITC_H
#define RK_ITC_H

#ifdef __cplusplus
extern "C" {
#endif
/* Note the header expose only the API needed by others kernel modules,
in this case, the System Task TimerHandlerTask uses Signals to be notified.
*/
RK_ERR kSignalGet( ULONG const, UINT const,  ULONG *const, RK_TICK const);
RK_ERR kSignalSet( RK_TASK_HANDLE const, ULONG const);

#ifdef __cplusplus
}
#endif
#endif /* RK_ITC_H */
