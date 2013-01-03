/*
 * exa_composite.c
 *
 *  Created on: 5 Aug 2012
 *      Author: Simon Hall
 * Implementation of the RENDER composite function.
 * This includes testing to see if a function can be run, selecting the compositor (either cpu and/or vpu)
 * packaging up all the date and running batches of composite operations.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"
#include "exa.h"

#include "exa_acc.h"

#include <time.h>

//#define COMPOSITE_DEBUG
#define CALL_RECORDING

//#define DEBUG_REJECTION(...) xf86DrvMsg(0, X_WARNING, __VA_ARGS__)
#define DEBUG_REJECTION(...)

//#define DEBUG_VPU_REJECTION(...) xf86DrvMsg(0, X_WARNING, __VA_ARGS__)
#define DEBUG_VPU_REJECTION(...)

//#define DEBUG_VPU_HYSTERESIS(...) xf86DrvMsg(0, X_INFO, __VA_ARGS__)
#define DEBUG_VPU_HYSTERESIS(...)

#define PIXEL_COUNT_THRESHOLD 600

//work to be run
//guh, const in c does not mean constant...
#define MAX_COMP_OPS 128

//for use when vpu offload is disabled
static struct CompositeOp g_opList[MAX_COMP_OPS];

unsigned int g_pendingOps = 0;
ptr2PdFunc g_pCompositorCpu;
ptr2PdFunc g_pCompositorVpu;
unsigned int g_pixelsToProcess;

//vpu versions
//struct PackedCompositeOp g_packed;
BOOL g_usingVpu;

//////////////
static PicturePtr g_pSrcPicture;
static PicturePtr g_pMaskPicture;
static PicturePtr g_pDstPicture;
static PixmapPtr g_pSrc;
static PixmapPtr g_pMask;
static PixmapPtr g_pDst;
//////////////
static struct CompositeOp *GetOpListBase(void)
{
	static struct CompositeOp *pBase = 0;

	if (!pBase)
	{
		if (IsEnabledVpuOffload())
			pBase = (struct CompositeOp *)((unsigned long)GetVpuMemoryBase() + GetVpuCodeSize() - 4096 * 2);
		else
			pBase = g_opList;
	}

	return pBase;
}

static struct PackedCompositeOp *GetPackedOpListBase(void)
{
	static struct PackedCompositeOp *pBase = 0;

	if (!pBase)
	{
		MY_ASSERT(IsEnabledVpuOffload());
		pBase = (struct PackedCompositeOp *)((unsigned long)GetVpuMemoryBase() + GetVpuCodeSize() - 4096 * 3);
	}

	return pBase;
}
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
#ifdef COMPOSITE_DEBUG
	xf86DrvMsg(0, X_DEFAULT, "%s source %p + mask %p -> dest %p\n", __FUNCTION__, pSrcPicture, pMaskPicture, pDstPicture);
#endif
//	return FALSE;
#ifdef CALL_RECORDING
	RecordCheckComposite();
#endif

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

	//reset the compositors
	g_pCompositorCpu = 0;
	g_pCompositorVpu = 0;
	g_pixelsToProcess = 0;

	//repeat flag + no drawable = ok
	//repeat flag + drawable 1x1 = ok
	//repeat flag + drawable >1x1 = not ok
	BOOL source_wrap_reqd2 = pSrcPicture->repeat && (!pSrcPicture->pDrawable || pSrcPicture->pDrawable->width != 1 || pSrcPicture->pDrawable->height != 1);
	BOOL source_wrap_reqd = pSrcPicture->repeat && pSrcPicture->pDrawable && (pSrcPicture->pDrawable->width > 1 || pSrcPicture->pDrawable->height > 1);

	if (source_wrap_reqd != source_wrap_reqd2)
		fprintf(stderr, "systems disagree %d %d\n", source_wrap_reqd, source_wrap_reqd2);

	//get our composite function
	if (!IsEnabledVpuOffload() || source_wrap_reqd || !(g_pCompositorVpu = EnumToFuncVpuWrap(op,
				pSrcPicture->format, pDstPicture->format,
				maskpf)))
	{
		//this is the failed or disabled vpu path
		DEBUG_VPU_REJECTION("vpu reject: op %d, format %08x/%08x/%08x, repeat %d w/h %dx%d\n",
				op,
				pSrcPicture->format, pDstPicture->format,
				pMaskPicture ? pMaskPicture->format : 0,
				pSrcPicture->repeat,
				pSrcPicture->pDrawable ? pSrcPicture->pDrawable->width : -1,
				pSrcPicture->pDrawable ? pSrcPicture->pDrawable->height : -1);

		//vpu should be disabled or our reference version should also fail
//		MY_ASSERT(!IsEnabledVpuOffload() || !EnumToFuncVpu(op, pSrcPicture->format, pDstPicture->format, maskpf));

		//just get the cpu compositor
		g_pCompositorCpu = EnumToFunc(op,
					pSrcPicture->format, pDstPicture->format,
					maskpf);

		g_usingVpu = FALSE;
	}
	else
	{
		//successful vpu path
//		ptr2PdFunc func = EnumToFuncVpu(op,
//				pSrcPicture->format, pDstPicture->format,
//				maskpf);
//		MY_ASSERT(func);
		g_usingVpu = TRUE;

		//get the backup cpu route
		g_pCompositorCpu = EnumToFunc(op,
					pSrcPicture->format, pDstPicture->format,
					maskpf);
	}

	//if possible...
	if (!g_pCompositorCpu && !g_pCompositorVpu )
	{
		DEBUG_REJECTION("reject: op %d, format %08x/%08x/%08x\n",
				op,
				pSrcPicture->format, pDstPicture->format,
				pMaskPicture ? pMaskPicture->format : 0);

		return FALSE;
	}
#ifdef COMPOSITE_DEBUG
	else
		if (g_usingVpu)
		xf86DrvMsg(0, X_INFO, "accept: op %d, format %08x/%08x/%08x, compo %p & %p, vpu %s, rep %d w/h %dx%d\n",
				op,
				pSrcPicture->format, pDstPicture->format,
				pMaskPicture ? pMaskPicture->format : 0,
				g_pCompositorCpu, g_pCompositorVpu,
				g_usingVpu ? "ON" : "OFF",
				pSrcPicture->repeat,
				pSrcPicture->pDrawable ? pSrcPicture->pDrawable->width : -1,
				pSrcPicture->pDrawable ? pSrcPicture->pDrawable->height : -1);
#endif

	return TRUE;
//	return FALSE;
}

Bool PrepareComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrc,
		PixmapPtr pMask, PixmapPtr pDst)
{
	MY_ASSERT(pDst);

#ifdef CALL_RECORDING
	RecordPrepareComposite();
#endif

#ifdef COMPOSITE_DEBUG
	xf86DrvMsg(0, X_DEFAULT, "%s op %d source %p + mask %p -> dest %p\n", __FUNCTION__, op, pSrcPicture, pMaskPicture, pDstPicture);
#endif

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
#ifdef COMPOSITE_DEBUG
	xf86DrvMsg(0, X_DEFAULT, "%.3f, %s, (wh %2dx%2d s %2d,%2d m %2d,%2d d %2d,%2d, bpp %2d,%2d,%2d dim %2d,%2d %2d,%2d %2d,%2d)\n",
			(double)clock() / CLOCKS_PER_SEC,
			__FUNCTION__,
			width, height,
			srcX, srcY,
			maskX, maskY,
			dstX, dstY,
			g_pSrc ? g_pSrc->drawable.depth : -1,
			g_pDst ? g_pDst->drawable.depth : -1,
			g_pMask ? g_pMask->drawable.depth : -1,
			g_pSrc ? g_pSrc->drawable.width : -1, g_pSrc ? g_pSrc->drawable.height : -1,
			g_pDst ? g_pDst->drawable.width : -1, g_pDst ? g_pDst->drawable.height : -1,
			g_pMask ? g_pMask->drawable.width : -1, g_pMask ? g_pMask->drawable.height : -1);
#endif

	//record the state in the list

	struct CompositeOp *pOp = &GetOpListBase()[g_pendingOps];

	pOp->srcX = srcX;
	pOp->srcY = srcY;

	pOp->maskX = maskX;
	pOp->maskY = maskY;

	pOp->dstX = dstX;
	pOp->dstY = dstY;

	if (!g_pSrcPicture->repeat && g_pSrc && g_pSrc->drawable.width > 1 && g_pSrc->drawable.height > 1)
	{
		//compute last read source pixel
		int last_x = srcX + width;
		int last_y = srcY + height;

		if (last_x > g_pSrc->drawable.width)
		{
			width -= (last_x - g_pSrc->drawable.width);
			xf86DrvMsg(0, X_WARNING, "composite would have overread in X by %d pixels\n", last_x - g_pSrc->drawable.width);
		}

		if (last_y > g_pSrc->drawable.height)
		{
			height -= (last_y - g_pSrc->drawable.height);
			xf86DrvMsg(0, X_WARNING, "composite would have overread in Y by %d pixels\n", last_y - g_pSrc->drawable.height);
		}
	}

	pOp->width = width;
	pOp->height = height;

	g_pixelsToProcess += width * height;
#ifdef CALL_RECORDING
	RecordComposite(width * height);
#endif

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
		WritePGMGrey(filename_src, g_pSrc->drawable.width, g_pSrc->drawable.height, exaGetPixmapPitch(g_pSrc), exaGetPixmapAddressNEW(g_pSrc));
	if (g_pDstPicture->format == kA8)
		WritePGMGrey(filename_dest, g_pDst->drawable.width, g_pDst->drawable.height, exaGetPixmapPitch(g_pDst), exaGetPixmapAddressNEW(g_pDst));
	if (g_pMaskPicture && g_pMaskPicture->format == kA8)
		WritePGMGrey(filename_mask, g_pMask->drawable.width, g_pMask->drawable.height, exaGetPixmapPitch(g_pMask), exaGetPixmapAddressNEW(g_pMask));

	g_pCompositor(g_opList, g_pendingOps,
				exaGetPixmapAddressNEW(g_pSrc), exaGetPixmapAddressNEW(pDst), g_pMask ? exaGetPixmapAddressNEW(g_pMask) : 0,
				exaGetPixmapPitch(g_pSrc), exaGetPixmapPitch(pDst), g_pMask ? exaGetPixmapPitch(g_pMask) : 0,
				g_pSrc->drawable.width, g_pSrc->drawable.height,
				g_pSrcPicture->repeat);
	g_pendingOps = 0;

	if (g_pDstPicture->format == kA8)
		WritePGMGrey(filename_dest2, g_pDst->drawable.width, g_pDst->drawable.height, exaGetPixmapPitch(g_pDst), exaGetPixmapAddressNEW(g_pDst));
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
			source = exaGetPixmapAddressNEW(g_pSrc);
		else
			source = (unsigned char *)&g_pSrcPicture->pSourcePict->solidFill.color;

		//dest should always be legit
		dest = exaGetPixmapAddressNEW(pDst);

		//mask data selector
		if (g_pMask)								//exa mask
			mask = exaGetPixmapAddressNEW(g_pMask);
		else if (g_pMaskPicture)					//solid picture mask
			mask = (unsigned char *)&g_pMaskPicture->pSourcePict->solidFill.color;
		//else no mask (mask = 0, see above)

#ifdef CALL_RECORDING
		RecordDoneComposite(g_pixelsToProcess, 0);
#endif

		//todo - add the vpu path
		MY_ASSERT(g_pCompositorCpu);
		g_pCompositorCpu(GetOpListBase(), g_pendingOps,
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

#ifdef COMPOSITE_DEBUG
	xf86DrvMsg(0, X_DEFAULT, "%s dest pixmap %p\n", __FUNCTION__, pDst);
#endif

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
			source = exaGetPixmapAddressNEW(g_pSrc);
		else
			source = (unsigned char *)&g_pSrcPicture->pSourcePict->solidFill.color;

		//dest should always be legit
		dest = exaGetPixmapAddressNEW(pDst);

		//mask data selector
		if (g_pMask)								//exa mask
			mask = exaGetPixmapAddressNEW(g_pMask);
		else if (g_pMaskPicture)					//solid picture mask
			mask = (unsigned char *)&g_pMaskPicture->pSourcePict->solidFill.color;
		//else no mask (mask = 0, see above)

		MY_ASSERT(g_pCompositorCpu || g_pCompositorVpu);

		//final check to see if we should do the work
		if (g_usingVpu && g_pixelsToProcess < PIXEL_COUNT_THRESHOLD)
		{
			DEBUG_VPU_HYSTERESIS("insufficient pixels to bother with VPU path (%d)\n", g_pixelsToProcess);
			g_usingVpu = 0;
		}

		//if we're actually going to use the vpu, pack up the data
		if (g_usingVpu)
		{
			struct PackedCompositeOp *pPacked = GetPackedOpListBase();
			pPacked->m_pCompositor = g_pCompositorVpu;
			pPacked->m_pOp = GetOpListBase();
			pPacked->m_numOps = g_pendingOps;

			pPacked->m_pSource = source;
			pPacked->m_pDest = dest;
			pPacked->m_pMask = mask;

			pPacked->m_sourceStride = g_pSrc ? exaGetPixmapPitch(g_pSrc) : 0;
			pPacked->m_destStride = exaGetPixmapPitch(pDst);
			pPacked->m_maskStride = g_pMask ? exaGetPixmapPitch(g_pMask) : 0;

			pPacked->m_sourceWidth = g_pSrc ? g_pSrc->drawable.width : 1;
			pPacked->m_sourceHeight = g_pSrc ? g_pSrc->drawable.height : 1;
			pPacked->m_sourceWrap = g_pSrcPicture->repeat;

			MY_ASSERT(((unsigned long)dest & 3) == 0);
			MY_ASSERT(((unsigned long)source & 3) == 0);

#ifdef CALL_RECORDING
			RecordDoneComposite(g_pixelsToProcess, 1);
#endif

			VpuCompositeWrap(pPacked, 1);
		}
		else
		{	//else just kick the work
#ifdef CALL_RECORDING
			RecordDoneComposite(g_pixelsToProcess, 0);
#endif
			g_pCompositorCpu(GetOpListBase(), g_pendingOps,
							source,
							dest,
							mask,

							g_pSrc ? exaGetPixmapPitch(g_pSrc) : 0,																			//no pitch on solid
							exaGetPixmapPitch(pDst),
							g_pMask ? exaGetPixmapPitch(g_pMask) : 0,																		//only pitch on exa mask

							g_pSrc ? g_pSrc->drawable.width : 1, g_pSrc ? g_pSrc->drawable.height : 1,										//solid or not
							g_pSrcPicture->repeat);
		}

		g_pendingOps = 0;
	}
}
