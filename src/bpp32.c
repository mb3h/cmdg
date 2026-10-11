//#include <features.h>
//#include <endian.h>
#include <stdint.h>

#include "bpp32.h"
#include "helper/pow2_ge.h"

#include <stdio.h> // printf
#include <stdlib.h> // exit
#include <memory.h>
#include <malloc.h>
typedef uint32_t u32, u17, u21;
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
	u21 *values;
	u32 byte_size;
	u17 y_size;
	u15 x_size;
} bpp21_s;

///////////////////////////////////////////////////////////////
// readability sugar

#define _resize bpp21_resize
#define _get    bpp21_get
#define _set    bpp21_set
#define _copy   bpp21_copy
#define _read   bpp21_read
#define _write  bpp21_write

///////////////////////////////////////////////////////////////
// public

void _resize (bpp21 *this_,
              u15 width, u17 height,
              u21 pad_val)
{
header_bug(sizeof(bpp21_s) <= sizeof(bpp21))
bpp21_s *m_ = (bpp21_s *)this_;

caller_bug(0 < width && 0 < height)
	if (width == m_->x_size && height == m_->y_size)
		return;
unsigned needed_byte_size
	= usizeof(u21) * width * height;
	if (NULL == m_->values) {
		m_->values = (u21 *)malloc (pow2_ge(needed_byte_size));
		m_->byte_size = pow2_ge(needed_byte_size);
		memset (m_->values, 0, m_->byte_size);
	}
	else {
		if (m_->byte_size < pow2_ge(needed_byte_size)) {
			m_->values = (u21 *)realloc (m_->values, pow2_ge(needed_byte_size));
			m_->byte_size = pow2_ge(needed_byte_size);
		}
		// data moving
u17 y;
		if (width < m_->x_size) {
			for (y = 0; y < height; ++y) {
				if (0 < y)
					memcpy (m_->values +y * width,
					        m_->values +y * m_->x_size,
					        min(m_->x_size, width) * usizeof(u21));
			}
		}
		else if (m_->x_size < width) {
			for (y = height; 0 < y--;) {
				if (0 < y)
					memmove (m_->values +y * width,
					         m_->values +y * m_->x_size,
					         min(m_->x_size, width) * usizeof(u21));
u15 x;
				for (x = m_->x_size; x < width; ++x)
					m_->values[y * width +x] = pad_val;
			}
		}
	}
	m_->x_size = width, m_->y_size = height;
}

u21 _get (bpp21 *this_, u15 x, u17 y)
{
bpp21_s *m_ = (bpp21_s *)this_;

unsigned index
	= y * m_->x_size + x;
caller_bug(index * usizeof(u21) < m_->byte_size)
	if (! (index * usizeof(u21) < m_->byte_size))
		return 0x000000; // safety
u21 retval
	= m_->values[index];
	return retval;
}

void _set (bpp21 *this_,
           u15 x, u17 y, u15 width, u17 height,
           u21 val)
{
bpp21_s *m_ = (bpp21_s *)this_;

caller_bug(x < m_->x_size)
caller_bug(y < m_->y_size)
caller_bug(x +width <= m_->x_size)
caller_bug(y +height <= m_->y_size)
unsigned index
	= y * m_->x_size + x;
unsigned end_index
	= index +width +(height -1) * m_->x_size;
flow_bug(end_index * usizeof(u21) <= m_->byte_size)
	if (! (end_index * usizeof(u21) <= m_->byte_size))
		return; // safety

u15 x0 = x; u17 y0 = y;
	for (; y < y0 +height; ++y) {
		for (x = x0; x < x0 +width; ++x)
			m_->values[index++] = val;
		index -= width;
		index += m_->x_size;
	}
}

void _copy (bpp21 *this_,
            u15 x, u17 y, u15 width, u17 height,
            u15 src_x, u17 src_y)
{
bpp21_s *m_ = (bpp21_s *)this_;

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
			         true_width * usizeof(u21));
		}
	}
	else {
		for (; y < y_end; ++y, ++src_y) {
			memcpy (m_->values +    y * m_->x_size +    x,
			        m_->values +src_y * m_->x_size +src_x,
			        true_width * usizeof(u21));
		}
	}
}

void _read (bpp21 *this_,
            u15 x, u17 y, u15 width, u17 height,
            void *dst_)
{
bpp21_s *m_ = (bpp21_s *)this_;
u21 *dst = (u21 *)dst_;

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
		memcpy (dst, m_->values +y * m_->x_size +x,
		        (x_end -x) * usizeof(u21));
		if (x_end < x +width)
			memset (dst +x_end -x, 0,
			        (x +width -x_end) * usizeof(u21));
		dst += width;
	}
	if (y_end < y_begin +height)
		memset (dst, 0,
		        (y_begin +height -y_end) * width * usizeof(u21));
}

void _write (bpp21 *this_,
             u15 x, u17 y, u15 width, u17 height,
             const void *src_)
{
bpp21_s *m_ = (bpp21_s *)this_;
u21 *src = (u21 *)src_;

caller_bug(0 < width)
caller_bug(0 < height)
caller_bug(x < m_->x_size)
caller_bug(y < m_->y_size)

u15 x_end
	= min(x +width, m_->x_size);
u15 y_end
	= min(y +height, m_->y_size);

	for (; y < y_end; ++y) {
		memcpy (m_->values +y * m_->x_size +x, src,
		        (x_end -x) * usizeof(u21));
		src += width;
	}
}
