#ifndef _EXA_COPIES_INL_
#define _EXA_COPIES_INL_

static inline void CopyLinear(struct DmaControlBlock *pCB,
		void *pDestAddr, void *pSourceAddr, unsigned int length, unsigned int srcInc)
{
	MY_ASSERT(pCB);
	MY_ASSERT(pDestAddr);
	MY_ASSERT(pSourceAddr);
	MY_ASSERT(length > 0 && length <= 0x3fffffff);
	MY_ASSERT(srcInc == 0 || srcInc == 1);

#ifdef DEREFERENCE_TEST
	if (*(volatile unsigned char *)pSourceAddr == *(volatile unsigned char *)pSourceAddr);
	if (*(volatile unsigned char *)pDestAddr == *(volatile unsigned char *)pDestAddr);
	if (*(volatile unsigned char *)pSourceAddr + length - 1 == *(volatile unsigned char *)pSourceAddr + length - 1);
	if (*(volatile unsigned char *)pDestAddr + length - 1 == *(volatile unsigned char *)pDestAddr + length - 1);
#endif

#ifdef STRADDLE_TEST
	if (srcInc)
	{
		unsigned long source_start = (unsigned long)pSourceAddr >> 12;
		unsigned long source_end = (unsigned long)(pSourceAddr + length - 1) >> 12;

		if (source_start != source_end)
		{
			unsigned long source_page_end;
			unsigned long new_length_source;

			xf86DrvMsg(0, X_INFO, "linear source range straddles page boundary %p->%p, %lx->%lx\n",
					pSourceAddr, pSourceAddr + length, source_start, source_end);

			if (source_end - source_start > 1)
				xf86DrvMsg(0, X_INFO, "\tstraddles %ld pages\n", source_end - source_start);
		}
	}

	unsigned long dest_start = (unsigned long)pDestAddr >> 12;
	unsigned long dest_end = (unsigned long)(pDestAddr + length - 1) >> 12;

	if (dest_start != dest_end)
	{
		xf86DrvMsg(0, X_INFO, "linear dest range straddles page boundary %p->%p, %lx->%lx\n",
				pDestAddr, pDestAddr + length, dest_start, dest_end);

		if (dest_end - dest_start > 1)
				xf86DrvMsg(0, X_INFO, "\tstraddles %ld pages\n", dest_end - dest_start);
	}
#endif

	pCB->m_transferInfo = (srcInc << 8);			//do source increment?
	pCB->m_transferInfo |= (1 << 4);				//dest increment
	pCB->m_transferInfo |= (5 << 12);				//axi burst
	/*pCB->m_transferInfo |= (1 << 9);				//source burst
	pCB->m_transferInfo |= (1 << 5);				//dest burst*/

	pCB->m_pSourceAddr = pSourceAddr;
	pCB->m_pDestAddr = pDestAddr;
	pCB->m_xferLen = length;
	pCB->m_tdStride = 0xffffffff;
	pCB->m_pNext = 0;

	pCB->m_blank1 = pCB->m_blank2 = 0;

	AddBytesPending(length);
}



static inline void Copy2D(struct DmaControlBlock *pCB,
		void *pDestAddr, void *pSourceAddr, unsigned int xlength, unsigned int ylength,
		unsigned int srcInc, unsigned int destStride, unsigned int sourceStride)
{
	MY_ASSERT(pCB);
	MY_ASSERT(pDestAddr);
	MY_ASSERT(pSourceAddr);
	MY_ASSERT(xlength > 0 && xlength <= 0xffff);
	MY_ASSERT(ylength > 0 && ylength <= 0x3fff);
	MY_ASSERT(srcInc == 0 || srcInc == 1);
	MY_ASSERT(sourceStride <= 0xffff);
	MY_ASSERT(destStride <= 0xffff);

	MY_ASSERT(0);		//fix transferinfo too

#ifdef DEREFERENCE_TEST
	if (*(volatile unsigned char *)pSourceAddr == *(volatile unsigned char *)pSourceAddr);
	if (*(volatile unsigned char *)pDestAddr == *(volatile unsigned char *)pDestAddr);
#endif

#ifdef STRADDLE_TEST
	if (srcInc)
	{
		unsigned long source_start = (unsigned long)pSourceAddr >> 12;
		unsigned long source_end = (unsigned long)(pSourceAddr + (xlength + sourceStride) * ylength - 1) >> 12;

		if (source_start != source_end)
		{
			xf86DrvMsg(0, X_INFO, "2D source range straddles page boundary %p->%p, %lx->%lx, %dx%d (+%d)\n",
					pSourceAddr, pSourceAddr + (xlength + sourceStride) * ylength,
					source_start, source_end,
					xlength, ylength, sourceStride);

			if (source_end - source_start > 1)
				xf86DrvMsg(0, X_INFO, "\tstraddles %ld pages\n", source_end - source_start);
		}
	}

	unsigned long dest_start = (unsigned long)pDestAddr >> 12;
	unsigned long dest_end = (unsigned long)(pDestAddr + (xlength + destStride) * ylength - 1) >> 12;

	if (dest_start != dest_end)
	{
		xf86DrvMsg(0, X_INFO, "2D dest range straddles page boundary %p->%p, %lx->%lx, %dx%d (+%d)\n",
				pDestAddr, pDestAddr + (xlength + destStride) * ylength,
				dest_start, dest_end,
				xlength, ylength, destStride);

		if (dest_end - dest_start > 1)
				xf86DrvMsg(0, X_INFO, "\tstraddles %ld pages\n", dest_end - dest_start);
	}
#endif

	pCB->m_transferInfo = (srcInc << 8) | (1 << 1);
	pCB->m_pSourceAddr = pSourceAddr;
	pCB->m_pDestAddr = pDestAddr;
	pCB->m_xferLen = (ylength << 16) | xlength;
	pCB->m_tdStride = (destStride << 16) | sourceStride;
	pCB->m_pNext = 0;
}

#endif
