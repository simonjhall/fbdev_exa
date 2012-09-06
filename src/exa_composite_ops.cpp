#include "generic_types.h"

template <class T, class U, U scale>
class InOp
{
public:
	static inline T Op(const T a, const T b)
	{
		return ((U)a * (U)b) / scale;
	}
};

template <class T, class U, U scale>
class OverOp
{
public:
	static inline unsigned int Op(const T a, const T b, const T one_minus_alpha)
	{
		return a + (((U)b * (U)one_minus_alpha) / scale);
	}
};

/////////////////////////////////////

template <typename Derived>
class Coord
{
public:
	inline int GetX(int x) const
	{
		return static_cast<const Derived *>(this)->GetX_(x);
	};

	inline int GetY(int y) const
	{
		return static_cast<const Derived *>(this)->GetY_(y);
	}
};

/////////////////////////////////////
class ZeroSourceCoord : public Coord<ZeroSourceCoord>
{
public:
	inline ZeroSourceCoord() {};

	inline int GetX_(int) const { return 0; };
	inline int GetY_(int) const { return 0; };
};

class ClampedSourceCoord : public Coord<ClampedSourceCoord>
{
public:
	inline ClampedSourceCoord(int width, int height)
	: m_width (width),
	  m_height (height)
	{
	}

	inline int GetX_(int x) const
	{
		return Clamp(x, m_width);
	};

	inline int GetY_(int y) const
	{
		return Clamp(y, m_height);
	};

private:
	static int Clamp(int v, int max)
	{
		while (v >= max)
			v -= max;
		return v;
	};

	int m_width;
	int m_height;
};

class NormalSourceCoord : public Coord<NormalSourceCoord>
{
public:
	inline NormalSourceCoord() {};

	inline int GetX_(int x) const { return x; };
	inline int GetY_(int y) const { return y; };
};
/////////////////////////////////////
template <class T, typename Derived>
class Value
{
public:
	template <class U = NormalSourceCoord>
	inline T GetValue(int x, int y, int offset, const U &coord = NormalSourceCoord()) const
	{
		return static_cast<const Derived *>(this)->GetValue_(x, y, offset, coord);
	}

	inline void SetValue(int x, int y, int offset, T value)
	{
		static_cast<Derived *>(this)->SetValue_(x, y, offset, value);
	}
};
/////////////////////////////////////

template <class T, int channels, int bpp, T alpha, int fixed_channel = -1>
class VaryingValue : public Value<T, VaryingValue<T, channels, bpp, alpha> >
{
public:
	inline VaryingValue(T * const __restrict p, int offX, int offY, int stride)
	: m_pArray (p),
	  m_offX (offX),
	  m_offY (offY),
	  m_stride (stride)
	{
	}

	template <class U = NormalSourceCoord>
	inline T GetValue_(int x, int y, int offset, const U &coord = NormalSourceCoord()) const
	{
		x = coord.GetX(m_offX + x);
		y = coord.GetY(m_offY + y);

		if (fixed_channel != -1)
			offset = fixed_channel;

		if (offset * 8 >= bpp)
			return alpha;
		else
			return m_pArray[y * m_stride + x * channels + offset];
	}

	inline void SetValue_(int x, int y, int offset, T value)
	{
		if (offset * 8 >= bpp)
		{
			return;
		}
		else
			m_pArray[(m_offY + y) * m_stride + (m_offX + x) * channels + offset] = value;
	}

private:
	T * const __restrict m_pArray;
	const int m_offX;
	const int m_offY;
	const int m_stride;
};


template <class T, int channels>
class FixedValue : public Value<T, FixedValue<T, channels> >
{
public:
	inline FixedValue(const T *a)
	{
		for (int count = 0; count < channels; count++)
			m_values[count] = a[count];
	}

	template <class U = NormalSourceCoord>
	inline T GetValue_(int, int, int offset, const U &coord = NormalSourceCoord()) const
	{
		return m_values[offset];
	}

private:
	T m_values[channels];
};
/////////////////////////////////////

class PDOver
{
public:
	template <
		class T, class U, U scale, T max,
		const int mask_channel,
		class Source, class Dest, class Mask, class SourceCoord>
	static void Op(const Source &source, Dest &dest, const Mask &mask, const SourceCoord &coord,
			const int width, const int height)
	{
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
			{
				T mask_pixel = mask.GetValue(x, y, mask_channel);

				if (mask_pixel == 0)
					continue;

				T source_r = source.GetValue(x, y, 0, coord);
				T source_g = source.GetValue(x, y, 1, coord);
				T source_b = source.GetValue(x, y, 2, coord);
				T source_a = source.GetValue(x, y, 3, coord);

				T dest_r = dest.GetValue(x, y, 0);
				T dest_g = dest.GetValue(x, y, 1);
				T dest_b = dest.GetValue(x, y, 2);
				T dest_a = dest.GetValue(x, y, 3);

				if (mask_pixel != max)
				{
					source_r = InOp<T, U, scale>::Op(source_r, mask_pixel);
					source_g = InOp<T, U, scale>::Op(source_g, mask_pixel);
					source_b = InOp<T, U, scale>::Op(source_b, mask_pixel);
					source_a = InOp<T, U, scale>::Op(source_a, mask_pixel);
				}

				T final_r = OverOp<T, U, scale>::Op(source_r, dest_r, max - source_a);
				T final_g = OverOp<T, U, scale>::Op(source_g, dest_g, max - source_a);
				T final_b = OverOp<T, U, scale>::Op(source_b, dest_b, max - source_a);
				T final_a = OverOp<T, U, scale>::Op(source_a, dest_a, max - source_a);

				dest.SetValue(x, y, 0, final_r);
				dest.SetValue(x, y, 1, final_g);
				dest.SetValue(x, y, 2, final_b);
				dest.SetValue(x, y, 3, final_a);
			}
	}
};

