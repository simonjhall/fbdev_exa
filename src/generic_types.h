#ifndef _GENERIC_TYPES_H_
#define GENERIC_TYPES_H_

#define MY_ASSERT(x) if (!(x)) *(int *)0 = 0;
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

//function pointer for the thing that'll do our work
typedef void(*ptr2PdFunc)(struct CompositeOp *pOp,
		unsigned char *pSource, unsigned char *pDest, unsigned char *pMask,
		int source_stride, int dest_stride, int mask_stride,
		int source_width, int source_height,
		int source_wrap);

#ifdef __cplusplus
extern "C" {
#endif

//translates from operation to function pointer
ptr2PdFunc EnumToFunc(const enum PorterDuffOp op,
		int source_bpp, int dest_bpp,
		int mask_bpp);

#ifdef __cplusplus
}
#endif

#endif
