#include "generic_types.h"

//#define DEBUG

/////////////////////////////////////
//crtp base class for mux
template <typename Derived>
class MuxBase
{
public:
	static inline int GetOffset(const Channel c)
	{
		return Derived::GetOffset_(c);
	}

	static inline void Combine(unsigned int &out,
			const unsigned char &red, const unsigned char &green, const unsigned char &blue, const unsigned char &alpha)
	{
		const int off_r = GetOffset(kRed);
		const int off_g = GetOffset(kGreen);
		const int off_b = GetOffset(kBlue);
		const int off_a = GetOffset(kAlpha);

		out = 0;
		if (off_r != -1)
			out |= (red << (off_r * 8));
		if (off_g != -1)
			out |= (green << (off_g * 8));
		if (off_b != -1)
			out |= (blue << (off_b * 8));
		if (off_a != -1)
			out |= (alpha << (off_a * 8));
		else
			out |= (255 << (Derived::sm_alphaOffsetAt * 8));
	}

	static inline void Separate(const unsigned int &in,
			unsigned char &red, unsigned char &green, unsigned char &blue, unsigned char &alpha)
	{
		const int off_r = GetOffset(kRed);
		const int off_g = GetOffset(kGreen);
		const int off_b = GetOffset(kBlue);
		const int off_a = GetOffset(kAlpha);

		if (off_r != -1)
			red = (in >> (off_r * 8)) & 0xff;
		if (off_g != -1)
			green = (in >> (off_g * 8)) & 0xff;
		if (off_b != -1)
			blue = (in >> (off_b * 8)) & 0xff;
		if (off_a != -1)
			alpha = (in >> (off_a * 8)) & 0xff;
		else
			alpha = 255;
	}


	static const int sm_numChannels = 0;
	static const int sm_alphaOffsetAt = 0;
};

template <PixelFormat pf>
class Mux : public MuxBase<Mux<pf> >
{
public:
	static inline int GetOffset_(const Channel c);

	static const int sm_numChannels = 0;
	static const int sm_alphaOffsetAt = 0;
};

template<>
class Mux<kA8> : public MuxBase<Mux<kA8> >
{
	public:
	static inline int GetOffset_(const Channel c)
	{
		if (c == kAlpha)
			return 0;
		else
		{
			return -1;
		}
	}

	static const int sm_numChannels = 1;
	static const int sm_alphaOffsetAt = 0;
	static const PixelFormat sm_formatPlusAlpha = kA8;
};

template<>
class Mux<kA8R8G8B8> : public MuxBase<Mux<kA8R8G8B8> >
{
	public:
	static inline int GetOffset_(const Channel c)
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
	static const int sm_alphaOffsetAt = 3;
	static const PixelFormat sm_formatPlusAlpha = kA8R8G8B8;
};

template<>
class Mux<kX8R8G8B8> : public MuxBase<Mux<kX8R8G8B8> >
{
	public:
	static inline int GetOffset_(const Channel c)
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
	static const int sm_alphaOffsetAt = 3;
	static const PixelFormat sm_formatPlusAlpha = kA8R8G8B8;
};

template<>
class Mux<kA8B8G8R8> : public MuxBase<Mux<kA8B8G8R8> >
{
	public:
	static inline int GetOffset_(const Channel c)
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
	static const int sm_alphaOffsetAt = 3;
	static const PixelFormat sm_formatPlusAlpha = kA8B8G8R8;
};

template<>
class Mux<kX8B8G8R8> : public MuxBase<Mux<kX8B8G8R8> >
{
	public:
	static inline int GetOffset_(const Channel c)
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
	static const int sm_alphaOffsetAt = 3;
	static const PixelFormat sm_formatPlusAlpha = kA8B8G8R8;
};

//convert from one pf to another
template <PixelFormat SourcePf, PixelFormat DestPf>
static inline unsigned int Rotate(const unsigned int &in)
{
	if (SourcePf != DestPf)
	{
		unsigned int out;
		unsigned char ar, ag, ab, aa;

		Mux<SourcePf>::Separate(in, ar, ag, ab, aa);
		Mux<DestPf>::Combine(out, ar, ag, ab, aa);

		return out;
	}
	else
		return in;
}

//alpha mix-up
template <>
inline unsigned int Rotate<kA8R8G8B8, kX8R8G8B8>(const unsigned int &in)
{
	return in | (255 << 24);
}

