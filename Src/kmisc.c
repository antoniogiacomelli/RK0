/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv7m
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 *
 *
 ******************************************************************************/
/******************************************************************************
 *  Module           : MISC
 *  Depends on       : SYSTEM
 *  Provides to      : APPLICATION
 *  Public API       : YES
 ******************************************************************************/



#define RK_CODE
#include "kexecutive.h"

ULONG kStrLen( CHAR* s)
{
	ULONG len = 0;
	while (*s != '\0')
	{
		s++;
		len++;
	}
	return (len);
}

ULONG kMemCpy( ADDR destPtr, ADDR const srcPtr, ULONG size)
{
	if ((RK_IS_NULL_PTR( destPtr)) || (RK_IS_NULL_PTR( srcPtr)))
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
	}
	ULONG n = 0;
	BYTE *destTempPtr = (BYTE*) destPtr;
	BYTE const *srcTempPtr = (BYTE const*) srcPtr;
	for (ULONG i = 0; i < size; ++i)
	{
		destTempPtr[i] = srcTempPtr[i];
		n++;
	}
	return (n);
}

ULONG kWordCpy( ADDR destPtr, ADDR const srcPtr, ULONG sizeInWords)
{
    if ((RK_IS_NULL_PTR( destPtr)) || (RK_IS_NULL_PTR( srcPtr)))
    {
        kErrHandler( RK_FAULT_OBJ_NULL);
    }
    ULONG n = 0;
    ULONG *destTempPtr = (ULONG*) destPtr;
    ULONG const *srcTempPtr = (ULONG const*) srcPtr;
    for (ULONG i = 0; i < sizeInWords; ++i)
    {
        destTempPtr[i] = srcTempPtr[i];
        n++;
    }
    return (n);
}
#ifdef RK_CONF_PRINTF
/*****************************************************************************
 * the glamorous blocking printf
 * deceiving and botching for the good
 * since 1902
 *****************************************************************************/
extern UART_HandleTypeDef huart2;

int _write( int file, char *ptr, int len)
{
	(VOID) file;
	int ret = len;
	while (len)
	{
		while (!(huart2.Instance->SR & UART_FLAG_TXE))
			;
		huart2.Instance->DR = (char) (*ptr) & 0xFF;
		while (!(huart2.Instance->SR & UART_FLAG_TC))
			;
		len--;
		ptr++;
	}
	return (ret);
}
#endif
