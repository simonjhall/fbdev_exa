#include <sys/time.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa.h"

#include "exa_acc.h"

//#define TIMING_GRAPH

#ifdef TIMING_GRAPH
static unsigned int last_marker = 0;
static unsigned long long last_us = 0;

unsigned long long GetMicroSeconds(void)
{
	static BOOL initial_time_set = FALSE;
	static struct timeval start;

	if (initial_time_set == FALSE)
	{
		initial_time_set = TRUE;
		gettimeofday(&start, NULL);
	}

	struct timeval stop;

	gettimeofday(&stop, NULL);

	//cheers dom
	return ((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec));
}

unsigned int GetTimingHits(BOOL inc)
{
	static unsigned int hits = 0;
	if (inc)
		return hits++;
	else
		return hits;
}


FILE *GetBytesFile(void)
{
	static FILE *fp = 0;

	if (!fp)
	{
		char filename[100];
		sprintf(filename, "/home/simon/Desktop/x/src/driver/xf86-video-fbdev_exa/bytes.txt");
		fp = fopen(filename, "w");
		MY_ASSERT(fp);
	}

	if ((GetTimingHits(FALSE) % 10 == 0))
		fprintf(stderr, "%d hits\n", GetTimingHits(FALSE));

	if (GetTimingHits(FALSE) >= 5000)
	{
		fclose(fp);
		exit(0);
	}

	return fp;
}

FILE *GetTimingFile(void)
{
	static FILE *fp = 0;

	if (!fp)
	{
		char filename[100];
		sprintf(filename, "/home/simon/Desktop/x/src/driver/xf86-video-fbdev_exa/graph.dot");
		fp = fopen(filename, "w");
		MY_ASSERT(fp);

		fprintf(fp, "digraph G {\n");
		fprintf(fp, "subgraph cluster_000 {\n");
	}

	if ((GetTimingHits(FALSE) % 10 == 0))
		fprintf(stderr, "%d hits\n", GetTimingHits(FALSE));

	if (GetTimingHits(FALSE) >= 5000)
	{
		fprintf(fp, "\n}\n}\n");
		fclose(fp);
		exit(0);
	}

	return fp;
}

BOOL GetEventInfo(PixmapPtr p, unsigned int *pPrevIndex, unsigned int *pPrevWait)
{
	unsigned int *ptr = (unsigned int *)exaGetPixmapAddress(p);
	unsigned int prev_code = ptr[0];
	*pPrevIndex = ptr[1];
	*pPrevWait = ptr[2];

	if (prev_code == 0x12345678)
		return TRUE;
	else
		return FALSE;
}

void SetEventInfo(PixmapPtr p)
{
	unsigned int *ptr = (unsigned int *)exaGetPixmapAddress(p);
	ptr[0] = 0x12345678;
	ptr[1] = GetTimingHits(TRUE);
	ptr[2] = last_marker;
}


PixmapPtr last_render_target = 0;
unsigned int bytes_written = 0;
unsigned int solid_written = 0;
unsigned int copy_written = 0;
unsigned int composite_written = 0;

static int copy_count = 0;
static PixmapPtr SrcPixmap, DstPixmap;
static PixmapPtr Src, Mask_, Dst;
static unsigned int solid_count;
#endif

/**************************/
// null versions

Bool NullPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg)
{
#ifdef TIMING_GRAPH
	if (last_us == 0)
		last_us = GetMicroSeconds();

	solid_count = 0;
#endif
	return TRUE;
}

void NullSolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
#ifdef TIMING_GRAPH
	if (last_render_target != pPixmap)
	{
		fprintf(GetBytesFile(), "%d\t%d\t%d\t%d\n", bytes_written, solid_written, copy_written, composite_written);
		bytes_written = 0;
		solid_written = 0;
		copy_written = 0;
		composite_written = 0;
	}
	bytes_written += (x2 - x1) * (y2 - y1);
	solid_written += (x2 - x1) * (y2 - y1);
	last_render_target = pPixmap;

	solid_count += (x2 - x1) * (y2 - y1);
#endif
}

void NullDoneSolid(PixmapPtr p)
{
#ifdef TIMING_GRAPH
	unsigned int prev_index, prev_wait;
	static int solid = 0;

	if (GetEventInfo(p, &prev_index, &prev_wait)/* && prev_wait == last_marker*/)
	{
		fprintf(GetTimingFile(), "\"%d_%d_%p\"->\"%d_%d_%p\"[label=\"S %d %.3f\"];\n",
				last_marker, prev_index, p,
				last_marker, GetTimingHits(FALSE), p,
				solid_count / 1, (float)(GetMicroSeconds() - last_us) / 1000);
	}
	else
	{
		fprintf(GetTimingFile(), "\"solid_%d\"->\"%d_%d_%p\"[label=\"S %d %.3f\"];\n",
				solid++,
				last_marker, GetTimingHits(FALSE), p,
				solid_count / 1, (float)(GetMicroSeconds() - last_us) / 1000);
	}

	SetEventInfo(p);
#endif
}

