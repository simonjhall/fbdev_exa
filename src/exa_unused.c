#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <time.h>

#include "xf86.h"
#include "fb.h"
#include "exa.h"

#include "exa_acc.h"

struct SysPtrCopy
{
	PixmapPtr m_pPixmap;
	void *m_pSysPtr;
} g_sysPtrCopies[EXA_NUM_PREPARE_INDICES];

Bool PrepareAccess(PixmapPtr pPix, int index)
{
	static int run_once = 0;
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

	return TRUE;
}

void FinishAccess(PixmapPtr pPix, int index)
{
	xf86DrvMsg(0, X_DEFAULT, "%s %p %d\n", __FUNCTION__, pPix, index);

	struct DmaPixmap *pInner = (struct DmaPixmap *)exaGetPixmapDriverPrivate(pPix);
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

	MY_ASSERT(found);
}

void *CreatePixmap(ScreenPtr pScreen, int size, int align)
{
	return 0;
}

void *CreatePixmap2(ScreenPtr pScreen, int width, int height,
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
}

void DestroyPixmap(ScreenPtr pScreen, void *driverPriv)
{
	struct DmaPixmap *pInner = (struct DmaPixmap *)driverPriv;

	if (!pInner)
		return;

	free(pInner->m_pData);
	free(pInner);
}

Bool PixmapIsOffscreen(PixmapPtr pPix)
{
	if (!pPix)
		return FALSE;
	if (!exaGetPixmapDriverPrivate(pPix))
		return FALSE;

	return TRUE;
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
