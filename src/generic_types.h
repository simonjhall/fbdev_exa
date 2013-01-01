#ifndef _GENERIC_TYPES_H_
#define GENERIC_TYPES_H_

#ifdef __llvm__
#define MY_ASSERT(x) if (!(x)) __builtin_trap();
#else
#define MY_ASSERT(x) if (!(x)) asm volatile ("bkpt\n");
#endif
//#define MY_ASSERT(x) ;

//must match the structure expected by the hardware
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

struct CompositeOp
{
	int srcX;
	int srcY;

	int maskX;
	int maskY;

	int dstX;
	int dstY;

	int width;
	int height;
};

//list of things we might want to do
enum PorterDuffOp
{
	kPictOpMinimum = 0,
	kPictOpClear = 0,
	kPictOpSrc = 1,
	kPictOpDst = 2,
	kPictOpOver = 3,
	kPictOpOverReverse = 4,
	kPictOpIn = 5,
	kPictOpInReverse = 6,
	kPictOpOut = 7,
	kPictOpOutReverse = 8,
	kPictOpAtop = 9,
	kPictOpAtopReverse = 10,
	kPictOpXor = 11,
	kPictOpAdd = 12,
	kPictOpSaturate = 13,
	kPictOpMaximum = 13,
};

enum PixelFormat
{
	kNoData,
	kA8 = 0x8018000,		//134316032
	kA8R8G8B8 = 0x20028888,			//argb, 537036936
	kX8R8G8B8 = 0x20020888,			//xrgb, 537004168
	kA8B8G8R8 = 0x20038888,			//abgr, 537102472
	kX8B8G8R8 = 0x20030888,			//xbgr, 537069704
};

//channel enumeration
enum Channel
{
	kRed,
	kGreen,
	kBlue,
	kAlpha,
};

//coordinate axis
enum Axis
{
	kX,
	kY,
};

//function pointer for the thing that'll do our work
typedef void(*ptr2PdFunc)(struct CompositeOp *pOp, const unsigned int numOps,
		unsigned char *pSource, unsigned char *pDest, unsigned char *pMask,
		const unsigned int source_stride, const unsigned int dest_stride, const unsigned int mask_stride,
		const unsigned int source_width, const unsigned int source_height,
		const unsigned int source_wrap);

#ifdef __cplusplus
extern "C" {
#endif

//translates from operation to function pointer
ptr2PdFunc EnumToFunc(const enum PorterDuffOp op,
		enum PixelFormat source_pf, enum PixelFormat dest_pf, enum PixelFormat mask_pf);


#ifdef __cplusplus
}
#endif

struct PackedCompositeOp
{
	//vpu function to call
	ptr2PdFunc m_pCompositor;

	//list of operations to do
	struct CompositeOp *m_pOp;
	unsigned int m_numOps;

	//shared data between each operation
	unsigned char *m_pSource;
	unsigned char *m_pDest;
	unsigned char *m_pMask;

	unsigned int m_sourceStride;
	unsigned int m_destStride;
	unsigned int m_maskStride;

	unsigned int m_sourceWidth;
	unsigned int m_sourceHeight;
	unsigned int m_sourceWrap;
};

#endif