template <>
inline unsigned int Rotate<kX8R8G8B8, kA8R8G8B8>(const unsigned int &in)
{
	return in | (255 << 24);
}

template <>
inline unsigned int Rotate<kA8B8G8R8, kX8B8G8R8>(const unsigned int &in)
{
	return in | (255 << 24);
}

template <>
inline unsigned int Rotate<kX8B8G8R8, kA8B8G8R8>(const unsigned int &in)
{
	return in | (255 << 24);
}

//channel order swap
template <>
inline unsigned int Rotate<kA8R8G8B8, kA8B8G8R8>(const unsigned int &in)
{
	unsigned int a_g = in & 0xff00ff00;
	unsigned int b = in & 0xff;
	unsigned int r = in & 0x00ff0000;

	return a_g | (b << 16) | (r >> 16);
}

template <>
inline unsigned int Rotate<kA8B8G8R8, kA8R8G8B8>(const unsigned int &in)
{
	unsigned int a_g = in & 0xff00ff00;
	unsigned int r = in & 0xff;
	unsigned int b = in & 0x00ff0000;

	return a_g | (b >> 16) | (r << 16);
}

template <>
inline unsigned int Rotate<kX8R8G8B8, kX8B8G8R8>(const unsigned int &in)
{
	unsigned int a_g = in & 0xff00ff00;
	unsigned int b = in & 0xff;
	unsigned int r = in & 0x00ff0000;

	return a_g | (b << 16) | (r >> 16);
}

template <>
inline unsigned int Rotate<kX8B8G8R8, kX8R8G8B8>(const unsigned int &in)
{
	unsigned int a_g = in & 0xff00ff00;
	unsigned int r = in & 0xff;
	unsigned int b = in & 0x00ff0000;

	return a_g | (b >> 16) | (r << 16);
}

//channel swap with alpha
template <>
inline unsigned int Rotate<kA8R8G8B8, kX8B8G8R8>(const unsigned int &in)
{
	unsigned int a_g = (in & 0x0000ff00) | (255 << 24);
	unsigned int b = in & 0xff;
	unsigned int r = in & 0x00ff0000;

	return a_g | (r >> 16) | (b << 16);
}
template <>
inline unsigned int Rotate<kX8B8G8R8, kA8R8G8B8>(const unsigned int &in)
{
	unsigned int a_g = (in & 0x0000ff00) | (255 << 24);
	unsigned int r = in & 0xff;
	unsigned int b = in & 0x00ff0000;

	return a_g | (b >> 16) | (r << 16);
}
/////////////////////////////////////

#ifdef __arm__
inline unsigned int __uxtab16(unsigned int a, unsigned int b, unsigned int ror)
{
	MY_ASSERT(ror == 8);
	return __builtin_arm_qsub(a, b);
}

inline unsigned int __uxtb16(unsigned int b, const unsigned int ror)
{
	MY_ASSERT(ror == 8);
	return __builtin_arm_usat(b, 8);
}

#else

inline unsigned int __uxtab16(unsigned int a, unsigned int b, unsigned int ror)
{
	unsigned int ror_b = b;
	switch (ror)
	{
	case 8:
		ror_b = (b >> 8) | ((b & 0xff) << 24);
		break;
	case 16:
		ror_b = (b >> 16) | ((b & 0xffff) << 16);
		break;
	case 24:
		ror_b = (b >> 24) | ((b & 0xffffff) << 8);
		break;
	default:
		MY_ASSERT(0);
	}

	unsigned short a_low = a & 0xffff;
	unsigned short a_high = (a >> 16) & 0xffff;
	unsigned short b_low = ror_b & 0xff;
	unsigned short b_high = (ror_b >> 16) & 0xff;

	a_low += b_low;
	a_high += b_high;

	return a_low | (a_high << 16);
}

inline unsigned int __uxtb16(unsigned int b, unsigned int ror)
{
	unsigned int ror_b = b;
	switch (ror)
	{
	case 8:
		ror_b = (b >> 8) | ((b & 0xff) << 24);
		break;
	case 16:
		ror_b = (b >> 16) | ((b & 0xffff) << 16);
		break;
	case 24:
		ror_b = (b >> 24) | ((b & 0xffffff) << 8);
		break;
	default:
		MY_ASSERT(0);
	}

	unsigned short b_low = ror_b & 0xff;
	unsigned short b_high = (ror_b >> 16) & 0xff;

	return b_low | (b_high << 16);
}

