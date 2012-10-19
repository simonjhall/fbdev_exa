/*
 * exa_composite.c
 *
 *  Created on: 5 Aug 2012
 *      Author: simon
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"
#include "exa.h"

#include "exa_acc.h"

#include <time.h>

//#define DEBUG_REJECTION(...) xf86DrvMsg(0, X_WARNING, __VA_ARGS__)
#define DEBUG_REJECTION(...)

//work to be run
//guh, const in c does not mean constant...
#define MAX_COMP_OPS 200
static struct CompositeOp g_opList[MAX_COMP_OPS];
unsigned int g_pendingOps = 0;
ptr2PdFunc g_pCompositor;

//////////////
static PicturePtr g_pSrcPicture;
static PicturePtr g_pMaskPicture;
static PicturePtr g_pDstPicture;
static PixmapPtr g_pSrc;
static PixmapPtr g_pMask;
static PixmapPtr g_pDst;
//////////////

int WritePPMRGBA(const char *filename, const int width, const int height, const int stride, const unsigned char *pImage)
{
	FILE *fp = fopen(filename, "wb");
	if (!fp)
		return 0;
	else
	{
		int x, y;
		fprintf(fp, "P6\n%d %d\n255\n", width, height);

		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				unsigned char rgb[3];
				rgb[0] = pImage[y * stride + x * 4 + 0];
				rgb[1] = pImage[y * stride + x * 4 + 1];
				rgb[2] = pImage[y * stride + x * 4 + 2];
				fwrite(rgb, 1, 3, fp);
			}

		fclose(fp);
		return 0;
	}
}

int WritePGMGrey(const char *filename, const int width, const int height, const int stride, const unsigned char *pImage)
{
	FILE *fp = fopen(filename, "wb");
	if (!fp)
		return 0;
	else
	{
		int x, y;
		fprintf(fp, "P5\n%d %d\n255\n", width, height);

		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				unsigned char grey;
				grey = pImage[y * stride + x];
				fwrite(&grey, 1, 1, fp);
			}

		fclose(fp);
		return 0;
	}
}

int WritePGMAlpha(const char *filename, const int width, const int height, const int stride, const int channel, const unsigned char *pImage)
{
	FILE *fp = fopen(filename, "wb");
	if (!fp)
		return 0;
	else
	{
		int x, y;
		fprintf(fp, "P5\n%d %d\n255\n", width, height);

		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				unsigned char grey;
				grey = pImage[y * stride + x * 4 + channel];
				fwrite(&grey, 1, 1, fp);
			}

		fclose(fp);
		return 0;
	}
}

//////////////

Bool CheckComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p+%p->%p\n", __FUNCTION__, pSrcPicture, pMaskPicture, pDstPicture);
//	return FALSE;

	//check operations accelerated
	if (op != PictOpOver && op != PictOpAdd && op != PictOpSrc/* && op != PictOpOutReverse*/)
	{
		DEBUG_REJECTION("rejected operation %d\n", op);
		return FALSE;
	}

	//do not support component alpha
	if (pMaskPicture && pMaskPicture->componentAlpha)
	{
		DEBUG_REJECTION("rejected mask component alpha\n");
		return FALSE;
	}

	//do not support transformations
	if (pSrcPicture->transform)
	{
		DEBUG_REJECTION("rejected source transformation\n");
		return FALSE;
	}

	if (pMaskPicture && pMaskPicture->transform)
	{
		DEBUG_REJECTION("rejected mask transformation\n");
		return FALSE;
	}

	//no mask wrapping
	if (pMaskPicture && pMaskPicture->repeat)
	{
		DEBUG_REJECTION("rejected mask repeat\n");
		return FALSE;
	}

	//currently only support exa masks, not solid ones
	enum PixelFormat maskpf = kNoData;

	if (pMaskPicture && !pMaskPicture->pSourcePict)
		maskpf = pMaskPicture->format;
	else if (pMaskPicture && pMaskPicture->pSourcePict)
	{
		DEBUG_REJECTION("rejected solid mask\n");
		return FALSE;
	}

	//get our composite function
	g_pCompositor = EnumToFunc(op,
				pSrcPicture->format, pDstPicture->format,
				maskpf);

	//if possible...
	if (!g_pCompositor)
	{
		DEBUG_REJECTION("reject: op %d, format %08x/%08x/%08x\n",
				op,
				pSrcPicture->format, pDstPicture->format,
				pMaskPicture ? pMaskPicture->format : 0);

		return FALSE;
	}
