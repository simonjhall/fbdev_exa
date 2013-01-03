/*
 * Author: Simon Hall
 * This is the main controlling file for the driver. Managed DMA kicks and offscreen memory management.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "xf86.h"
#include "fb.h"
#include "exa.h"

#include "exa_acc.h"
#include "exa_copylinear_copy2d.inl"

//#define EMULATE_DMA

#define CALL_RECORDING

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
unsigned int g_maxAxiBurst = 0;			//maximum axi burst suggested by the driver

//stats
unsigned int g_forcedWaits = 0;				//WaitDma forced to happen
unsigned int g_unforcedWaits = 0;			//WaitDma not forced to happen
unsigned int g_forcedStarts = 0;			//StartDma forced to start
unsigned int g_unforcedStarts = 0;			//StartDma not forced to start
unsigned int g_actualWaits = 0;				//amount of actual DMA waits
unsigned int g_actualStarts = 0;			//amount of actual DMA kicks

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

unsigned int GetCbsPending(void)
{
	return g_dmaTail - g_dmaUnkickedHead;
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

/**** validation ****/
void ValidateCbList(struct DmaControlBlock *pCB)
{
	int count = 0;
	while (pCB)
	{
		unsigned char *pSource = (unsigned char *)pCB->m_pSourceAddr;
		unsigned char *pDest = (unsigned char *)pCB->m_pDestAddr;

		//verifying that the data equals itself is not what we're after, we just want to see if we can dereference the pointers
		MY_ASSERT((*(volatile unsigned char *)pSource, 1));
		MY_ASSERT((*(volatile unsigned char *)pDest, 1));

		pCB = pCB->m_pNext;

		count++;
	}

#ifdef CALL_RECORDING
	RecordCbValidate(count);
#endif
}

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
#ifdef CB_VALIDATION
		ValidateCbList(pCB);
#endif
#ifndef EMULATE_DMA
		if (kern_dma_prepare_kick(pCB))
			MY_ASSERT(0);
#endif
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

