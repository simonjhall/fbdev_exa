/*
 * exa_solid.c
 *
 *  Created on: 7 Aug 2012
 *      Author: simon
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa.h"

#include "exa_acc.h"

struct SolidDetails
{
	PixmapPtr m_pDst;
	int m_bpp;
	Pixel m_toFill;
	int m_syncPoint;
} g_solidDetails;

Bool PrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask,
		Pixel fg)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p\n", __FUNCTION__, pPixmap);
//	return FALSE;

	//check it's a valid pointer
	if (pPixmap == NULL)
	{
		xf86DrvMsg(0, X_WARNING, "%s (pPixmap == NULL)\n", __FUNCTION__);
		return FALSE;
	}

	if (exaGetPixmapAddress(pPixmap) == 0)
	{
		xf86DrvMsg(0, X_WARNING, "%s dest %p\n", __FUNCTION__, exaGetPixmapAddress(pPixmap));
		return FALSE;
	}

	//check it's either 8/16/24/32
	if (pPixmap->drawable.bitsPerPixel & 7)
	{
		xf86DrvMsg(0, X_WARNING, "%s pPixmap->drawable.bitsPerPixel & 7\n", __FUNCTION__);
		return FALSE;
	}

	//need a solid plane mask
	if (!EXA_PM_IS_SOLID(&pPixmap->drawable, planemask))
	{
		xf86DrvMsg(0, X_WARNING, "%s !EXA_PM_IS_SOLID\n", __FUNCTION__);
		return FALSE;
	}

	//check that it's a copy operation
	if (alu != GXcopy)
	{
		xf86DrvMsg(0, X_WARNING, "%s alu != GXcopy\n", __FUNCTION__);
		return FALSE;
	}

	g_solidDetails.m_pDst = pPixmap;
	g_solidDetails.m_bpp = pPixmap->drawable.bitsPerPixel / 8;
	g_solidDetails.m_toFill = fg;

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif

	return TRUE;
}

void Solid(PixmapPtr pPixmap, int X1, int Y1, int X2, int Y2)
{
	MY_ASSERT(pPixmap == g_solidDetails.m_pDst);

//	xf86DrvMsg(0, X_DEFAULT, "%s %p (%d,%d->%d,%d %dx%d)\n", __FUNCTION__, pPixmap,
//			X1, Y1, X2, Y2,
//			X2 - X1, Y2 - Y1);

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif

	unsigned char *pDst = exaGetPixmapAddress(g_solidDetails.m_pDst);
	unsigned long dstPitch = exaGetPixmapPitch(g_solidDetails.m_pDst);

	//perform the fill with a 2d blit and non-moving source

	//some variables to make this more manageable
	int bpp = g_solidDetails.m_bpp;
	int width = X2 - X1;
	int height = Y2 - Y1;
	//get a new dma block and some solid space
	unsigned char *pSolid = 0;

	//get some solid space
	while (!(pSolid = AllocSolidBuffer(32)))
	{
		fprintf(stderr, "unable to allocate solid space - kicking and waiting\n");

#ifdef CB_VALIDATION
		if (IsPendingUnkicked())
			ValidateCbList(GetUnkickedDmaHead());
#endif

		//there could be non-committed work taking up all the solid
		if (IsPendingUnkicked())
		{
			if (StartDma(GetUnkickedDmaHead(), TRUE))	//force
				UpdateKickedDmaHead();
			else
				MY_ASSERT(0);

			//dma pending will now be set
		}

		//it could be something already enqueued taking it
		if (IsDmaPending())
		{
			//if something's pending then we should wait
			WaitMarker(GetScreen(), 0);
		}

		//or simply we simply haven't reset it yet after past work
		ResetSolidBuffer();
	}

	//copy in the solid
	switch (bpp)
	{
	case 1:
		pSolid[0] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[1] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[2] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[3] = (unsigned char)g_solidDetails.m_toFill;
		break;
	case 2:
		pSolid[0] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[1] = (unsigned char)(g_solidDetails.m_toFill >> 8);
		pSolid[2] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[3] = (unsigned char)(g_solidDetails.m_toFill >> 8);
		break;
	case 3:
	case 4:
		pSolid[0] = (unsigned char)g_solidDetails.m_toFill;
		pSolid[1] = (unsigned char)(g_solidDetails.m_toFill >> 8);
		pSolid[2] = (unsigned char)(g_solidDetails.m_toFill >> 16);
		pSolid[3] = (unsigned char)(g_solidDetails.m_toFill >> 24);
		break;
	default:
		MY_ASSERT(0);
		break;
	}

	//dma will overread
	unsigned int *pSolidI = (unsigned int *)pSolid;
	pSolidI[1] = *pSolidI;
	pSolidI[2] = *pSolidI;
	pSolidI[3] = *pSolidI;
	pSolidI[4] = *pSolidI;
	pSolidI[5] = *pSolidI;
	pSolidI[6] = *pSolidI;
	pSolidI[7] = *pSolidI;

	//enqueue the dma
	Copy2D4kNoSrcInc(&pDst[Y1 * dstPitch + X1 * bpp],	//dest
			pSolid,					//source
			bpp * width,			//width
			height,					//height
			dstPitch - bpp * width);//dest stride

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif
}

void DoneSolid(PixmapPtr p)
{
	MY_ASSERT(g_solidDetails.m_pDst == p);

#ifdef CB_VALIDATION
	if (IsPendingUnkicked())
		ValidateCbList(GetUnkickedDmaHead());
#endif

	if (IsPendingUnkicked())
	{
		//give it a sync point (for future work)

		if (StartDma(GetUnkickedDmaHead(), FALSE))
			UpdateKickedDmaHead();
	}

	//work may have been kicked during Solid, yet nothing is pending
	exaMarkSync(g_solidDetails.m_pDst->drawable.pScreen);
}