#endif

//the individual porter-duff operations
template <class T, PixelFormat SourcePf, PixelFormat MaskPf, PixelFormat DestPf>
class InOp
{
public:
	static inline T Op(const T a, const T b);
	static inline unsigned int Op4(const unsigned int a, const unsigned int b);
};

template <PixelFormat SourcePf, PixelFormat MaskPf, PixelFormat DestPf>
class InOp<unsigned char, SourcePf, MaskPf, DestPf>
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

	static inline unsigned int Op4(const unsigned int a, const unsigned int b)
	{
#if 0
		unsigned char ar, ag, ab, aa;
		unsigned char br, bg, bb, ba;

		Mux<SourcePf>::Separate(a, ar, ag, ab, aa);
		Mux<MaskPf>::Separate(b, br, bg, bb, ba);

		unsigned char rr, rg, rb, ra;
		rr = Op(ar, ba);
		rg = Op(ag, ba);
		rb = Op(ab, ba);
		ra = Op(aa, ba);

		unsigned int output;
		Mux<DestPf>::Combine(output, rr, rg, rb, ra);
		return output;

#else
		unsigned int rebuilt_a;
		rebuilt_a = Rotate<SourcePf, DestPf>(a);

		unsigned char ba;
		if (Mux<MaskPf>::GetOffset(kAlpha) == -1)
			ba = 255;
		else
			ba = (b >> (Mux<MaskPf>::GetOffset(kAlpha) * 8)) & 0xff;

		unsigned int a_low = rebuilt_a & 0x00ff00ff;
		unsigned int a_high = (rebuilt_a >> 8) & 0x00ff00ff;

		//source * alpha + 128
		//mla please
		a_low = a_low * (unsigned int)ba + 0x00800080;
		a_high = a_high * (unsigned int)ba + 0x00800080;

		//source = (source + (source >> 8)) >> 8
		a_low = __uxtab16(a_low, a_low, 8);
		a_high = __uxtab16(a_high, a_high, 8);
		a_low = __uxtb16(a_low, 8);
		a_high = __uxtb16(a_high, 8);

		return a_low | (a_high << 8);
#endif
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

template <class T, PixelFormat SourcePf, PixelFormat MaskPf, PixelFormat DestPf>
class OverOp
{
public:
	static inline T Op(const T a, const T b, const T one_minus_alpha);
};

template <PixelFormat SourcePf, PixelFormat MaskPf, PixelFormat DestPf>
class OverOp<unsigned char, SourcePf, MaskPf, DestPf>
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

	static inline unsigned int Op4(const unsigned int a, const unsigned int b, const unsigned int one_minus_alpha)
	{
		unsigned char ar, ag, ab, aa;
		unsigned char br, bg, bb, ba;

		Mux<SourcePf>::Separate(a, ar, ag, ab, aa);
		Mux<DestPf>::Separate(b, br, bg, bb, ba);

		unsigned char rr, rg, rb, ra;
		rr = Op(ar, br, one_minus_alpha & 0xff);
		rg = Op(ag, bg, one_minus_alpha & 0xff);
		rb = Op(ab, bb, one_minus_alpha & 0xff);
		ra = Op(aa, ba, one_minus_alpha & 0xff);

		unsigned int output;
		Mux<DestPf>::Combine(output, rr, rg, rb, ra);
		return output;
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
//crtp base class for pixel data
//returns a linker error if function is not implemented
template <class T, typename Derived>
class Value
{
public:
	//get a value from the image at x/y channel
	inline T GetValue(int x, int y, Channel c) const
	{
		return static_cast<const Derived *>(this)->GetValue_(x, y, c);
	}

	inline unsigned int GetValue4(int x, int y) const
	{
		return static_cast<const Derived *>(this)->GetValue4_(x, y);
	}

	//preload a pixel + offset
	inline void Prefetch(int x, int y, const int offset) const
	{
		static_cast<const Derived *>(this)->Prefetch_(x, y, offset);
	}

	//set a value in the image at x/y channel
	inline void SetValue(int x, int y, Channel c, T value)
	{
		static_cast<Derived *>(this)->SetValue_(x, y, c, value);
	}

	inline void SetValue4(int x, int y, unsigned int value)
	{
		static_cast<Derived *>(this)->SetValue4_(x, y, value);
	}

	//return the offset into the buffer
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
	: Value<T, VaryingValue<T, pf, alpha> >(),
	  m_pArray (p),
	  m_offX (offX),
	  m_offY (offY),
	  m_stride (stride)
	{
	}

	inline void Prefetch_(int x, int y, const int offset) const
	{
		__builtin_prefetch(&m_pArray[y * m_stride + x * Mux<pf>::sm_numChannels + offset]);
	}

	inline T GetValue_(int x, int y, Channel c) const
	{
		int offset = Mux<pf>::GetOffset(c);

		//return the data or a fixed constant
		if (offset == -1)
			return alpha;
		else
			return m_pArray[y * m_stride + x * Mux<pf>::sm_numChannels + offset];
	}

	inline unsigned int GetValue4_(int x, int y) const
	{
		if (Mux<pf>::sm_numChannels == 1)
		{
			return m_pArray[y * m_stride + x];
		}
		else
		{
			unsigned int value = ((unsigned int *)m_pArray)[y * m_stride / Mux<pf>::sm_numChannels + x];

			if (Mux<pf>::GetOffset(kAlpha) == -1)
				value = value | (255 << (Mux<pf>::sm_alphaOffsetAt * 8));

			return value;
		}
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

	inline void SetValue4_(int x, int y, unsigned int value)
	{
		if (Mux<pf>::sm_numChannels == 1)
		{
			m_pArray[y * m_stride + x] = (unsigned char)value;
		}
		else
		{
			if (Mux<pf>::GetOffset(kAlpha) == -1)
				value = value | (255 << (Mux<pf>::sm_alphaOffsetAt * 8));

			((unsigned int *)m_pArray)[y * m_stride / Mux<pf>::sm_numChannels + x] = value;
		}
	}

	inline int GetOffX_(void) const { return m_offX; };
	inline int GetOffY_(void) const { return m_offY; };

	static const PixelFormat sm_format = pf;

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

	inline T GetValue_(int, int, Channel c) const
	{
		int offset = Mux<pf>::GetOffset(c);
		MY_ASSERT(offset >= 0);

		return m_values[offset];
	}

	inline unsigned int GetValue4_(int, int) const
	{
		MY_ASSERT(Mux<pf>::sm_numChannels == 1);
		return (unsigned int)m_values[0] << (Mux<pf>::sm_alphaOffsetAt * 8);
	}

	inline void Prefetch_(int, int, int) const
	{
		//do nothing for fixed value
	}

	inline int GetOffX_(void) const { return 0; };
	inline int GetOffY_(void) const { return 0; };

	static const PixelFormat sm_format = pf;

private:
	T m_values[Mux<pf>::sm_numChannels];
};
/////////////////////////////////////

//crtp iterator base class
template <typename Derived>
class BaseIterator
{
public:
	inline BaseIterator(const int maxLoop)
	: m_maxLoop (maxLoop),
	  m_loop (0)
	{
	};

	//get the source coordinate
	inline int GetSource(void) const
	{
		return static_cast<const Derived *>(this)->GetSource_();
	};

	//get the mask coordinate
	inline int GetMask(void) const
	{
		return static_cast<const Derived *>(this)->GetMask_();
	};

	//get the destination coordinate
	inline int GetDest(void) const
	{
		return static_cast<const Derived *>(this)->GetDest_();
	};

	//move to the next element
	inline void Next(void)
	{
		static_cast<Derived *>(this)->Next_();
	};

	//are we NOT at the end?
	inline bool End(void) const
	{
		return m_loop < m_maxLoop;
	};

	inline bool DoSourcePrefetch(void) const
	{
		return static_cast<const Derived *>(this)->DoSourcePrefetch_();
	};

protected:
	const int m_maxLoop;
	int m_loop;

	//not necessary
private:
	inline int GetLoop(void) const
	{
		return m_loop;
	};
};

//generic version (unused)
template <class SourceCoord, Axis axis>
class Iterator : public BaseIterator<Iterator<SourceCoord, axis> >
{
public:
	inline Iterator(const int sourceOff, const int maskOff, const int destOff, const int maxLoop, const SourceCoord &);
	inline int GetSource_(void) const;
	inline int GetMask_(void) const;
	inline int GetDest_(void) const;
	inline void Next_(void);
	inline bool DoSourcePrefetch_(void) const;
};

//specialised for source = (0, 0)
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

	inline int GetSource_(void) const
	{
		return 0;
	};

	inline int GetMask_(void) const
	{
		return this->m_loop + m_maskOff;
	};

	inline int GetDest_(void) const
	{
		return this->m_loop + m_destOff;
	};

	inline void Next_(void)
	{
		this->m_loop++;
	};

	inline bool DoSourcePrefetch_(void) const
	{
		return false;
	};

private:
	const int m_sourceOff, m_maskOff, m_destOff;
};

//specialised for passthrough for source
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

	inline int GetSource_(void) const
	{
		return this->m_loop + m_sourceOff;
	};

	inline int GetMask_(void) const
	{
		return this->m_loop + m_maskOff;
	};

	inline int GetDest_(void) const
	{
		return this->m_loop + m_destOff;
	};

	inline void Next_(void)
	{
		this->m_loop++;
	};

	inline bool DoSourcePrefetch_(void) const
	{
		return true;
	};

private:
	const int m_sourceOff, m_maskOff, m_destOff;
};

//specialised for wrapping on source
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
		//first go through, use an expensive wrap function
		if (axis == kX)
			m_sourceOff = coord.GetX(m_sourceOff);
		else if (axis == kY)
			m_sourceOff = coord.GetY(m_sourceOff);
//		else	//compile-time assert here

	};

	inline int GetSource_(void) const
	{
		return m_sourceOff;
	};

	inline int GetMask_(void) const
	{
		return this->m_loop + m_maskOff;
	};

	inline int GetDest_(void) const
	{
		return this->m_loop + m_destOff;
	};

	inline void Next_(void)
	{
		this->m_loop++;

		//however on subsequent goes it's a simple test and subtract
		if (axis == kX)
			m_sourceOff = m_coord.GetXStep(m_sourceOff);
		else if (axis == kY)
			m_sourceOff = m_coord.GetYStep(m_sourceOff);
	};

	inline bool DoSourcePrefetch_(void) const
	{
		return false;
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
	template <class Source, class Dest, class Mask, class SourceCoord>
	static void Op(const Source &source, Dest &dest, const Mask &mask, const SourceCoord &coord,
			const int width, const int height)
	{
		Iterator<SourceCoord, kY> y(source.GetOffY(), mask.GetOffY(), dest.GetOffY(), height, coord);

		for (/* y */; y.End(); y.Next())
		{
			Iterator<SourceCoord, kX> x(source.GetOffX(), mask.GetOffX(), dest.GetOffX(), width, coord);
			for (/* x */; x.End(); x.Next())
			{
				//get mask coordinates
				const int mask_x = x.GetMask();
				const int mask_y = y.GetMask();

				unsigned char mask_pixel = mask.GetValue(mask_x, mask_y, kAlpha);
				unsigned int mask4 = mask.GetValue4(mask_x, mask_y);

				mask.Prefetch(mask_x, mask_y, 64);

				if (mask_pixel == 0)
					continue;

				//get source coordinates
				const int source_x = x.GetSource();
				const int source_y = y.GetSource();

#ifdef DEBUG
				unsigned char source_r = source.GetValue(source_x, source_y, kRed);
				unsigned char source_g = source.GetValue(source_x, source_y, kGreen);
				unsigned char source_b = source.GetValue(source_x, source_y, kBlue);
				unsigned char source_a = source.GetValue(source_x, source_y, kAlpha);

				unsigned char debug_source_r = source_r;
				unsigned char debug_source_g = source_g;
				unsigned char debug_source_b = source_b;
				unsigned char debug_source_a = source_a;
#endif

				unsigned int source4 = source.GetValue4(source_x, source_y);
#ifdef DEBUG
				unsigned int debug_source4 = source4;
#endif

				if (x.DoSourcePrefetch())
					source.Prefetch(source_x, source_y, 64);

				//get dest coordinates
				const int dest_x = x.GetDest();
				const int dest_y = y.GetDest();

#ifdef DEBUG
				unsigned char dest_r = dest.GetValue(dest_x, dest_y, kRed);
				unsigned char dest_g = dest.GetValue(dest_x, dest_y, kGreen);
				unsigned char dest_b = dest.GetValue(dest_x, dest_y, kBlue);
				unsigned char dest_a = dest.GetValue(dest_x, dest_y, kAlpha);
#endif

				unsigned int dest4 = dest.GetValue4(dest_x, dest_y);
#ifdef DEBUG
				{
					unsigned char dr, dg, db, da;
					Mux<Dest::sm_format>::Separate(dest4, dr, dg, db, da);

					if (Mux<Dest::sm_format>::GetOffset(kRed) != -1)
						MY_ASSERT(dr == dest_r);
					if (Mux<Dest::sm_format>::GetOffset(kGreen) != -1)
						MY_ASSERT(dg == dest_g);
					if (Mux<Dest::sm_format>::GetOffset(kBlue) != -1)
						MY_ASSERT(db == dest_b);
					if (Mux<Dest::sm_format>::GetOffset(kAlpha) != -1)
						MY_ASSERT(da == dest_a);
				}
#endif

				dest.Prefetch(dest_x, dest_y, 64);

#ifdef DEBUG
				{
					unsigned char sr, sg, sb, sa;
					Mux<Source::sm_format>::Separate(source4, sr, sg, sb, sa);

					if (Mux<Source::sm_format>::GetOffset(kRed) != -1)
						MY_ASSERT(sr == source_r);
					if (Mux<Source::sm_format>::GetOffset(kGreen) != -1)
						MY_ASSERT(sg == source_g);
					if (Mux<Source::sm_format>::GetOffset(kBlue) != -1)
						MY_ASSERT(sb == source_b);
					if (Mux<Source::sm_format>::GetOffset(kAlpha) != -1)
						MY_ASSERT(sa == source_a);
				}
#endif

				source4 = InOp<unsigned char, Source::sm_format, Mask::sm_format, Mux<Source::sm_format>::sm_formatPlusAlpha>::Op4(source4, mask4);


#ifdef DEBUG
//				if (mask_pixel != 255)
				{
					source_r = InOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_r, mask_pixel);
					source_g = InOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_g, mask_pixel);
					source_b = InOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_b, mask_pixel);
					source_a = InOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_a, mask_pixel);
				}

				{
					unsigned char vr, vg, vb, va;
					Mux<Mux<Source::sm_format>::sm_formatPlusAlpha>::Separate(source4, vr, vg, vb, va);
					if (Mux<Mux<Source::sm_format>::sm_formatPlusAlpha>::GetOffset(kRed) != -1)
						MY_ASSERT(vr == source_r);
					if (Mux<Mux<Source::sm_format>::sm_formatPlusAlpha>::GetOffset(kGreen) != -1)
						MY_ASSERT(vg == source_g);
					if (Mux<Mux<Source::sm_format>::sm_formatPlusAlpha>::GetOffset(kBlue) != -1)
						MY_ASSERT(vb == source_b);
					if (Mux<Mux<Source::sm_format>::sm_formatPlusAlpha>::GetOffset(kAlpha) != -1)
						MY_ASSERT(va == source_a);
				}

				unsigned char final_r = OverOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_r, dest_r, 255 - source_a);
				unsigned char final_g = OverOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_g, dest_g, 255 - source_a);
				unsigned char final_b = OverOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_b, dest_b, 255 - source_a);
				unsigned char final_a = OverOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_a, dest_a, 255 - source_a);
