#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <time.h>
#include <malloc.h>

#include "xf86.h"
#include "fb.h"
#include "exa.h"

#include "exa_acc.h"

/*struct SysPtrCopy
{
	PixmapPtr m_pPixmap;
	void *m_pSysPtr;
} g_sysPtrCopies[EXA_NUM_PREPARE_INDICES];*/

////////////////////////////////
typedef void* mspace;
void *create_mspace_with_base(void* base, size_t capacity, int locked);
size_t destroy_mspace(mspace msp);
struct mallinfo mspace_mallinfo(mspace msp);

void* mspace_malloc(mspace msp, size_t bytes);
void* mspace_memalign(mspace msp, size_t alignment, size_t bytes);
void mspace_free(mspace msp, void* mem);

mspace g_offscreenMspace = 0;

struct RpiPixmapPriv
{
	void *m_pBuffer;
	int m_mapped;
};

extern _X_EXPORT CARD8 *exaGetPixmapAddressNEW(PixmapPtr pPix)
{
	if (g_offscreenMspace)
	{
		struct RpiPixmapPriv *priv = (struct RpiPixmapPriv *)exaGetPixmapDriverPrivate(pPix);
		if (!priv)
			return 0;
		return priv->m_pBuffer;
	}
	else
		return exaGetPixmapAddress(pPix);
}

void InitOffscreenAlloc(void)
{
	MY_ASSERT(g_offscreenMspace == 0);
	g_offscreenMspace = create_mspace_with_base(GetMemoryBase(), GetMemorySize(), 0);
	MY_ASSERT(g_offscreenMspace != 0);
}

void DestroyOffscreenAlloc(void)
{
	if (g_offscreenMspace == 0)
		return;

	destroy_mspace(g_offscreenMspace);
	g_offscreenMspace = 0;
}

void OffscreenUsedUnused(int *pUsed, int *pMostUsed, int *pUnused)
{
	if (g_offscreenMspace == 0)
	{
		*pUsed = *pMostUsed = *pUnused = 0;
	}
	else
	{
		struct mallinfo info = mspace_mallinfo(g_offscreenMspace);

		MY_ASSERT(pUsed);
		MY_ASSERT(pMostUsed);
		MY_ASSERT(pUnused);

		*pUsed = info.uordblks;
		*pMostUsed = info.usmblks;
		*pUnused = info.fordblks;
	}
}

void *MallocOffscreen(size_t bytes)
{
	MY_ASSERT(g_offscreenMspace != 0);
	mspace_malloc(g_offscreenMspace, bytes);
}

void *MemalignOffscreen(size_t alignment, size_t bytes)
{
	MY_ASSERT(g_offscreenMspace != 0);
	mspace_memalign(g_offscreenMspace, alignment, bytes);
}

void FreeOffscreen(void *ptr)
{
	MY_ASSERT(g_offscreenMspace != 0);
	mspace_free(g_offscreenMspace, ptr);
}

////////////////////////////////

static int blocked = 0;
Bool PrepareAccess(PixmapPtr pPix, int index)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p %d\n", __FUNCTION__, pPix, index);

	/*static int run_once = 0;
	if (!run_once)
	{
		run_once = 1;
		int count;
		for (count = 0; count < EXA_NUM_PREPARE_INDICES; count++)
		{
			g_sysPtrCopies[count].m_pPixmap = 0;
			g_sysPtrCopies[count].m_pSysPtr = 0;
		}
	}

	struct DmaPixmap *pInner = (struct DmaPixmap *)exaGetPixmapDriverPrivate(pPix);
	if (!pInner)
		return FALSE;
	if (!pInner->m_pData)
		return FALSE;

	xf86DrvMsg(0, X_DEFAULT, "%s %p %d\n", __FUNCTION__, pPix, index);
	WaitMarker(pPix->drawable.pScreen, 0);

	//find a slot
	int found = 0;
	int count;

	for (count = 0; count < EXA_NUM_PREPARE_INDICES; count++)
		if (!g_sysPtrCopies[count].m_pPixmap)
		{
			g_sysPtrCopies[count].m_pPixmap = pPix;
			g_sysPtrCopies[count].m_pSysPtr = pPix->devPrivate.ptr;

			pPix->devPrivate.ptr = pInner->m_pData;

			found = 1;
			break;
		}

	MY_ASSERT(found);
*/
	struct RpiPixmapPriv *priv = (struct RpiPixmapPriv *)exaGetPixmapDriverPrivate(pPix);
	if (!priv)
		return FALSE;

	if (pPix->drawable.bitsPerPixel & 7)
		return FALSE;

	WaitMarker(GetScreen(), 0);
	blocked++;

	MY_ASSERT(priv->m_mapped == 0);
	priv->m_mapped++;

	pPix->devPrivate.ptr = priv->m_pBuffer;

	return TRUE;
}

