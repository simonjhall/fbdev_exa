/*
 * recording.c
 *
 *  Created on: 18 Dec 2012
 *      Author: Simon Hall
 * Used to record the number of each call.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa_acc.h"

static int g_cbValidateCount;

static int g_checkComposite;
static int g_prepareComposite;
static int g_composite;
static int g_compositePixelCount;
static int g_doneComposite;
static int g_doneCompositePixelCount;
static int g_doneCompositePixelCountVpu;
static int g_doneCompositePixelCountNonVpu;
static int g_doneCompositeVpuCount;

static int g_prepareSolid;
static int g_solid;
static int g_solidFallback;
static int g_solidPixelCount;
static int g_solidPixelCountFallback;
static int g_doneSolid;

static int g_prepareCopy;
static int g_copy;
static int g_copyPixelCount;
static int g_doneCopy;

static int g_wait;
static int g_waitUsCount;

static int g_upload;
static int g_uploadPixelCount;

static int g_download;
static int g_downloadPixelCount;
static int g_downloadFallback;
static int g_downloadPixelCountFallback;

static unsigned long long g_totalUpload = 0;
static unsigned long long g_totalDownload = 0;

static BOOL g_recordVerbose = FALSE;
/////////////
void RecordToggle(BOOL b)
{
	g_recordVerbose = b;
}

void RecordReset(void)
{
	g_cbValidateCount = 0;

	g_checkComposite = 0;
	g_prepareComposite = 0;
	g_composite = 0;
	g_compositePixelCount = 0;
	g_doneComposite = 0;
	g_doneCompositePixelCount = 0;
	g_doneCompositePixelCountVpu = 0;
	g_doneCompositePixelCountNonVpu = 0;
	g_doneCompositeVpuCount = 0;

	g_prepareSolid = 0;
	g_solid = 0;
	g_solidFallback = 0;
	g_solidPixelCount = 0;
	g_solidPixelCountFallback = 0;
	g_doneSolid = 0;

	g_prepareCopy = 0;
	g_copy = 0;
	g_copyPixelCount = 0;
	g_doneCopy = 0;

	g_wait = 0;
	g_waitUsCount = 0;

	g_upload = 0;
	g_uploadPixelCount = 0;

	g_download = 0;
	g_downloadPixelCount = 0;
	g_downloadFallback = 0;
	g_downloadPixelCountFallback = 0;
}

void RecordPrint(void)
{
	int count;
	const int callScale = 2;
	const int pixScale = 9;
	const int max = 80;

	if (!g_recordVerbose)
		return;

#define BUFFER_SIZE 2048
	static char buffer[BUFFER_SIZE];

	int pos = snprintf(buffer, BUFFER_SIZE - pos, "\033c");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "--------------------------------------------------------\n");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "                  %3d-------%3d-------%3d-------%3d-------%3d\n", 0, 10 << callScale, 20 << callScale, 30 << callScale, 40 << callScale);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "CheckComposite:   ");
	for (count = 0; count < ((g_checkComposite >> callScale) > max ? max : (g_checkComposite >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "PrepareComposite: ");
	for (count = 0; count < ((g_prepareComposite >> callScale) > max ? max : (g_prepareComposite >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Composite:        ");
	for (count = 0; count < ((g_composite >> callScale) > max ? max : (g_composite >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "DoneComposite:    ");
	for (count = 0; count < ((g_doneComposite >> callScale) > max ? max : (g_doneComposite >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "PrepareSolid:     ");
	for (count = 0; count < ((g_prepareSolid >> callScale) > max ? max : (g_prepareSolid >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "S");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Solid:            ");
	for (count = 0; count < ((g_solid >> callScale) > max ? max : (g_solid >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "S");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "DoneSolid:        ");
	for (count = 0; count < ((g_doneSolid >> callScale) > max ? max : (g_doneSolid >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "S");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "PrepareCopy:      ");
	for (count = 0; count < ((g_prepareCopy >> callScale) > max ? max : (g_prepareCopy >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Copy:             ");
	for (count = 0; count < ((g_copy >> callScale) > max ? max : (g_copy >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "DoneCopy:         ");
	for (count = 0; count < ((g_doneCopy >> callScale) > max ? max : (g_doneCopy >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Upload:           ");
	for (count = 0; count < ((g_upload >> callScale) > max ? max : (g_upload >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "U");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Download:         ");
	for (count = 0; count < ((g_download >> callScale) > max ? max : (g_download >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "D");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Wait:             ");
	for (count = 0; count < ((g_wait >> callScale) > max ? max : (g_wait >> callScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "W");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "                  %4d------%4d------%4d------%4d------%4d\n", 0, 10 << pixScale, 20 << pixScale, 30 << pixScale, 40 << pixScale);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Total comp pix:   ");
	for (count = 0; count < ((g_doneCompositePixelCount >> pixScale) > max ? max : (g_doneCompositePixelCount >> pixScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Avg comp pix:     ");
	if (g_doneComposite)
		for (count = 0; count < (((g_doneCompositePixelCount / g_doneComposite) >> pixScale) > max ? max : ((g_doneCompositePixelCount / g_doneComposite) >> pixScale)); count++)
			pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Total solid pix:  ");
	for (count = 0; count < ((g_solidPixelCount >> pixScale) > max ? max : (g_solidPixelCount >> pixScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "S");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Avg solid pix:    ");
	if (g_doneSolid)
		for (count = 0; count < (((g_solidPixelCount / g_doneSolid) >> pixScale) > max ? max : ((g_solidPixelCount / g_doneSolid) >> pixScale)); count++)
			pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "S");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Total copy pix:   ");
	for (count = 0; count < ((g_solidPixelCount >> pixScale) > max ? max : (g_solidPixelCount >> pixScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Avg copy pix:     ");
	if (g_doneCopy)
		for (count = 0; count < (((g_copyPixelCount / g_doneCopy) >> pixScale) > max ? max : ((g_copyPixelCount / g_doneCopy) >> pixScale)); count++)
			pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "C");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Total upload pix: ");
	for (count = 0; count < ((g_uploadPixelCount >> pixScale) > max ? max : (g_uploadPixelCount >> pixScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "U");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Avg upload pix:   ");
	if (g_upload)
		for (count = 0; count < (((g_uploadPixelCount / g_upload) >> pixScale) > max ? max : ((g_uploadPixelCount / g_upload) >> pixScale)); count++)
			pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "U");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Total download pix");
	for (count = 0; count < ((g_downloadPixelCount >> pixScale) > max ? max : (g_downloadPixelCount >> pixScale)); count++)
		pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "D");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Avg download pix: ");
	if (g_download)
		for (count = 0; count < (((g_downloadPixelCount / g_download) >> pixScale) > max ? max : ((g_downloadPixelCount / g_download) >> pixScale)); count++)
			pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "D");
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\n\n");

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "VPU/CPU comp job: %.2f%%\n", (double)g_doneCompositeVpuCount / g_doneComposite * 100.0);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "VPU/CPU comp pix: %.2f%%\n", (double)g_doneCompositePixelCountVpu / (g_doneCompositePixelCountVpu + g_doneCompositePixelCountNonVpu) * 100.0);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "DMA/CPU fill job: %.2f%%\n", (double)(g_solid - g_solidFallback) / g_solid * 100.0);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "DMA/CPU fill pix: %.2f%%\n", (double)(g_solidPixelCount - g_solidPixelCountFallback) / g_solidPixelCount * 100.0);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "DMA/CPU down job: %.2f%%\n", (double)(g_download - g_downloadFallback) / g_download * 100.0);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "DMA/CPU down pix: %.2f%%\n", (double)(g_downloadPixelCount - g_downloadPixelCountFallback) / g_downloadPixelCount * 100.0);

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "CBs validated   : %d\n", g_cbValidateCount);

	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\nTotal upload pix: %.2f million\n", (double)g_totalUpload / 1000000);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Total downl pix : %.2f million\n", (double)g_totalDownload / 1000000);

	int used, most, unused;
	OffscreenUsedUnused(&used, &most, &unused);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "\nUsed bytes      : %d (%.2f MB)\n", used, (float)used / 1048576);
//	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Highest used    : %d (%.2f MB)\n", most, (float)most / 1048576);
	pos += snprintf(&buffer[pos], BUFFER_SIZE - pos, "Free bytes      : %d (%.2f MB)\n", unused, (float)unused / 1048576);

	fprintf(stderr, buffer);
}

void RecordCheckComposite(void)
{
	g_checkComposite++;
}

void RecordPrepareComposite(void)
{
	g_prepareComposite++;
}

void RecordComposite(int numPixels)
{
	g_composite++;
	g_compositePixelCount += numPixels;
}

void RecordDoneComposite(int totalPixels, int vpu)
{
	g_doneComposite++;
	g_doneCompositePixelCount += totalPixels;

	if (vpu)
		g_doneCompositePixelCountVpu += totalPixels;
	else
		g_doneCompositePixelCountNonVpu += totalPixels;

	g_doneCompositeVpuCount += vpu;
}

void RecordPrepareSolid(void)
{
	g_prepareSolid++;
}

void RecordSolid(int numPixels, int fallback)
{
	g_solid++;
	g_solidPixelCount += numPixels;

	if (fallback)
	{
		g_solidPixelCountFallback += numPixels;
		g_solidFallback++;
	}
}

void RecordDoneSolid(void)
{
	g_doneSolid++;
}

void RecordPrepareCopy(void)
{
	g_prepareCopy++;
}

void RecordCopy(int numPixels)
{
	g_copy++;
	g_copyPixelCount += numPixels;
}

void RecordDoneCopy(void)
{
	g_doneCopy++;
}

void RecordWait(int us)
{
	g_wait++;
	g_waitUsCount += us;
}

void RecordUpload(int numPixels)
{
	g_upload++;
	g_uploadPixelCount += numPixels;
	g_totalUpload += numPixels;
}

void RecordDownload(int numPixels, int fallback)
{
	g_download++;
	g_downloadPixelCount += numPixels;
	g_totalDownload += numPixels;

	if (fallback)
	{
		g_downloadFallback++;
		g_downloadPixelCountFallback += numPixels;
	}
}

void RecordCbValidate(int cbs)
{
	g_cbValidateCount += cbs;
}

