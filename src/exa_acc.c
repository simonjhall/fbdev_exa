#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <time.h>

#include "xf86.h"
#include "fb.h"
#include "exa.h"

#include "exa_acc.h"

/******** GENERIC STUFF ******/
struct CopyDetails
{
	PixmapPtr m_pSrc;
	PixmapPtr m_pDst;
	int m_dx, m_dy;
	int m_bpp;
	int m_syncPoint;
} g_copyDetails;

struct SolidDetails
{
	PixmapPtr m_pDst;
	int m_bpp;
	Pixel m_toFill;
	int m_syncPoint;
} g_solidDetails;

struct UpDownloadDetails
{
	Bool m_up;
	PixmapPtr m_pPixmap;
	int m_x, m_y;
	int m_w, m_h;
	char *m_pImage;
	int m_pitch;
} g_upDownloadDetails;

struct DmaControlBlock *g_pDmaBuffer = 0;
unsigned int g_dmaUnkickedHead = 0;
unsigned int g_dmaTail = 0;
unsigned char *g_pSolidBuffer = 0;
unsigned int g_solidOffset = 0;
unsigned int g_highestSolid = 0;
BOOL g_dmaPending = FALSE;
unsigned int g_bytesPending = 0;
unsigned int g_totalBytesPendingForced = 0;
unsigned int g_totalBytesPendingUnforced = 0;
BOOL g_headOfDma = TRUE;

unsigned int g_forcedWaits = 0;
unsigned int g_unforcedWaits = 0;
unsigned int g_forcedStarts = 0;
unsigned int g_unforcedStarts = 0;
unsigned int g_actualWaits = 0;
unsigned int g_actualStarts = 0;

struct DmaControlBlock *g_pEmuCB = 0;

/******* DMA ********/
/**** starting ******/

inline int RunDma(struct DmaControlBlock *pCB)
{
	MY_ASSERT(0);
	if (pCB->m_pNext == 0xcdcdcdcd)
		return 0;
	return EmulateDma(pCB);
//	kern_dma_prepare(pCB);
//	return kern_dma_prepare_kick_wait(pCB);
//	return 0;
}

inline BOOL StartDma(struct DmaControlBlock *pCB, BOOL force)
{
	if (force)
	{
		g_forcedStarts++;
		g_totalBytesPendingForced += g_bytesPending;
	}
	else
	{
		g_unforcedStarts++;
		g_totalBytesPendingUnforced += g_bytesPending;
	}

//	MY_ASSERT(pCB->m_pNext != 0xcdcdcdcd)

	if (WaitDma(force))
	{
		MY_ASSERT(g_dmaPending == FALSE);

		g_dmaPending = TRUE;			//stuff's in the system and we need to ensure we don't overload our DMA controller
		g_headOfDma = TRUE;				//allocations shouldn't patch up previous DMA CBs

	//	return EmulateDma(pCB);
		MY_ASSERT(g_pEmuCB == 0);
		g_pEmuCB = pCB;
//		kern_dma_prepare(pCB);
//		return kern_dma_prepare_kick_wait(pCB);
		kern_dma_prepare_kick(pCB);
		g_actualStarts++;

		return TRUE;
	}
	else
		return FALSE;
}

/********* waiting ************/
inline BOOL WaitDma(BOOL force)
{
	if (force)
		g_forcedWaits++;
	else
		g_unforcedWaits++;

	if (force || (g_bytesPending >= 8192))
//	if (1)
	{
		RealWaitDma(g_bytesPending);
		g_dmaPending = FALSE;

		return TRUE;
	}
	else
		return FALSE;
}

inline void EmulateWaitDma(void)
{
	if(g_dmaPending)
	{
		MY_ASSERT(g_pEmuCB);
		EmulateDma(g_pEmuCB);
		g_pEmuCB = 0;

		g_bytesPending = 0;
	}
}

inline void RealWaitDma(unsigned int bytesPending)
{
	if(g_dmaPending)
	{
		MY_ASSERT(g_pEmuCB);
		kern_dma_wait_all(g_bytesPending);
		g_pEmuCB = 0;
		g_actualWaits++;
		g_bytesPending = 0;
	}
}

/****** DMA allocation ****/

inline struct DmaControlBlock *GetDmaBlock(void)
{
	if (!g_pDmaBuffer)
	{
		g_pDmaBuffer = (struct DmaControlBlock *)kern_alloc(4096 * 50);
		MY_ASSERT(g_pDmaBuffer);
		//g_pDmaBuffer->m_pNext = 0xcdcdcdcd;
//		memset(g_pDmaBuffer, 0xcd, 4096 * 50);
	}

	BOOL wasHead = g_headOfDma;
	g_headOfDma = FALSE;

	if (g_dmaTail == 0)
		return &g_pDmaBuffer[g_dmaTail++];			//first in the list, just return it
	else if (g_dmaTail < 128 * 50)
	{
		if (!wasHead)						//patch up everything bar the head
			g_pDmaBuffer[g_dmaTail - 1].m_pNext = &g_pDmaBuffer[g_dmaTail];
		return &g_pDmaBuffer[g_dmaTail++];
	}
	else
	{	//out of list space, kick the lot and start fresh
		MY_ASSERT(g_dmaUnkickedHead != g_dmaTail || g_dmaPending == TRUE);

		if (g_dmaUnkickedHead != g_dmaTail)
		{
			if (StartDma(g_pDmaBuffer + g_dmaUnkickedHead, TRUE))
				g_dmaUnkickedHead = g_dmaTail;
		}

		WaitMarker(0, 0);
		//head will be reset, but we need to clear it before the return
		g_headOfDma = FALSE;
		MY_ASSERT(g_dmaTail == 0);
		return &g_pDmaBuffer[g_dmaTail++];
	}
}

