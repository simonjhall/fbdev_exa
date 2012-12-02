/*
 * simple_composite.c
 *
 *  Created on: 1 Dec 2012
 *      Author: Simon
 */

#include "generic_types.h"

#define ENABLE_EARLY_OUTS

//this is what the IN operation does, for one channel
static inline unsigned char InPerChannel(const unsigned char a, const unsigned char b)
{
	//add and round
	unsigned short result = (unsigned short)a * (unsigned short)b + 128;

	//divide by 255
	result = (result + (result >> 8)) >> 8;

	//clamp
	if (result > 255)
		result = 255;

	return (unsigned char)result;
}

//this is what the OVER operation does, for one channel
static inline unsigned char OverPerChannel(const unsigned char a, const unsigned char b, const unsigned char one_minus_alpha)
{
	//multiply and round b by 1-alpha
	unsigned short result = ((unsigned short)b * (unsigned short)one_minus_alpha + 128);
	//divide by 255
	result = (result + (result >> 8)) >> 8;

	//add a
	result += a;

	//clamp
	if (result > 255)
		result = 255;

	return (unsigned char)result;
}

/* take A=abgr and B=xxxa and emit A IN B, with abgr being returned
 * for reference it is done like this in 32-bit ARMv6 SIMD
 *
 * //get two channels of the source
	unsigned int a_low = A & 0x00ff00ff;
	//the other two channels of the source
	unsigned int a_high = (A >> 8) & 0x00ff00ff;

	//source * alpha + 128
	//mla please
	a_low = a_low * (unsigned int)b_alpha + 0x00800080;
	a_high = a_high * (unsigned int)b_alpha + 0x00800080;

	//source = (source + (source >> 8)) >> 8
	a_low = __uxtab16(a_low, a_low, 8);
	a_high = __uxtab16(a_high, a_high, 8);
	a_low = __uxtb16(a_low, 8);
	a_high = __uxtb16(a_high, 8);

	return a_low | (a_high << 8);
 */
static inline unsigned int In_abgr_a(const unsigned int a, const unsigned int b)
{
	unsigned char rr, rg, rb, ra;
	rr = InPerChannel(a & 0xff, b);
	rg = InPerChannel((a >> 8) & 0xff, b);
	rb = InPerChannel((a >> 16) & 0xff, b);
	ra = InPerChannel((a >> 24) & 0xff, b);

	return (ra << 24) | (rb << 16) | (rg << 8) | rr;
}

/* take A=abgr and B=xbgr and emit A OVER B, with xbgr being returned
 * for reference it is done like this in 32-bit ARMv6 SIMD
 *
 * //get two channels at a time (in 16 hwords)
	unsigned int b_low = b & 0x00ff00ff;
	unsigned int b_high = (b >> 8) & 0x00ff00ff;

	//b * 1-alpha + 128
	//mla please
	b_low = b_low * one_minus_alpha + 0x00800080;
	b_high = b_high * one_minus_alpha + 0x00800080;

	//source = (source + (source >> 8)) >> 8
	b_low = __uxtab16(b_low, b_low, 8);
	b_high = __uxtab16(b_high, b_high, 8);
	b_low = __uxtb16(b_low, 8);
	b_high = __uxtb16(b_high, 8);

	unsigned int combined = b_low | (b_high << 8);

	//add a and saturate
	return __uqadd8(combined, a);
 */
static inline unsigned int Over_abgr_xbgr(const unsigned int a, const unsigned int b, const unsigned char one_minus_alpha)
{
	unsigned char rr, rg, rb, ra;
	rr = OverPerChannel(a & 0xff, b & 0xff, one_minus_alpha);
	rg = OverPerChannel((a >> 8) & 0xff, (b >> 8) & 0xff, one_minus_alpha);
	rb = OverPerChannel((a >> 16) & 0xff, (b >> 16) & 0xff, one_minus_alpha);

	ra = 255;		//just for the sake of it, we can leave it undefined

	return (ra << 24) | (rb << 16) | (rg << 8) | rr;
}

////////////////////////////////////////////////////////////////

//convert from argb->abgr by swapping the r and b around and leaving g and a as they are
unsigned int ReformatAbgrArgb(unsigned int argb)
{
	return (argb & 0xff00ff00) | ((argb & 0xff) << 16) | ((argb & 0xff0000) >> 16);
}

