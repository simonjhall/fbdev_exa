/*
 * exa_upload_download.c
 *
 *  Created on: 5 Aug 2012
 *      Author: Simon Hall
 * Implements the EXA upload/download functionality with either DMA or the CPU fallback
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa_acc.h"
#include "exa.h"

//#define UPDOWNLOAD_DEBUG
#define CALL_RECORDING
#define UPDOWNLOAD_FALLBACK 100

struct UpDownloadDetails
{
	Bool m_up;
	PixmapPtr m_pPixmap;
	int m_x, m_y;
	int m_w, m_h;
	char *m_pImage;
	int m_pitch;
} g_upDownloadDetails;

static inline void Download(struct UpDownloadDetails *p, int fallback)
{
	unsigned char *src = exaGetPixmapAddressNEW(p->m_pPixmap);
	int bpp = p->m_pPixmap->drawable.bitsPerPixel / 8;
	unsigned long src_pitch = exaGetPixmapPitch(p->m_pPixmap);

	if (fallback)
	{
#ifdef UPDOWNLOAD_DEBUG
		xf86DrvMsg(0, X_DEFAULT, "download fallback\n");
#endif
		Copy2D4kSrcInc_fallback(p->m_pImage,						//destination
				&src[p->m_y * src_pitch + p->m_x * bpp], 			//source
				p->m_w * bpp,										//x bytes to copy
				p->m_h,												//y rows to copy
				p->m_pitch - p->m_w * bpp,							//add per dest row
				src_pitch - p->m_w * bpp);							//add per source row
	}
	else
		Copy2D4kSrcInc(p->m_pImage,									//destination
				&src[p->m_y * src_pitch + p->m_x * bpp], 			//source
				p->m_w * bpp,										//x bytes to copy
				p->m_h,												//y rows to copy
				p->m_pitch - p->m_w * bpp,							//add per dest row
				src_pitch - p->m_w * bpp);							//add per source row
}

static inline void Upload(struct UpDownloadDetails *p, int fallback)
{
	unsigned char *dst = exaGetPixmapAddressNEW(p->m_pPixmap);
	int bpp = p->m_pPixmap->drawable.bitsPerPixel / 8;
	unsigned long dst_pitch = exaGetPixmapPitch(p->m_pPixmap);

	if (fallback)
	{
#ifdef UPDOWNLOAD_DEBUG
		xf86DrvMsg(0, X_DEFAULT, "upload fallback\n");
#endif
		Copy2D4kSrcInc_fallback(&dst[p->m_y * dst_pitch + p->m_x * bpp],		//destination
				p->m_pImage,							 			//source
				p->m_w * bpp,										//x bytes to copy
				p->m_h,												//y rows to copy
				dst_pitch - p->m_w * bpp,							//add per dest row
				p->m_pitch - p->m_w * bpp);							//add per source row
	}
	else
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
//	return FALSE;
#ifdef UPDOWNLOAD_DEBUG
	xf86DrvMsg(0, X_INFO, "%s %p->%p (%d, %d %dx%d)\n", __FUNCTION__, pSrc, dst, x, y, w, h);
#endif

	g_upDownloadDetails.m_up = FALSE;
	g_upDownloadDetails.m_pPixmap = pSrc;
	g_upDownloadDetails.m_x = x;
	g_upDownloadDetails.m_y = y;
	g_upDownloadDetails.m_w = w;
	g_upDownloadDetails.m_h = h;
	g_upDownloadDetails.m_pitch = dst_pitch;
	g_upDownloadDetails.m_pImage = dst;


	if (w * h < UPDOWNLOAD_FALLBACK && !IsPendingUnkicked() && !IsDmaPending())
	{
#ifdef CALL_RECORDING
		RecordDownload(w * h, 1);
#endif
		Download(&g_upDownloadDetails, 1);
	}
	else
	{
#ifdef CALL_RECORDING
		RecordDownload(w * h, 0);
#endif
		Download(&g_upDownloadDetails, 0);

		//mark as async work done
		exaMarkSync(pSrc->drawable.pScreen);

#ifdef CB_VALIDATION
		if (IsPendingUnkicked())
			ValidateCbList(GetUnkickedDmaHead());
#endif

		if (IsPendingUnkicked())
			if (StartDma(GetUnkickedDmaHead(), FALSE))
				UpdateKickedDmaHead();
	}

	return TRUE;
}

Bool UploadToScreen(PixmapPtr pDst, int x, int y, int w, int h,
		char *src, int src_pitch)
{
//	return FALSE;
#ifdef UPDOWNLOAD_DEBUG
	xf86DrvMsg(0, X_INFO, "%s %p<-%p (%d,%d %dx%d)\n", __FUNCTION__, pDst, src, x, y, w, h);
#endif

#ifdef CALL_RECORDING
	RecordUpload(w * h);
#endif

	g_upDownloadDetails.m_up = TRUE;
	g_upDownloadDetails.m_pPixmap = pDst;
	g_upDownloadDetails.m_x = x;
	g_upDownloadDetails.m_y = y;
	g_upDownloadDetails.m_w = w;
	g_upDownloadDetails.m_h = h;
	g_upDownloadDetails.m_pitch = src_pitch;
	g_upDownloadDetails.m_pImage = src;

	if (!exaGetPixmapAddressNEW(pDst))
	{
		struct RpiPixmapPriv *priv = (struct RpiPixmapPriv *)exaGetPixmapDriverPrivate(pDst);
		fprintf(stderr, "no pointer, priv is %p\n", priv);
		MY_ASSERT(0);
	}

	if (w * h < UPDOWNLOAD_FALLBACK && !IsPendingUnkicked() && !IsDmaPending())
		Upload(&g_upDownloadDetails, 1);
	else
	{
		Upload(&g_upDownloadDetails, 0);

		/* testing - doing upload asynchronously
		 * can sometimes cause crashes within the X server
		 */

		//mark as async work done
		exaMarkSync(pDst->drawable.pScreen);

#ifdef CB_VALIDATION
		if (IsPendingUnkicked())
			ValidateCbList(GetUnkickedDmaHead());
#endif

		if (IsPendingUnkicked())
			if (StartDma(GetUnkickedDmaHead(), TRUE))
				UpdateKickedDmaHead();

	//	WaitMarker(GetScreen(), 0);			//there's clearly something to wait on
		//nothing pending here, so don't mark
	}

	return TRUE;
}
