#include "generic_types.h"

//channel enumeration
enum Channel
{
	kRed,
	kGreen,
	kBlue,
	kAlpha,
};

//the individual porter-duff operations
template <class T>
class InOp
{
public:
	static inline T Op(const T a, const T b);
};

template <>
class InOp<unsigned char>
{
public:
	static inline unsigned char Op(const unsigned char a, const unsigned char b)
	{
		unsigned short result = (unsigned short)a * (unsigned short)b + 128;
		result = (result + (result >> 8)) >> 8;

		if (result > 255)
			result = 255;

		return (unsigned char)result;
	}
};

template <class T>
class AddOp
{
public:
	static inline T Op(const T a, const T b);
};

template <>
class AddOp<unsigned char>
{
public:
	static inline unsigned char Op(const unsigned char a, const unsigned char b)
	{
		unsigned short result = (unsigned short)a + (unsigned short)b;

		if (result > 255)
			result = 255;

		return (unsigned char)result;
	}
};

template <class T>
class OverOp
{
public:
	static inline T Op(const T a, const T b, const T one_minus_alpha);
};

template <>
class OverOp<unsigned char>
{
public:
	static inline unsigned char Op(const unsigned char a, const unsigned char b, const unsigned char one_minus_alpha)
	{
		unsigned short result = ((unsigned short)b * (unsigned short)one_minus_alpha + 128);
		result = (result + (result >> 8)) >> 8;

		result += a;


		if (result > 255)
			result = 255;

		return result;
	}
};

template <class T>
class OutReverseOp
{
public:
	static inline T Op(const T b, const T one_minus_alpha);
};

template <>
class OutReverseOp<unsigned char>
{
public:
	static inline unsigned char Op(const unsigned char b, const unsigned char one_minus_alpha)
	{
		unsigned short result = (((unsigned short)b * (unsigned short)one_minus_alpha + 127) / 255);

		if (result > 255)
			result = 255;

		return result;
	}
};

/////////////////////////////////////

//crtp base class for a co-ordinate
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
//clamps the input coordinate to zero
class ZeroSourceCoord : public Coord<ZeroSourceCoord>
{
public:
	inline ZeroSourceCoord() {};

	inline int GetX_(int) const { return 0; };
	inline int GetY_(int) const { return 0; };
};

//wraps the input coordinate based on the width/height of the image
class WrappedSourceCoord : public Coord<WrappedSourceCoord>
{
public:
	inline WrappedSourceCoord(int width, int height)
	: m_width (width),
	  m_height (height)
	{
	}

	inline int GetX_(int x) const
	{
		return Wrap(x, m_width);
	};

	inline int GetY_(int y) const
	{
		return Wrap(y, m_height);
	};

private:
	static int Wrap(int v, int max)
	{
		//this is poor
//		while (v >= max)
//			v -= max;
		v = v % max;
		return v;
	};

	int m_width;
	int m_height;
};

//passthrough
class NormalSourceCoord : public Coord<NormalSourceCoord>
{
public:
	inline NormalSourceCoord() {};

	inline int GetX_(int x) const { return x; };
	inline int GetY_(int y) const { return y; };
};
/////////////////////////////////////
//base class for mux
template <PixelFormat pf>
class Mux
{
public:
	static inline const int GetOffset(const Channel c)
	{
		__builtin_trap();
		return 0;
	}
	static const int sm_numChannels = 0;
};

template<>
class Mux<kA8>
{
	public:
	static inline const int GetOffset(const Channel c)
	{
		if (c == kAlpha)
			return 0;
		else
		{
			return 0;
		}
	}
	static const int sm_numChannels = 1;
};

template<>
class Mux<kA8R8G8B8>
{
	public:
	static inline const int GetOffset(const Channel c)
	{
		switch (c)
		{
		case kRed:
			return 2;
		case kGreen:
			return 1;
		case kBlue:
			return 0;
		case kAlpha:
			return 3;
		default:
			__builtin_trap();
			return 0;
		}
	}
	static const int sm_numChannels = 4;
};

template<>
class Mux<kX8R8G8B8>
{
	public:
	static inline const int GetOffset(const Channel c)
	{
		switch (c)
		{
		case kRed:
			return 2;
		case kGreen:
			return 1;
		case kBlue:
			return 0;
		case kAlpha:
			return -1;
		default:
			__builtin_trap();
			return 0;
		}
	}
	static const int sm_numChannels = 4;
};

template<>
class Mux<kA8B8G8R8>
{
	public:
	static inline const int GetOffset(const Channel c)
	{
		switch (c)
		{
		case kRed:
			return 0;
		case kGreen:
			return 1;
		case kBlue:
			return 2;
		case kAlpha:
			return 3;
		default:
			__builtin_trap();
			return 0;
		}
	}
	static const int sm_numChannels = 4;
};

template<>
class Mux<kX8B8G8R8>
{
	public:
	static inline const int GetOffset(const Channel c)
	{
		switch (c)
		{
		case kRed:
			return 0;
		case kGreen:
			return 1;
		case kBlue:
			return 2;
		case kAlpha:
			return -1;
		default:
			__builtin_trap();
			return 0;
		}
	}
	static const int sm_numChannels = 4;
};