inline unsigned char *GetSolidBuffer(unsigned int bytes)
{
	const unsigned int solid_size = 4096 * 10;		//page multiple

	if (!g_pSolidBuffer)
	{
		g_pSolidBuffer = kern_alloc(solid_size);
		MY_ASSERT(g_pSolidBuffer);
		g_solidOffset = 0;
	}

	if (g_solidOffset + bytes >= solid_size)
	{
		fprintf(stderr, "ran out of solid space...hopefully flushing\n");
		return 0;
	}

	unsigned char *p = &g_pSolidBuffer[g_solidOffset];
	g_solidOffset += bytes;

	return p;
}

/******** COPIES *********/

inline void CopyLinear(struct DmaControlBlock *pCB,
		void *pDestAddr, void *pSourceAddr, unsigned int length, unsigned int srcInc)
{
	MY_ASSERT(pCB);
	MY_ASSERT(pDestAddr);
	MY_ASSERT(pSourceAddr);
	MY_ASSERT(length > 0 && length <= 0x3fffffff);
	MY_ASSERT(srcInc == 0 || srcInc == 1);

#ifdef DEREFERENCE_TEST
	if (*(volatile unsigned char *)pSourceAddr == *(volatile unsigned char *)pSourceAddr);
	if (*(volatile unsigned char *)pDestAddr == *(volatile unsigned char *)pDestAddr);
	if (*(volatile unsigned char *)pSourceAddr + length - 1 == *(volatile unsigned char *)pSourceAddr + length - 1);
	if (*(volatile unsigned char *)pDestAddr + length - 1 == *(volatile unsigned char *)pDestAddr + length - 1);
#endif

#ifdef STRADDLE_TEST
	if (srcInc)
	{
		unsigned long source_start = (unsigned long)pSourceAddr >> 12;
		unsigned long source_end = (unsigned long)(pSourceAddr + length - 1) >> 12;

		if (source_start != source_end)
		{
			unsigned long source_page_end;
			unsigned long new_length_source;

			xf86DrvMsg(0, X_INFO, "linear source range straddles page boundary %p->%p, %lx->%lx\n",
					pSourceAddr, pSourceAddr + length, source_start, source_end);

			if (source_end - source_start > 1)
				xf86DrvMsg(0, X_INFO, "\tstraddles %ld pages\n", source_end - source_start);
		}
	}

	unsigned long dest_start = (unsigned long)pDestAddr >> 12;
	unsigned long dest_end = (unsigned long)(pDestAddr + length - 1) >> 12;

	if (dest_start != dest_end)
	{
		xf86DrvMsg(0, X_INFO, "linear dest range straddles page boundary %p->%p, %lx->%lx\n",
				pDestAddr, pDestAddr + length, dest_start, dest_end);

		if (dest_end - dest_start > 1)
				xf86DrvMsg(0, X_INFO, "\tstraddles %ld pages\n", dest_end - dest_start);
	}
#endif

	pCB->m_transferInfo = (srcInc << 8);			//do source increment?
	pCB->m_transferInfo |= (1 << 4);				//dest increment
	pCB->m_transferInfo |= (5 << 12);				//axi burst
	/*pCB->m_transferInfo |= (1 << 9);				//source burst
	pCB->m_transferInfo |= (1 << 5);				//dest burst*/

	pCB->m_pSourceAddr = pSourceAddr;
	pCB->m_pDestAddr = pDestAddr;
	pCB->m_xferLen = length;
	pCB->m_tdStride = 0xffffffff;
	pCB->m_pNext = 0;

	pCB->m_blank1 = pCB->m_blank2 = 0;

	g_bytesPending += length;
}

