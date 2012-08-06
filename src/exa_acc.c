#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <time.h>

#include "xf86.h"
#include "fb.h"
#include "exa.h"

#include "exa_acc.h"
#include "exa_copylinear_copy2d.inl"

/******** GENERIC STUFF ******/


struct SolidDetails
{
	PixmapPtr m_pDst;
	int m_bpp;
	Pixel m_toFill;
	int m_syncPoint;
} g_solidDetails;

struct DmaControlBlock *g_pDmaBuffer = 0;	//all dma control blocks
unsigned int g_dmaUnkickedHead = 0;		//first unkicked control block (potentially pointing to unused entry)
unsigned int g_dmaTail = 0;				//first unused dma entry
unsigned char *g_pSolidBuffer = 0;			//all solid pixels
unsigned int g_solidOffset = 0;			//amount of solid bytes in use
unsigned int g_highestSolid = 0;
BOOL g_dmaPending = FALSE;
unsigned int g_bytesPending = 0;
unsigned int g_totalBytesPendingForced = 0;
unsigned int g_totalBytesPendingUnforced = 0;
BOOL g_headOfDma = TRUE;

//stats
unsigned int g_forcedWaits = 0;
unsigned int g_unforcedWaits = 0;
unsigned int g_forcedStarts = 0;
unsigned int g_unforcedStarts = 0;
unsigned int g_actualWaits = 0;
unsigned int g_actualStarts = 0;

struct DmaControlBlock *g_pEmuCB = 0;

BOOL IsPendingUnkicked(void)
{
	if (g_dmaUnkickedHead != g_dmaTail)
		return TRUE;
	else
		return FALSE;
}

void UpdateKickedDmaHead(void)
{
	g_dmaUnkickedHead = g_dmaTail;
}

struct DmaControlBlock *GetUnkickedDmaHead(void)
{
	return g_pDmaBuffer + g_dmaUnkickedHead;
}

void ClearBytesPending(void)
{
	g_bytesPending = 0;
}

unsigned int GetBytesPending(void)
{
	return g_bytesPending;
}

void AddBytesPending(unsigned int a)
{
	g_bytesPending += a;
}

/******* DMA ********/
/**** starting ******/

inline int RunDma(struct DmaControlBlock *pCB)
{
	MY_ASSERT(0);
	if (pCB->m_pNext == (void *)0xcdcdcdcd)
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
		g_totalBytesPendingForced += GetBytesPending();
	}
	else
	{
		g_unforcedStarts++;
		g_totalBytesPendingUnforced += GetBytesPending();
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

	if (force || (GetBytesPending() >= 8192))
//	if (1)
	{
		RealWaitDma(GetBytesPending());
//		EmulateWaitDma();
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

		ClearBytesPending();
	}
}

inline void RealWaitDma(unsigned int bytesPending)
{
	if(g_dmaPending)
	{
		MY_ASSERT(g_pEmuCB);
		kern_dma_wait_all(GetBytesPending());
		g_pEmuCB = 0;
		g_actualWaits++;
		ClearBytesPending();
	}
}

/****** DMA allocation ****/

struct DmaControlBlock *GetBaseDmaBlock(void)
{
	return g_pDmaBuffer;
}

void SetBaseDmaBlock(struct DmaControlBlock *p)
{
	MY_ASSERT(p);
	g_pDmaBuffer = p;
}

struct DmaControlBlock *AllocDmaBlock(void)
{
	//get some dma-able memory
	if (!GetBaseDmaBlock())
	{
		SetBaseDmaBlock((struct DmaControlBlock *)kern_alloc(4096 * sizeof(struct DmaControlBlock)));
	}

	BOOL wasHead = g_headOfDma;
	g_headOfDma = FALSE;

	if (g_dmaTail == 0)
		return &g_pDmaBuffer[g_dmaTail++];			//first in the list, just return it
	else if (g_dmaTail < 4096)
	{
		if (!wasHead)						//patch up everything bar the head
			g_pDmaBuffer[g_dmaTail - 1].m_pNext = &g_pDmaBuffer[g_dmaTail];
		return &g_pDmaBuffer[g_dmaTail++];
	}
	else
	{	//out of list space, kick the lot and start fresh
		MY_ASSERT(IsPendingUnkicked() || g_dmaPending == TRUE);

		if (IsPendingUnkicked())
		{
			if (StartDma(GetUnkickedDmaHead(), TRUE))
				UpdateKickedDmaHead();
			else
				MY_ASSERT(0);
			//pending is true
		}

		WaitMarker(0, 0);
		//head will be reset, but we need to clear it before the return
		g_headOfDma = FALSE;
		MY_ASSERT(g_dmaTail == 0);
		return &g_pDmaBuffer[g_dmaTail++];
	}
}

inline unsigned char *AllocSolidBuffer(unsigned int bytes)
{
	const unsigned int solid_size = 4096 * 10;		//page multiple

	//allocate some solid memory
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
	if (IsPendingUnkicked())
	{
		StartDma(GetUnkickedDmaHead(), TRUE);
		UpdateKickedDmaHead();
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

	//get some solid space
	while (!(pSolid = AllocSolidBuffer(32)))
	{
		fprintf(stderr, "unable to allocate solid space - kicking and waiting\n");

		//there could be non-committed work taking up all the solid
		if (IsPendingUnkicked())
		{
			if (StartDma(GetUnkickedDmaHead(), TRUE))	//force
				UpdateKickedDmaHead();
			else
				MY_ASSERT(0);

			//dma pending will now be set
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

	//dma will overread
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

	if (IsPendingUnkicked())
	{
		//give it a sync point (for future work)
		exaMarkSync(g_solidDetails.m_pDst->drawable.pScreen);

		if (StartDma(GetUnkickedDmaHead(), FALSE))
			UpdateKickedDmaHead();
//		WaitMarker(0, 0);
	}
}
