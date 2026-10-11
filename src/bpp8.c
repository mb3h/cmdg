//#include <features.h>
//#include <endian.h>
#include <stdint.h>

#include "bpp8.h"
#include "helper/pow2_ge.h"

#include <stdio.h> // printf
#include <stdlib.h> // exit
#include <memory.h>
#include <malloc.h>
typedef uint32_t u32, u17;
typedef  uint8_t u8;
typedef uint16_t u15;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
///////////////////////////////////////////////////////////////
// VT100

#define VTO  "\033[0m"
#define VTRR "\033[1;31m"
#define VTGG "\033[1;32m"
#define VTYY "\033[1;33m"
#define VTBB "\033[1;34m"
#define VTMM "\033[1;35m"
#define VTCC "\033[1;36m"

///////////////////////////////////////////////////////////////
// assertion

#define assert(expr) \
	do { \
		if (! (expr)) { \
printf (VTRR "ASSERT" VTO ": " #expr " [%s]" "\n", __func__); \
			exit (-1); \
		} \
	} while (0);

#define caller_bug  assert
#define   flow_bug  assert
#define header_bug  assert

///////////////////////////////////////////////////////////////
// object memory

typedef struct {
	u8 *values;
	u32 byte_size;
	u17 y_size;
	u15 x_size;
} bpp8_s;

///////////////////////////////////////////////////////////////
// readability sugar

#define _resize bpp8_resize
#define _get    bpp8_get
#define _set    bpp8_set
#define _copy   bpp8_copy
#define _read   bpp8_read
#define _write  bpp8_write

///////////////////////////////////////////////////////////////
// public

void _resize (bpp8 *this_,
              u15 width, u17 height,
              u8 pad_val)
{
header_bug(sizeof(bpp8_s) <= sizeof(bpp8))
bpp8_s *m_ = (bpp8_s *)this_;

caller_bug(0 < width && 0 < height)
	if (width == m_->x_size && height == m_->y_size)
		return;
unsigned needed_byte_size
	= width * height;
	if (NULL == m_->values) {
		m_->values = (u8 *)malloc (pow2_ge(needed_byte_size));
		m_->byte_size = pow2_ge(needed_byte_size);
		memset (m_->values, 0, m_->byte_size);
	}
	else {
		if (m_->byte_size < pow2_ge(needed_byte_size)) {
			m_->values = (u8 *)realloc (m_->values, pow2_ge(needed_byte_size));
			m_->byte_size = pow2_ge(needed_byte_size);
		}
		// data moving
u17 y;
		if (width < m_->x_size) {
			for (y = 0; y < height; ++y) {
				if (0 < y)
					memcpy (m_->values +y * width,
					        m_->values +y * m_->x_size,
					        min(m_->x_size, width));
			}
		}
		else if (m_->x_size < width) {
			for (y = height; 0 < y--;) {
				if (0 < y)
					memmove (m_->values +y * width,
					         m_->values +y * m_->x_size,
					         min(m_->x_size, width));
				memset (m_->values +y * width +m_->x_size,
				        pad_val, width -m_->x_size);
			}
		}
	}
	m_->x_size = width, m_->y_size = height;
}

u8 _get (bpp8 *this_, u15 x, u17 y)
{
bpp8_s *m_ = (bpp8_s *)this_;

unsigned index
	= y * m_->x_size + x;
caller_bug(index < m_->byte_size)
	if (! (index < m_->byte_size))
		return 0x00; // safety
u8 retval
	= m_->values[index];
	return retval;
}

void _set (bpp8 *this_,
           u15 x, u17 y, u15 width, u17 height,
           u8 val)
{
bpp8_s *m_ = (bpp8_s *)this_;

caller_bug(x < m_->x_size)
caller_bug(y < m_->y_size)
caller_bug(x +width <= m_->x_size)
caller_bug(y +height <= m_->y_size)
unsigned index
	= y * m_->x_size + x;
unsigned end_index
	= index +width +(height -1) * m_->x_size;
flow_bug(end_index <= m_->byte_size)
	if (! (end_index <= m_->byte_size))
		return; // safety

u15 x0 = x; u17 y0 = y;
	for (; y < y0 +height; ++y) {
		for (x = x0; x < x0 +width; ++x)
			m_->values[index++] = val;
		index -= width;
		index += m_->x_size;
	}
}

void _copy (bpp8 *this_,
            u15 x, u17 y, u15 width, u17 height,
            u15 src_x, u17 src_y)
{
bpp8_s *m_ = (bpp8_s *)this_;

caller_bug(x < m_->x_size)
caller_bug(y < m_->y_size)
caller_bug(x +width <= m_->x_size)
caller_bug(y +height <= m_->y_size)
caller_bug(src_x < m_->x_size)
caller_bug(src_y < m_->y_size)
caller_bug(0 < width && 0 < height)
	if (! (0 < width && 0 < height))
		return; // safety

u15 true_width
	= min(src_x +width, m_->x_size) -src_x;
	true_width = min(x +true_width, m_->x_size) -x;
u17 true_height
	= min(src_y +height, m_->y_size) -src_y;
	true_height = min(y +true_height, m_->y_size) -y;
u17 y_end
	= y +true_height;

	if (src_y < y || src_y == y && src_x < x) {
		y += true_height, src_y += true_height;
		for (--src_y; y_end -true_height < y--; --src_y) {
			memmove (m_->values +    y * m_->x_size +    x,
			         m_->values +src_y * m_->x_size +src_x,
			         true_width);
		}
	}
	else {
		for (; y < y_end; ++y, ++src_y) {
			memcpy (m_->values +    y * m_->x_size +    x,
			        m_->values +src_y * m_->x_size +src_x,
			        true_width);
		}
	}
}

void _read (bpp8 *this_,
            u15 x, u17 y, u15 width, u17 height,
            void *dst_)
{
bpp8_s *m_ = (bpp8_s *)this_;
u8 *dst = (u8 *)dst_;

caller_bug(0 < width)
caller_bug(0 < height)
caller_bug(x < m_->x_size)
caller_bug(y < m_->y_size)

u15 x_end
	= min(x +width, m_->x_size);
u15 y_end
	= min(y +height, m_->y_size);

u15 y_begin = y;
	for (; y < y_end; ++y) {
		memcpy (dst, m_->values +y * m_->x_size +x, x_end -x);
		if (x_end < x +width)
			memset (dst +x_end -x, 0, x +width -x_end);
		dst += width;
	}
	if (y_end < y_begin +height)
		memset (dst, 0, (y_begin +height -y_end) * width);
}

void _write (bpp8 *this_,
             u15 x, u17 y, u15 width, u17 height,
             const void *src_)
{
bpp8_s *m_ = (bpp8_s *)this_;
u8 *src = (u8 *)src_;

caller_bug(0 < width)
caller_bug(0 < height)
caller_bug(x < m_->x_size)
caller_bug(y < m_->y_size)

u15 x_end
	= min(x +width, m_->x_size);
u15 y_end
	= min(y +height, m_->y_size);

	for (; y < y_end; ++y) {
		memcpy (m_->values +y * m_->x_size +x, src, x_end -x);
		src += width;
	}
}
