/*
 * exa_copies.c
 *
 *  Created on: 5 Aug 2012
 *      Author: Simon Hall
 * Wrapping up the various copy functions, to be run with DMA.
 * This includes all the code for breaking into 4k pages where appropriate.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa_acc.h"

#include "exa_copylinear_copy2d.inl"


void ForwardCopy(unsigned char *pDst, unsigned char *pSrc, int bytes)
{
#ifdef DEREFERENCE_TEST
	if (*(volatile unsigned char *)pSrc == *(volatile unsigned char *)pSrc);
	if (*(volatile unsigned char *)pDst == *(volatile unsigned char *)pDst);
	if (*(volatile unsigned char *)pSrc + bytes - 1 == *(volatile unsigned char *)pSrc + bytes - 1);
	if (*(volatile unsigned char *)pDst + bytes - 1 == *(volatile unsigned char *)pDst + bytes - 1);
#endif

#ifdef BREAK_PAGES
	extern unsigned long g_devMemBase;
	extern unsigned long g_devMemBaseHigh;

	//if both live in contigous memory
	if ((unsigned long)pSrc >= g_devMemBase && (unsigned long)pSrc < g_devMemBaseHigh
			&& (unsigned long)pDst >= g_devMemBase && (unsigned long)pDst < g_devMemBaseHigh)
	{
		struct DmaControlBlock *pCB = AllocDmaBlock();
		CopyLinear(pCB, pDst, pSrc, bytes, 1);
	}
	else
		while (bytes)
		{
			//which one comes first
			unsigned long srcOffset = (unsigned long)pSrc & 4095;
			unsigned long dstOffset = (unsigned long)pDst & 4095;
			unsigned long pageOffset = dstOffset > srcOffset ? dstOffset : srcOffset;
			unsigned long endOffset = pageOffset + (unsigned long)bytes;
			struct DmaControlBlock *pCB = AllocDmaBlock();

			//nothing interesting
			if (endOffset <= 4096)
			{
	//			xf86DrvMsg(0, X_INFO, "\tpart %d from %p->%p, %d bytes, %d to go, cb %p\n", __LINE__, pSrc, pDst, bytes, bytes, pCB);
				CopyLinear(pCB,
						pDst,			//destination
						pSrc,			//source
						bytes,			//bytes to copy
						1);				//source increment

				//done
				bytes = 0;
			}
			else if (bytes >= 4096)
			{	//and cap our max transfer to 4k
				unsigned int to_copy = 4096 - pageOffset;

	//			xf86DrvMsg(0, X_INFO, "\tpart %d from %p->%p, %d bytes, %d to go, cb %p\n", __LINE__, pSrc, pDst, to_copy, bytes, pCB);
				CopyLinear(pCB,
						pDst,
						pSrc,
						to_copy,
						1);

				bytes -= to_copy;
				pDst += to_copy;
				pSrc += to_copy;
			}
			else
			{
				unsigned long to_copy = bytes - ((pageOffset + bytes) & 4095);

	//			xf86DrvMsg(0, X_INFO, "\tpart %d from %p->%p, %d bytes, %d to go, cb %p\n", __LINE__, pSrc, pDst, to_copy, bytes, pCB);
				CopyLinear(pCB,
						pDst,
						pSrc,
						to_copy,
						1);

				bytes -= to_copy;
				pDst += to_copy;
				pSrc += to_copy;
			}
		}
#else
	struct DmaControlBlock *pCB = AllocDmaBlock();
	CopyLinear(pCB, pDst, pSrc, bytes, 1);
#endif
}

void ForwardCopyNoSrcInc(unsigned char *pDst, unsigned char *pSrc, int bytes)
{
#ifdef BREAK_PAGES
	extern unsigned long g_devMemBase;
	extern unsigned long g_devMemBaseHigh;

	//if the destination lives in contiguous memory
	if ((unsigned long)pDst >= g_devMemBase && (unsigned long)pDst < g_devMemBaseHigh)
	{
		struct DmaControlBlock *pCB = AllocDmaBlock();
		CopyLinear(pCB, pDst, pSrc, bytes, 0);
	}
	else
	{
		unsigned long computedEnd = (unsigned long)pDst + bytes;
		unsigned long srcAlign = 0;

		while (bytes)
		{
			//which one comes first
			unsigned long pageOffset = (unsigned long)pDst & 4095;
			unsigned long endOffset = pageOffset + (unsigned long)bytes;
			struct DmaControlBlock *pCB = AllocDmaBlock();

			//nothing interesting
			if (endOffset <= 4096)
			{
				CopyLinear(pCB,
						pDst,			//destination
						pSrc + srcAlign,	//source
						bytes,			//bytes to copy
						0);				//source increment

				//done
				pDst += bytes;
				bytes = 0;
			}
			else if (bytes >= 4096)
			{	//and cap our max transfer to 4k
				unsigned int to_copy = 4096 - pageOffset;

				CopyLinear(pCB,
						pDst,
						pSrc + srcAlign,
						to_copy,
						0);

				bytes -= to_copy;
				pDst += to_copy;

				srcAlign = (srcAlign + (to_copy & 0xf)) & 0xf;
			}
			else
			{
				unsigned long to_copy = bytes - ((pageOffset + bytes) & 4095);	//pageoffset + bytes says where the end alignment is

				CopyLinear(pCB,
						pDst,
						pSrc + srcAlign,
						to_copy,
						0);

				bytes -= to_copy;
				pDst += to_copy;

				srcAlign = (srcAlign + (to_copy & 0xf)) & 0xf;
			}
		}

		MY_ASSERT((unsigned long)pDst == computedEnd);
	}
#else
	struct DmaControlBlock *pCB = AllocDmaBlock();
	CopyLinear(pCB, pDst, pSrc, bytes, 0);
#endif
}

void Copy2D4kSrcInc(void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int destStride, unsigned int sourceStride)
{
	extern unsigned long g_devMemBase;
	extern unsigned long g_devMemBaseHigh;

	//if both live in contiguous memory
	if ((unsigned long)pSourceAddr >= g_devMemBase && (unsigned long)pSourceAddr < g_devMemBaseHigh
			&& (unsigned long)pDestAddr >= g_devMemBase && (unsigned long)pDestAddr < g_devMemBaseHigh)
	{
		struct DmaControlBlock *pCB = AllocDmaBlock();
		Copy2D(pCB, pDestAddr, pSourceAddr, xlength, ylength - 1, 1, destStride, sourceStride);
	}
	else
	{
		for (int y = 0; y < ylength; y++)
		{
			ForwardCopy(pDestAddr, pSourceAddr, xlength);
			pDestAddr = (void *)((unsigned long)pDestAddr + xlength + destStride);
			pSourceAddr = (void *)((unsigned long)pSourceAddr + xlength + sourceStride);
		}
	}
}

void Copy2D4kSrcInc_fallback(void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int destStride, unsigned int sourceStride)
{
	for (int y = 0; y < ylength; y++)
	{
		memcpy(pDestAddr, pSourceAddr, xlength);
		pDestAddr = (void *)((unsigned long)pDestAddr + xlength + destStride);
		pSourceAddr = (void *)((unsigned long)pSourceAddr + xlength + sourceStride);
	}
}

void Copy2D4kNoSrcInc(void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int destStride)
{
	extern unsigned long g_devMemBase;
	extern unsigned long g_devMemBaseHigh;

	//no need to validate the source address
	if ((unsigned long)pDestAddr >= g_devMemBase && (unsigned long)pDestAddr < g_devMemBaseHigh)
	{
		struct DmaControlBlock *pCB = AllocDmaBlock();
		Copy2D(pCB, pDestAddr, pSourceAddr, xlength, ylength - 1, 0, destStride, 0);
	}
	else
	{
		int y;
		for (y = 0; y < ylength; y++)
		{
			ForwardCopyNoSrcInc(pDestAddr, pSourceAddr, xlength);
			pDestAddr = (void *)((unsigned long)pDestAddr + xlength + destStride);
		}
	}
}

