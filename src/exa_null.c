#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa_acc.h"

/**************************/
// null versions

Bool NullPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg)
{
	return TRUE;
}

void NullSolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
}

void NullDoneSolid(PixmapPtr p)
{
}

Bool NullPrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int dx,
		int dy, int alu, Pixel planemask)
{
	return TRUE;
}

void NullCopy(PixmapPtr pDstPixmap, int srcX, int srcY,
		int dstX, int dstY, int width, int height)
{
}

void NullDoneCopy(PixmapPtr p)
{
}

Bool NullDownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h,
		char *dst, int dst_pitch)
{
	return TRUE;
}

Bool NullUploadToScreen(PixmapPtr pDst, int x, int y, int w, int h,
		char *src, int src_pitch)
{
	return TRUE;
}

void NullWaitMarker(ScreenPtr pScreen, int Marker)
{
}