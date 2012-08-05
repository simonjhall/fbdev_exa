/*
 * exa_upload_download.c
 *
 *  Created on: 5 Aug 2012
 *      Author: simon
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa_acc.h"
#include "exa.h"

struct UpDownloadDetails
{
	Bool m_up;
	PixmapPtr m_pPixmap;
	int m_x, m_y;
	int m_w, m_h;
	char *m_pImage;
	int m_pitch;
} g_upDownloadDetails;

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

	if (StartDma(GetUnkickedDmaHead(), FALSE))
		UpdateKickedDmaHead();
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

	if (StartDma(GetUnkickedDmaHead(), TRUE))
		UpdateKickedDmaHead();

	WaitMarker(pDst->drawable.pScreen, 0);			//seems be necessary

	return TRUE;
}