//	else
//		xf86DrvMsg(0, X_INFO, "accept: op %d, format %08x/%08x/%08x\n",
//				op,
//				pSrcPicture->format, pDstPicture->format,
//				pMaskPicture ? pMaskPicture->format : 0);

	return TRUE;
//	return FALSE;
}

Bool PrepareComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrc,
		PixmapPtr pMask, PixmapPtr pDst)
{
	MY_ASSERT(pDst);

	//save some state necessary
	g_pSrcPicture = pSrcPicture;
	g_pMaskPicture = pMaskPicture;
	g_pDstPicture = pDstPicture;
	g_pSrc = pSrc;
	g_pMask = pMask;
	g_pDst = pDst;

	//ready to have data added
	MY_ASSERT(g_pendingOps == 0);
	g_pendingOps = 0;

	return TRUE;
//	return FALSE;
}

void Composite(PixmapPtr pDst, int srcX, int srcY, int maskX,
		int maskY, int dstX, int dstY, int width, int height)
{
//	static int operation = 0;
//	xf86DrvMsg(0, X_DEFAULT, "%.3f, %d, %s, (wh %2dx%2d s %2d,%2d m %2d,%2d d %2d,%2d, bpp %2d,%2d,%2d dim %2d,%2d %2d,%2d %2d,%2d)\n",
//			(double)clock() / CLOCKS_PER_SEC,
//			operation,
//			__FUNCTION__,
////			g_pSrcPicture, g_pMaskPicture, g_pDstPicture,
////			g_compositeOp,
////			g_pSrc, g_pMask, g_pDst,
//			width, height,
//			srcX, srcY,
//			maskX, maskY,
//			dstX, dstY,
//			g_pSrc->drawable.depth,
//			g_pDst->drawable.depth,
//			g_pMask ? g_pMask->drawable.depth : -1,
//			g_pSrc->drawable.width, g_pSrc->drawable.height,
//			g_pDst->drawable.width, g_pDst->drawable.height,
//			g_pMask ? g_pMask->drawable.width : -1, g_pMask ? g_pMask->drawable.height : -1);

	//record the state in the list
	g_opList[g_pendingOps].srcX = srcX;
	g_opList[g_pendingOps].srcY = srcY;

	g_opList[g_pendingOps].maskX = maskX;
	g_opList[g_pendingOps].maskY = maskY;

	g_opList[g_pendingOps].dstX = dstX;
	g_opList[g_pendingOps].dstY = dstY;

	g_opList[g_pendingOps].width = width;
	g_opList[g_pendingOps].height = height;

	g_pendingOps++;


	/*char filename_src[300];
	char filename_dest[300];
	char filename_dest2[300];
	char filename_mask[300];
	sprintf(filename_src, "/mnt/remote/raspbian/x/xserver-xorg-video-fbdev-0.4.2/src/images/%04d_src.pgm", operation);
	sprintf(filename_dest, "/mnt/remote/raspbian/x/xserver-xorg-video-fbdev-0.4.2/src/images/%04d_dest.pgm", operation);
	sprintf(filename_dest2, "/mnt/remote/raspbian/x/xserver-xorg-video-fbdev-0.4.2/src/images/%04d_dest2.pgm", operation);
	sprintf(filename_mask, "/mnt/remote/raspbian/x/xserver-xorg-video-fbdev-0.4.2/src/images/%04d_mask.pgm", operation);

	operation++;

	WaitMarker(GetScreen(), 0);

	if (g_pSrcPicture->format == kA8)
		WritePGMGrey(filename_src, g_pSrc->drawable.width, g_pSrc->drawable.height, exaGetPixmapPitch(g_pSrc), exaGetPixmapAddress(g_pSrc));
	if (g_pDstPicture->format == kA8)
		WritePGMGrey(filename_dest, g_pDst->drawable.width, g_pDst->drawable.height, exaGetPixmapPitch(g_pDst), exaGetPixmapAddress(g_pDst));
	if (g_pMaskPicture && g_pMaskPicture->format == kA8)
		WritePGMGrey(filename_mask, g_pMask->drawable.width, g_pMask->drawable.height, exaGetPixmapPitch(g_pMask), exaGetPixmapAddress(g_pMask));

	g_pCompositor(g_opList, g_pendingOps,
				exaGetPixmapAddress(g_pSrc), exaGetPixmapAddress(pDst), g_pMask ? exaGetPixmapAddress(g_pMask) : 0,
				exaGetPixmapPitch(g_pSrc), exaGetPixmapPitch(pDst), g_pMask ? exaGetPixmapPitch(g_pMask) : 0,
				g_pSrc->drawable.width, g_pSrc->drawable.height,
				g_pSrcPicture->repeat);
	g_pendingOps = 0;

	if (g_pDstPicture->format == kA8)
		WritePGMGrey(filename_dest2, g_pDst->drawable.width, g_pDst->drawable.height, exaGetPixmapPitch(g_pDst), exaGetPixmapAddress(g_pDst));
*/

	//out of space, do now
	if (g_pendingOps == MAX_COMP_OPS)
	{
		xf86DrvMsg(0, X_INFO, "max composite ops - flushing now\n");

		//this needs to happen, nothing should be outstanding
		WaitMarker(GetScreen(), 0);

		unsigned char *source;
		unsigned char *mask = 0;
		unsigned char *dest;

		//source data selector
		if (g_pSrc)
			source = exaGetPixmapAddress(g_pSrc);
		else
			source = (unsigned char *)&g_pSrcPicture->pSourcePict->solidFill.color;

		//dest should always be legit
		dest = exaGetPixmapAddress(pDst);

		//mask data selector
		if (g_pMask)								//exa mask
			mask = exaGetPixmapAddress(g_pMask);
		else if (g_pMaskPicture)					//solid picture mask
			mask = (unsigned char *)&g_pMaskPicture->pSourcePict->solidFill.color;
		//else no mask (mask = 0, see above)

		MY_ASSERT(g_pCompositor);
		g_pCompositor(g_opList, g_pendingOps,
						source,
						dest,
						mask,

						g_pSrc ? exaGetPixmapPitch(g_pSrc) : 0,																			//no pitch on solid
						exaGetPixmapPitch(pDst),
						g_pMask ? exaGetPixmapPitch(g_pMask) : 0,																		//only pitch on exa mask

						g_pSrc ? g_pSrc->drawable.width : 1, g_pSrc ? g_pSrc->drawable.height : 1,										//solid or not
						g_pSrcPicture->repeat);
		g_pendingOps = 0;

//		exaMarkSync(pDst->drawable.pScreen);
	}
}