////////////////////////////////////////////////////////////////

//fixed source, fixed mask, varying destination; source co-ordinate clamped to 0,0
static void vpu_over_a8r8g8b8_x8b8g8r8_invalid_nonvarying(const unsigned int source_argb, const unsigned char mask, unsigned int * const pDest,
		unsigned int width, unsigned int height, unsigned int dest_stride_bytes)
{
	//no sense in doing any work if this is zero
	//(does not affect the result if this is skipped though, just wasted time)
#ifdef ENABLE_EARLY_OUTS
	if (mask == 0)
		return;
#endif

	//work in 32-bit pixels
	const unsigned int dest_stride_words = dest_stride_bytes >> 2;

	//reformat the source to match the destination (not discarding alpha though)
	unsigned int source_abgr = ReformatAbgrArgb(source_argb);

	//pre-compute source IN mask (optimisation if it is 255)
#ifdef ENABLE_EARLY_OUTS
	if (mask != 255)
#endif
		source_abgr = In_abgr_a(source_abgr, mask);

	//get the alpha channel
	const unsigned char one_minus_source_a = 255 - (source_abgr >> 24);

	//iterate over each pixel in the box
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
		{
			//read the next pixel from the destination image
			unsigned int dest_xbgr = pDest[y * dest_stride_words + x];

			//perform (source IN mask) OVER dest
			dest_xbgr = Over_abgr_xbgr(source_abgr, dest_xbgr, one_minus_source_a);

			//write it back out
			pDest[y * dest_stride_words + x] = dest_xbgr;
		}
}

//varying source, fixed mask, varying destination; source co-ordinate does nothing fancy (already offset by srcX,srcY)
static void vpu_over_a8r8g8b8_x8b8g8r8_invalid_normal(unsigned int * const pSourceArgb, const unsigned char mask, unsigned int * const pDest,
		unsigned int width, unsigned int height, unsigned int source_stride_bytes, unsigned int dest_stride_bytes)
{
	//no sense in doing any work if this is zero
	//(does not affect the result if this is skipped though, just wasted time)
	if (mask == 0)
		return;

	//work in 32-bit pixels
	const unsigned int source_stride_words = source_stride_bytes >> 2;
	const unsigned int dest_stride_words = dest_stride_bytes >> 2;

	//iterate over each pixel in the box
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
		{
			//read the next pixel from the source image
			unsigned int source_argb = pSourceArgb[y * source_stride_words + x];

			//reformat the source to match the destination (not discarding alpha though)
			unsigned int source_abgr = ReformatAbgrArgb(source_argb);

			//compute source IN mask (if statement = optimisation)
#ifdef ENABLE_EARLY_OUTS
			if (mask != 255)
#endif
				source_abgr = In_abgr_a(source_abgr, mask);

			//get the alpha channel
			const unsigned char one_minus_source_a = 255 - (source_abgr >> 24);

			//read the next pixel from the destination image
			unsigned int dest_xbgr = pDest[y * dest_stride_words + x];

			//perform (source IN mask) OVER dest
			dest_xbgr = Over_abgr_xbgr(source_abgr, dest_xbgr, one_minus_source_a);

			//write it back out
			pDest[y * dest_stride_words + x] = dest_xbgr;
		}
}

//fixed source, varying mask, varying destination; source co-ordinate clamped to 0,0
static void vpu_over_a8r8g8b8_x8b8g8r8_a8_valid_nonvarying(const unsigned int source_argb_ref, const unsigned char * const pMask, unsigned int * const pDest,
		unsigned int width, unsigned int height, unsigned int dest_stride_bytes, unsigned int mask_stride_bytes)
{
	//work in 32-bit pixels
	const unsigned int dest_stride_words = dest_stride_bytes >> 2;

	//reformat the source to match the destination (not discarding alpha though)
	unsigned int source_abgr_ref = ReformatAbgrArgb(source_argb_ref);

	//iterate over each pixel in the box
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
		{
			//read the next mask pixel
			unsigned char mask_a8 = pMask[y * mask_stride_bytes + x];

			//take a copy of the source pixel, as we may modify it
			unsigned int source_abgr = source_abgr_ref;

			//compute source IN mask (if statements = optimisation for scalar code)
#ifdef ENABLE_EARLY_OUTS
			if (mask_a8 == 0)
				continue;
			else if (mask_a8 != 255)
#endif
				source_abgr = In_abgr_a(source_abgr, mask_a8);

			//get the alpha channel
			const unsigned char one_minus_source_a = 255 - (source_abgr >> 24);

			//read the next pixel from the destination image
			unsigned int dest_xbgr = pDest[y * dest_stride_words + x];

			//perform (source IN mask) OVER dest
			dest_xbgr = Over_abgr_xbgr(source_abgr, dest_xbgr, one_minus_source_a);

			//write it back out
			pDest[y * dest_stride_words + x] = dest_xbgr;
		}
}

