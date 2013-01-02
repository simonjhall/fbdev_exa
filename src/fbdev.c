/*
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *	     Michel Dänzer, <michel@tungstengraphics.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

/* all driver need this */
#include "xf86.h"
#include "xf86_OSproc.h"

#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"
#include "colormapst.h"
#include "xf86cmap.h"
#include "screenint.h"

#include "exa.h"
#include "exa_acc.h"

/* for visuals */
#include "fb.h"

#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
#include "xf86Resources.h"
#include "xf86RAC.h"
#endif

#include "fbdevhw.h"

#include "xf86xv.h"

static Bool g_usingExa = FALSE;
static Bool g_nullDriver = FALSE;

/* -------------------------------------------------------------------- */
/* prototypes                                                           */

static const OptionInfoRec * FBDevAvailableOptions(int chipid, int busid);
static void FBDevIdentify(int flags);
static Bool FBDevProbe(DriverPtr drv, int flags);
static Bool FBDevPreInit(ScrnInfoPtr pScrn, int flags);
static Bool FBDevScreenInit(int Index, ScreenPtr pScreen, int argc, char **argv);
static Bool FBDevCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool FBDevDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr);

/* -------------------------------------------------------------------- */

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;

#define FBDEV_VERSION		4000
#define FBDEV_NAME		"FBDEV_RPI"
#define FBDEV_DRIVER_NAME	"fbdev"

_X_EXPORT DriverRec FBDEV =
{ FBDEV_VERSION, FBDEV_DRIVER_NAME, FBDevIdentify, FBDevProbe,
		FBDevAvailableOptions, NULL, 0, FBDevDriverFunc, };

/* Supported "chipsets" */
static SymTabRec FBDevChipsets[] =
{
{ 0, "fbdev" },
{ -1, NULL } };

/* Supported options */
typedef enum
{
	OPTION_ALLOC_BLOCK,
	OPTION_ACCELMETHOD,
	OPTION_FAULT_IN_IMM,
	OPTION_BASE_MEM,
	OPTION_MEM_MODE,
	OPTION_VPU_OFFLOAD,
	OPTION_VPU_CODE,
	OPTION_MBOX_FILE,
	OPTION_VERBOSE_REPORTING,
	OPTION_SELF_MANAGED_OFFSCREEN,

} FBDevOpts;

static const OptionInfoRec FBDevOptions[] =
{
	{ OPTION_FAULT_IN_IMM, "FaultInImm", OPTV_BOOLEAN, { 0 }, FALSE },
	{ OPTION_ACCELMETHOD, "AccelMethod", OPTV_STRING, { 0 }, FALSE },
	{ OPTION_ALLOC_BLOCK, "BlockSize", OPTV_INTEGER, { 0 }, FALSE },
	{ OPTION_BASE_MEM, "BlockBase", OPTV_INTEGER, { 0 }, FALSE },
	{ OPTION_MEM_MODE, "MemMode", OPTV_STRING, { 0 }, FALSE },
	{ OPTION_VPU_OFFLOAD, "VpuOffload", OPTV_BOOLEAN, { 0 }, FALSE },
	{ OPTION_MBOX_FILE, "MboxFile", OPTV_STRING, { 0 }, FALSE },
	{ OPTION_VPU_CODE, "VpuElf", OPTV_STRING, { 0 }, FALSE },
	{ OPTION_VERBOSE_REPORTING, "VerboseReporting", OPTV_BOOLEAN, { 0 }, FALSE },
	{ OPTION_SELF_MANAGED_OFFSCREEN, "SelfManagedOffscreen", OPTV_BOOLEAN, { 0 }, FALSE },

	{ -1, NULL, OPTV_NONE, { 0 }, FALSE }
};

/* -------------------------------------------------------------------- */

#ifdef XFree86LOADER

MODULESETUPPROTO(FBDevSetup);

static XF86ModuleVersionInfo FBDevVersRec =
{ "fbdev", MODULEVENDORSTRING, MODINFOSTRING1, MODINFOSTRING2,
		XORG_VERSION_CURRENT, PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR,
		PACKAGE_VERSION_PATCHLEVEL, ABI_CLASS_VIDEODRV, ABI_VIDEODRV_VERSION,
		MOD_CLASS_VIDEODRV,
		{ 0, 0, 0, 0 } };

