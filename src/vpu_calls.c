/*
 * vpu_calls.c
 *
 *  Created on: 3 Dec 2012
 *      Author: Simon Hall
 * VPU equivalent of composite_instantiation.cpp
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "exa_acc.h"

//#define USE_CPU_FALLBACK

static ptr2PdFunc EnumToFuncVpuWrap_internal(const enum PorterDuffOp op,
		enum PixelFormat source_pf, enum PixelFormat dest_pf, enum PixelFormat mask_pf)
{
//	unsigned long code_at = (unsigned long)GetVpuMemoryBase() + GetBusOffset();
//
//	MY_ASSERT((code_at >> 32) == 0);		//this should be a 32-bit number
//
//	unsigned int ret = ExecuteVcCode(code_at,
//			(unsigned int)op, (unsigned int)source_pf, (unsigned int)dest_pf, (unsigned int)mask_pf,
//			0, 0);
//
//	MY_ASSERT(ret != 1);
//
//	return (ptr2PdFunc)ret;

	MY_ASSERT(0);
	return 0;
}

ptr2PdFunc EnumToFuncVpuWrap(const enum PorterDuffOp op,
		enum PixelFormat source_pf, enum PixelFormat dest_pf, enum PixelFormat mask_pf)
{
#ifdef USE_CPU_FALLBACK
	return EnumToFuncVpu(op, source_pf, dest_pf, mask_pf);
#else

	static ptr2PdFunc vpu_over_a8r8g8b8_x8b8g8r8_invalid_vpu = 0;
	static ptr2PdFunc vpu_over_a8r8g8b8_x8b8g8r8_a8_valid_vpu = 0;

	static ptr2PdFunc vpu_over_a8r8g8b8_a8r8g8b8_invalid_vpu = 0;
	static ptr2PdFunc vpu_over_a8r8g8b8_a8r8g8b8_a8_valid_vpu = 0;

	static ptr2PdFunc vpu_over_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid_vpu = 0;
	static ptr2PdFunc vpu_over_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid_vpu = 0;

	static ptr2PdFunc vpu_add_a8r8g8b8_x8b8g8r8_invalid_vpu = 0;
	static ptr2PdFunc vpu_add_a8r8g8b8_x8b8g8r8_a8_valid_vpu = 0;

	static ptr2PdFunc vpu_add_a8r8g8b8_a8r8g8b8_invalid_vpu = 0;
	static ptr2PdFunc vpu_add_a8r8g8b8_a8r8g8b8_a8_valid_vpu = 0;

	static ptr2PdFunc vpu_add_a8r8g8b8_a8_invalid_vpu = 0;
	static ptr2PdFunc vpu_add_a8r8g8b8_a8_a8_valid_vpu = 0;

	static ptr2PdFunc vpu_add_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid_vpu = 0;
	static ptr2PdFunc vpu_add_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid_vpu = 0;

	if (op == kPictOpOver)
	{
		if (mask_pf == kNoData)
		{
			if (source_pf == kA8R8G8B8 && dest_pf == kX8B8G8R8)
			{
				if (!vpu_over_a8r8g8b8_x8b8g8r8_invalid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_over_a8r8g8b8_x8b8g8r8_invalid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_over_a8r8g8b8_x8b8g8r8_invalid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_over_a8r8g8b8_x8b8g8r8_invalid_vpu;
			}
			if (source_pf == kA8R8G8B8 && dest_pf == kA8R8G8B8)
			{
				if (!vpu_over_a8r8g8b8_a8r8g8b8_invalid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_over_a8r8g8b8_a8r8g8b8_invalid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_over_a8r8g8b8_a8r8g8b8_invalid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_over_a8r8g8b8_a8r8g8b8_invalid_vpu;
			}
		}
		else if (mask_pf == kA8)
		{
			if (source_pf == kA8R8G8B8 && dest_pf == kX8B8G8R8)
			{
				if (!vpu_over_a8r8g8b8_x8b8g8r8_a8_valid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_over_a8r8g8b8_x8b8g8r8_a8_valid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_over_a8r8g8b8_x8b8g8r8_a8_valid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_over_a8r8g8b8_x8b8g8r8_a8_valid_vpu;
			}
			if (source_pf == kA8R8G8B8 && dest_pf == kA8R8G8B8)
			{
				if (!vpu_over_a8r8g8b8_a8r8g8b8_a8_valid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_over_a8r8g8b8_a8r8g8b8_a8_valid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_over_a8r8g8b8_a8r8g8b8_a8_valid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_over_a8r8g8b8_a8r8g8b8_a8_valid_vpu;
			}
		}
		else if (mask_pf == kA8B8G8R8)
		{
			if (source_pf == kX8R8G8B8 && dest_pf == kX8R8G8B8)
			{
				if (!vpu_over_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_over_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_over_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_over_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid_vpu;
			}
		}
		else if (mask_pf == kA8R8G8B8)
		{
			if (source_pf == kX8B8G8R8 && dest_pf == kX8B8G8R8)
			{
				if (!vpu_over_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_over_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_over_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_over_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid_vpu;
			}
		}
	}
	else if (op == kPictOpAdd)
	{
		if (mask_pf == kNoData)
		{
			if (source_pf == kA8R8G8B8 && dest_pf == kX8B8G8R8)
			{
				if (!vpu_add_a8r8g8b8_x8b8g8r8_invalid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_add_a8r8g8b8_x8b8g8r8_invalid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_add_a8r8g8b8_x8b8g8r8_invalid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_add_a8r8g8b8_x8b8g8r8_invalid_vpu;
			}
			if (source_pf == kA8R8G8B8 && dest_pf == kA8R8G8B8)
			{
				if (!vpu_add_a8r8g8b8_a8r8g8b8_invalid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_add_a8r8g8b8_a8r8g8b8_invalid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_add_a8r8g8b8_a8r8g8b8_invalid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_add_a8r8g8b8_a8r8g8b8_invalid_vpu;
			}
			if (source_pf == kA8R8G8B8 && dest_pf == kA8)
			{
				if (!vpu_add_a8r8g8b8_a8_invalid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_add_a8r8g8b8_a8_invalid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_add_a8r8g8b8_a8_invalid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_add_a8r8g8b8_a8_invalid_vpu;
			}
		}
		else if (mask_pf == kA8)
		{
			if (source_pf == kA8R8G8B8 && dest_pf == kX8B8G8R8)
			{
				if (!vpu_add_a8r8g8b8_x8b8g8r8_a8_valid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_add_a8r8g8b8_x8b8g8r8_a8_valid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_add_a8r8g8b8_x8b8g8r8_a8_valid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_add_a8r8g8b8_x8b8g8r8_a8_valid_vpu;
			}
			if (source_pf == kA8R8G8B8 && dest_pf == kA8R8G8B8)
			{
				if (!vpu_add_a8r8g8b8_a8r8g8b8_a8_valid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_add_a8r8g8b8_a8r8g8b8_a8_valid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_add_a8r8g8b8_a8r8g8b8_a8_valid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_add_a8r8g8b8_a8r8g8b8_a8_valid_vpu;
			}
			if (source_pf == kA8R8G8B8 && dest_pf == kA8)
			{
				if (!vpu_add_a8r8g8b8_a8_a8_valid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_add_a8r8g8b8_a8_a8_valid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_add_a8r8g8b8_a8_a8_valid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_add_a8r8g8b8_a8_a8_valid_vpu;
			}
		}
		else if (mask_pf == kA8B8G8R8)
		{
			if (source_pf == kX8R8G8B8 && dest_pf == kX8R8G8B8)
			{
				if (!vpu_add_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_add_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_add_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_add_x8r8g8b8_x8r8g8b8_a8b8g8r8_valid_vpu;
			}
		}
		else if (mask_pf == kA8R8G8B8)
		{
			if (source_pf == kX8B8G8R8 && dest_pf == kX8B8G8R8)
			{
				if (!vpu_add_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid_vpu)
				{
					unsigned int offset;
					if (FindSymbolByName("vpu_add_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid", &offset))
					{
						MY_ASSERT(0);
						return 0;
					}
					vpu_add_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid_vpu = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;
				}

				return vpu_add_x8b8g8r8_x8b8g8r8_a8r8g8b8_valid_vpu;
			}
		}
	}

	return 0;
#endif
}

void VpuCompositeWrap(struct PackedCompositeOp * const pOps, const int numOps)
{
	unsigned int *stack = (unsigned int *)((unsigned long)GetVpuMemoryBase() + GetVpuCodeSize() - 4);
	unsigned int subs = 0;

	MY_ASSERT((((unsigned long)pOps >> 28) & 0xc) == 0x4);

//	fprintf(stderr, "BEFORE\n%d packed ops\n", numOps);

	for (int outer = 0; outer < numOps; outer++)
	{
		struct PackedCompositeOp * const pOuter = &pOps[outer];

		MY_ASSERT((((unsigned long)pOuter->m_pOp >> 28) & 0xc) == 0x4);

//		fprintf(stderr, "\tcompositor is %p\n", pOuter->m_pCompositor);
//		fprintf(stderr, "\tops are %p\n", pOuter->m_pOp);
//		fprintf(stderr, "\tnum ops %d\n", pOuter->m_numOps);

		if ((((unsigned long)pOuter->m_pDest >> 28) & 0xc) != 0x4)
		{
//			fprintf(stderr, "\t\tinaccessible dest %p\n", pOuter->m_pDest);
			*stack = *(unsigned int *)pOuter->m_pDest;
			pOuter->m_pDest = (unsigned char *)stack;

#ifndef USE_CPU_FALLBACK
			pOuter->m_pDest = (unsigned char *)((unsigned long)pOuter->m_pDest + GetBusOffset());
#endif
			stack--;
			subs++;
		}

		if ((((unsigned long)pOuter->m_pSource >> 28) & 0xc) != 0x4)
		{
//			fprintf(stderr, "\t\tinaccessible source %p\n", pOuter->m_pSource);
			*stack = *(unsigned int *)pOuter->m_pSource;
			pOuter->m_pSource = (unsigned char *)stack;

#ifndef USE_CPU_FALLBACK
			pOuter->m_pSource = (unsigned char *)((unsigned long)pOuter->m_pSource + GetBusOffset());
#endif
			stack--;
			subs++;
		}

		if (pOuter->m_pMask && (((unsigned long)pOuter->m_pMask >> 28) & 0xc) != 0x4)
		{
//			fprintf(stderr, "\t\tinaccessible mask %p\n", pOuter->m_pMask);
			*stack = *(unsigned int *)pOuter->m_pMask;
			pOuter->m_pMask = (unsigned char *)stack;

#ifndef USE_CPU_FALLBACK
			pOuter->m_pMask = (unsigned char *)((unsigned long)pOuter->m_pMask + GetBusOffset());
#endif
			stack--;
			subs++;
		}

		MY_ASSERT(subs <= 1024);											//we've only one page allocated
#ifndef USE_CPU_FALLBACK
		MY_ASSERT((((unsigned long)pOuter->m_pCompositor >> 28) & 0xc) == 0x4);		//and it's a gpu func
#endif

		MY_ASSERT((pOuter->m_sourceWrap == 1 && pOuter->m_sourceWidth == 1 && pOuter->m_sourceHeight == 1) || pOuter->m_sourceWrap == 0);
//		fprintf(stderr, "\tsource %p, source %08x\n", pOuter->m_pSource, *(unsigned int *)pOuter->m_pSource);
//		fprintf(stderr, "\tdest %p\n", pOuter->m_pDest);
//		fprintf(stderr, "\tmask %p\n", pOuter->m_pMask);
//		fprintf(stderr, "\tsource stride %d\n", pOuter->m_sourceStride);
//		fprintf(stderr, "\tdest stride %d\n", pOuter->m_destStride);
//		fprintf(stderr, "\tmask stride %d\n", pOuter->m_maskStride);
//		fprintf(stderr, "\tsource width %d\n", pOuter->m_sourceWidth);
//		fprintf(stderr, "\tsource height %d\n", pOuter->m_sourceHeight);
//		fprintf(stderr, "\tsource wrap %d\n", pOuter->m_sourceWrap);
	}


#ifdef USE_CPU_FALLBACK
	VpuComposite(pOps, numOps);
#else
	//get the address of the function
	static unsigned long code_at = 0;
	if (!code_at)
	{
		unsigned int offset;
		if (FindSymbolByName("VpuComposite", &offset))
			MY_ASSERT(0);

		code_at = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + offset;

		MY_ASSERT((code_at >> 32) == 0);		//this should be a 32-bit number
	}

//	unsigned long code_at = (unsigned long)GetVpuMemoryBase() + GetBusOffset() + 0x60;

	unsigned int ret = ExecuteVcCode(code_at,
			(unsigned long)pOps + GetBusOffset(), numOps,
			0, 0, 0, 0);

//	fprintf(stderr, "AFTER\n");

//	for (int outer = 0; outer < numOps; outer++)
//	{
//		struct PackedCompositeOp * const pOuter = &pOps[outer];
//
//		fprintf(stderr, "\tcompositor is %p\n", pOuter->m_pCompositor);
//		fprintf(stderr, "\tops are %p\n", pOuter->m_pOp);
//		fprintf(stderr, "\tnum ops %d\n", pOuter->m_numOps);
//		fprintf(stderr, "\tsource %p\n", pOuter->m_pSource);
//		fprintf(stderr, "\tdest %p\n", pOuter->m_pDest);
//		fprintf(stderr, "\tmask %p\n", pOuter->m_pMask);
//		fprintf(stderr, "\tsource stride %d\n", pOuter->m_sourceStride);
//		fprintf(stderr, "\tdest stride %d\n", pOuter->m_destStride);
//		fprintf(stderr, "\tmask stride %d\n", pOuter->m_maskStride);
//		fprintf(stderr, "\tsource width %d\n", pOuter->m_sourceWidth);
//		fprintf(stderr, "\tsource height %d\n", pOuter->m_sourceHeight);
//		fprintf(stderr, "\tsource wrap %d\n", pOuter->m_sourceWrap);
//	}

//	fprintf(stderr, "ret is %08x\n", ret);

	MY_ASSERT(ret != 1);
#endif
}