//varying source, varying mask, varying destination; source co-ordinate does nothing fancy (already offset by srcX,srcY)
static void vpu_over_a8r8g8b8_x8b8g8r8_a8_valid_normal(const unsigned int * const pSourceArgb, const unsigned char * const pMask, unsigned int * const pDest,
		unsigned int width, unsigned int height,
		unsigned int source_stride_bytes, unsigned int dest_stride_bytes, unsigned int mask_stride_bytes)
{
	//work in 32-bit pixels
	const unsigned int source_stride_words = source_stride_bytes >> 2;
	const unsigned int dest_stride_words = dest_stride_bytes >> 2;

	//iterate over each pixel in the box
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
		{
			//read the next source pixel
			unsigned int source_argb = pSourceArgb[y * source_stride_words + x];

			//reformat the source to match the destination (not discarding alpha though)
			unsigned int source_abgr = ReformatAbgrArgb(source_argb);

			//read the next mask pixel
			unsigned char mask_a8 = pMask[y * mask_stride_bytes + x];

			//compute source IN mask (if statements = optimisation for scalar code)
#ifdef ENABLE_EARLY_OUTS
			if (mask_a8 == 0)
				continue;
			else if (mask_a8 != 255)
#endif
				source_abgr = In_abgr_a(source_abgr, mask_a8);

			//get the alpha channel (also, 255 - a == a ^ 255)
			const unsigned char one_minus_source_a = 255 - (source_abgr >> 24);

			//read the next pixel from the destination image
			unsigned int dest_xbgr = pDest[y * dest_stride_words + x];

			//perform (source IN mask) OVER dest
			dest_xbgr = Over_abgr_xbgr(source_abgr, dest_xbgr, one_minus_source_a);

			//write it back out
			pDest[y * dest_stride_words + x] = dest_xbgr;
		}
}

////////////////////////////////////////////////////////////////

//main entry point for (source IN mask) OVER dest with an invalid mask (ie no data, just a constant of 255)
void vpu_over_a8r8g8b8_x8b8g8r8_invalid(struct CompositeOp *pOpList, const unsigned int numOps,
		unsigned char *pSourceB, unsigned char *pDestB, unsigned char *pMaskB,
		const unsigned int source_stride, const unsigned int dest_stride, const unsigned int mask_stride,
		const unsigned int source_width, const unsigned int source_height,
		const unsigned int source_wrap)
{
	unsigned int *pSourceI = (unsigned int *)pSourceB;
	unsigned int *pDestI = (unsigned int *)pDestB;

	//non-varying source coordinate
	if (source_width == 1 && source_height == 1)
	{
		//loop through each thing in the list
		for (unsigned int count = 0; count < numOps; count++)
		{
			struct CompositeOp *pOp = &pOpList[count];

			//use a constant for the mask
			const unsigned char m = 255;

			//use a single pixel value for the source
			const unsigned int argb = *pSourceI;

			//do the operation
			vpu_over_a8r8g8b8_x8b8g8r8_invalid_nonvarying(argb, m,
					&pDestI[pOp->dstY * (dest_stride >> 2) + pOp->dstX],
					pOp->width, pOp->height,
					dest_stride);
		}
	}
	else if (source_wrap)		//wrapping source coordinate
	{
		//reasonably rare to call this function
		MY_ASSERT(0);
	}
	else	//normal source coordinates
	{
		//loop through each thing in the list
		for (unsigned int count = 0; count < numOps; count++)
		{
			struct CompositeOp *pOp = &pOpList[count];

			//use a constant for the mask
			const unsigned char m = 255;

			//do the operation
			vpu_over_a8r8g8b8_x8b8g8r8_invalid_normal(&pSourceI[pOp->srcY * (source_stride >> 2) + pOp->srcX],
					m, &pDestI[pOp->dstY * (dest_stride >> 2) + pOp->dstX],
					pOp->width, pOp->height,
					source_stride, dest_stride);
		}
	}
}