static inline void ForwardCopy(unsigned char *pDst, unsigned char *pSrc, int bytes)
{
#ifdef BREAK_PAGES
	while (bytes)
	{
		//which one comes first
		unsigned long srcOffset = (unsigned long)pSrc & 4095;
		unsigned long dstOffset = (unsigned long)pDst & 4095;
		unsigned long pageOffset = dstOffset > srcOffset ? dstOffset : srcOffset;
		unsigned long endOffset = pageOffset + (unsigned long)bytes;
		struct DmaControlBlock *pCB = GetDmaBlock();

		//nothing interesting
		if (endOffset <= 4096)
		{
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
	struct DmaControlBlock *pCB = GetDmaBlock();
	CopyLinear(pCB, pDst, pSrc, bytes, 1);
#endif
}

static inline void ForwardCopyNoSrcInc(unsigned char *pDst, unsigned char *pSrc, int bytes)
{
#ifdef BREAK_PAGES
	unsigned long computedEnd = (unsigned long)pDst + bytes;
	unsigned long srcAlign = 0;

	while (bytes)
	{
		//which one comes first
		unsigned long pageOffset = (unsigned long)pDst & 4095;
		unsigned long endOffset = pageOffset + (unsigned long)bytes;
		struct DmaControlBlock *pCB = GetDmaBlock();

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
#else
	struct DmaControlBlock *pCB = GetDmaBlock();
	CopyLinear(pCB, pDst, pSrc, bytes, 0);
#endif
}

static inline void Copy2D4kSrcInc(void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int destStride, unsigned int sourceStride)
{
	/*unsigned long destOffset = (unsigned long)pDestAddr & 4095;
	unsigned long destChange = (xlength + destStride) * ylength;

	unsigned long sourceOffset = (unsigned long)pSourceAddr & 4095;
	unsigned long sourceChange = (xlength + sourceStride) * ylength;

	//fast way
	if (destOffset + destChange <= 4096 && sourceOffset + sourceChange <= 4096)
	{
		struct DmaControlBlock *pCB = GetDmaBlock();
		Copy2D(pCB, pDestAddr, pSourceAddr, xlength, ylength, 1, destStride, sourceStride);
	}
	else*/
	{
		int y;
		for (y = 0; y < ylength; y++)
		{
			ForwardCopy(pDestAddr, pSourceAddr, xlength);
			pDestAddr = (void *)((unsigned long)pDestAddr + xlength + destStride);
			pSourceAddr = (void *)((unsigned long)pSourceAddr + xlength + sourceStride);
		}
	}
//	struct DmaControlBlock *pCB = GetDmaBlock();
//	Copy2D(pCB, pDestAddr, pSourceAddr, xlength, ylength, 1, destStride, sourceStride);
}

static inline void Copy2D4kNoSrcInc(void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int destStride)
{
	/*unsigned long destOffset = (unsigned long)pDestAddr & 4095;
	unsigned long destChange = (xlength + destStride) * ylength;

	//fast way
	if (destOffset + destChange <= 4096)
	{
		struct DmaControlBlock *pCB = GetDmaBlock();
		Copy2D(pCB, pDestAddr, pSourceAddr, xlength, ylength, 0, destStride, 0);
	}
	else*/
	{
		int y;
		for (y = 0; y < ylength; y++)
		{
			ForwardCopyNoSrcInc(pDestAddr, pSourceAddr, xlength);
			pDestAddr = (void *)((unsigned long)pDestAddr + xlength + destStride);
		}
	}
//	struct DmaControlBlock *pCB = GetDmaBlock();
//	Copy2D(pCB, pDestAddr, pSourceAddr, xlength, ylength, 0, destStride, 0);
}

inline void Copy2D(struct DmaControlBlock *pCB,
		void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int srcInc, unsigned int destStride, unsigned int sourceStride)
{
	MY_ASSERT(pCB);
	MY_ASSERT(pDestAddr);
	MY_ASSERT(pSourceAddr);
	MY_ASSERT(xlength > 0 && xlength <= 0xffff);
	MY_ASSERT(ylength > 0 && ylength <= 0x3fff);
	MY_ASSERT(srcInc == 0 || srcInc == 1);
	MY_ASSERT(sourceStride <= 0xffff);
	MY_ASSERT(destStride <= 0xffff);

	MY_ASSERT(0);		//fix transferinfo too

#ifdef DEREFERENCE_TEST
	if (*(volatile unsigned char *)pSourceAddr == *(volatile unsigned char *)pSourceAddr);
	if (*(volatile unsigned char *)pDestAddr == *(volatile unsigned char *)pDestAddr);
#endif

#ifdef STRADDLE_TEST
	if (srcInc)
	{
		unsigned long source_start = (unsigned long)pSourceAddr >> 12;
		unsigned long source_end = (unsigned long)(pSourceAddr + (xlength + sourceStride) * ylength - 1) >> 12;

		if (source_start != source_end)
		{
			xf86DrvMsg(0, X_INFO, "2D source range straddles page boundary %p->%p, %lx->%lx, %dx%d (+%d)\n",
					pSourceAddr, pSourceAddr + (xlength + sourceStride) * ylength,
					source_start, source_end,
					xlength, ylength, sourceStride);

			if (source_end - source_start > 1)
				xf86DrvMsg(0, X_INFO, "\tstraddles %ld pages\n", source_end - source_start);
		}
	}

	unsigned long dest_start = (unsigned long)pDestAddr >> 12;
	unsigned long dest_end = (unsigned long)(pDestAddr + (xlength + destStride) * ylength - 1) >> 12;

	if (dest_start != dest_end)
	{
		xf86DrvMsg(0, X_INFO, "2D dest range straddles page boundary %p->%p, %lx->%lx, %dx%d (+%d)\n",
				pDestAddr, pDestAddr + (xlength + destStride) * ylength,
				dest_start, dest_end,
				xlength, ylength, destStride);

		if (dest_end - dest_start > 1)
				xf86DrvMsg(0, X_INFO, "\tstraddles %ld pages\n", dest_end - dest_start);
	}
#endif

	pCB->m_transferInfo = (srcInc << 8) | (1 << 1);
	pCB->m_pSourceAddr = pSourceAddr;
	pCB->m_pDestAddr = pDestAddr;
	pCB->m_xferLen = (ylength << 16) | xlength;
	pCB->m_tdStride = (destStride << 16) | sourceStride;
	pCB->m_pNext = 0;
}

/******** EXA ********/

static CARD8 *g_pOffscreenBase;
static unsigned long g_offscreenSize;

void *GetMemoryBase(void)
{
	static int run = 0;
	if (!run)
	{
		run = 1;
		g_pOffscreenBase = kern_alloc(12 * 1024 * 1024);
		g_offscreenSize = 12 * 1024 * 1024;

		MY_ASSERT(g_pOffscreenBase);

		//touch every page
//		memset(g_pOffscreenBase, 0xcd, g_offscreenSize);
	}

	return g_pOffscreenBase;
}

unsigned long GetMemorySize(void)
{
	return g_offscreenSize;
}

int MarkSync(ScreenPtr pScreen)
{
	static int marker = 0;
//	xf86DrvMsg(0, X_DEFAULT, "%s %p %d\n", __FUNCTION__, pScreen, marker);
	return marker++;
}

void WaitMarker(ScreenPtr pScreen, int Marker)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p %d\n", __FUNCTION__, pScreen, Marker);

	time_t start = clock();

//	static int dmas = 0;
	//int kick = RunDma(g_pDmaBuffer);
//	int kick = StartDma(g_pDmaBuffer);
	if (g_dmaUnkickedHead != g_dmaTail)
	{
		StartDma(g_pDmaBuffer + g_dmaUnkickedHead, TRUE);
		g_dmaUnkickedHead = g_dmaTail;
	}

	if (g_dmaPending)
		WaitDma(TRUE);
//	dmas += kick;

	/*xf86DrvMsg(0, X_INFO, "kick %d dmas, solid %d, took %.3f ms\n", g_dmaTail, g_solidOffset,
			(float)(clock() - start) / CLOCKS_PER_SEC * 1000.0f);*/

	g_dmaTail = 0;			//write into CB 0
	g_dmaUnkickedHead = 0;	//and the head of any work starts again at zero
	g_headOfDma = TRUE;			//patch up -1 CB next pointers
//	memset(g_pDmaBuffer, 0xcd, 32);
//	memset(g_pDmaBuffer, 0xcd, 4096 * 50);

	static unsigned int last_starts = 0;
	if (g_actualStarts - last_starts > 1000)
	{
		last_starts = g_actualStarts;
		fprintf(stderr, "actual s/w %d %d, forced s/w %d %d, unforced s/w %d %d, average kick %d %d bytes\n",
				g_actualStarts, g_actualWaits, g_forcedStarts, g_forcedWaits, g_unforcedStarts, g_unforcedWaits,
				g_totalBytesPendingForced / g_forcedStarts, g_totalBytesPendingUnforced / g_unforcedStarts);
	}
}


Bool PrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask,
		Pixel fg)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p\n", __FUNCTION__, pPixmap);

	//check it's a valid pointer
	if (pPixmap == NULL)
	{
		xf86DrvMsg(0, X_WARNING, "%s (pPixmap == NULL)\n", __FUNCTION__);
		return FALSE;
	}

	//check it's either 8/16/24/32
	if (pPixmap->drawable.bitsPerPixel & 7)
	{
		xf86DrvMsg(0, X_WARNING, "%s pPixmap->drawable.bitsPerPixel & 7\n", __FUNCTION__);
		return FALSE;
	}

	//need a solid plane mask
	if (!EXA_PM_IS_SOLID(&pPixmap->drawable, planemask))
	{
		xf86DrvMsg(0, X_WARNING, "%s !EXA_PM_IS_SOLID\n", __FUNCTION__);
		return FALSE;
	}

	//check that it's a copy operation
	if (alu != GXcopy)
	{
		xf86DrvMsg(0, X_WARNING, "%s alu != GXcopy\n", __FUNCTION__);
		return FALSE;
	}

	g_solidDetails.m_pDst = pPixmap;
	g_solidDetails.m_bpp = pPixmap->drawable.bitsPerPixel / 8;
	g_solidDetails.m_toFill = fg;
//	g_pSolidHead = g_pSolidTail = 0;

	return TRUE;
}

