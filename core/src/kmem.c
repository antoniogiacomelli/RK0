/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.4.0
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

/******************************************************************************
 * 	Module           : PARTITION MEMORY 
 * 	Provides to      : INTER-TASK COMMUNICATION, APPLICATION
 * 	Depends on       : N/A
 *  Public API       : YES
 *****************************************************************************/

#define RK_CODE
#include <kservices.h>

RK_ERR kMemInit( RK_MEM *const kobj, VOID *memPoolPtr, ULONG blkSize,
		ULONG const numBlocks)
{
	RK_CR_AREA

	RK_CR_ENTER

	if (kobj == NULL)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	/* rounds up to next multiple of 4*/
	blkSize = (blkSize + 0x03) & (ULONG) (~0x03);

	/* initialise freelist of blocks */

	ULONG *blockPtr = (ULONG*) memPoolPtr;
	VOID **nextAddrPtr = (VOID **) memPoolPtr; /* next block address */

	for (ULONG i = 0; i < numBlocks - 1; i++)
	{
		ULONG wordSize = blkSize / 4;
		blockPtr += wordSize;
		/* save blockPtr addr as the next */
		*nextAddrPtr = (VOID *) blockPtr;
		/* update  */
		nextAddrPtr = (VOID **) (blockPtr);

	}
	*nextAddrPtr = NULL;

	/* init the control block */
	kobj->blkSize = blkSize;
	kobj->nMaxBlocks = numBlocks;
	kobj->nFreeBlocks = numBlocks;
	kobj->freeListPtr = memPoolPtr;
	kobj->poolPtr = memPoolPtr;
	kobj->init = TRUE;
	RK_CR_EXIT
	return (RK_SUCCESS);
}

VOID *kMemAlloc( RK_MEM *const kobj)
{

	if (kobj->nFreeBlocks == 0)
	{
		return (NULL); /* there is no available memory partition */
	}
	RK_CR_AREA

	RK_CR_ENTER
	VOID *allocPtr = kobj->freeListPtr;
	kobj->freeListPtr = *(VOID **) allocPtr;
#if(MEMBLKLAST)
    kobj->lastUsed = allocPtr;
    #endif
	if (allocPtr != NULL)
		kobj->nFreeBlocks -= 1;
	RK_CR_EXIT
	return (allocPtr);
}

RK_ERR kMemFree( RK_MEM *const kobj, VOID *blockPtr)
{

	if (kobj->nFreeBlocks == kobj->nMaxBlocks)
	{
		return (RK_ERR_MEM_FREE);
	}
	if (kobj == NULL || blockPtr == NULL)
	{
		return (RK_ERR_OBJ_NULL);
	}
	RK_CR_AREA
	RK_CR_ENTER
	*(VOID **) blockPtr = kobj->freeListPtr;
	kobj->freeListPtr = blockPtr;
	kobj->nFreeBlocks += 1;
	RK_CR_EXIT
	return (RK_SUCCESS);
}