_X_EXPORT XF86ModuleData fbdevModuleData =
{ &FBDevVersRec, FBDevSetup, NULL };

pointer FBDevSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	if (!setupDone)
	{
		setupDone = TRUE;
		xf86AddDriver(&FBDEV, module, HaveDriverFuncs);
		return (pointer) 1;
	}
	else
	{
		if (errmaj)
			*errmaj = LDR_ONCEONLY;
		return NULL ;
	}
}

#endif /* XFree86LOADER */

/* -------------------------------------------------------------------- */
/* our private data, and two functions to allocate/free this            */

typedef struct
{
	unsigned char* fbstart;
	unsigned char* fbmem;
	int fboff;
	int lineLength;
	CloseScreenProcPtr CloseScreen;
	CreateScreenResourcesProcPtr CreateScreenResources;
	void (*PointerMoved)(int index, int x, int y);
	EntityInfoPtr pEnt;
	OptionInfoPtr Options;
} FBDevRec, *FBDevPtr;

#define FBDEVPTR(p) ((FBDevPtr)((p)->driverPrivate))

static Bool FBDevGetRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate != NULL )
		return TRUE;

	pScrn->driverPrivate = xnfcalloc(sizeof(FBDevRec), 1);
	return TRUE;
}

//sjh this is not called in a consistent fashion in the original driver
//this memory appears to leak
static void FBDevFreeRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate == NULL )
		return;
	free(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

/* -------------------------------------------------------------------- */

static const OptionInfoRec *
FBDevAvailableOptions(int chipid, int busid)
{
	return FBDevOptions;
}

static void FBDevIdentify(int flags)
{
	xf86PrintChipsets(FBDEV_NAME, "driver for framebuffer", FBDevChipsets);
}

static Bool FBDevProbe(DriverPtr drv, int flags)
{
	int i;
	ScrnInfoPtr pScrn;
	GDevPtr *devSections;
	int numDevSections;
	const char *dev;
	Bool foundScreen = FALSE;

	/* For now, just bail out for PROBE_DETECT. */
	if (flags & PROBE_DETECT)
		return FALSE;

	if ((numDevSections = xf86MatchDevice(FBDEV_DRIVER_NAME, &devSections))
			<= 0)
		return FALSE;

	if (!xf86LoadDrvSubModule(drv, "fbdevhw"))
		return FALSE;

	for (i = 0; i < numDevSections; i++)
	{

		dev = xf86FindOptionValue(devSections[i]->options, "fbdev");

		//sjh wish I'd never fixed that const warning...
		if (fbdevHWProbe(NULL, (char *) dev, NULL ))
		{
			int entity;

			entity = xf86ClaimFbSlot(drv, 0, devSections[i], TRUE);

			pScrn = NULL;
			pScrn = xf86ConfigFbEntity(pScrn, 0, entity, NULL, NULL, NULL,
					NULL );

			if (pScrn)
			{
				foundScreen = TRUE;

				pScrn->driverVersion = FBDEV_VERSION;
				pScrn->driverName = FBDEV_DRIVER_NAME;
				pScrn->name = FBDEV_NAME;
				pScrn->Probe = FBDevProbe;
				pScrn->PreInit = FBDevPreInit;
				pScrn->ScreenInit = FBDevScreenInit;
				pScrn->SwitchMode = fbdevHWSwitchModeWeak();
				pScrn->AdjustFrame = fbdevHWAdjustFrameWeak();
				pScrn->EnterVT = fbdevHWEnterVTWeak();
				pScrn->LeaveVT = fbdevHWLeaveVTWeak();
				pScrn->ValidMode = fbdevHWValidModeWeak();

				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "using %s\n",
						dev ? dev : "default device");
			}
		}
	}
	free(devSections);
	return foundScreen;
}