template <class Operation,
	int SourceBpp,
	int DestBpp,
	int MaskBpp,
	bool ValidMask>
inline void Op(CompositeOp *pOp,
		unsigned char *pSource, unsigned char *pDest, unsigned char *pMask,
		int source_stride, int dest_stride, int mask_stride,
		int source_width, int source_height,
		int source_wrap)
{
	VaryingValue<unsigned char, 4, SourceBpp, 255> source(pSource, pOp->srcX, pOp->srcY, source_stride);
	VaryingValue<unsigned char, 4, DestBpp, 255> dest(pDest, pOp->dstX, pOp->dstY, dest_stride);

	if (ValidMask)
	{
		VaryingValue<unsigned char, MaskBpp / 8, MaskBpp, 255> mask(pMask, pOp->maskX, pOp->maskY, mask_stride);

		if (source_width == 1 && source_height == 1)
		{
			const ZeroSourceCoord zero;
			Operation::template Op<unsigned char, unsigned short, 256, 255, MaskBpp == 8 ? 0 : 3>
				(source, dest, mask, zero, pOp->width, pOp->height);
		}
		else if (source_wrap)
		{
			const ClampedSourceCoord clamp(source_width, source_height);
			Operation::template Op<unsigned char, unsigned short, 256, 255, MaskBpp == 8 ? 0 : 3>
				(source, dest, mask, clamp, pOp->width, pOp->height);
		}
		else
		{
			const NormalSourceCoord normal;
			Operation::template Op<unsigned char, unsigned short, 256, 255, MaskBpp == 8 ? 0 : 3>
				(source, dest, mask, normal, pOp->width, pOp->height);
		}

	}
	else
	{
		const unsigned char m = 255;
		FixedValue<unsigned char, MaskBpp / 8> mask(&m);		//todo add assert here == 8 bit

		if (source_width == 1 && source_height == 1)
		{
			const ZeroSourceCoord zero;
			Operation::template Op<unsigned char, unsigned short, 256, 255, 0>
				(source, dest, mask, zero, pOp->width, pOp->height);
		}
		else if (source_wrap)
		{
			const ClampedSourceCoord clamp(source_width, source_height);
			Operation::template Op<unsigned char, unsigned short, 256, 255, 0>
				(source, dest, mask, clamp, pOp->width, pOp->height);
		}
		else
		{
			const NormalSourceCoord normal;
			Operation::template Op<unsigned char, unsigned short, 256, 255, 0>
				(source, dest, mask, normal, pOp->width, pOp->height);
		}
	}
}


extern "C" ptr2PdFunc EnumToFunc(const PorterDuffOp op,
		int source_bpp, int dest_bpp, int mask_bpp)
{
	if (mask_bpp == 0)
	{
		if (source_bpp == 24 && dest_bpp == 24)
			return &Op<PDOver, 24, 24, 8, false>;
		else if (source_bpp == 24 && dest_bpp == 32)
			return &Op<PDOver, 24, 32, 8, false>;
		else if (source_bpp == 32 && dest_bpp == 24)
			return &Op<PDOver, 32, 24, 8, false>;
		else if (source_bpp == 32 && dest_bpp == 32)
			return &Op<PDOver, 32, 32, 8, false>;
	}
	else if (mask_bpp == 8)
	{
		if (source_bpp == 24 && dest_bpp == 24)
			return &Op<PDOver, 24, 24, 8, true>;
		else if (source_bpp == 24 && dest_bpp == 32)
			return &Op<PDOver, 24, 32, 8, true>;
		else if (source_bpp == 32 && dest_bpp == 24)
			return &Op<PDOver, 32, 24, 8, true>;
		else if (source_bpp == 32 && dest_bpp == 32)
			return &Op<PDOver, 32, 32, 8, true>;
	}
	else if (mask_bpp == 32)
	{
		if (source_bpp == 24 && dest_bpp == 24)
			return &Op<PDOver, 24, 24, 32, true>;
		else if (source_bpp == 24 && dest_bpp == 32)
			return &Op<PDOver, 24, 32, 32, true>;
		else if (source_bpp == 32 && dest_bpp == 24)
			return &Op<PDOver, 32, 24, 32, true>;
		else if (source_bpp == 32 && dest_bpp == 32)
			return &Op<PDOver, 32, 32, 32, true>;
	}

	return 0;
}
