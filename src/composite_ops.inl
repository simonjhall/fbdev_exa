#include "generic_types.h"

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

		return (unsigned char)result;
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

		return (unsigned char)result;
	}
};

/////////////////////////////////////

/////////////////////////////////////
//clamps the input coordinate to zero
class ZeroSourceCoord
{
public:
	inline ZeroSourceCoord() {};
};

//wraps the input coordinate based on the width/height of the image
class WrappedSourceCoord
{
public:
	inline WrappedSourceCoord(int width, int height)
	: m_width (width),
	  m_height (height)
	{
	}

	inline int GetX(int x) const
	{
		return Wrap(x, m_width);
	};

	inline int GetY(int y) const
	{
		return Wrap(y, m_height);
	};

	////////////////
	inline int GetXStep(int x) const
	{
		x++;

		if (x >= m_width)
			x -= m_width;

		return x;
	};

	inline int GetYStep(int y) const
	{
		y++;
		if (y >= m_height)
			y -= m_height;
		return y;
	};

private:
	static int Wrap(int v, int max)
	{
		//this is poor
		v = v % max;
		return v;
	};

	int m_width;
	int m_height;
};

//passthrough
class NormalSourceCoord
{
public:
	inline NormalSourceCoord() {};
};
/////////////////////////////////////
//base class for mux
template <PixelFormat pf>
class Mux
{
public:
	static inline int GetOffset(const Channel c);
	static const int sm_numChannels = 0;
};

template<>
class Mux<kA8>
{
	public:
	static inline int GetOffset(const Channel c)
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
	static inline int GetOffset(const Channel c)
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
	static inline int GetOffset(const Channel c)
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
	static inline int GetOffset(const Channel c)
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
	static inline int GetOffset(const Channel c)
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
	inline T GetValue(int x, int y, Channel c) const
	{
		return static_cast<const Derived *>(this)->GetValue_(x, y, c);
	}

	//set a value in the image at x/y channel
	inline void SetValue(int x, int y, Channel c, T value)
	{
		static_cast<Derived *>(this)->SetValue_(x, y, c, value);
	}

	inline int GetOffX(void) const
	{
		return static_cast<const Derived *>(this)->GetOffX_();
	}
	inline int GetOffY(void) const
	{
		return static_cast<const Derived *>(this)->GetOffY_();
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
	inline T GetValue_(int x, int y, Channel c) const
	{
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
			m_pArray[y * m_stride + x * Mux<pf>::sm_numChannels + offset] = value;
	}

	inline int GetOffX_(void) const { return m_offX; };
	inline int GetOffY_(void) const { return m_offY; };

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
	inline T GetValue_(int, int, Channel c) const
	{
		int offset = Mux<pf>::GetOffset(c);

		return m_values[offset];
	}

	inline int GetOffX_(void) const { return 0; };
	inline int GetOffY_(void) const { return 0; };

private:
	T m_values[Mux<pf>::sm_numChannels];
};
/////////////////////////////////////

template <typename Derived>
class BaseIterator
{
public:
	inline BaseIterator(const int maxLoop)
	: m_maxLoop (maxLoop),
	  m_loop (0)
	{
	};


	inline int GetSource(void) const
	{
		return static_cast<const Derived *>(this)->GetSource_();
	};

	inline int GetMask(void) const
	{
		return static_cast<const Derived *>(this)->GetMask_();
	};

	inline int GetDest(void) const
	{
		return static_cast<const Derived *>(this)->GetDest_();
	};

	inline void Next(void)
	{
		static_cast<Derived *>(this)->Next_();
	};

	inline bool End(void) const
	{
		return m_loop < m_maxLoop;
	};

protected:
	const int m_maxLoop;
	int m_loop;

private:
	inline int GetLoop(void) const
	{
		return m_loop;
	};
};

template <class SourceCoord, Axis axis>
class Iterator : public BaseIterator<Iterator<SourceCoord, axis> >
{
public:
	inline Iterator(const int sourceOff, const int maskOff, const int destOff, const int maxLoop, const SourceCoord &);
	inline int GetSource(void) const;
	inline int GetMask(void) const;
	inline int GetDest(void) const;
	inline void Next(void);
};

template <Axis axis>
class Iterator<ZeroSourceCoord, axis> : public BaseIterator<Iterator<ZeroSourceCoord, axis> >
{
public:
	inline Iterator(const int sourceOff, const int maskOff, const int destOff, const int maxLoop, const ZeroSourceCoord &)
	: BaseIterator<Iterator<ZeroSourceCoord, axis> >(maxLoop),
	  m_sourceOff (sourceOff),
	  m_maskOff (maskOff),
	  m_destOff (destOff)
	{
	};

	inline int GetSource(void) const
	{
		return 0;
	};

	inline int GetMask(void) const
	{
		return this->m_loop + m_maskOff;
	};

	inline int GetDest(void) const
	{
		return this->m_loop + m_destOff;
	};

	inline void Next(void)
	{
		this->m_loop++;
	};

private:
	const int m_sourceOff, m_maskOff, m_destOff;
};

template <Axis axis>
class Iterator<NormalSourceCoord, axis> : public BaseIterator<Iterator<NormalSourceCoord, axis> >
{
public:
	inline Iterator(const int sourceOff, const int maskOff, const int destOff, const int maxLoop, const NormalSourceCoord &)
	: BaseIterator<Iterator<NormalSourceCoord, axis> >(maxLoop),
	  m_sourceOff (sourceOff),
	  m_maskOff (maskOff),
	  m_destOff (destOff)
	{
	};