void Solid(PixmapPtr pPixmap, int X1, int Y1, int X2, int Y2)
{
	MY_ASSERT(pPixmap == g_solidDetails.m_pDst);

//	xf86DrvMsg(0, X_DEFAULT, "%s %p (%d,%d->%d,%d %dx%d)\n", __FUNCTION__, pPixmap,
//			X1, Y1, X2, Y2,
//			X2 - X1, Y2 - Y1);

	unsigned char *pDst = exaGetPixmapAddress(g_solidDetails.m_pDst);
	unsigned long dstPitch = exaGetPixmapPitch(g_solidDetails.m_pDst);

	//perform the fill with a 2d blit and non-moving source

	//some variables to make this more manageable
	int bpp = g_solidDetails.m_bpp;
	int width = X2 - X1;
	int height = Y2 - Y1;
	//get a new dma block and some solid space
	unsigned char *pSolid = 0;

//	xf86DrvMsg(0, X_INFO, "Solid %p, pitch %ld, bpp %d colour %08x\n",
//		pDst, dstPitch, g_solidDetails.m_bpp, g_solidDetails.m_toFill);

	while (!(pSolid = GetSolidBuffer(32)))
	{
		fprintf(stderr, "unable to allocate solid space - kicking and waiting\n");

		//there could be non-committed work taking up all the solid
		if (g_dmaUnkickedHead != g_dmaTail)
		{
			if (StartDma(g_pDmaBuffer + g_dmaUnkickedHead, TRUE))
				g_dmaUnkickedHead = g_dmaTail;
		}

		//it could be something already enqueued taking it
		if (g_dmaPending)
			WaitMarker(g_solidDetails.m_pDst->drawable.pScreen, 0);

		//or simply we simply haven't reset it yet after past work
		g_solidOffset = 0;
	}

	//copy in the solid
	switch (bpp)
	{
	case 1:
		pSolid[0] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[1] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[2] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[3] = (unsigned char)g_solidDetails.m_toFill;
		break;
	case 2:
		pSolid[0] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[1] = (unsigned char)(g_solidDetails.m_toFill >> 8);
		pSolid[2] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[3] = (unsigned char)(g_solidDetails.m_toFill >> 8);
		break;
	case 3:
	case 4:
		pSolid[0] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[1] = (unsigned char)(g_solidDetails.m_toFill >> 8);
		pSolid[2] = (unsigned char)(g_solidDetails.m_toFill >> 16);
		pSolid[3] = (unsigned char)(g_solidDetails.m_toFill >> 24);
		break;
	default:
		MY_ASSERT(0);
		break;
	}

	unsigned int *pSolidI = (unsigned int *)pSolid;
	pSolidI[1] = *pSolidI;
	pSolidI[2] = *pSolidI;
	pSolidI[3] = *pSolidI;
	pSolidI[4] = *pSolidI;
	pSolidI[5] = *pSolidI;
	pSolidI[6] = *pSolidI;
	pSolidI[7] = *pSolidI;

	//enqueue the dma
	Copy2D4kNoSrcInc(&pDst[Y1 * dstPitch + X1 * bpp],	//dest
			pSolid,					//source
			bpp * width,			//width
			height,					//height
			dstPitch - bpp * width);//dest stride

}

