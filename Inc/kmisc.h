/******************************************************************************
 *
 *     [[RK0] | [VERSION: 0.4.0]]
 *
 ******************************************************************************
 ******************************************************************************
 *  In this header:
 *                  o Misc utils
 *****************************************************************************/

#ifndef RK_UTILS_H
#define RK_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif
ULONG kStrLen( const CHAR *s);

ADDR kMemCpy( ADDR const destPtr, ADDR const srcPtr, ULONG const size);

ULONG kWordCpy( ADDR destPtr, ADDR const srcPtr, ULONG const sizeInWords);
ADDR kMemSet( ADDR const destPtr, ULONG const val, ULONG   size);

/* Misc: printf */

//#define RK_CONF_PRINTF
#ifdef RK_CONF_PRINTF
extern UART_HandleTypeDef huart2;
#define kprintf(...) \
            do \
            { \
                __disable_irq();\
                printf(__VA_ARGS__); \
                __enable_irq(); \
            } while(0U)

#else
#define kprintf(...) (void)0
#endif

#ifdef __cplusplus
 }
#endif

#endif