	inline int GetSource(void) const
	{
		return this->m_loop + m_sourceOff;
	};

	inline int GetMask(void) const
	{
		return this->m_loop + m_maskOff;
	};

	inline int GetDest(void) const
	{
		return this->m_loop + m_destOff;
	};

	inline void Next(void)
	{
		this->m_loop++;
	};

private:
	const int m_sourceOff, m_maskOff, m_destOff;
};

template <Axis axis>
class Iterator<WrappedSourceCoord, axis> : public BaseIterator<Iterator<WrappedSourceCoord, axis> >
{
public:
	inline Iterator(const int sourceOff, const int maskOff, const int destOff, const int maxLoop, const WrappedSourceCoord &coord)
	: BaseIterator<Iterator<WrappedSourceCoord, axis> >(maxLoop),
	  m_sourceOff (sourceOff),
	  m_maskOff (maskOff),
	  m_destOff (destOff),
	  m_coord (coord)
	{
		if (axis == kX)
			m_sourceOff = coord.GetX(m_sourceOff);
		else if (axis == kY)
			m_sourceOff = coord.GetY(m_sourceOff);
//		else	//compile-time assert here

	};

	inline int GetSource(void) const
	{
		return m_sourceOff;
	};

	inline int GetMask(void) const
	{
		return this->m_loop + m_maskOff;
	};

	inline int GetDest(void) const
	{
		return this->m_loop + m_destOff;
	};

	inline void Next(void)
	{
		this->m_loop++;

		if (axis == kX)
			m_sourceOff = m_coord.GetXStep(m_sourceOff);
		else if (axis == kY)
			m_sourceOff = m_coord.GetYStep(m_sourceOff);
	};

private:

	int m_sourceOff;
	const int m_maskOff, m_destOff;
	const WrappedSourceCoord &m_coord;
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
		Iterator<SourceCoord, kY> y(source.GetOffY(), mask.GetOffY(), dest.GetOffY(), height, coord);

		for (/* y */; y.End(); y.Next())
		{
			Iterator<SourceCoord, kX> x(source.GetOffX(), mask.GetOffX(), dest.GetOffX(), width, coord);
			for (/* x */; x.End(); x.Next())
			{
				const int mask_x = x.GetMask();
				const int mask_y = y.GetMask();

				T mask_pixel = mask.GetValue(mask_x, mask_y, kAlpha);

				if (mask_pixel == 0)
					continue;

				const int source_x = x.GetSource();
				const int source_y = y.GetSource();

				T source_r = source.GetValue(source_x, source_y, kRed);
				T source_g = source.GetValue(source_x, source_y, kGreen);
				T source_b = source.GetValue(source_x, source_y, kBlue);
				T source_a = source.GetValue(source_x, source_y, kAlpha);

				const int dest_x = x.GetDest();
				const int dest_y = y.GetDest();

				T dest_r = dest.GetValue(dest_x, dest_y, kRed);
				T dest_g = dest.GetValue(dest_x, dest_y, kGreen);
				T dest_b = dest.GetValue(dest_x, dest_y, kBlue);
				T dest_a = dest.GetValue(dest_x, dest_y, kAlpha);

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

				dest.SetValue(dest_x, dest_y, kRed, final_r);
				dest.SetValue(dest_x, dest_y, kGreen, final_g);
				dest.SetValue(dest_x, dest_y, kBlue, final_b);
				dest.SetValue(dest_x, dest_y, kAlpha, final_a);
			}
		}
	}
};

#if 0
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
//				T debug_source_a = source_a;

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
#endif

class PDAdd
{
public:
	template <
		class T, T max,
		class Source, class Dest, class Mask, class SourceCoord>
	static void Op(const Source &source, Dest &dest, const Mask &mask, const SourceCoord &coord,
			const int width, const int height)
	{
		Iterator<SourceCoord, kY> y(source.GetOffY(), mask.GetOffY(), dest.GetOffY(), height, coord);

		for (/* y */; y.End(); y.Next())
		{
			Iterator<SourceCoord, kX> x(source.GetOffX(), mask.GetOffX(), dest.GetOffX(), width, coord);
			for (/* x */; x.End(); x.Next())
			{
				const int mask_x = x.GetMask();
				const int mask_y = y.GetMask();

				T mask_pixel = mask.GetValue(mask_x, mask_y, kAlpha);

				if (mask_pixel == 0)
					continue;

				const int source_x = x.GetSource();
				const int source_y = y.GetSource();

				T source_r = source.GetValue(source_x, source_y, kRed);
				T source_g = source.GetValue(source_x, source_y, kGreen);
				T source_b = source.GetValue(source_x, source_y, kBlue);
				T source_a = source.GetValue(source_x, source_y, kAlpha);

				const int dest_x = x.GetDest();
				const int dest_y = y.GetDest();

				T dest_r = dest.GetValue(dest_x, dest_y, kRed);
				T dest_g = dest.GetValue(dest_x, dest_y, kGreen);
				T dest_b = dest.GetValue(dest_x, dest_y, kBlue);
				T dest_a = dest.GetValue(dest_x, dest_y, kAlpha);

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

				dest.SetValue(dest_x, dest_y, kRed, final_r);
				dest.SetValue(dest_x, dest_y, kGreen, final_g);
				dest.SetValue(dest_x, dest_y, kBlue, final_b);
				dest.SetValue(dest_x, dest_y, kAlpha, final_a);
			}
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