void DoneSolid(PixmapPtr p)
{
	MY_ASSERT(g_solidDetails.m_pDst == p);

	if (g_dmaUnkickedHead != g_dmaTail)
	{
		//give it a sync point (for future work)
		exaMarkSync(g_solidDetails.m_pDst->drawable.pScreen);

		if (StartDma(g_pDmaBuffer + g_dmaUnkickedHead, FALSE))
			g_dmaUnkickedHead = g_dmaTail;
//		WaitMarker(0, 0);
	}
}

Bool PrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int dx,
		int dy, int alu, Pixel planemask)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p->%p\n", __FUNCTION__, pSrcPixmap, pDstPixmap);

	//check they're valid pointers
	if ((pDstPixmap == NULL) || (pSrcPixmap == NULL))
	{
		xf86DrvMsg(0, X_WARNING, "%s (pPixmapDst == NULL) || (pPixmapSrc == NULL)\n", __FUNCTION__);
		return FALSE;
	}

	//check they both have the same pixel format
	if (pDstPixmap->drawable.bitsPerPixel != pSrcPixmap->drawable.bitsPerPixel)
	{
		xf86DrvMsg(0, X_WARNING, "%s pPixmapDst->drawable.bitsPerPixel != pPixmapSrc->drawable.bitsPerPixel\n", __FUNCTION__);
		return FALSE;
	}

	//check they're either 8/16/24/32
	if (pDstPixmap->drawable.bitsPerPixel & 7)
	{
		xf86DrvMsg(0, X_WARNING, "%s pDstPixmap->drawable.bitsPerPixel & 7\n", __FUNCTION__);
		return FALSE;
	}

	//need a solid plane mask
	if (!EXA_PM_IS_SOLID(&pDstPixmap->drawable, planemask))
	{
		xf86DrvMsg(0, X_WARNING, "%s !EXA_PM_IS_SOLID\n", __FUNCTION__);
		return FALSE;
	}

	//check that it's a copy operation
	if (alu != GXcopy)
	{
		xf86DrvMsg(0, X_WARNING, "%s alu (%d) != GXcopy\n", __FUNCTION__, alu);
		return FALSE;
	}

	//only support left to right
	if (dx < 0)
	{
//		xf86DrvMsg(0, X_WARNING, "%s dx < 0\n", __FUNCTION__);
		return FALSE;
	}

	g_copyDetails.m_pSrc = pSrcPixmap;
	g_copyDetails.m_pDst = pDstPixmap;
	g_copyDetails.m_dx = dx;
	g_copyDetails.m_dy = dy;
	g_copyDetails.m_bpp = pDstPixmap->drawable.bitsPerPixel / 8;

	return TRUE;
}

