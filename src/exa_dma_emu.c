/*
 * Author: Simon Hall
 * Emulates execution of DMA CB chains
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa_acc.h"

int EmulateDma(struct DmaControlBlock *pCB)
{
	int dmas = 0;

	while (pCB)
	{
#ifdef DEREFERENCE_TEST
		MY_ASSERT(pCB->m_transferInfo == pCB->m_transferInfo);
#endif
//		xf86DrvMsg(0, X_INFO, "dma block at %p\n", pCB);
		//pick out the interesting fields from TI
		unsigned int src_inc = (pCB->m_transferInfo >> 8) & 0x1;
		unsigned int td_mode = (pCB->m_transferInfo >> 1) & 0x1;

		//source/dest base pointers we'll increment
		unsigned char *pSource = (unsigned char *)pCB->m_pSourceAddr;
		unsigned char *pDest = (unsigned char *)pCB->m_pDestAddr;

#ifdef DEREFERENCE_TEST
		MY_ASSERT(*(volatile unsigned char *)pSource == *(volatile unsigned char *)pSource);
		MY_ASSERT(*(volatile unsigned char *)pDest == *(volatile unsigned char *)pDest);
#endif

		if (td_mode)
		{
			//x, y dims
			unsigned int ylength = (pCB->m_xferLen & 0x3fffffff) >> 16;
			unsigned int xlength = pCB->m_xferLen & 0xffff;

			//stride to add at the end of each copied row
			unsigned int dest_stride = pCB->m_tdStride >> 16;
			unsigned int source_stride = pCB->m_tdStride & 0xffff;
			int source_offset = 0;

//			xf86DrvMsg(0, X_INFO, "\t%p->%p (%dx%d, +%d +%d (%d))\n",
//					pSource, pDest, xlength, ylength,
//					source_stride, dest_stride, src_inc);

			//do the copy
			int x, y;
			for (y = 0; y < ylength; y++)
			{
				pSource -= source_offset;
				source_offset = 0;

				//x loop
				for (x = 0; x < xlength; x++)
				{
					*pDest = *pSource;
					pDest++;
					pSource++;

					if (!src_inc)
					{
						source_offset++;
						if (source_offset == 4)
						{
							source_offset = 0;
							pSource -= 4;
						}
					}
				}

				//next row
				pSource += source_stride;
				pDest += dest_stride;
			}
		}
		else
		{
			//basic copy
			unsigned int length = pCB->m_xferLen & 0x3fffffff;
			int source_offset = 0;

//			xf86DrvMsg(0, X_INFO, "\t%p->%p (%d bytes (%d))\n",
//					pSource, pDest, length, src_inc);

			if (!src_inc)
			{
				int count;
				for (count = 0; count < length; count++)
				{
					*pDest = *pSource;
					pDest++;
					pSource++;
					source_offset++;

					if (source_offset == 4)
					{
						source_offset = 0;
						pSource -= 4;
					}
				}
			}
			else
			{
				memcpy(pDest, pSource, length);
			}
		}

		//count how many dmas we've done
		dmas++;

		//move to the next dma
		pCB = pCB->m_pNext;
	}

	return dmas;
}
