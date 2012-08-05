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


Bool CheckComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p+%p->%p\n", __FUNCTION__, pSrcPicture, pMaskPicture, pDstPicture);

	return TRUE;
//	return FALSE;
}

static int g_compositeOp;
static PicturePtr g_pSrcPicture;
static PicturePtr g_pMaskPicture;
static PicturePtr g_pDstPicture;
static PixmapPtr g_pSrc;
static PixmapPtr g_pMask;
static PixmapPtr g_pDst;

Bool PrepareComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrc,
		PixmapPtr pMask, PixmapPtr pDst)
{
	static int accept = 0;
	static int reject_op = 0;
	static int reject_mask = 0;
	static int reject_mask_alpha = 0;
	static int reject_src_repeat = 0;
	static int reject_mask_repeat = 0;
	static int reject_transform = 0;
	static int reject_bpp = 0;

//	xf86DrvMsg(0, X_DEFAULT, "%s %p+%p->%p\n", __FUNCTION__, pSrc, pMask, pDst);
	g_compositeOp = op;
	g_pSrcPicture = pSrcPicture;
	g_pMaskPicture = pMaskPicture;
	g_pDstPicture = pDstPicture;
	g_pSrc = pSrc;
	g_pMask = pMask;
	g_pDst = pDst;

	if (op != PictOpOver && op != PictOpAdd && op != PictOpOutReverse)
	{
		reject_op++;
//		fprintf(stderr, "op is %d\n", op);
		return FALSE;
	}

	if (g_pMask && (op == PictOpAdd || op == PictOpOutReverse))
	{
		reject_mask++;
		return FALSE;
	}

	if (g_pMask && g_pMaskPicture->componentAlpha)
	{
		reject_mask_alpha++;
		return FALSE;
	}

	if (pSrcPicture->repeat && (op == PictOpAdd || op == PictOpOutReverse))
	{
		reject_src_repeat++;
		return FALSE;
	}

	if (pSrcPicture->transform)
	{
		reject_transform++;
		return FALSE;
	}

	if (pMaskPicture && pMaskPicture->repeat)
	{
		reject_mask_repeat++;
		return FALSE;
	}

	if (pMaskPicture && pMaskPicture->transform)
	{
		reject_transform++;
		return FALSE;
	}

	if (pSrc->drawable.bitsPerPixel != pDst->drawable.bitsPerPixel)
	{
		reject_bpp++;
		return FALSE;
	}

//	MY_ASSERT(0);
//	WaitMarker(0, 0);

	accept++;

	if ((accept % 20) == 0)
		fprintf(stderr, "accept %d reject %d/%d/%d/%d/%d/%d/%d\n",
				accept,
				reject_op, reject_mask, reject_mask_alpha, reject_src_repeat, reject_mask_repeat, reject_transform, reject_bpp);


	return TRUE;
//	return FALSE;
}

void Composite(PixmapPtr pDst, int srcX, int srcY, int maskX,
		int maskY, int dstX, int dstY, int width, int height)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p+%p->%p (%d), %p+%p->%p (wh %dx%d s %d,%d m %d,%d d %d,%d)\n",
