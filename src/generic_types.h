#ifndef _GENERIC_TYPES_H_
#define GENERIC_TYPES_H_

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

typedef void(*ptr2PdFunc)(struct CompositeOp *,
		unsigned char *, unsigned char *, unsigned char *,
		int, int, int,
		int, int);

#endif
