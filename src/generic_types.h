#ifndef _GENERIC_TYPES_H_
#define _GENERIC_TYPES_H_

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

#endif