void FinishAccess(PixmapPtr pPix, int index)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p %d\n", __FUNCTION__, pPix, index);

	struct RpiPixmapPriv *priv = (struct RpiPixmapPriv *)exaGetPixmapDriverPrivate(pPix);
	if (!priv || !priv->m_mapped)
		return;

	priv->m_mapped--;
	MY_ASSERT(priv->m_mapped == 0);
	pPix->devPrivate.ptr = 0;

	/*struct DmaPixmap *pInner = (struct DmaPixmap *)exaGetPixmapDriverPrivate(pPix);
	MY_ASSERT(pInner);
	MY_ASSERT(pInner->m_pData);

	//find the slot
	int found = 0;
	int count;

	for (count = 0; count < EXA_NUM_PREPARE_INDICES; count++)
		if (g_sysPtrCopies[count].m_pPixmap == pPix)
		{
			g_sysPtrCopies[count].m_pPixmap = 0;
			pPix->devPrivate.ptr = g_sysPtrCopies[count].m_pSysPtr;

			found = 1;
			break;
		}

	MY_ASSERT(found);*/

	MY_ASSERT(!IsPendingUnkicked());
	blocked--;
}

void *CreatePixmap(ScreenPtr pScreen, int size, int align)
{
	struct RpiPixmapPriv *priv = malloc(sizeof(struct RpiPixmapPriv));

	if (!priv)
		return 0;

	priv->m_pBuffer = 0;
	priv->m_mapped = 0;

	if (!size)
		return priv;

	//priv->m_pBuffer = kern_alloc(size);
	void *ptr = 0;
	if (align == 0)
		ptr = MallocOffscreen(size);
	else
		MY_ASSERT(0);

	if (ptr)
	{
		priv->m_pBuffer = ptr;
	}
	else
	{
		free(priv);
		return 0;
	}

	return priv;
}

/*void *CreatePixmap2(ScreenPtr pScreen, int width, int height,
                            int depth, int usage_hint, int bitsPerPixel,
                            int *new_fb_pitch)
{
	struct DmaPixmap *pInner = malloc(sizeof(struct DmaPixmap));

	if (!pInner)
		return 0;

	pInner->m_width = width;
	pInner->m_height = height;
	pInner->m_depth = depth;
	pInner->m_bpp = bitsPerPixel;
	pInner->m_pitchBytes = width * bitsPerPixel / 8;

	MY_ASSERT(new_fb_pitch);

	if (width == 0 || height == 0)
	{
		pInner->m_pData = 0;
		*new_fb_pitch = 0;
		return pInner;
	}

	pInner->m_pData = malloc(height * pInner->m_pitchBytes);
	*new_fb_pitch = pInner->m_pitchBytes;

	if (!pInner->m_pData)
	{
		free(pInner);
		return FALSE;
	}

	return pInner;
}*/

void DestroyPixmap(ScreenPtr pScreen, void *driverPriv)
{
	/*struct DmaPixmap *pInner = (struct DmaPixmap *)driverPriv;

	if (!pInner)
		return;

	free(pInner->m_pData);
	free(pInner);*/

	struct RpiPixmapPriv *priv = (struct RpiPixmapPriv *)driverPriv;

	if (!priv)
		return;

	if (priv->m_pBuffer)
	{
		WaitMarker(GetScreen(), 0);

		//kern_free(priv->m_pBuffer);

		FreeOffscreen(priv->m_pBuffer);

		priv->m_pBuffer = 0;
		free(priv);
	}
}

Bool PixmapIsOffscreen(PixmapPtr pPix)
{
	/*if (!pPix)
		return FALSE;
	if (!exaGetPixmapDriverPrivate(pPix))
		return FALSE;

	return TRUE;*/

	struct RpiPixmapPriv *priv = (struct RpiPixmapPriv *)exaGetPixmapDriverPrivate(pPix);

	if (!priv)
		return FALSE;
	if (priv->m_pBuffer)
		return TRUE;
	return FALSE;
}

Bool ModifyPixmapHeader(PixmapPtr pPixmap, int width, int height,
                                int depth, int bitsPerPixel, int devKind,
                                pointer pPixData)
{
	if (!pPixmap)
		return FALSE;

	struct DmaPixmap *pInner = (struct DmaPixmap *)exaGetPixmapDriverPrivate(pPixmap);
	if (!pInner)
		return FALSE;

	if (width > 0)
		pInner->m_width = width;

	if (height > 0)
		pInner->m_height = height;

	if (depth > 0)
		pInner->m_depth = depth;

	if (bitsPerPixel > 0)
		pInner->m_bpp = bitsPerPixel;

	if (pPixData)
		pInner->m_pData = pPixData;

	if (devKind > 0)
		pInner->m_pitchBytes = devKind;

	pPixmap->drawable.width = pInner->m_width;
	pPixmap->drawable.height = pInner->m_height;
	pPixmap->drawable.bitsPerPixel = pInner->m_bpp;
	pPixmap->drawable.depth = pInner->m_depth;
	pPixmap->devPrivate.ptr = pInner->m_pData;
	pPixmap->devKind = pInner->m_pitchBytes;

	return TRUE;
}