static Bool FBDevPreInit(ScrnInfoPtr pScrn, int flags)
{
	FBDevPtr fPtr;
	int default_depth, fbbpp;
	const char *s;
	int type;

	if (flags & PROBE_DETECT)
		return FALSE;

	/* Check the number of entities, and fail if it isn't one. */
	if (pScrn->numEntities != 1)
		return FALSE;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Raspberry Pi Xorg driver build date %s build time %s\n", __DATE__, __TIME__);

	pScrn->monitor = pScrn->confScreen->monitor;

	FBDevGetRec(pScrn);
	fPtr = FBDEVPTR(pScrn);

	fPtr->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

	/* open device */
	//sjh arbitary cast
	if (!fbdevHWInit(pScrn, NULL,
			(char *) xf86FindOptionValue(fPtr->pEnt->device->options, "fbdev")))
		return FALSE;
	default_depth = fbdevHWGetDepth(pScrn, &fbbpp);
	if (!xf86SetDepthBpp(pScrn, default_depth, default_depth, fbbpp,
			Support24bppFb | Support32bppFb | SupportConvert32to24
					| SupportConvert24to32))
		return FALSE;
	xf86PrintDepthBpp(pScrn);

	/* Get the depth24 pixmap format */
	if (pScrn->depth == 24 && pix24bpp == 0)
		pix24bpp = xf86GetBppFromDepth(pScrn, 24);

	/* color weight */
	if (pScrn->depth > 8)
	{
		rgb zeros =
		{ 0, 0, 0 };
		if (!xf86SetWeight(pScrn, zeros, zeros))
			return FALSE;
	}

	/* visual init */
	if (!xf86SetDefaultVisual(pScrn, -1))
		return FALSE;

	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor)
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "requested default visual"
				" (%s) is not supported at depth %d\n",
				xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
		return FALSE;
	}

	{
		Gamma zeros =
		{ 0.0, 0.0, 0.0 };

		if (!xf86SetGamma(pScrn, zeros))
		{
			return FALSE;
		}
	}

	pScrn->progClock = FALSE;
	pScrn->rgbBits = 8;
	pScrn->chipset = "fbdev";
	pScrn->videoRam = fbdevHWGetVidmem(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "hardware: %s (video memory:"
			" %dkB)\n", fbdevHWGetName(pScrn), pScrn->videoRam / 1024);

	/* handle options */
	xf86CollectOptions(pScrn, NULL );
	if (!(fPtr->Options = malloc(sizeof(FBDevOptions))))
		return FALSE;
	memcpy(fPtr->Options, FBDevOptions, sizeof(FBDevOptions));
	xf86ProcessOptions(pScrn->scrnIndex, fPtr->pEnt->device->options,
			fPtr->Options);

	/****** EXA *******/
	s = xf86GetOptValString(fPtr->Options, OPTION_ACCELMETHOD);

	//default to exa=on
	if (!s)
		g_usingExa = TRUE;
	else if (s && !xf86NameCmp(s, "EXA"))			//regular exa
		g_usingExa = TRUE;
	else if (s && !xf86NameCmp(s, "EXA_NULL"))//passthrough null driver (expect complete screen corruption)
	{
		g_usingExa = TRUE;
		g_nullDriver = TRUE;
	}

	if (!xf86LoadSubModule(pScrn, "fb"))
		return FALSE;

	//needed regardless of EXA usage or not
	switch (kern_init())
	{
	case 0:	//ok
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "kernel interface initialised\n");
		break;
	case 1:
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"kernel interface already initialised\n");
		break;
	case 2:
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"failed to initialise kernel interface (does /dev/dmaer_4k exist?)\n");
		return FALSE;
	case 3:
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Incompatible dmaer version\n");
		return FALSE;
	default:
		MY_ASSERT(0);
		return FALSE;
	}

	//check for exa in any form
	if (g_usingExa)
	{
		g_usingExa = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using EXA acceleration\n");

		if (g_nullDriver)
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
					"********USING NULL DRIVER EXPECT FULL SCREEN CORRUPTION - THIS IS INTENTIONAL********\n");

		//load the module
		if (!xf86LoadSubModule(pScrn, "exa"))
			return FALSE;

		//see how much memory we want to use for offscreen
		unsigned long buffer_size;
		if (!xf86GetOptValULong(fPtr->Options, OPTION_ALLOC_BLOCK,
				&buffer_size))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
					"Memory size not specified! (use BlockSize)\n");
			return FALSE;
		}

		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
				"Using %d bytes (%.2f MB) of memory as offscreen\n",
				buffer_size, (float) buffer_size / 1048576);
		SetMemorySize(buffer_size);

		//toggle the dumping of recording info
		if (xf86ReturnOptValBool(fPtr->Options, OPTION_VERBOSE_REPORTING, FALSE))
			RecordToggle(TRUE);
		else
			RecordToggle(FALSE);

		//find out if we're using vpu offload (off by default)
		if (xf86ReturnOptValBool(fPtr->Options, OPTION_VPU_OFFLOAD, FALSE))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Attempting to open VCIO mailbox\n");

			s = xf86GetOptValString(fPtr->Options, OPTION_MBOX_FILE);

			//try and open the file if it has been provided
			if (!s || (s && OpenVcMbox(s)))
			{
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to open VCIO mailbox via device file \"%s\"\n", s);
				return FALSE;
			}

			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Attempting to load VPU code\n");

			//locate the vpu code
			s = xf86GetOptValString(fPtr->Options, OPTION_VPU_CODE);

			//check they've supplied the option
			if (!s)
			{
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No VpuElf section in configuration file\n");
				CloseVcMbox();
				return FALSE;
			}

			//get the size needed to be reserved in gpu-visible memory
			unsigned int space_needed = LoadVcCode(s, 1, -1);

			if (!space_needed)
			{
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "File failed to load from \"%s\" or is zero bytes in length\n", s);
				CloseVcMbox();
				return FALSE;
			}

			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "%d bytes of GPU-visible memory needed for VPU code\n", space_needed);

			SetVpuCodeSize(space_needed);
			EnableVpuOffload(TRUE);
		}

		//find the way in which we're going to get this memory
		s = xf86GetOptValString(fPtr->Options, OPTION_MEM_MODE);

		//could choose /dev/dmaer by default?
		if (!s)
		{
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No memory mode specified\n");
			return FALSE;
		}
		else if (s && !xf86NameCmp(s, "4k"))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
					"Virtually contiguous physically discontiguous memory via /dev/dmaer_4k 4k interface selected\n");
			//no extra options needed bar the size
			if (CreateOffscreenMemory(kDevDmaer))
				return FALSE;

			if (IsEnabledVpuOffload())
			{
				xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "VPU offload is not compatible with physically discontinuous memory, disabling\n");

				EnableVpuOffload(FALSE);
				UnloadVcCode();
				CloseVcMbox();
			}
		}
		else if (s && !xf86NameCmp(s, "mem"))
		{
			//get the address of the hole made in memory
			unsigned long buffer_base;

			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
					"Virtually contiguous physically contiguous memory via /dev/mem and kernel mem=xyzMB interface selected\n");

			if (!xf86GetOptValULong(fPtr->Options, OPTION_BASE_MEM,
					&buffer_base))
			{
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
						"Memory base not specified! (use BlockBase)\n");
				return FALSE;
			}

			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
					"Requesting memory range from %p-%p\n", buffer_base,
					buffer_base + buffer_size - 1);

			//tell it where it is
			SetMemoryBase(buffer_base);

			//and try and open it
			if (CreateOffscreenMemory(kDevMem))
				return FALSE;

		}
		else if (s && !xf86NameCmp(s, "vc"))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
					"Virtually contiguous physically contiguous memory via /dev/dmaer_4k and VideoCore interface selected\n");
			xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
					"Either from static memory allocation or CMA\n");

			//no extra options needed bar the size
			if (CreateOffscreenMemory(kCma))
				return FALSE;
		}
		else
		{
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "unknown memory mode %s\n", s);
			return FALSE;
		}

		//upload the vpu code if appropriate
		if (IsEnabledVpuOffload())
		{
			if (UploadVcCode(GetVpuMemoryBase()))
			{
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "failed to upload VPU code\n");

				UnloadVcCode();
				CloseVcMbox();

				return FALSE;
			}
			else
			{
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VPU code uploaded to bus address %lx\n", (unsigned long)GetVpuMemoryBase() + GetBusOffset());

				//check if it works (fingers crossed)
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NOTE: if it hangs at the next line, ctrl-z, sync and reboot your Raspberry Pi\n");
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Checking that it works...\n");
				int offset;
				if (FindSymbolByName("Identify", &offset))
				{
					xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Could not find Identify function\n");
					UnloadVcCode();
					CloseVcMbox();

					return FALSE;
				}

				unsigned long code_at = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				unsigned int ret = ExecuteVcCode(code_at, 0, 0, 0, 0, 0, 0);

				//if this simply fails then either the binary is invalid or the execute commands doesn't work correctly
				//if it hangs then either the code is invalid or the behaviour of the execute command has changed
				if (ret != 0xc0dec0de)
				{
					xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Could not identify itself correctly (error %08x)\n", ret);
					UnloadVcCode();
					CloseVcMbox();

					return FALSE;
				}

				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Identified correctly\n");

				//get and check the version number
				if (FindSymbolByName("GetVersion", &offset))
				{
					xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Could not find GetVersion function\n");
					UnloadVcCode();
					CloseVcMbox();

					return FALSE;
				}

				code_at = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				ret = ExecuteVcCode(code_at, 0, 0, 0, 0, 0, 0);

				unsigned int major = ret >> 16;
				unsigned int minor = ret & 0xffff;

				if (major > 1 || minor > 2)
				{
					xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Version of VPU binary is too new for this driver, %d.%d\n", major, minor);
					UnloadVcCode();
					CloseVcMbox();

					return FALSE;
				}
				else
					xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using version %d.%d of binary\n", major, minor);
			}
		}

	}
	else
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");

	/******************/

	/* select video modes */

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"checking modes against framebuffer device...\n");
	fbdevHWSetVideoModes(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "checking modes against monitor...\n");
	{
		DisplayModePtr mode, first = mode = pScrn->modes;

		if (mode != NULL )
			do
			{
				mode->status = xf86CheckModeForMonitor(mode, pScrn->monitor);
				mode = mode->next;
			} while (mode != NULL && mode != first);

		xf86PruneDriverModes(pScrn);
	}

	if (NULL == pScrn->modes)
		fbdevHWUseBuildinMode(pScrn);
	pScrn->currentMode = pScrn->modes;

	/* First approximation, may be refined in ScreenInit */
	pScrn->displayWidth = pScrn->virtualX;

	xf86PrintModes(pScrn);

	/* Set display resolution */
	xf86SetDpi(pScrn, 0, 0);

	/* Load bpp-specific modules */
	switch ((type = fbdevHWGetType(pScrn)))
	{
	case FBDEVHW_PACKED_PIXELS:
		switch (pScrn->bitsPerPixel)
		{
		case 8:
		case 16:
		case 24:
		case 32:
			break;
		default:
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
					"unsupported number of bits per pixel: %d",
					pScrn->bitsPerPixel);
			return FALSE;
		}
		break;
	default:
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"unrecognised fbdev hardware type (%d)\n", type);
		return FALSE;
	}

	return TRUE;
}