//main entry point for (source IN mask) OVER dest with a valid mask (ie one backed by data)
void vpu_over_a8r8g8b8_x8b8g8r8_a8_valid(struct CompositeOp *pOpList, const unsigned int numOps,
		unsigned char *pSourceB, unsigned char *pDestB, unsigned char *pMaskB,
		const unsigned int source_stride, const unsigned int dest_stride, const unsigned int mask_stride,
		const unsigned int source_width, const unsigned int source_height,
		const unsigned int source_wrap)
{
	unsigned int *pSourceI = (unsigned int *)pSourceB;
	unsigned int *pDestI = (unsigned int *)pDestB;

	//non-varying source coordinate
	if (source_width == 1 && source_height == 1)
	{
		//loop through each thing in the list
		for (unsigned int count = 0; count < numOps; count++)
		{
			struct CompositeOp *pOp = &pOpList[count];

			//use a single pixel value for the source
			const unsigned int argb = *pSourceI;

			//do the operation
			vpu_over_a8r8g8b8_x8b8g8r8_a8_valid_nonvarying(argb,
					&pMaskB[pOp->maskY * mask_stride + pOp->maskX],
					&pDestI[pOp->dstY * (dest_stride >> 2) + pOp->dstX],
					pOp->width, pOp->height, dest_stride, mask_stride);
		}
	}
	else if (source_wrap)		//wrapping source coordinate
	{
		//reasonably rare to call this function
		MY_ASSERT(0);
	}
	else	//normal source coordinates
	{
		//loop through each thing in the list
		for (unsigned int count = 0; count < numOps; count++)
		{
			struct CompositeOp *pOp = &pOpList[count];

			//do the operation
			vpu_over_a8r8g8b8_x8b8g8r8_a8_valid_normal(&pSourceI[pOp->srcY * (source_stride >> 2) + pOp->srcX],
					&pMaskB[pOp->maskY * mask_stride + pOp->maskX],
					&pDestI[pOp->dstY * (dest_stride >> 2) + pOp->dstX],
					pOp->width, pOp->height,
					source_stride, dest_stride, mask_stride);
		}
	}
}

////////////////////////////////////////////////////

//vpu entry point to be called by the cpu
//actually performs the composition
void VpuComposite(const struct PackedCompositeOp * const pOps, const int numOps)
{
	//loop through each packed operation
	for (int count = 0; count < numOps; count++)
	{
		const struct PackedCompositeOp * const p = &pOps[count];

		//each packed operation shares the same composite function and images
		//only the offsets within these images differ
		p->m_pCompositor(p->m_pOp, p->m_numOps,
				p->m_pSource, p->m_pDest, p->m_pMask,
				p->m_sourceStride, p->m_destStride, p->m_maskStride,
				p->m_sourceWidth, p->m_sourceHeight,
				p->m_sourceWrap);
	}
}

//vpu entry point to be called by the cpu
//returns a function pointer that the vpu can call to perfom an individual composition
ptr2PdFunc EnumToFuncVpu(const enum PorterDuffOp op,
		enum PixelFormat source_pf, enum PixelFormat dest_pf, enum PixelFormat mask_pf)
{
	if (op == kPictOpOver)
	{
		if (mask_pf == kNoData)
		{
			if (source_pf == kA8R8G8B8 && dest_pf == kX8B8G8R8)
				return vpu_over_a8r8g8b8_x8b8g8r8_invalid;
		}
		else if (mask_pf == kA8)
		{
			if (source_pf == kA8R8G8B8 && dest_pf == kX8B8G8R8)
				return vpu_over_a8r8g8b8_x8b8g8r8_a8_valid;
		}
	}

	return 0;
}
