#ifndef _EXA_ACC_H_
#define _EXA_ACC_H_

////////////////////////////////////

struct DmaControlBlock
{
	unsigned int m_transferInfo;
	void *m_pSourceAddr;
	void *m_pDestAddr;
	unsigned int m_xferLen;
	unsigned int m_tdStride;
	struct DmaControlBlock *m_pNext;
	unsigned int m_blank1, m_blank2;
};

void CopyLinear(struct DmaControlBlock *pCB,
		void *pDestAddr, void *pSourceAddr, unsigned int length, unsigned int srcInc);
void Copy2D(struct DmaControlBlock *pCB,
		void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int srcInc, unsigned int destStride, unsigned int sourceStride);

struct DmaControlBlock *GetDmaBlock(void);
unsigned char *GetSolidBuffer(unsigned int bytes);

////////////////////////////////////
void *GetMemoryBase(void);
unsigned long GetMemorySize(void);

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

void NullWaitMarker(ScreenPtr pScreen, int Marker);
////////////////


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

struct DmaPixmap
{
	int m_width;
	int m_height;
	int m_depth;
	int m_bpp;
	int m_pitchBytes;
	void *m_pData;
};
/////////////////////////////////////

int RunDma(struct DmaControlBlock *);
BOOL StartDma(struct DmaControlBlock *, BOOL force);
BOOL WaitDma(BOOL force);

int EmulateDma(struct DmaControlBlock *);
void EmulateWaitDma(void);
int RealDma(struct DmaControlBlock *);
void RealWaitDma(unsigned int);

#endif