void Copy(PixmapPtr pDstPixmap, int srcX, int srcY,
		int dstX, int dstY, int width, int height)
{
	MY_ASSERT(pDstPixmap == g_copyDetails.m_pDst);
	unsigned char *pSrc, *pDst;
	pSrc = exaGetPixmapAddress(g_copyDetails.m_pSrc);
	pDst = exaGetPixmapAddress(g_copyDetails.m_pDst);

	unsigned long srcPitch, dstPitch;
	srcPitch = exaGetPixmapPitch(g_copyDetails.m_pSrc);
	dstPitch = exaGetPixmapPitch(g_copyDetails.m_pDst);


	//perform the copy as single lines
	if (g_copyDetails.m_dy < 0)
	{
		int y;
		for (y = height - 1; y >= 0; y--)
		{
			unsigned char *src = &pSrc[(y + srcY) * srcPitch + srcX * g_copyDetails.m_bpp];
			unsigned char *dst = &pDst[(y + dstY) * dstPitch + dstX * g_copyDetails.m_bpp];

			ForwardCopy(dst, src, width * g_copyDetails.m_bpp);
		}
	}
	else	//do the whole thing as one 2d operation
	{
		//get a new dma block
		Copy2D4kSrcInc(&pDst[dstY * dstPitch + dstX * g_copyDetails.m_bpp],
				&pSrc[+ srcY * srcPitch + srcX * g_copyDetails.m_bpp],
				width * g_copyDetails.m_bpp,
				height,
				dstPitch - width * g_copyDetails.m_bpp,
				srcPitch - width * g_copyDetails.m_bpp);
	}

}

void DoneCopy(PixmapPtr p)
{
	MY_ASSERT(g_copyDetails.m_pDst == p);

	if (g_dmaUnkickedHead != g_dmaTail)
	{
		exaMarkSync(g_copyDetails.m_pDst->drawable.pScreen);

		if (StartDma(g_pDmaBuffer + g_dmaUnkickedHead, FALSE))
			g_dmaUnkickedHead = g_dmaTail;
//		WaitMarker(0, 0);
	}
}

Bool CheckComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p+%p->%p\n", __FUNCTION__, pSrcPicture, pMaskPicture, pDstPicture);

	return TRUE;
//	return FALSE;
}

static int g_compositeOp;
static PicturePtr g_pSrcPicture;
static PicturePtr g_pMaskPicture;
static PicturePtr g_pDstPicture;
static PixmapPtr g_pSrc;
static PixmapPtr g_pMask;
static PixmapPtr g_pDst;

Bool PrepareComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrc,
		PixmapPtr pMask, PixmapPtr pDst)
{
	static int accept = 0;
	static int reject_op = 0;
	static int reject_mask = 0;
	static int reject_mask_alpha = 0;
	static int reject_src_repeat = 0;
	static int reject_mask_repeat = 0;
	static int reject_transform = 0;
	static int reject_bpp = 0;

//	xf86DrvMsg(0, X_DEFAULT, "%s %p+%p->%p\n", __FUNCTION__, pSrc, pMask, pDst);
	g_compositeOp = op;
	g_pSrcPicture = pSrcPicture;
	g_pMaskPicture = pMaskPicture;
	g_pDstPicture = pDstPicture;
	g_pSrc = pSrc;
	g_pMask = pMask;
	g_pDst = pDst;

	if (op != PictOpOver && op != PictOpAdd && op != PictOpOutReverse)
	{
		reject_op++;
//		fprintf(stderr, "op is %d\n", op);
		return FALSE;
	}

	if (g_pMask /*&& (op == PictOpAdd || op == PictOpOutReverse)*/)
	{
		reject_mask++;
		return FALSE;
	}

	if (g_pMask && g_pMaskPicture->componentAlpha)
	{
		reject_mask_alpha++;
		return FALSE;
	}

	if (pSrcPicture->repeat && (op == PictOpAdd || op == PictOpOutReverse))
	{
		reject_src_repeat++;
		return FALSE;
	}

	if (pSrcPicture->transform)
	{
		reject_transform++;
		return FALSE;
	}

	if (pMaskPicture && pMaskPicture->repeat)
	{
		reject_mask_repeat++;
		return FALSE;
	}

	if (pMaskPicture && pMaskPicture->transform)
	{
		reject_transform++;
		return FALSE;
	}

	if (pSrc->drawable.bitsPerPixel != pDst->drawable.bitsPerPixel)
	{
		reject_bpp++;
		return FALSE;
	}

	MY_ASSERT(0);
	WaitMarker(0, 0);

	accept++;

	if ((accept % 20) == 0)
		fprintf(stderr, "accept %d reject %d/%d/%d/%d/%d/%d/%d\n",
				accept,
				reject_op, reject_mask, reject_mask_alpha, reject_src_repeat, reject_mask_repeat, reject_transform, reject_bpp);


	return TRUE;
//	return FALSE;
}