void DoneComposite(PixmapPtr pDst)
{
	MY_ASSERT(pDst == g_pDst);

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif

	//if work to do
	if (g_pendingOps)
	{
		//block on the work stuff being done

		//this needs to happen, nothing should be outstanding
		WaitMarker(GetScreen(), 0);

		unsigned char *source;
		unsigned char *mask = 0;
		unsigned char *dest;

		//source data selector
		if (g_pSrc)
			source = exaGetPixmapAddress(g_pSrc);
		else
			source = (unsigned char *)&g_pSrcPicture->pSourcePict->solidFill.color;

		//dest should always be legit
		dest = exaGetPixmapAddress(pDst);

		//mask data selector
		if (g_pMask)								//exa mask
			mask = exaGetPixmapAddress(g_pMask);
		else if (g_pMaskPicture)					//solid picture mask
			mask = (unsigned char *)&g_pMaskPicture->pSourcePict->solidFill.color;
		//else no mask (mask = 0, see above)

		MY_ASSERT(g_pCompositor);
		g_pCompositor(g_opList, g_pendingOps,
						source,
						dest,
						mask,

						g_pSrc ? exaGetPixmapPitch(g_pSrc) : 0,																			//no pitch on solid
						exaGetPixmapPitch(pDst),
						g_pMask ? exaGetPixmapPitch(g_pMask) : 0,																		//only pitch on exa mask

						g_pSrc ? g_pSrc->drawable.width : 1, g_pSrc ? g_pSrc->drawable.height : 1,										//solid or not
						g_pSrcPicture->repeat);
		g_pendingOps = 0;
	}
}