//	fprintf(stderr, "%d bytes pending %d cbs pending\n", GetBytesPending(), GetCbsPending());

	if (force || (GetBytesPending() >= 8192) || (GetCbsPending() >= 100))
	{
#ifdef EMULATE_DMA
		EmulateWaitDma();
#else
		RealWaitDma(GetBytesPending());
#endif
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
#ifdef CB_VALIDATION
		ValidateCbList(g_pEmuCB);
#endif
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

		WaitMarker(GetScreen(), 0);				//this must happen

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
//		xf86DrvMsg(0, X_INFO, "ran out of solid space...hopefully flushing\n");
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

unsigned long g_devMemBase = 0;
unsigned long g_devMemBaseHigh = 0;

static CARD8 *g_pOffscreenBase = 0;
static unsigned long g_offscreenSize = 0;
static unsigned long g_vpuCodeSize = 0;
static void *g_pVpuMemoryBase = 0;

static unsigned long g_busOffset = 0;

BOOL g_vpuOffload = FALSE;

static FILE *dev_mem_file = 0;

int CreateOffscreenMemory(enum OffscreenMemMode m)
{
	if (g_offscreenSize < 1048576)
	{
		xf86DrvMsg(0, X_ERROR, "memory size of %d bytes is too small to be useable (realistically you need more like >4MB)\n", g_offscreenSize);
		return 1;
	}

	switch (m)
	{
	case kDevDmaer:
		g_pOffscreenBase = kern_alloc(g_offscreenSize);
		break;

	case kDevMem:
	{
		dev_mem_file = fopen("/dev/mem", "r+b");
		if (!dev_mem_file)
		{
			xf86DrvMsg(0, X_ERROR, "failed to open /dev/mem\n");
			return 1;
		}

		unsigned long offset = g_devMemBase;
		unsigned long size = g_offscreenSize;

		//the first byte we can't read
		g_devMemBaseHigh = g_devMemBase + g_offscreenSize;

		//map in the reserved piece of memory *with a virtual address that looks exactly the same as the physical address*
		void *ptr = mmap((void *)offset, size + g_vpuCodeSize,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				fileno(dev_mem_file),
				offset);

		if (ptr != (void *)offset)
		{
			g_busOffset = offset - (unsigned long)ptr + 0x40000000;
			kern_set_min_max_phys(ptr, (void *)((unsigned long)ptr + size), g_busOffset);
			MY_ASSERT(!IsEnabledVpuOffload());
		}
		else
		{
			g_busOffset = 0x40000000;
			kern_set_min_max_phys((void *)offset, (void *)(offset + size), g_busOffset);		//add this to get the 0x4 bus address
			g_pVpuMemoryBase = (void *)&((unsigned char *)ptr)[size];
		}

		g_pOffscreenBase = (CARD8 *)ptr;

		break;
	}

	case kCma:
	{
		//basic check
		if (g_offscreenSize & 4095)
		{
			xf86DrvMsg(0, X_ERROR, "VC requires memory size to be multiple of page size (4096 bytes)\n");
			return 1;
		}

		dev_mem_file = fopen("/dev/mem", "r+b");
		if (!dev_mem_file)
		{
			xf86DrvMsg(0, X_ERROR, "failed to open /dev/mem\n");
			return 1;
		}

		//try and allocate the memory from VC
		void *arm_phys = kern_cma_set_size(g_offscreenSize + g_vpuCodeSize);

		if (!arm_phys)
		{
			xf86DrvMsg(0, X_ERROR, "failed to allocate memory from VC\n");
			return 1;
		}

		unsigned long size = g_offscreenSize;

		//map in the reserved piece of memory *with a virtual address that looks exactly the same as the physical address*
		void *ptr = mmap(arm_phys, size + g_vpuCodeSize,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				fileno(dev_mem_file),
				(unsigned long)arm_phys);

		if (ptr != arm_phys)
		{
			g_busOffset = (unsigned long)arm_phys - (unsigned long)ptr;
			kern_set_min_max_phys(ptr, (void *)((unsigned long)ptr + size), g_busOffset);
			MY_ASSERT(!IsEnabledVpuOffload());
		}
		else
		{
			g_busOffset = 0;
			kern_set_min_max_phys(arm_phys, (void *)((unsigned long)arm_phys + size), g_busOffset);		//should include the prefix to choose which region we're in
			g_pVpuMemoryBase = (void *)&((unsigned char *)ptr)[size];
		}

		g_devMemBase = (unsigned long)ptr;
		//the first byte we can't read
		g_devMemBaseHigh = g_devMemBase + g_offscreenSize;

		g_pOffscreenBase = (CARD8 *)ptr;

		break;
	}

	default:
		MY_ASSERT(0);
		break;
	};

	if (!g_pOffscreenBase)
	{
		xf86DrvMsg(0, X_ERROR, "failed to allocate and map memory\n");
		return 1;
	}

	return 0;
}

void DestroyOffscreenMemory(void);

void *GetMemoryBase(void)
{
	MY_ASSERT(g_pOffscreenBase);
	return g_pOffscreenBase;
}

void *GetVpuMemoryBase(void)
{
	MY_ASSERT(IsEnabledVpuOffload());
	return g_pVpuMemoryBase;
}

unsigned long GetBusOffset(void)
{
	return g_busOffset;
}

void FreeMemoryBase(void)
{
	//change this as this no longer is correct for reserved memory (just unmap and close the file)
	MY_ASSERT(0);

	if (g_pOffscreenBase)
	{
		g_pOffscreenBase = kern_free(g_pOffscreenBase);
		MY_ASSERT(!g_pOffscreenBase);
	}
}

void SetMaxAxiBurst(unsigned int v)
{
	g_maxAxiBurst = v;
}

unsigned long GetMemorySize(void)
{
	return g_offscreenSize;
}

void SetMemoryBase(unsigned long p)
{
	MY_ASSERT(!g_devMemBase);

	if (!g_devMemBase)
		g_devMemBase = p;
}

void SetMemorySize(unsigned long s)
{
	if (!g_pOffscreenBase)
		g_offscreenSize = s;

	MY_ASSERT(!g_pOffscreenBase);
}

void SetVpuCodeSize(unsigned long s)
{
	//round it up to a page
	g_vpuCodeSize = (s + 4095) & ~4095;

	//and add one just for some slack (unpacked solids)
	g_vpuCodeSize += 4096;

	//and another two for composite ops
	g_vpuCodeSize += 4096 * 2;
}

unsigned long GetVpuCodeSize(void)
{
	return g_vpuCodeSize;
}

void EnableVpuOffload(BOOL b)
{
	g_vpuOffload = b;
}

BOOL IsEnabledVpuOffload(void)
{
	return g_vpuOffload;
}

int MarkSync(ScreenPtr pScreen)
{
	static int marker = 0;
	return marker++;
}

void WaitMarker(ScreenPtr pScreen, int Marker)
{
//	xf86DrvMsg(0, X_INFO, "%s\n", __FUNCTION__);
//	time_t start = clock();

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

//	static unsigned int last_starts = 0;
//	if (g_actualStarts - last_starts > 1000)
//	{
//		last_starts = g_actualStarts;
//		xf86DrvMsg(0, X_INFO, "actual s/w %d %d, forced s/w %d %d, unforced s/w %d %d, average kick %d %d bytes\n",
//				g_actualStarts, g_actualWaits, g_forcedStarts, g_forcedWaits, g_unforcedStarts, g_unforcedWaits,
//				g_totalBytesPendingForced / g_forcedStarts, g_totalBytesPendingUnforced / g_unforcedStarts);
//	}

#ifdef CALL_RECORDING
	RecordWait(0);

	static int calls = 0;
	if (calls > 50)
	{
		calls = 0;
		RecordPrint();
		RecordReset();
	}
	else
		calls++;
#endif
}