void Composite(PixmapPtr pDst, int srcX, int srcY, int maskX,
		int maskY, int dstX, int dstY, int width, int height)
{
	xf86DrvMsg(0, X_DEFAULT, "%s %p+%p->%p (%d), %p+%p->%p (wh %dx%d s %d,%d m %d,%d d %d,%d)\n",
			__FUNCTION__,
			g_pSrcPicture, g_pMaskPicture, g_pDstPicture,
			g_compositeOp,
			g_pSrc, g_pMask, g_pDst,
			width, height,
			srcX, srcY,
			maskX, maskY,
			dstX, dstY);

	int y, x;
	unsigned long dest_stride = exaGetPixmapPitch(pDst);
	unsigned long src_stride = exaGetPixmapPitch(g_pSrc);
	unsigned long mask_stride;
	unsigned int mask_inc;

	unsigned char *dest = exaGetPixmapAddress(pDst);
	unsigned char *source = exaGetPixmapAddress(g_pSrc);
	unsigned char *mask;
	unsigned char mask_val = 0xff;

	unsigned short source_width = g_pSrc->drawable.width;
	unsigned short source_height = g_pSrc->drawable.height;

	if (g_pMask)
	{
		mask = exaGetPixmapAddress(g_pMask);
		mask_stride = exaGetPixmapPitch(g_pMask);
		mask_inc = 1;
	}
	else
	{
		mask = &mask_val;
		mask_inc = 0;
		mask_stride = 0;
	}

	unsigned int bpp = pDst->drawable.bitsPerPixel / 8;

	if (g_compositeOp == PictOpAdd)
	{
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				unsigned char m = mask[(y + maskY ) * mask_stride + (x + maskX) * mask_inc];
				int b;
				for (b = 0; b < bpp; b++)
				{
					unsigned short source_x = x + srcX;
					unsigned short source_y = y + srcY;

					if (source_x > source_width)
						source_x -= source_width;
					if (source_y > source_height)
						source_y -= source_height;

					unsigned int temp = dest[(y + dstY) * dest_stride + (x + dstX) * bpp + b]
											 + source[(y + srcY) * src_stride + (x + srcX) * bpp + b];
					if (temp > 255)
						temp = 255;

//					if (m)
						dest[(y + dstY) * dest_stride + (x + dstX) * bpp + b] = temp;
				}
			}
	}
	else if (g_compositeOp == PictOpOver)
	{
		MY_ASSERT(bpp == 4);
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				unsigned char sr, sg, sb, sa;
				unsigned char dr, dg, db, da;

				unsigned char m = mask[(y + maskY ) * mask_stride + (x + maskX) * mask_inc];

//				if (!m)
//					continue;

				unsigned short source_x = x + srcX;
				unsigned short source_y = y + srcY;

				while (source_x >= source_width)
					source_x -= source_width;
				while (source_y >= source_height)
					source_y -= source_height;


				sr = source[source_y * src_stride + source_x * 4];
				sg = source[source_y * src_stride + source_x * 4 + 1];
				sb = source[source_y * src_stride + source_x * 4 + 2];
				sa = source[source_y * src_stride + source_x * 4 + 3];

				sr = ((unsigned int)sr * (unsigned int)m) >> 8;
				sg = ((unsigned int)sg * (unsigned int)m) >> 8;
				sb = ((unsigned int)sb * (unsigned int)m) >> 8;
				sa = ((unsigned int)sa * (unsigned int)m) >> 8;

				dr = dest[(y + dstY) * dest_stride + (x + dstX) * 4];
				dg = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1];
				db = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2];
				da = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3];

				unsigned int r, g, b, a;
				r = sr * 255 + dr * (255 - sa);
				g = sg * 255 + dg * (255 - sa);
				b = sb * 255 + db * (255 - sa);
				a = sa * 255 + da * (255 - sa);

				r = r >> 8;
				g = g >> 8;
				b = b >> 8;
				a = a >> 8;

				if (r > 255)
					r = 255;
				if (g > 255)
					g = 255;
				if (b > 255)
					b = 255;
				if (a > 255)
					a = 255;

				dest[(y + dstY) * dest_stride + (x + dstX) * 4] = r;
				dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1] = g;
				dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2] = b;
				dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3] = a;
			}
	}
	else if (g_compositeOp == PictOpOutReverse)
	{
		MY_ASSERT(bpp == 4);
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				unsigned char sa;
				unsigned char dr, dg, db, da;

				unsigned char m = mask[(y + maskY ) * mask_stride + (x + maskX) * mask_inc];

				unsigned short source_x = x + srcX;
				unsigned short source_y = y + srcY;

				if (source_x > source_width)
					source_x -= source_width;
				if (source_y > source_height)
					source_y -= source_height;

				sa = source[source_y * src_stride + source_x * 4 + 3];
				sa = ((unsigned int)sa * (unsigned int)m) >> 8;

				dr = dest[(y + dstY) * dest_stride + (x + dstX) * 4];
				dg = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1];
				db = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2];
				da = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3];

				unsigned int r, g, b, a;
				r = dr * (255 - sa);
				g = dg * (255 - sa);
				b = db * (255 - sa);
				a = da * (255 - sa);

				r = r >> 8;
				g = g >> 8;
				b = b >> 8;
				a = a >> 8;

				if (r > 255)
					r = 255;
				if (g > 255)
					g = 255;
				if (b > 255)
					b = 255;
				if (a > 255)
					a = 255;

				dest[(y + dstY) * dest_stride + (x + dstX) * 4] = r;
				dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1] = g;
				dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2] = b;
				dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3] = a;
			}
	}
	else
		MY_ASSERT(0);
}

void DoneComposite(PixmapPtr p)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p\n", __FUNCTION__, p);
}

