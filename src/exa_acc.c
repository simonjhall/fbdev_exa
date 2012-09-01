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

ScreenPtr g_pScreen = 0;

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

BOOL IsDmaPending(void)
{
	if (g_dmaPending)
		return TRUE;
	else
		return FALSE;
}

/******* DMA ********/
/**** starting ******/

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

	if (WaitDma(force))
	{
		MY_ASSERT(IsDmaPending() == FALSE);

		g_dmaPending = TRUE;			//stuff's in the system and we need to ensure we don't overload our DMA controller
		g_headOfDma = TRUE;				//allocations shouldn't patch up previous DMA CBs

		MY_ASSERT(g_pEmuCB == 0);
		g_pEmuCB = pCB;
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
	if(IsDmaPending())
	{
		MY_ASSERT(g_pEmuCB);
		EmulateDma(g_pEmuCB);
		g_pEmuCB = 0;

		ClearBytesPending();
	}
}

inline void RealWaitDma(unsigned int bytesPending)
{
	if(IsDmaPending())
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
		MY_ASSERT(IsPendingUnkicked() || IsDmaPending() == TRUE);

		if (IsPendingUnkicked())
		{
			if (StartDma(GetUnkickedDmaHead(), TRUE))
				UpdateKickedDmaHead();
			else
				MY_ASSERT(0);
			//pending is true
		}

		exaWaitSync(GetScreen());
		//head will be reset, but we need to clear it before the return
		g_headOfDma = FALSE;
		MY_ASSERT(g_dmaTail == 0);
		return &g_pDmaBuffer[g_dmaTail++];
	}
}

unsigned char *AllocSolidBuffer(unsigned int bytes)
{
	const unsigned int solid_size = 4096 * 10;		//page multiple

	//allocate some solid memory
	if (!g_pSolidBuffer)
	{
		g_pSolidBuffer = kern_alloc(solid_size);
		MY_ASSERT(g_pSolidBuffer);
		ResetSolidBuffer();
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

void ResetSolidBuffer(void)
{
	g_solidOffset = 0;
}

inline ScreenPtr GetScreen(void)
{
	MY_ASSERT(g_pScreen);
	return g_pScreen;
}

void SetScreen(ScreenPtr p)
{
	g_pScreen = p;
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
	return marker++;
}

void WaitMarker(ScreenPtr pScreen, int Marker)
{
	time_t start = clock();

//	static int dmas = 0;
	//int kick = RunDma(g_pDmaBuffer);
//	int kick = StartDma(g_pDmaBuffer);
	if (IsPendingUnkicked())
	{
		StartDma(GetUnkickedDmaHead(), TRUE);
		UpdateKickedDmaHead();
	}

	if (IsDmaPending())
		WaitDma(TRUE);
//	dmas += kick;

	/*xf86DrvMsg(0, X_INFO, "kick %d dmas, solid %d, took %.3f ms\n", g_dmaTail, g_solidOffset,
			(float)(clock() - start) / CLOCKS_PER_SEC * 1000.0f);*/

	g_dmaTail = 0;			//write into CB 0
	g_dmaUnkickedHead = 0;	//and the head of any work starts again at zero
	g_headOfDma = TRUE;			//patch up -1 CB next pointers

	static unsigned int last_starts = 0;
	if (g_actualStarts - last_starts > 1000)
	{
		last_starts = g_actualStarts;
		fprintf(stderr, "actual s/w %d %d, forced s/w %d %d, unforced s/w %d %d, average kick %d %d bytes\n",
				g_actualStarts, g_actualWaits, g_forcedStarts, g_forcedWaits, g_unforcedStarts, g_unforcedWaits,
				g_totalBytesPendingForced / g_forcedStarts, g_totalBytesPendingUnforced / g_unforcedStarts);
	}
}

