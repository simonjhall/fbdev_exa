#ifndef _EXA_ACC_H_
#define _EXA_ACC_H_

////////////////////////////////////

#include "generic_types.h"

//basic copies
static void CopyLinear(struct DmaControlBlock *pCB,
		void *pDestAddr, void *pSourceAddr, unsigned int length, unsigned int srcInc);
static void Copy2D(struct DmaControlBlock *pCB,
		void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int srcInc, unsigned int destStride, unsigned int sourceStride);

//proper copies
void ForwardCopy(unsigned char *pDst, unsigned char *pSrc, int bytes);
void ForwardCopyNoSrcInc(unsigned char *pDst, unsigned char *pSrc, int bytes);
void Copy2D4kSrcInc(void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int destStride, unsigned int sourceStride);
void Copy2D4kNoSrcInc(void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int destStride);

struct DmaControlBlock *AllocDmaBlock(void);
unsigned char *AllocSolidBuffer(unsigned int bytes);
void ResetSolidBuffer(void);

struct DmaControlBlock *GetUnkickedDmaHead(void);
void UpdateKickedDmaHead(void);
BOOL IsPendingUnkicked(void);
BOOL IsDmaPending(void);

//setting up the driver
void SetMemoryBase(unsigned long);			//set /dev/mem high address
void *GetMemoryBase(void);					//return user virtual offscreen memory address
void FreeMemoryBase(void);					//unmap the offscreen memory somehow

void SetMemorySize(unsigned long);			//set the amount reserved
unsigned long GetMemorySize(void);			//get the size of the offscreen memory

ScreenPtr GetScreen(void);
void SetScreen(ScreenPtr);

void SetMaxAxiBurst(unsigned int);

////////////////////////////////////
//driver exa functions

int MarkSync(ScreenPtr pScreen);
void WaitMarker(ScreenPtr pScreen, int Marker);

Bool PrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg);
void Solid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2);
void DoneSolid(PixmapPtr p);

Bool PrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int dx,
		int dy, int alu, Pixel planemask);
void Copy(PixmapPtr pDstPixmap, int srcX, int srcY,
		int dstX, int dstY, int width, int height);
void DoneCopy(PixmapPtr p);

Bool CheckComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture);
Bool PrepareComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrc,
		PixmapPtr pMask, PixmapPtr pDst);
void Composite(PixmapPtr pDst, int srcX, int srcY, int maskX,
		int maskY, int dstX, int dstY, int width, int height);
void DoneComposite(PixmapPtr p);

Bool DownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h,
		char *dst, int dst_pitch);
Bool UploadToScreen(PixmapPtr pDst, int x, int y, int w, int h,
		char *src, int src_pitch);

///////////////////
//null versions of driver exa functions
Bool NullPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg);
void NullSolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2);
void NullDoneSolid(PixmapPtr p);

Bool NullPrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int dx,
		int dy, int alu, Pixel planemask);
void NullCopy(PixmapPtr pDstPixmap, int srcX, int srcY,
		int dstX, int dstY, int width, int height);
void NullDoneCopy(PixmapPtr p);

Bool NullDownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h,
		char *dst, int dst_pitch);
Bool NullUploadToScreen(PixmapPtr pDst, int x, int y, int w, int h,
		char *src, int src_pitch);

Bool NullCheckComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture);
Bool NullPrepareComposite(int op, PicturePtr pSrcPicture,
		PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrc,
		PixmapPtr pMask, PixmapPtr pDst);
void NullComposite(PixmapPtr pDst, int srcX, int srcY, int maskX,
		int maskY, int dstX, int dstY, int width, int height);
void NullDoneComposite(PixmapPtr p);

void NullWaitMarker(ScreenPtr pScreen, int Marker);
////////////////
//not working exa stuff...

Bool PrepareAccess(PixmapPtr pPix, int index);
void FinishAccess(PixmapPtr pPix, int index);

void *CreatePixmap2(ScreenPtr pScreen, int width, int height,
                            int depth, int usage_hint, int bitsPerPixel,
                            int *new_fb_pitch);
void DestroyPixmap(ScreenPtr pScreen, void *driverPriv);
Bool PixmapIsOffscreen(PixmapPtr pPix);
Bool ModifyPixmapHeader(PixmapPtr pPixmap, int width, int height,
                                int depth, int bitsPerPixel, int devKind,
                                pointer pPixData);
//end not working
////////////////

struct DmaPixmap
{
	int m_width;
	int m_height;
	int m_depth;
	int m_bpp;
	int m_pitchBytes;
	void *m_pData;
};
//end not working
/////////////////////////////////////

void SetBaseDmaBlock(struct DmaControlBlock *);
struct DmaControlBlock *GetBaseDmaBlock(void);

void ClearBytesPending(void);
unsigned int GetBytesPending(void);
void AddBytesPending(unsigned int);

//master dma control
BOOL StartDma(struct DmaControlBlock *, BOOL force);
BOOL WaitDma(BOOL force);

//internal dma control
int EmulateDma(struct DmaControlBlock *);
void EmulateWaitDma(void);
int RealDma(struct DmaControlBlock *);
void RealWaitDma(unsigned int);

//validation
void ValidateCbList(struct DmaControlBlock *);

//misc
void BenchCopy(void);
void BenchFill(void);

/////////////////////////////////////

//allocation of kernel (un-pagable) memory
void *kern_alloc(size_t);
void *kern_free(void *);

//internal kernel dma control
int kern_init(void);
int kern_dma_prepare(void *);
int kern_dma_kick(void *);
int kern_dma_prepare_kick_wait(void *);
int kern_dma_prepare_kick(void *ptr);
int kern_dma_wait_one(void *);
int kern_dma_wait_all(unsigned int bytesPending);
int kern_get_max_burst(void);
int kern_set_min_max_phys(void *pMin, void *pMax);

//#define DEREFERENCE_TEST
//#define STRADDLE_TEST
#define BREAK_PAGES
//#define CB_VALIDATION

#endif
