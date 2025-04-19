/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv6/7m
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 *
 * Header: ERROR/FAULT HANDLER
 * 
 ******************************************************************************/

#ifndef RK_ERR_H
#define RK_ERR_H
#ifdef __cplusplus
extern "C" {
#endif
extern volatile RK_FAULT faultID;
VOID kErrHandler(RK_FAULT);
#ifdef __cplusplus
}
#endif
#endif /* RK_ERR_H*/
