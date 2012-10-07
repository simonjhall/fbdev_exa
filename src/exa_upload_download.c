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
//	xf86DrvMsg(0, X_INFO, "%s %p->%p (%d, %d %dx%d)\n", __FUNCTION__, pSrc, dst, x, y, w, h);

	g_upDownloadDetails.m_up = FALSE;
	g_upDownloadDetails.m_pPixmap = pSrc;
	g_upDownloadDetails.m_x = x;
	g_upDownloadDetails.m_y = y;
	g_upDownloadDetails.m_w = w;
	g_upDownloadDetails.m_h = h;
	g_upDownloadDetails.m_pitch = dst_pitch;
	g_upDownloadDetails.m_pImage = dst;

	Download(&g_upDownloadDetails);

	//mark as async work done
	exaMarkSync(pSrc->drawable.pScreen);

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif

	if (StartDma(GetUnkickedDmaHead(), FALSE))
		UpdateKickedDmaHead();

	return TRUE;
}

Bool UploadToScreen(PixmapPtr pDst, int x, int y, int w, int h,
		char *src, int src_pitch)
{
//	return FALSE;
//	xf86DrvMsg(0, X_INFO, "%s %p<-%p (%d,%d %dx%d)\n", __FUNCTION__, pDst, src, x, y, w, h);

	g_upDownloadDetails.m_up = TRUE;
	g_upDownloadDetails.m_pPixmap = pDst;
	g_upDownloadDetails.m_x = x;
	g_upDownloadDetails.m_y = y;
	g_upDownloadDetails.m_w = w;
	g_upDownloadDetails.m_h = h;
	g_upDownloadDetails.m_pitch = src_pitch;
	g_upDownloadDetails.m_pImage = src;

	Upload(&g_upDownloadDetails);

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif

	if (StartDma(GetUnkickedDmaHead(), TRUE))
		UpdateKickedDmaHead();

	WaitMarker(GetScreen(), 0);			//there's clearly something to wait on
	//nothing pending here, so don't mark

	return TRUE;
}
