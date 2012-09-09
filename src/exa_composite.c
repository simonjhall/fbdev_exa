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

Bool CheckComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p+%p->%p\n", __FUNCTION__, pSrcPicture, pMaskPicture, pDstPicture);

	//only two operations accelerated
	if (op != PictOpOver && op != PictOpAdd/* && op != PictOpOutReverse*/)
		return FALSE;

	//do not support component alpha
	if (pMaskPicture && pMaskPicture->componentAlpha)
		return FALSE;

	//do not support transformations
	if (pSrcPicture->transform)
		return FALSE;

	if (pMaskPicture && pMaskPicture->transform)
		return FALSE;

	//no mask wrapping
	if (pMaskPicture && pMaskPicture->repeat)
		return FALSE;

	return TRUE;
//	return FALSE;
}

Bool PrepareComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrc,
		PixmapPtr pMask, PixmapPtr pDst)
{
	//sometimes these are bad
	if (!pSrc || !pDst)
		return FALSE;
	if (pMaskPicture && !pMask)
		return FALSE;

	//get our composite function
	g_pCompositor = EnumToFunc(op,
				pSrcPicture->format, pDstPicture->format,
				pMask ? pMaskPicture->format : kNoData);

	//if possible...
	if (!g_pCompositor)
	{
		fprintf(stderr, "reject: op %d, format %08x/%08x/%08x\n",
				op,
				pSrcPicture->format, pDstPicture->format,
				pMask ? pMaskPicture->format : 0);

		return FALSE;
	}

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
//	xf86DrvMsg(0, X_DEFAULT, "%.3f, %s (%2d), (wh %2dx%2d s %2d,%2d m %2d,%2d d %2d,%2d, bpp %2d,%2d,%2d dim %2d,%2d %2d,%2d %2d,%2d)\n",
//			(double)clock() / CLOCKS_PER_SEC,
//			__FUNCTION__,
////			g_pSrcPicture, g_pMaskPicture, g_pDstPicture,
//			g_compositeOp,
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

	//out of space, do now
	if (g_pendingOps == MAX_COMP_OPS)
	{
		exaWaitSync(pDst->drawable.pScreen);

		MY_ASSERT(g_pCompositor);
		g_pCompositor(g_opList, g_pendingOps,
				exaGetPixmapAddress(g_pSrc), exaGetPixmapAddress(pDst), g_pMask ? exaGetPixmapAddress(g_pMask) : 0,
				exaGetPixmapPitch(g_pSrc), exaGetPixmapPitch(pDst), g_pMask ? exaGetPixmapPitch(g_pMask) : 0,
				g_pSrc->drawable.width, g_pSrc->drawable.height,
				g_pSrcPicture->repeat);
		g_pendingOps = 0;

		exaMarkSync(pDst->drawable.pScreen);
	}
}

void DoneComposite(PixmapPtr pDst)
{
	MY_ASSERT(pDst == g_pDst);

	//if work to do
	if (g_pendingOps)
	{
		//block on
		exaWaitSync(pDst->drawable.pScreen);

		MY_ASSERT(g_pCompositor);
		g_pCompositor(g_opList, g_pendingOps,
				exaGetPixmapAddress(g_pSrc), exaGetPixmapAddress(pDst), g_pMask ? exaGetPixmapAddress(g_pMask) : 0,
				exaGetPixmapPitch(g_pSrc), exaGetPixmapPitch(pDst), g_pMask ? exaGetPixmapPitch(g_pMask) : 0,
				g_pSrc->drawable.width, g_pSrc->drawable.height,
				g_pSrcPicture->repeat);
		g_pendingOps = 0;

		exaMarkSync(pDst->drawable.pScreen);
	}
}
