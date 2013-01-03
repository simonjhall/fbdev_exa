/*
 * benchmark.c
 *
 *  Created on: 25 Nov 2012
 *      Author: Simon Hall
 *  Some code to test the speed of DMA and all the composition operations, both CPU and VPU.
 *  Also includes the fallback code.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "xf86.h"
#include "fb.h"
#include "exa.h"

#include "exa_acc.h"
#include "exa_copylinear_copy2d.inl"

void BenchCopy(void)
{
	struct timeval start, stop;

	MY_ASSERT(IsPendingUnkicked() == FALSE);
	MY_ASSERT(IsDmaPending() == FALSE);

	unsigned long size = GetMemorySize() / 2;

	//fill our CBs with X copies from the low end to the high end
	const int its = 30;
	int count;
	for (count = 0; count < its; count++)
		ForwardCopy(GetMemoryBase() + size, GetMemoryBase(), size);

	size *= its;

	MY_ASSERT(IsPendingUnkicked());
	MY_ASSERT(IsDmaPending() == FALSE);

	//kick
	gettimeofday(&start, 0);
	StartDma(GetUnkickedDmaHead(), TRUE);
	UpdateKickedDmaHead();

	MY_ASSERT(IsDmaPending());

	//wait
	WaitDma(TRUE);
	gettimeofday(&stop, 0);

	//reset the dma system
	extern unsigned int g_dmaTail, g_dmaUnkickedHead;
	extern BOOL g_headOfDma;

	g_dmaTail = 0;
	g_dmaUnkickedHead = 0;
	g_headOfDma = TRUE;

	xf86DrvMsg(0, X_INFO, "DMA copy at %.2f MB/s (%.2f MB in %d us, in %.2f MB chunks)\n",
			(float)size / 1048576 / ((double)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)) / 1000000),
			(float)size / 1048576,
			(int)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)),
			(float)(size / its) / 1048576);
}

void BenchFill(void)
{
	struct timeval start, stop;

	gettimeofday(&start, 0);
	for (int count = 0; count < 100; count++)
	{
		kern_get_max_burst();
	}
	gettimeofday(&stop, 0);

	xf86DrvMsg(0, X_INFO, "syscall latency appears to be %.2f us\n", (double)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)) / 100);

	MY_ASSERT(IsPendingUnkicked() == FALSE);
	MY_ASSERT(IsDmaPending() == FALSE);

	unsigned long max_size = GetMemorySize();
	unsigned long size_array[8] = { 1, 1,  1, 1,  max_size, max_size, 1000, 1000 };
	unsigned long it_array[8] = { 1, 1,  4000, 4000,  15, 15,  1, 1 };

	extern unsigned int g_dmaTail, g_dmaUnkickedHead;
	extern BOOL g_headOfDma;

	for (int size_it = 0; size_it < 6; size_it++)
	{
		unsigned long size = size_array[size_it];

		//fill our CBs with X fills all over
		const int its = it_array[size_it];
		for (int count = 0; count < its; count++)
			ForwardCopyNoSrcInc(GetMemoryBase(), GetMemoryBase(), size);

		size *= its;

		MY_ASSERT(IsPendingUnkicked());
		MY_ASSERT(IsDmaPending() == FALSE);

		//kick
		gettimeofday(&start, 0);
		StartDma(GetUnkickedDmaHead(), TRUE);
		UpdateKickedDmaHead();

		MY_ASSERT(IsDmaPending());

		//wait
		WaitDma(TRUE);
		gettimeofday(&stop, 0);

		xf86DrvMsg(0, X_INFO, "DMA fill at %.2f MB/s (%.2f MB in %d us, in %.2f MB chunks, %d DMA CBs %.2f us / CB)\n",
				(float)size / 1048576 / ((double)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)) / 1000000),
				(float)size / 1048576,
				(int)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)),
				(float)(size / its) / 1048576,
				g_dmaTail,
				(float)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)) / g_dmaTail);

		//reset the dma system
		g_dmaTail = 0;
		g_dmaUnkickedHead = 0;
		g_headOfDma = TRUE;
	}

	for (int size_it = 0; size_it < 8; size_it++)
	{
		unsigned long size = size_array[size_it];

		//fill our CBs with X fills all over
		const int its = it_array[size_it];
		gettimeofday(&start, 0);

		for (int count = 0; count < its; count++)
			FallbackFill32(GetMemoryBase(), 0, size / 4, 1, 0);

		gettimeofday(&stop, 0);
		size *= its;

		xf86DrvMsg(0, X_INFO, "fallback fill at %.2f MB/s (%.2f MB in %d us, in %.2f MB chunks, %d iterations %.2f us / it)\n",
				(float)size / 1048576 / ((double)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)) / 1000000),
				(float)size / 1048576,
				(int)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)),
				(float)(size / its) / 1048576,
				its,
				(float)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)) / its);
	}
}

void BenchComposite(void)
{
	enum PixelFormat pf_list[] = {kNoData, kA8, kA8R8G8B8, kX8R8G8B8, kA8B8G8R8, kX8B8G8R8};
	const char *pf_names[] = {"kNoData", "kA8", "kA8R8G8B8", "kX8R8G8B8", "kA8B8G8R8", "kX8B8G8R8"};
	enum PorterDuffOp op_list[] = {kPictOpOver, kPictOpAdd, kPictOpSrc};
	const char *op_names[] = {"kPictOpOver", "kPictOpAdd", "kPictOpSrc"};

	int op, source_pf, dest_pf, mask_pf;
	int repeat, source_one;
	int vpu;

	FILE *fp = fopen("composite_timings.txt", "w");

	char *buffer = malloc(100000);
	MY_ASSERT(buffer);
	buffer[0] = 0;

//	for (vpu = 1; vpu < (IsEnabledVpuOffload() ? 2 : 1); vpu++)
//		for (repeat = 0; repeat < 1; repeat++)
//			for (source_one = 0; source_one < 2; source_one++)
//						for (mask_pf = 0; mask_pf < 3; mask_pf++)
	for (repeat = 0; repeat < 2; repeat++)
		for (source_one = 0; source_one < 2; source_one++)
		{
			if (source_one == 1 && repeat == 1)
				continue;

			for (op = 0; op < 3; op++)
				for (source_pf = 0; source_pf < 6; source_pf++)
					for (mask_pf = 0; mask_pf < 6; mask_pf++)
						for (dest_pf = 0; dest_pf < 6; dest_pf++)
							for (vpu = 0; vpu < 2; vpu++)
							{
								ptr2PdFunc pCompositor;
								if (vpu)
								{
									if (repeat == 1)
										continue;
									pCompositor = EnumToFuncVpuWrap(op_list[op], pf_list[source_pf], pf_list[dest_pf], pf_list[mask_pf]);
								}
								else
									pCompositor = EnumToFunc(op_list[op], pf_list[source_pf], pf_list[dest_pf], pf_list[mask_pf]);

								if (!pCompositor)
									continue;

								const int width = 512;
								const int height = 512;

//								unsigned char *source = (unsigned char *)malloc(width * height * 4);
//								unsigned char *alpha = (unsigned char *)malloc(width * height * 4);
//								unsigned char *dest = (unsigned char *)malloc(width * height * 4);

								unsigned char *pBase = GetMemoryBase();

								unsigned char *source = pBase;
								pBase += (width * height * 4);

								unsigned char *alpha = pBase;
								pBase += (width * height * 4);

								unsigned char *dest = pBase;
								pBase += (width * height * 4);

								struct CompositeOp *pOpList = (struct CompositeOp *)pBase;
								pBase += sizeof(struct CompositeOp);

								struct PackedCompositeOp *pPacked = (struct PackedCompositeOp *)pBase;
								pPacked->m_pCompositor = pCompositor;
								pPacked->m_pOp = pOpList;
								pPacked->m_numOps = 1;

								pPacked->m_pSource = source;
								pPacked->m_pDest = dest;
								pPacked->m_pMask = alpha;

								pPacked->m_sourceStride = width * 4;
								pPacked->m_destStride = width * 4;
								pPacked->m_maskStride = width * 4;

								pPacked->m_sourceWidth = source_one ? 1 : width;
								pPacked->m_sourceHeight = source_one ? 1 : height;
								pPacked->m_sourceWrap = repeat;

								pOpList->srcX = pOpList->srcY = 0;
								pOpList->maskX = pOpList->maskY = 0;
								pOpList->dstX = pOpList->dstY = 0;
								pOpList->width = width;
								pOpList->height = height;

								memset(source, 0xcd, width * height * 4);
								memset(alpha, 0xcd, width * height * 4);
								memset(dest, 0xcd, width * height * 4);

								struct timeval start, stop;

								gettimeofday(&start, 0);

								if (vpu)
								{
									VpuCompositeWrap(pPacked, 1);
								}
								else
								{
									pCompositor(pOpList, 1,
											source, dest, alpha,
											width,
											width,
											width,
											source_one ? 1 : width, source_one ? 1 : height,
											repeat);
								}

								gettimeofday(&stop, 0);

								char shorter[500];

								sprintf(shorter, "%p\t%s\t%s\t%s\t%s\t%s\t%s\t",
										pCompositor,
										op_names[op],
										pf_names[source_pf], pf_names[dest_pf], pf_names[mask_pf],
										repeat ? "true" : "false",
										source_one ? "Zero" : (repeat ? "Wrapped" : "Normal"));
								strcat(buffer, shorter);

								sprintf(shorter, "\t%d\tpix in\t%lld\tus,\t%.3f\tMP/s\n",
										width * height, (stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec),
										(double)(width * height) / ((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec)));
								strcat(buffer, shorter);

//								free(source);
//								free(alpha);
//								free(dest);
							}
			}

	if (fp)
	{
		fwrite(buffer, 1, strlen(buffer), fp);
		fclose(fp);
	}

	free(buffer);
}
