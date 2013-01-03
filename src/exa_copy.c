/*
 * exa_copy.c
 *
 *  Created on: 5 Aug 2012
 *      Author: Simon Hall
 * Implements the EXA copy functionality.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa.h"

#include "exa_acc.h"

//#define COPY_DEBUG
#define CALL_RECORDING
#define COPY_FALLBACK 100

struct CopyDetails
{
	PixmapPtr m_pSrc;
	PixmapPtr m_pDst;
	int m_dx, m_dy;
	int m_bpp;
	int m_syncPoint;
} g_copyDetails;


Bool PrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int dx,
		int dy, int alu, Pixel planemask)
{
#ifdef COPY_DEBUG
	xf86DrvMsg(0, X_INFO, "%s pixmaps %p->%p\n", __FUNCTION__, pSrcPixmap, pDstPixmap);
#endif
//	return FALSE;
#ifdef CALL_RECORDING
	RecordPrepareCopy();
#endif

	//check they're valid pointers
	if ((pDstPixmap == NULL) || (pSrcPixmap == NULL))
	{
		xf86DrvMsg(0, X_WARNING, "%s (pPixmapDst == NULL) || (pPixmapSrc == NULL)\n", __FUNCTION__);
		return FALSE;
	}

	if (exaGetPixmapAddressNEW(pDstPixmap) == 0 && exaGetPixmapAddressNEW(pSrcPixmap) == 0)
	{
		xf86DrvMsg(0, X_WARNING, "%s source %p dest %p\n", __FUNCTION__, exaGetPixmapAddressNEW(pDstPixmap), exaGetPixmapAddressNEW(pSrcPixmap));
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

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif

	return TRUE;
}

void Copy(PixmapPtr pDstPixmap, int srcX, int srcY,
		int dstX, int dstY, int width, int height)
{
#ifdef COPY_DEBUG
	xf86DrvMsg(0, X_DEFAULT, "%s dest pixmap %p, source %d,%d, dest %d,%d, dims %dx%d\n",
			__FUNCTION__, pDstPixmap, srcX, srcY, dstX, dstY, width, height);
#endif
	MY_ASSERT(pDstPixmap == g_copyDetails.m_pDst);
	unsigned char *pSrc, *pDst;
	pSrc = exaGetPixmapAddressNEW(g_copyDetails.m_pSrc);
	pDst = exaGetPixmapAddressNEW(g_copyDetails.m_pDst);

	unsigned long srcPitch, dstPitch;
	srcPitch = exaGetPixmapPitch(g_copyDetails.m_pSrc);
	dstPitch = exaGetPixmapPitch(g_copyDetails.m_pDst);

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif

#ifdef CALL_RECORDING
	RecordCopy(width * height);
#endif


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
		if (width * height < COPY_FALLBACK && !IsPendingUnkicked() && !IsDmaPending())
		{
#ifdef COPY_DEBUG
			xf86DrvMsg(0, X_DEFAULT, "copy fallback\n");
#endif
			Copy2D4kSrcInc_fallback(&pDst[dstY * dstPitch + dstX * g_copyDetails.m_bpp],
					&pSrc[+ srcY * srcPitch + srcX * g_copyDetails.m_bpp],
					width * g_copyDetails.m_bpp,
					height,
					dstPitch - width * g_copyDetails.m_bpp,
					srcPitch - width * g_copyDetails.m_bpp);
		}
		else
			Copy2D4kSrcInc(&pDst[dstY * dstPitch + dstX * g_copyDetails.m_bpp],
					&pSrc[+ srcY * srcPitch + srcX * g_copyDetails.m_bpp],
					width * g_copyDetails.m_bpp,
					height,
					dstPitch - width * g_copyDetails.m_bpp,
					srcPitch - width * g_copyDetails.m_bpp);
	}

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif
}

void DoneCopy(PixmapPtr p)
{
	MY_ASSERT(g_copyDetails.m_pDst == p);
#ifdef CALL_RECORDING
	RecordDoneCopy();
#endif

#ifdef COPY_DEBUG
	xf86DrvMsg(0, X_DEFAULT, "%s pixmap %p\n", __FUNCTION__, p);
#endif

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif

	if (IsPendingUnkicked())
	{
		if (StartDma(GetUnkickedDmaHead(), FALSE))
			UpdateKickedDmaHead();
	}

	//there may be nothing pending, as work may have been kicked when
	//trying to allocate a block
	//todo do a count to see if Copy was actually called and any async work was enqueued
	exaMarkSync(g_copyDetails.m_pDst->drawable.pScreen);
}