static Bool FBDevScreenInit(int scrnIndex, ScreenPtr pScreen, int argc,
		char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	FBDevPtr fPtr = FBDEVPTR(pScrn);
	VisualPtr visual;
	int init_picture = 0;
	int ret, flags;
	int type;

	if (NULL == (fPtr->fbmem = fbdevHWMapVidmem(pScrn)))
	{
		xf86DrvMsg(scrnIndex, X_ERROR, "mapping of video memory"
				" failed\n");
		return FALSE;
	}
	fPtr->fboff = fbdevHWLinearOffset(pScrn);

	fbdevHWSave(pScrn);

	if (!fbdevHWModeInit(pScrn, pScrn->currentMode))
	{
		xf86DrvMsg(scrnIndex, X_ERROR, "mode initialization failed\n");
		return FALSE;
	}
	fbdevHWSaveScreen(pScreen, SCREEN_SAVER_ON);
	fbdevHWAdjustFrame(scrnIndex, 0, 0, 0);

	/* mi layer */
	miClearVisualTypes();
	if (pScrn->bitsPerPixel > 8)
	{
		if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits,
				TrueColor))
		{
			xf86DrvMsg(scrnIndex, X_ERROR, "visual type setup failed"
					" for %d bits per pixel [1]\n", pScrn->bitsPerPixel);
			return FALSE;
		}
	}
	else
	{
		if (!miSetVisualTypes(pScrn->depth,
				miGetDefaultVisualMask(pScrn->depth), pScrn->rgbBits,
				pScrn->defaultVisual))
		{
			xf86DrvMsg(scrnIndex, X_ERROR, "visual type setup failed"
					" for %d bits per pixel [2]\n", pScrn->bitsPerPixel);
			return FALSE;
		}
	}
	if (!miSetPixmapDepths())
	{
		xf86DrvMsg(scrnIndex, X_ERROR, "pixmap depth setup failed\n");
		return FALSE;
	}

	/* FIXME: this doesn't work for all cases, e.g. when each scanline
	 has a padding which is independent from the depth (controlfb) */
	pScrn->displayWidth = fbdevHWGetLineLength(pScrn)
			/ (pScrn->bitsPerPixel / 8);

	if (pScrn->displayWidth != pScrn->virtualX)
	{
		xf86DrvMsg(scrnIndex, X_INFO,
				"Pitch updated to %d after ModeInit\n",
				pScrn->displayWidth);
	}

	fPtr->fbstart = fPtr->fbmem + fPtr->fboff;

	switch ((type = fbdevHWGetType(pScrn)))
	{
	case FBDEVHW_PACKED_PIXELS:
		switch (pScrn->bitsPerPixel)
		{
		case 8:
		case 16:
		case 24:
		case 32:
			ret = fbScreenInit(pScreen,
					fPtr->fbstart,
					pScrn->virtualX, pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
					pScrn->displayWidth, pScrn->bitsPerPixel);
			init_picture = 1;
			break;
		default:
			xf86DrvMsg(scrnIndex, X_ERROR,
					"internal error: invalid number of bits per"
							" pixel (%d) encountered in"
							" FBDevScreenInit()\n", pScrn->bitsPerPixel);
			ret = FALSE;
			break;
		}
		break;
	default:
		xf86DrvMsg(scrnIndex, X_ERROR,
				"internal error: unrecognised hardware type (%d) "
						"encountered in FBDevScreenInit()\n", type);
		ret = FALSE;
		break;
	}
	if (!ret)
		return FALSE;

	/* Fixup RGB ordering */
	visual = pScreen->visuals + pScreen->numVisuals;
	while (--visual >= pScreen->visuals)
	{
		if ((visual->class | DynamicClass) == DirectColor)
		{
			visual->offsetRed = pScrn->offset.red;
			visual->offsetGreen = pScrn->offset.green;
			visual->offsetBlue = pScrn->offset.blue;
			visual->redMask = pScrn->mask.red;
			visual->greenMask = pScrn->mask.green;
			visual->blueMask = pScrn->mask.blue;
		}
	}

	/* must be after RGB ordering fixed */
	if (init_picture && !fbPictureInit(pScreen, NULL, 0))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
				"Render extension initialisation failed\n");
		return FALSE;
	}

	xf86SetBlackWhitePixels(pScreen);

	/*********** EXA ************/
	if (g_usingExa)
	{
		ExaDriverPtr pExa;
		if (!(pExa = exaDriverAlloc()))
			return FALSE;

		pExa->flags =
				0 | EXA_OFFSCREEN_PIXMAPS/* | EXA_HANDLES_PIXMAPS | EXA_SUPPORTS_PREPARE_AUX*/;
		pExa->memoryBase = GetMemoryBase();
		pExa->memorySize = GetMemorySize();
		pExa->offScreenBase = 0;
		pExa->pixmapOffsetAlign = 4;
		pExa->pixmapPitchAlign = 4;
		pExa->maxX = 2048;
		pExa->maxY = 2048;

		if (xf86ReturnOptValBool(fPtr->Options, OPTION_FAULT_IN_IMM, FALSE))
		{
			xf86DrvMsg(scrnIndex, X_CONFIG,
					"faulting in all offscreen memory now\n");
			memset(GetMemoryBase(), 0xcd, GetMemorySize());
		}

		//get the max axi burst
		xf86DrvMsg(scrnIndex, X_CONFIG, "max AXI burst suggested: %d\n",
				kern_get_max_burst());
		SetMaxAxiBurst(kern_get_max_burst());		//and program X to use it

//		xf86DrvMsg(scrnIndex, X_INFO, "beginning benchmarks\n");
//		BenchCopy();
//		BenchFill();
//		BenchComposite();
//		xf86DrvMsg(scrnIndex, X_INFO, "benchmarks done\n");

		if (g_nullDriver)
		{
			pExa->WaitMarker = NullWaitMarker;

			pExa->PrepareSolid = NullPrepareSolid;
			pExa->Solid = NullSolid;
			pExa->DoneSolid = NullDoneSolid;

			pExa->PrepareCopy = NullPrepareCopy;
			pExa->Copy = NullCopy;
			pExa->DoneCopy = NullDoneCopy;

			pExa->DownloadFromScreen = NullDownloadFromScreen;
			pExa->UploadToScreen = NullUploadToScreen;

			pExa->CheckComposite = NullCheckComposite;
			pExa->PrepareComposite = NullPrepareComposite;
			pExa->Composite = NullComposite;
			pExa->DoneComposite = NullDoneComposite;
		}
		else
		{
			pExa->WaitMarker = WaitMarker;
			//		pExa->MarkSync = MarkSync;

			//////////////////////////
			if (xf86ReturnOptValBool(fPtr->Options, OPTION_SELF_MANAGED_OFFSCREEN, FALSE))
			{
				xf86DrvMsg(scrnIndex, X_CONFIG, "Experimental self-managed memory\n");

				//turn on the memory allocator, after we stamp over the memory
				InitOffscreenAlloc();

				pExa->flags |= EXA_SUPPORTS_PREPARE_AUX;
				pExa->flags |= EXA_HANDLES_PIXMAPS;
				pExa->flags |= EXA_MIXED_PIXMAPS;

				pExa->PrepareAccess = PrepareAccess;
				pExa->FinishAccess = FinishAccess;

				pExa->CreatePixmap = CreatePixmap;
				pExa->DestroyPixmap = DestroyPixmap;
				pExa->PixmapIsOffscreen = PixmapIsOffscreen;
				//		pExa->ModifyPixmapHeader = ModifyPixmapHeader;
			}
			//////////////////////////

			pExa->PrepareSolid = PrepareSolid;
			pExa->Solid = Solid;
			pExa->DoneSolid = DoneSolid;

			pExa->PrepareCopy = PrepareCopy;
			pExa->Copy = Copy;
			pExa->DoneCopy = DoneCopy;

			pExa->DownloadFromScreen = DownloadFromScreen;
			pExa->UploadToScreen = UploadToScreen;

			pExa->CheckComposite = CheckComposite;
			pExa->PrepareComposite = PrepareComposite;
			pExa->Composite = Composite;
			pExa->DoneComposite = DoneComposite;
		}

		pExa->exa_major = 2;
		pExa->exa_minor = 5;

		if (exaDriverInit(pScreen, pExa) == FALSE)
			return FALSE;

		SetScreen(pScreen);
	}
	/***************************/

	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);

	/* software cursor */
	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

	/* colormap */
	switch ((type = fbdevHWGetType(pScrn)))
	{
	/* XXX It would be simpler to use miCreateDefColormap() in all cases. */
	case FBDEVHW_PACKED_PIXELS:
		if (!miCreateDefColormap(pScreen))
		{
			xf86DrvMsg(scrnIndex, X_ERROR,
					"internal error: miCreateDefColormap failed "
							"in FBDevScreenInit()\n");
			return FALSE;
		}
		break;
	default:
		xf86DrvMsg(scrnIndex, X_ERROR,
				"internal error: unrecognised fbdev hardware type "
						"(%d) encountered in FBDevScreenInit()\n", type);
		return FALSE;
	}
	flags = CMAP_PALETTED_TRUECOLOR;
	if (!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPaletteWeak(), NULL,
			flags))
		return FALSE;

	xf86DPMSInit(pScreen, fbdevHWDPMSSetWeak(), 0);

	pScreen->SaveScreen = fbdevHWSaveScreenWeak();

	/* Wrap the current CloseScreen function */
	fPtr->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = FBDevCloseScreen;

#if XV
	{
		XF86VideoAdaptorPtr *ptr;

		int n = xf86XVListGenericAdaptors(pScrn, &ptr);
		if (n)
		{
			xf86XVScreenInit(pScreen, ptr, n);
		}
	}
#endif

	return TRUE;
}

static Bool FBDevCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	FBDevPtr fPtr = FBDEVPTR(pScrn);

	fbdevHWRestore(pScrn);
	fbdevHWUnmapVidmem(pScrn);

	pScrn->vtSema = FALSE;

	pScreen->CreateScreenResources = fPtr->CreateScreenResources;
	pScreen->CloseScreen = fPtr->CloseScreen;
	Bool ret = (*pScreen->CloseScreen)(scrnIndex, pScreen);

	//turn off the allocator
	DestroyOffscreenAlloc();

	return ret;
}

static Bool FBDevDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr)
{
	xorgHWFlags *flag;

	switch (op)
	{
	case GET_REQUIRED_HW_INTERFACES:
		flag = (CARD32*) ptr;
		(*flag) = 0;
		return TRUE;
	default:
		return FALSE;
	}
}