Bool NullPrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int dx,
		int dy, int alu, Pixel planemask)
{
#ifdef TIMING_GRAPH
	if (last_us == 0)
		last_us = GetMicroSeconds();

	SrcPixmap = pSrcPixmap;
	DstPixmap = pDstPixmap;
#endif

	return TRUE;
}

void NullCopy(PixmapPtr pDstPixmap, int srcX, int srcY,
		int dstX, int dstY, int width, int height)
{
#ifdef TIMING_GRAPH
	if (last_render_target != pDstPixmap)
	{
		fprintf(GetBytesFile(), "%d\t%d\t%d\t%d\n", bytes_written, solid_written, copy_written, composite_written);
		bytes_written = 0;
		solid_written = 0;
		copy_written = 0;
		composite_written = 0;
	}
	bytes_written += width * height;
	copy_written += width * height;
	last_render_target = pDstPixmap;

	copy_count += width * height;

	unsigned int prev_dest_index, prev_dest_wait;
	unsigned int prev_src_index, prev_src_wait;

	static unsigned int copy = 0;

	if (GetEventInfo(DstPixmap, &prev_dest_index, &prev_dest_wait)/* && prev_dest_wait == last_marker*/)
	{
		if (prev_dest_wait != last_marker && prev_dest_index != GetTimingHits(FALSE))
			fprintf(GetTimingFile(), "\"%d_%d_%p\"->\"%d_%d_%p\"[label=\"dest copy\"];\n",
					last_marker, prev_dest_index, DstPixmap,
					last_marker, GetTimingHits(FALSE), DstPixmap);
	}
	else
	{
		fprintf(GetTimingFile(), "\"copy_%d\"->\"%d_%d_%p\"[label=\"dest copy\"];\n",
				copy,
				last_marker, GetTimingHits(FALSE), DstPixmap);
	}

	if (GetEventInfo(SrcPixmap, &prev_src_index, &prev_src_wait)/* && prev_src_wait == last_marker*/)
	{
		fprintf(GetTimingFile(), "\"%d_%d_%p\"->\"%d_%d_%p\"[label=\"src copy %d %.3f\"];\n",
				last_marker, prev_src_index, SrcPixmap,
				last_marker, GetTimingHits(FALSE), DstPixmap,
				copy_count / 1, (float)(GetMicroSeconds() - last_us) / 1000);
	}
	else
	{
		fprintf(GetTimingFile(), "\"copy_%d\"->\"%d_%d_%p\"[label=\"src copy %d %.3f\"];\n",
				copy,
				last_marker, GetTimingHits(FALSE), DstPixmap,
				copy_count / 1, (float)(GetMicroSeconds() - last_us) / 1000);
	}


	copy++;
	copy_count = 0;
#endif
}

void NullDoneCopy(PixmapPtr p)
{
#ifdef TIMING_GRAPH
	SetEventInfo(p);
#endif
}

Bool NullDownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h,
		char *dst, int dst_pitch)
{
#ifdef TIMING_GRAPH
	static unsigned int download_count = 0;
	unsigned int prev_index, prev_wait;

	if (last_us == 0)
		last_us = GetMicroSeconds();

	if (GetEventInfo(pSrc, &prev_index, &prev_wait)/* && prev_wait == last_marker*/)
	{
		fprintf(GetTimingFile(), "\"%d_%d_%p\"->\"%d_%d_%p\"->\"download %d %d %.3f\";\n",
				last_marker, prev_index, pSrc,
				last_marker, GetTimingHits(FALSE), pSrc,
				download_count++,
				(w * h) / 1, (float)(GetMicroSeconds() - last_us) / 1000);
	}
#endif
	return TRUE;
}

Bool NullUploadToScreen(PixmapPtr pDst, int x, int y, int w, int h,
		char *src, int src_pitch)
{
#ifdef TIMING_GRAPH
	static unsigned int upload_count = 0;

	if (last_us == 0)
		last_us = GetMicroSeconds();

	fprintf(GetTimingFile(), "\"upload %d %d %.3f\"->\"%d_%d_%p\";\n",
			upload_count++, (w * h) / 1, (float)(GetMicroSeconds() - last_us) / 1000,
			last_marker, GetTimingHits(FALSE), pDst);

	SetEventInfo(pDst);
#endif
	return TRUE;
}

