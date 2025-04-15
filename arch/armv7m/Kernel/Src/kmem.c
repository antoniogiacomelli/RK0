/******************************************************************************
 *
 *     [[RK0] | [VERSION: 0.4.0]]
 *
 ******************************************************************************
 ******************************************************************************
 * 	Module           : Memory Block Allocator
 * 	Provides to      : Inter-task Communication, Application Timers
 * 	Depends on       : Inter-task Synchronisation
 *  Public API       : Yes
 * 	In this unit	 :
 * 					    o Memory Block Allocator
 *
 *****************************************************************************/

#define RK_CODE
#include "kexecutive.h"

RK_ERR kMemInit( RK_MEM *const kobj, VOID * memPoolPtr, ULONG blkSize,
		ULONG const numBlocks)
{
	RK_CR_AREA

	RK_CR_ENTER

	if (RK_IS_NULL_PTR( kobj))
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_MEM_INIT);
	}
	/* rounds up to next multiple of 4*/
	blkSize = (blkSize + 0x03) & (ULONG) (~0x03);

	/* initialise freelist of blocks */

	ULONG *blockPtr = (ULONG*) memPoolPtr;
	VOID * *nextAddrPtr = (VOID **) memPoolPtr; /* next block address */

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

VOID * kMemAlloc( RK_MEM *const kobj)
{

	if (kobj->nFreeBlocks == 0)
	{
		return (NULL);
	}
	RK_CR_AREA

	RK_CR_ENTER
	VOID * allocPtr = kobj->freeListPtr;
	kobj->freeListPtr = *(VOID **) allocPtr;
#if(MEMBLKLAST)
    kobj->lastUsed = allocPtr;
    #endif
	if (allocPtr != NULL)
		kobj->nFreeBlocks -= 1;
	RK_CR_EXIT
	return (allocPtr);
}

RK_ERR kMemFree( RK_MEM *const kobj, VOID * blockPtr)
{

	if (kobj->nFreeBlocks == kobj->nMaxBlocks)
	{
		return (RK_ERR_MEM_FREE);
	}
	if (RK_IS_NULL_PTR(kobj) || RK_IS_NULL_PTR( blockPtr))
	{
		return (RK_ERR_MEM_FREE);
	}
	RK_CR_AREA
	RK_CR_ENTER
	*(VOID **) blockPtr = kobj->freeListPtr;
	kobj->freeListPtr = blockPtr;
	kobj->nFreeBlocks += 1;
	RK_CR_EXIT
	return (RK_SUCCESS);
}

