/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RK_SEMA_PRIVATE_H
#define RK_SEMA_PRIVATE_H

#include <ksema.h>
#include <ktrace.h>

#if (RK_CONF_SEMAPHORE == ON)

#ifndef K_SEMA_IS_BINARY
#define K_SEMA_IS_BINARY(kobj) ((kobj)->maxValue == 1U)
#endif

#endif /* RK_CONF_SEMAPHORE */

#endif /* RK_SEMA_PRIVATE_H */