//			__FUNCTION__,
//			g_pSrcPicture, g_pMaskPicture, g_pDstPicture,
//			g_compositeOp,
//			g_pSrc, g_pMask, g_pDst,
//			width, height,
//			srcX, srcY,
//			maskX, maskY,
//			dstX, dstY);

	WaitMarker(0, 0);

	int y, x;
	unsigned long dest_stride = exaGetPixmapPitch(pDst);
	unsigned long src_stride = exaGetPixmapPitch(g_pSrc);
	unsigned long mask_stride;
	unsigned int mask_inc;
	unsigned int mask_offset;

	unsigned char *dest = exaGetPixmapAddress(pDst);
	unsigned char *source = exaGetPixmapAddress(g_pSrc);
	unsigned char *mask;
	unsigned char mask_val = 0xff;

	unsigned short source_width = g_pSrc->drawable.width;
	unsigned short source_height = g_pSrc->drawable.height;

	if (g_pMask)
	{
		mask = exaGetPixmapAddress(g_pMask);
		mask_stride = exaGetPixmapPitch(g_pMask);
		if (g_pMask->drawable.depth == 32)
		{
			mask_inc = 4;
			mask_offset = 3;
		}
		else
		{
			mask_inc = 1;
			mask_offset = 0;
		}
	}
	else
	{
		mask = &mask_val;
		mask_inc = 0;
		mask_stride = 0;
		mask_offset = 0;
	}

	unsigned int bpp = pDst->drawable.bitsPerPixel / 8;

	if (g_compositeOp == PictOpAdd)
	{
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				unsigned char m = mask[(y + maskY ) * mask_stride + (x + maskX) * mask_inc];
				int b;
				for (b = 0; b < bpp; b++)
				{
					unsigned short source_x = x + srcX;
					unsigned short source_y = y + srcY;

					if (source_x > source_width)
						source_x -= source_width;
					if (source_y > source_height)
						source_y -= source_height;

					unsigned int temp = dest[(y + dstY) * dest_stride + (x + dstX) * bpp + b]
											 + source[(y + srcY) * src_stride + (x + srcX) * bpp + b];
					if (temp > 255)
						temp = 255;

//					if (m)
						dest[(y + dstY) * dest_stride + (x + dstX) * bpp + b] = temp;
				}
			}
	}
	else if (g_compositeOp == PictOpOver)
	{
		MY_ASSERT(bpp == 4);
		MY_ASSERT(g_pSrc->drawable.depth == 24 || g_pSrc->drawable.depth == 32
				|| g_pDst->drawable.depth == 24 || g_pDst->drawable.depth == 32);
		if (g_pMask)
			MY_ASSERT(g_pMask->drawable.depth == 8 || g_pMask->drawable.depth == 32);

		if (source_width == 1 && source_height == 1)
		{
			unsigned char ref_sr = source[0];
			unsigned char ref_sg = source[1];
			unsigned char ref_sb = source[2];
			unsigned char ref_sa = source[3];

			if (g_pSrc->drawable.depth == 24)
				ref_sa = 255;

			for (y = 0; y < height; y++)
				for (x = 0; x < width; x++)
				{
					unsigned char sr, sg, sb, sa;
					unsigned char dr, dg, db, da;

					unsigned char m = mask[(y + maskY ) * mask_stride + (x + maskX) * mask_inc + mask_offset];

					if (!m)
						continue;

					sr = ref_sr;
					sg = ref_sg;
					sb = ref_sb;
					sa = ref_sa;

					sr = ((unsigned short)sr * (unsigned short)m) >> 8;
					sg = ((unsigned short)sg * (unsigned short)m) >> 8;
					sb = ((unsigned short)sb * (unsigned short)m) >> 8;
					sa = ((unsigned short)sa * (unsigned short)m) >> 8;

					unsigned char one_minus_sa = 255 - sa;

					dr = dest[(y + dstY) * dest_stride + (x + dstX) * 4];
					dg = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1];
					db = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2];
					if (g_pDst->drawable.depth == 24)
						da = 255;
					else
						da = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3];

					unsigned int r, g, b, a;
					r = (dr * one_minus_sa) / 256 + sr;
					g = (dg * one_minus_sa) / 256 + sg;
					b = (db * one_minus_sa) / 256 + sb;
					a = (da * one_minus_sa) / 256 + sa;

					dest[(y + dstY) * dest_stride + (x + dstX) * 4] = r;
					dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1] = g;
					dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2] = b;
					dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3] = a;
				}
		}
		else
		{
			for (y = 0; y < height; y++)
				for (x = 0; x < width; x++)
				{
					unsigned char sr, sg, sb, sa;
					unsigned char dr, dg, db, da;

					unsigned char m = mask[(y + maskY ) * mask_stride + (x + maskX) * mask_inc + mask_offset];

					if (!m)
						continue;

					unsigned short source_x = x + srcX;
					unsigned short source_y = y + srcY;

					while (source_x >= source_width)
						source_x -= source_width;
					while (source_y >= source_height)
						source_y -= source_height;

					sr = source[source_y * src_stride + source_x * 4];
					sg = source[source_y * src_stride + source_x * 4 + 1];
					sb = source[source_y * src_stride + source_x * 4 + 2];
					if (g_pSrc->drawable.depth == 24)
						sa = 255;
					else
						sa = source[source_y * src_stride + source_x * 4 + 3];

					sr = ((unsigned short)sr * (unsigned short)m) / 256;
					sg = ((unsigned short)sg * (unsigned short)m) / 256;
					sb = ((unsigned short)sb * (unsigned short)m) / 256;
					sa = ((unsigned short)sa * (unsigned short)m) / 256;

					unsigned char one_minus_sa = 255 - sa;

					dr = dest[(y + dstY) * dest_stride + (x + dstX) * 4];
					dg = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1];
					db = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2];
					if (g_pDst->drawable.depth == 24)
						da = 255;
					else
						da = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3];

					unsigned int r, g, b, a;
					r = (dr * one_minus_sa) / 256 + sr;
					g = (dg * one_minus_sa) / 256 + sg;
					b = (db * one_minus_sa) / 256 + sb;
					a = (da * one_minus_sa) / 256 + sa;

					dest[(y + dstY) * dest_stride + (x + dstX) * 4] = r;
					dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1] = g;
					dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2] = b;
					dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3] = a;
				}
		}
	}
	else if (g_compositeOp == PictOpOutReverse)
	{
		MY_ASSERT(bpp == 4);
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				unsigned char sa;
				unsigned char dr, dg, db, da;

				unsigned char m = mask[(y + maskY ) * mask_stride + (x + maskX) * mask_inc];

				unsigned short source_x = x + srcX;
				unsigned short source_y = y + srcY;

				if (source_x > source_width)
					source_x -= source_width;
				if (source_y > source_height)
					source_y -= source_height;

				sa = source[source_y * src_stride + source_x * 4 + 3];
				sa = ((unsigned int)sa * (unsigned int)m) >> 8;

				dr = dest[(y + dstY) * dest_stride + (x + dstX) * 4];
				dg = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1];
				db = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2];
				da = dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3];

				unsigned int r, g, b, a;
				r = dr * (255 - sa);
				g = dg * (255 - sa);
				b = db * (255 - sa);
				a = da * (255 - sa);

				r = r >> 8;
				g = g >> 8;
				b = b >> 8;
				a = a >> 8;

				if (r > 255)
					r = 255;
				if (g > 255)
					g = 255;
				if (b > 255)
					b = 255;
				if (a > 255)
					a = 255;

				dest[(y + dstY) * dest_stride + (x + dstX) * 4] = r;
				dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 1] = g;
				dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 2] = b;
				dest[(y + dstY) * dest_stride + (x + dstX) * 4 + 3] = a;
			}
	}
	else
		MY_ASSERT(0);
}

void DoneComposite(PixmapPtr p)
{
//	xf86DrvMsg(0, X_DEFAULT, "%s %p\n", __FUNCTION__, p);
}