#endif

				unsigned char sa;
				if (Mux<Mux<Source::sm_format>::sm_formatPlusAlpha>::GetOffset(kAlpha) == -1)
					sa = 255;
				else
					sa = (source4 >> (Mux<Mux<Source::sm_format>::sm_formatPlusAlpha>::GetOffset(kAlpha) * 8)) & 0xff;

				dest4 = OverOp<unsigned char, Mux<Mux<Source::sm_format>::sm_formatPlusAlpha>::sm_formatPlusAlpha, Mask::sm_format, Dest::sm_format>::Op4(source4, dest4, 255 - sa);
#ifdef DEBUG
				{
					unsigned char vr, vg, vb, va;
					Mux<Dest::sm_format>::Separate(dest4, vr, vg, vb, va);
					if (Mux<Dest::sm_format>::GetOffset(kRed) != -1)
						MY_ASSERT(vr == final_r);
					if (Mux<Dest::sm_format>::GetOffset(kGreen) != -1)
						MY_ASSERT(vg == final_g);
					if (Mux<Dest::sm_format>::GetOffset(kBlue) != -1)
						MY_ASSERT(vb == final_b);
					if (Mux<Dest::sm_format>::GetOffset(kAlpha) != -1)
						MY_ASSERT(va == final_a);
				}
