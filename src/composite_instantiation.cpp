/*
 * composite_instantiation.cpp
 *
 *  Created on: 9 Sep 2012
 *      Author: Simon
 */


#include "composite_ops.inl"

#define TEST_FUNC(op, source, dest, mask, wrap) if (source_pf == source && dest_pf == dest) return &Op<op, source, dest, mask, wrap>;

//convert RENDER operation type into a function pointer
extern "C" ptr2PdFunc EnumToFunc(const PorterDuffOp op,
		PixelFormat source_pf, PixelFormat dest_pf, PixelFormat mask_pf)
{
	if (op == kPictOpOver)
	{
		if (mask_pf == kNoData)
		{
			TEST_FUNC(PDOver, kA8R8G8B8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDOver, kA8R8G8B8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDOver, kA8R8G8B8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDOver, kA8R8G8B8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDOver, kX8R8G8B8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDOver, kX8R8G8B8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDOver, kX8R8G8B8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDOver, kX8R8G8B8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDOver, kA8B8G8R8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDOver, kA8B8G8R8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDOver, kA8B8G8R8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDOver, kA8B8G8R8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDOver, kX8B8G8R8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDOver, kX8B8G8R8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDOver, kX8B8G8R8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDOver, kX8B8G8R8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDOver, kA8, kA8, kA8, false);
		}
		else if (mask_pf == kA8)
		{
			TEST_FUNC(PDOver, kA8R8G8B8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDOver, kA8R8G8B8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDOver, kA8R8G8B8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDOver, kA8R8G8B8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDOver, kX8R8G8B8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDOver, kX8R8G8B8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDOver, kX8R8G8B8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDOver, kX8R8G8B8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDOver, kA8B8G8R8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDOver, kA8B8G8R8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDOver, kA8B8G8R8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDOver, kA8B8G8R8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDOver, kX8B8G8R8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDOver, kX8B8G8R8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDOver, kX8B8G8R8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDOver, kX8B8G8R8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDOver, kA8, kA8, kA8, true);
		}
		else if (mask_pf == kA8R8G8B8)
		{
			TEST_FUNC(PDOver, kA8R8G8B8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kA8R8G8B8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kA8R8G8B8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kA8R8G8B8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDOver, kX8R8G8B8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kX8R8G8B8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kX8R8G8B8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kX8R8G8B8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDOver, kA8B8G8R8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kA8B8G8R8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kA8B8G8R8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kA8B8G8R8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDOver, kX8B8G8R8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kX8B8G8R8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kX8B8G8R8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDOver, kX8B8G8R8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDOver, kA8, kA8, kA8R8G8B8, true);
		}
	}
#if 0
	else if (op == kPictOpOutReverse)
	{
		if (mask_pf == kNoData)
		{
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDOutReverse, kX8R8G8B8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDOutReverse, kX8R8G8B8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDOutReverse, kX8R8G8B8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDOutReverse, kX8R8G8B8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDOutReverse, kA8B8G8R8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDOutReverse, kA8B8G8R8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDOutReverse, kA8B8G8R8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDOutReverse, kA8B8G8R8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDOutReverse, kX8B8G8R8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDOutReverse, kX8B8G8R8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDOutReverse, kX8B8G8R8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDOutReverse, kX8B8G8R8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDOver, kA8, kA8, kA8, false);
		}
		else if (mask_pf == kA8)
		{
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDOutReverse, kX8R8G8B8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDOutReverse, kX8R8G8B8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDOutReverse, kX8R8G8B8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDOutReverse, kX8R8G8B8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDOutReverse, kA8B8G8R8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDOutReverse, kA8B8G8R8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDOutReverse, kA8B8G8R8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDOutReverse, kA8B8G8R8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDOutReverse, kX8B8G8R8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDOutReverse, kX8B8G8R8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDOutReverse, kX8B8G8R8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDOutReverse, kX8B8G8R8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDOver, kA8, kA8, kA8, true);
		}
		else if (mask_pf == kA8R8G8B8)
		{
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kA8R8G8B8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDOutReverse, kX8R8G8B8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kX8R8G8B8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kX8R8G8B8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kX8R8G8B8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDOutReverse, kA8B8G8R8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kA8B8G8R8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kA8B8G8R8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kA8B8G8R8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDOutReverse, kX8B8G8R8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kX8B8G8R8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kX8B8G8R8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDOutReverse, kX8B8G8R8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDOver, kA8, kA8, kA8R8G8B8, true);
		}
	}
#endif
	else if (op == kPictOpAdd)
	{
		if (mask_pf == kNoData)
		{
			TEST_FUNC(PDAdd, kA8R8G8B8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDAdd, kA8R8G8B8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDAdd, kA8R8G8B8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDAdd, kA8R8G8B8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDAdd, kX8R8G8B8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDAdd, kX8R8G8B8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDAdd, kX8R8G8B8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDAdd, kX8R8G8B8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDAdd, kA8B8G8R8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDAdd, kA8B8G8R8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDAdd, kA8B8G8R8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDAdd, kA8B8G8R8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDAdd, kX8B8G8R8, kA8R8G8B8, kA8, false)
			TEST_FUNC(PDAdd, kX8B8G8R8, kX8R8G8B8, kA8, false)
			TEST_FUNC(PDAdd, kX8B8G8R8, kA8B8G8R8, kA8, false)
			TEST_FUNC(PDAdd, kX8B8G8R8, kX8B8G8R8, kA8, false)

			TEST_FUNC(PDAdd, kA8, kA8, kA8, false)
		}
		else if (mask_pf == kA8)
		{
			TEST_FUNC(PDAdd, kA8R8G8B8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDAdd, kA8R8G8B8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDAdd, kA8R8G8B8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDAdd, kA8R8G8B8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDAdd, kX8R8G8B8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDAdd, kX8R8G8B8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDAdd, kX8R8G8B8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDAdd, kX8R8G8B8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDAdd, kA8B8G8R8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDAdd, kA8B8G8R8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDAdd, kA8B8G8R8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDAdd, kA8B8G8R8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDAdd, kX8B8G8R8, kA8R8G8B8, kA8, true)
			TEST_FUNC(PDAdd, kX8B8G8R8, kX8R8G8B8, kA8, true)
			TEST_FUNC(PDAdd, kX8B8G8R8, kA8B8G8R8, kA8, true)
			TEST_FUNC(PDAdd, kX8B8G8R8, kX8B8G8R8, kA8, true)

			TEST_FUNC(PDAdd, kA8, kA8, kA8, true)
		}
		else if (mask_pf == kA8R8G8B8)
		{
			TEST_FUNC(PDAdd, kA8R8G8B8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kA8R8G8B8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kA8R8G8B8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kA8R8G8B8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDAdd, kX8R8G8B8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kX8R8G8B8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kX8R8G8B8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kX8R8G8B8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDAdd, kA8B8G8R8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kA8B8G8R8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kA8B8G8R8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kA8B8G8R8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDAdd, kX8B8G8R8, kA8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kX8B8G8R8, kX8R8G8B8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kX8B8G8R8, kA8B8G8R8, kA8R8G8B8, true)
			TEST_FUNC(PDAdd, kX8B8G8R8, kX8B8G8R8, kA8R8G8B8, true)

			TEST_FUNC(PDAdd, kA8, kA8, kA8R8G8B8, true)
		}
	}

	return 0;
}