/////////////////////////////////////
//crtp base class for pixel data
//returns a linker error if function is not implemented
template <class T, typename Derived>
class Value
{
public:
	//get a value from the image at x/y channel
	template <class U = NormalSourceCoord>
	inline T GetValue(int x, int y, Channel c, const U &coord = NormalSourceCoord()) const
	{
		return static_cast<const Derived *>(this)->GetValue_(x, y, c, coord);
	}

	//set a value in the image at x/y channel
	inline void SetValue(int x, int y, Channel c, T value)
	{
		static_cast<Derived *>(this)->SetValue_(x, y, c, value);
	}
};
/////////////////////////////////////

//looks up pixel data from an array in memory
template <class T, PixelFormat pf, T alpha>
class VaryingValue : public Value<T, VaryingValue<T, pf, alpha> >
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
	inline T GetValue_(int x, int y, Channel c, const U &coord = NormalSourceCoord()) const
	{
		//update the coordinate
		x = coord.GetX(m_offX + x);
		y = coord.GetY(m_offY + y);

		int offset = Mux<pf>::GetOffset(c);

		//return the data or a fixed constant
		if (offset == -1)
			return alpha;
		else
			return m_pArray[y * m_stride + x * Mux<pf>::sm_numChannels + offset];
	}

	inline void SetValue_(int x, int y, Channel c, T value)
	{
		int offset = Mux<pf>::GetOffset(c);

		if (offset == -1)
		{
			return;
		}
		else
			m_pArray[(m_offY + y) * m_stride + (m_offX + x) * Mux<pf>::sm_numChannels + offset] = value;
	}

private:
	T * const __restrict m_pArray;
	const int m_offX;
	const int m_offY;
	const int m_stride;
};

//loads pixel data from a constant array
template <class T, PixelFormat pf>
class FixedValue : public Value<T, FixedValue<T, pf> >
{
public:
	inline FixedValue(const T *a)
	{
		for (int count = 0; count < Mux<pf>::sm_numChannels; count++)
			m_values[count] = a[count];
	}

	template <class U = NormalSourceCoord>
	inline T GetValue_(int, int, Channel c, const U &coord = NormalSourceCoord()) const
	{
		int offset = Mux<pf>::GetOffset(c);

		return m_values[offset];
	}

private:
	T m_values[Mux<pf>::sm_numChannels];
};
/////////////////////////////////////

class PDOver
{
public:
	template <
		class T, T max,
		class Source, class Dest, class Mask, class SourceCoord>
	static void Op(const Source &source, Dest &dest, const Mask &mask, const SourceCoord &coord,
			const int width, const int height)
	{
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
			{
				T mask_pixel = mask.GetValue(x, y, kAlpha);

				if (mask_pixel == 0)
					continue;

				T source_r = source.GetValue(x, y, kRed, coord);
				T source_g = source.GetValue(x, y, kGreen, coord);
				T source_b = source.GetValue(x, y, kBlue, coord);
				T source_a = source.GetValue(x, y, kAlpha, coord);

				T debug_source_r = source_r;
				T debug_source_g = source_g;
				T debug_source_b = source_b;
				T debug_source_a = source_a;

				T dest_r = dest.GetValue(x, y, kRed);
				T dest_g = dest.GetValue(x, y, kGreen);
				T dest_b = dest.GetValue(x, y, kBlue);
				T dest_a = dest.GetValue(x, y, kAlpha);

				if (mask_pixel != max)
				{
					source_r = InOp<T>::Op(source_r, mask_pixel);
					source_g = InOp<T>::Op(source_g, mask_pixel);
					source_b = InOp<T>::Op(source_b, mask_pixel);
					source_a = InOp<T>::Op(source_a, mask_pixel);
				}

				T final_r = OverOp<T>::Op(source_r, dest_r, max - source_a);
				T final_g = OverOp<T>::Op(source_g, dest_g, max - source_a);
				T final_b = OverOp<T>::Op(source_b, dest_b, max - source_a);
				T final_a = OverOp<T>::Op(source_a, dest_a, max - source_a);

				dest.SetValue(x, y, kRed, final_r);
				dest.SetValue(x, y, kGreen, final_g);
				dest.SetValue(x, y, kBlue, final_b);
				dest.SetValue(x, y, kAlpha, final_a);
			}
	}
};