#endif

				//write back to destination
//				dest.SetValue(dest_x, dest_y, kRed, final_r);
//				dest.SetValue(dest_x, dest_y, kGreen, final_g);
//				dest.SetValue(dest_x, dest_y, kBlue, final_b);
//				dest.SetValue(dest_x, dest_y, kAlpha, final_a);

				dest.SetValue4(dest_x, dest_y, dest4);
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
	template <class Source, class Dest, class Mask, class SourceCoord>
	static void Op(const Source &source, Dest &dest, const Mask &mask, const SourceCoord &coord,
			const int width, const int height)
	{
		Iterator<SourceCoord, kY> y(source.GetOffY(), mask.GetOffY(), dest.GetOffY(), height, coord);

		for (/* y */; y.End(); y.Next())
		{
			Iterator<SourceCoord, kX> x(source.GetOffX(), mask.GetOffX(), dest.GetOffX(), width, coord);
			for (/* x */; x.End(); x.Next())
			{
				//get mask coordinates
				const int mask_x = x.GetMask();
				const int mask_y = y.GetMask();

				unsigned char mask_pixel = mask.GetValue(mask_x, mask_y, kAlpha);

				if (mask_pixel == 0)
					continue;

				//get source coordinates
				const int source_x = x.GetSource();
				const int source_y = y.GetSource();

				unsigned char source_r = source.GetValue(source_x, source_y, kRed);
				unsigned char source_g = source.GetValue(source_x, source_y, kGreen);
				unsigned char source_b = source.GetValue(source_x, source_y, kBlue);
				unsigned char source_a = source.GetValue(source_x, source_y, kAlpha);

				//get dest coordinates
				const int dest_x = x.GetDest();
				const int dest_y = y.GetDest();

				unsigned char dest_r = dest.GetValue(dest_x, dest_y, kRed);
				unsigned char dest_g = dest.GetValue(dest_x, dest_y, kGreen);
				unsigned char dest_b = dest.GetValue(dest_x, dest_y, kBlue);
				unsigned char dest_a = dest.GetValue(dest_x, dest_y, kAlpha);

				if (mask_pixel != 255)
				{
					source_r = InOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_r, mask_pixel);
					source_g = InOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_g, mask_pixel);
					source_b = InOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_b, mask_pixel);
					source_a = InOp<unsigned char, Source::sm_format, Mask::sm_format, Dest::sm_format>::Op(source_a, mask_pixel);
				}

				unsigned char final_r = AddOp<unsigned char>::Op(dest_r, source_r);
				unsigned char final_g = AddOp<unsigned char>::Op(dest_g, source_g);
				unsigned char final_b = AddOp<unsigned char>::Op(dest_b, source_b);
				unsigned char final_a = AddOp<unsigned char>::Op(dest_a, source_a);

				//write back to dest image
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
				Operation::template Op(source, dest, mask, zero, pOp[count].width, pOp[count].height);
			}
			else if (source_wrap)
			{
				const WrappedSourceCoord wrap(source_width, source_height);
				Operation::template Op(source, dest, mask, wrap, pOp[count].width, pOp[count].height);
			}
			else
			{
				const NormalSourceCoord normal;
				Operation::template Op(source, dest, mask, normal, pOp[count].width, pOp[count].height);
			}

		}
		else
		{
			const unsigned char m = 255;
			FixedValue<unsigned char, MaskPf> mask(&m);		//todo add compile-time assert here == 8 bit

			if (source_width == 1 && source_height == 1)
			{
				const ZeroSourceCoord zero;
				Operation::template Op(source, dest, mask, zero, pOp[count].width, pOp[count].height);
			}
			else if (source_wrap)
			{
				const WrappedSourceCoord wrap(source_width, source_height);
				Operation::template Op(source, dest, mask, wrap, pOp[count].width, pOp[count].height);
			}
			else
			{
				const NormalSourceCoord normal;
				Operation::template Op(source, dest, mask, normal, pOp[count].width, pOp[count].height);
			}
		}
	}
}