static inline void Download(struct UpDownloadDetails *p)
{
	unsigned char *src = exaGetPixmapAddress(p->m_pPixmap);
	int bpp = p->m_pPixmap->drawable.bitsPerPixel / 8;
	unsigned long src_pitch = exaGetPixmapPitch(p->m_pPixmap);

//	if (!src)
//	{
//		struct DmaPixmap *pInner = (struct DmaPixmap *)exaGetPixmapDriverPrivate(p->m_pPixmap);
//		MY_ASSERT(pInner);
//
//		src = pInner->m_pData;
//		src_pitch = pInner->m_pitchBytes;
//		bpp = pInner->m_bpp / 8;
//	}

//	xf86DrvMsg(0, X_INFO, "Download %p->%p, pitch %ld %d, bpp %d, from %d,%d size %dx%d\n",
//			src, p->m_pImage,
//			src_pitch, p->m_pitch,
//			bpp,
//			p->m_x, p->m_y,
//			p->m_w, p->m_h);

	Copy2D4kSrcInc(p->m_pImage,									//destination
			&src[p->m_y * src_pitch + p->m_x * bpp], 			//source
			p->m_w * bpp,										//x bytes to copy
			p->m_h,												//y rows to copy
			p->m_pitch - p->m_w * bpp,							//add per dest row
			src_pitch - p->m_w * bpp);							//add per source row
}

static inline void Upload(struct UpDownloadDetails *p)
{
	unsigned char *dst = exaGetPixmapAddress(p->m_pPixmap);
	int bpp = p->m_pPixmap->drawable.bitsPerPixel / 8;
	unsigned long dst_pitch = exaGetPixmapPitch(p->m_pPixmap);

//	if (!dst)
//	{
//		struct DmaPixmap *pInner = (struct DmaPixmap *)exaGetPixmapDriverPrivate(p->m_pPixmap);
//		MY_ASSERT(pInner);
//
//		dst = pInner->m_pData;
//		dst_pitch = pInner->m_pitchBytes;
//		bpp = pInner->m_bpp / 8;
//	}

//	xf86DrvMsg(0, X_INFO, "Upload %p<-%p, pitch %ld %d, bpp %d, from %d,%d size %dx%d\n",
//			dst, p->m_pImage,
//			dst_pitch, p->m_pitch,
//			bpp,
//			p->m_x, p->m_y,
//			p->m_w, p->m_h);

	Copy2D4kSrcInc(&dst[p->m_y * dst_pitch + p->m_x * bpp],		//destination
			p->m_pImage,							 			//source
			p->m_w * bpp,										//x bytes to copy
			p->m_h,												//y rows to copy
			dst_pitch - p->m_w * bpp,							//add per dest row
			p->m_pitch - p->m_w * bpp);							//add per source row
}

Bool DownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h,
		char *dst, int dst_pitch)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p->%p (%d, %d %dx%d)\n", __FUNCTION__, pSrc, dst, x, y, w, h);

//	struct DmaPixmap *pInner = (struct DmaPixmap *)exaGetPixmapDriverPrivate(pSrc);
//	if (!pInner)
//		return FALSE;
//	if (!pInner->m_pData)
		return FALSE;

	g_upDownloadDetails.m_up = FALSE;
	g_upDownloadDetails.m_pPixmap = pSrc;
	g_upDownloadDetails.m_x = x;
	g_upDownloadDetails.m_y = y;
	g_upDownloadDetails.m_w = w;
	g_upDownloadDetails.m_h = h;
	g_upDownloadDetails.m_pitch = dst_pitch;
	g_upDownloadDetails.m_pImage = dst;

	exaMarkSync(pSrc->drawable.pScreen);

	Download(&g_upDownloadDetails);

	if (StartDma(g_pDmaBuffer + g_dmaUnkickedHead, FALSE))
		g_dmaUnkickedHead = g_dmaTail;
//	WaitMarker(pSrc->drawable.pScreen, 0);

	return TRUE;
}

Bool UploadToScreen(PixmapPtr pDst, int x, int y, int w, int h,
		char *src, int src_pitch)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p<-%p (%d,%d %dx%d)\n", __FUNCTION__, pDst, src, x, y, w, h);

//	struct DmaPixmap *pInner = (struct DmaPixmap *)exaGetPixmapDriverPrivate(pDst);
//	if (!pInner)
//		return FALSE;
//	if (!pInner->m_pData)
		return FALSE;

	g_upDownloadDetails.m_up = TRUE;
	g_upDownloadDetails.m_pPixmap = pDst;
	g_upDownloadDetails.m_x = x;
	g_upDownloadDetails.m_y = y;
	g_upDownloadDetails.m_w = w;
	g_upDownloadDetails.m_h = h;
	g_upDownloadDetails.m_pitch = src_pitch;
	g_upDownloadDetails.m_pImage = src;

	exaMarkSync(pDst->drawable.pScreen);

	Upload(&g_upDownloadDetails);

	if (StartDma(g_pDmaBuffer + g_dmaUnkickedHead, TRUE))
		g_dmaUnkickedHead = g_dmaTail;

	WaitMarker(pDst->drawable.pScreen, 0);			//seems be necessary

	return TRUE;
}