void NullWaitMarker(ScreenPtr pScreen, int Marker)
{
#ifdef TIMING_GRAPH
	if (last_us == 0)
		last_us = GetMicroSeconds();

	last_marker++;
	unsigned long long us = GetMicroSeconds();

	fprintf(GetTimingFile(), "label=\"%.3f ms\"\n", (double)(us - last_us) / 1000);
	fprintf(GetTimingFile(), "\n}\nsubgraph cluster_%03d {\n", last_marker);

	last_us = 0;

	fprintf(GetBytesFile(), "%d\t%d\t%d\t%d\n", bytes_written, solid_written, copy_written, composite_written);
	bytes_written = 0;
	solid_written = 0;
	copy_written = 0;
	composite_written = 0;
	last_render_target = 0;
#endif
}

Bool NullCheckComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture)
{
#ifdef TIMING_GRAPH
	if (last_us == 0)
		last_us = GetMicroSeconds();
#endif

	return TRUE;
}

Bool NullPrepareComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrc,
		PixmapPtr pMask, PixmapPtr pDst)
{
#ifdef TIMING_GRAPH
	if (last_us == 0)
		last_us = GetMicroSeconds();

	Src = pSrc;
	Mask_ = pMask;
	Dst = pDst;
#endif
	return TRUE;
}

void NullComposite(PixmapPtr pDst, int srcX, int srcY, int maskX,
		int maskY, int dstX, int dstY, int width, int height)
{
#ifdef TIMING_GRAPH
	unsigned int prev_dest_index, prev_dest_wait;
	unsigned int prev_src_index, prev_src_wait;
	unsigned int prev_mask_index, prev_mask_wait;

	if (last_render_target != pDst)
	{
		fprintf(GetBytesFile(), "%d\t%d\t%d\t%d\n", bytes_written, solid_written, copy_written, composite_written);
		bytes_written = 0;
		solid_written = 0;
		copy_written = 0;
		composite_written = 0;
	}
	bytes_written += width * height;
	composite_written += width * height;
	last_render_target = pDst;

	if (Mask_)
	{
		static int mask = 0;
		if (GetEventInfo(Mask_, &prev_mask_index, &prev_mask_wait) /*&& prev_mask_wait == last_marker*/)
		{
			fprintf(GetTimingFile(), "\"%d_%d_%p\"->\"%d_%d_%p\"[label=\"C M %d\"];\n",
					last_marker, prev_mask_index, Mask_,
					last_marker, GetTimingHits(FALSE), Dst,
					(width * height) / 1);
		}
		else
		{
			fprintf(GetTimingFile(), "\"CM_%d\"->\"%d_%d_%p\"[label=\"C M %d\"];\n",
					mask++,
					last_marker, GetTimingHits(FALSE), Dst,
					(width * height) / 1);
		}
	}

	static int dest = 0;
	if (GetEventInfo(Dst, &prev_dest_index, &prev_dest_wait)/* && prev_dest_wait == last_marker*/)
	{
		if (prev_dest_wait != last_marker && prev_dest_index != GetTimingHits(FALSE))
			fprintf(GetTimingFile(), "\"%d_%d_%p\"->\"%d_%d_%p\"[label=\"C D %d %.3f\"];\n",
					last_marker, prev_dest_index, Dst,
					last_marker, GetTimingHits(FALSE), Dst,
					(width * height) / 1, (float)(GetMicroSeconds() - last_us) / 1000);
	}
	else
	{
		fprintf(GetTimingFile(), "\"CD_%d\"->\"%d_%d_%p\"[label=\"C D %d %.3f\"];\n",
				dest++,
				last_marker, GetTimingHits(FALSE), Dst,
				(width * height) / 1, (float)(GetMicroSeconds() - last_us) / 1000);
	}


	static int source = 0;
	if (GetEventInfo(Src, &prev_src_index, &prev_src_wait) /*&& prev_src_wait == last_marker*/)
	{
		fprintf(GetTimingFile(), "\"%d_%d_%p\"->\"%d_%d_%p\"[label=\"C S %d %.3f\"];\n",
				last_marker, prev_src_index, Src,
				last_marker, GetTimingHits(FALSE), Dst,
				(width * height) / 1, (float)(GetMicroSeconds() - last_us) / 1000);
	}
	else
	{
		fprintf(GetTimingFile(), "\"CS_%d\"->\"%d_%d_%p\"[label=\"C S %d %.3f\"];\n",
				source++,
				last_marker, GetTimingHits(FALSE), Dst,
				(width * height) / 1, (float)(GetMicroSeconds() - last_us) / 1000);
	}

#endif
}

void NullDoneComposite(PixmapPtr p)
{
#ifdef TIMING_GRAPH
	SetEventInfo(p);
#endif
}