class PDOutReverse
{
public:
	template <
		class T, T max,
		class Source, class Dest, class Mask, class SourceCoord>
	static void Op(const Source &source, Dest &dest, const Mask &mask, const SourceCoord &coord,
			const int width, const int height)
	{
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
			{
				T mask_pixel = mask.GetValue(x, y, kAlpha);

//				if (mask_pixel == 0)
//					continue;

				T source_a = source.GetValue(x, y, kAlpha, coord);
				T debug_source_a = source_a;

				T dest_r = dest.GetValue(x, y, kRed);
				T dest_g = dest.GetValue(x, y, kGreen);
				T dest_b = dest.GetValue(x, y, kBlue);
				T dest_a = dest.GetValue(x, y, kAlpha);

//				if (mask_pixel != max)
				{
					source_a = InOp<T>::Op(source_a, mask_pixel);
				}

				T final_r = OutReverseOp<T>::Op(dest_r, max - source_a);
				T final_g = OutReverseOp<T>::Op(dest_g, max - source_a);
				T final_b = OutReverseOp<T>::Op(dest_b, max - source_a);
				T final_a = OutReverseOp<T>::Op(dest_a, max - source_a);

				dest.SetValue(x, y, kRed, final_r);
				dest.SetValue(x, y, kGreen, final_g);
				dest.SetValue(x, y, kBlue, final_b);
				dest.SetValue(x, y, kAlpha, final_a);
			}
	}
};

class PDAdd
{
public:
	template <
		class T, T max,
		class Source, class Dest, class Mask, class SourceCoord>
	static void Op(const Source &source, Dest &dest, const Mask &mask, const SourceCoord &coord,
			const int width, const int height)
	{
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
			{
				T mask_pixel = mask.GetValue(x, y, kAlpha);

				if (mask_pixel == 0)
					continue;

				T source_r = source.GetValue(x, y, kRed, coord);
				T source_g = source.GetValue(x, y, kGreen, coord);
				T source_b = source.GetValue(x, y, kBlue, coord);
				T source_a = source.GetValue(x, y, kAlpha, coord);

				T debug_source_r = source_r;
				T debug_source_g = source_g;
				T debug_source_b = source_b;
				T debug_source_a = source_a;

				T dest_r = dest.GetValue(x, y, kRed);
				T dest_g = dest.GetValue(x, y, kGreen);
				T dest_b = dest.GetValue(x, y, kBlue);
				T dest_a = dest.GetValue(x, y, kAlpha);

				if (mask_pixel != max)
				{
					source_r = InOp<T>::Op(source_r, mask_pixel);
					source_g = InOp<T>::Op(source_g, mask_pixel);
					source_b = InOp<T>::Op(source_b, mask_pixel);
					source_a = InOp<T>::Op(source_a, mask_pixel);
				}

				T final_r = AddOp<T>::Op(dest_r, source_r);
				T final_g = AddOp<T>::Op(dest_g, source_g);
				T final_b = AddOp<T>::Op(dest_b, source_b);
				T final_a = AddOp<T>::Op(dest_a, source_a);

				dest.SetValue(x, y, kRed, final_r);
				dest.SetValue(x, y, kGreen, final_g);
				dest.SetValue(x, y, kBlue, final_b);
				dest.SetValue(x, y, kAlpha, final_a);
			}
	}
};

//generic operation class, for 4x8bit channel images
template <class Operation,
	PixelFormat SourcePf,
	PixelFormat DestPf,
	PixelFormat MaskPf,
	bool ValidMask>
inline void Op(CompositeOp *pOp, int numOps,
		unsigned char *pSource, unsigned char *pDest, unsigned char *pMask,
		int source_stride, int dest_stride, int mask_stride,
		int source_width, int source_height,
		int source_wrap)
{
	for (int count = 0; count < numOps; count++)
	{
		VaryingValue<unsigned char, SourcePf, 255> source(pSource, pOp[count].srcX, pOp[count].srcY, source_stride);
		VaryingValue<unsigned char, DestPf, 255> dest(pDest, pOp[count].dstX, pOp[count].dstY, dest_stride);

		if (ValidMask)
		{
			VaryingValue<unsigned char, MaskPf, 255> mask(pMask, pOp[count].maskX, pOp[count].maskY, mask_stride);

			if (source_width == 1 && source_height == 1)
			{
				const ZeroSourceCoord zero;
				Operation::template Op<unsigned char, 255>
					(source, dest, mask, zero, pOp[count].width, pOp[count].height);
			}
			else if (source_wrap)
			{
				const WrappedSourceCoord wrap(source_width, source_height);
				Operation::template Op<unsigned char, 255>
					(source, dest, mask, wrap, pOp[count].width, pOp[count].height);
			}
			else
			{
				const NormalSourceCoord normal;
				Operation::template Op<unsigned char, 255>
					(source, dest, mask, normal, pOp[count].width, pOp[count].height);
			}

		}
		else
		{
			const unsigned char m = 255;
			FixedValue<unsigned char, MaskPf> mask(&m);		//todo add compile-time assert here == 8 bit

			if (source_width == 1 && source_height == 1)
			{
				const ZeroSourceCoord zero;
				Operation::template Op<unsigned char, 255>
					(source, dest, mask, zero, pOp[count].width, pOp[count].height);
			}
			else if (source_wrap)
			{
				const WrappedSourceCoord wrap(source_width, source_height);
				Operation::template Op<unsigned char, 255>
					(source, dest, mask, wrap, pOp[count].width, pOp[count].height);
			}
			else
			{
				const NormalSourceCoord normal;
				Operation::template Op<unsigned char, 255>
					(source, dest, mask, normal, pOp[count].width, pOp[count].height);
			}
		}
	}
}


