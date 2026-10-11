//#include <features.h>
#include <endian.h>
#include <stdint.h>

#include "image_backlog.h"
#include "helper/pow2_ge.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // exit
#include <memory.h>
typedef uint32_t u32, u24;
typedef  uint8_t u8, u1, u5, u6;
typedef uint16_t u16;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define PADDING(a,x)    ((a) -1 - ((x) +(a) -1) % (a))
#define ALIGN(a,x) ((x) +(a) -1 - ((x) +(a) -1) % (a))

#define FALSE 0
#define TRUE  1
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
#define input_error assert

///////////////////////////////////////////////////////////////
// endian portability

static u32 load_le32 (const void *src)
{
u32 val
	= *(const u32 *)src;
#if __BYTE_ORDER == __LITTLE_ENDIAN
#else
	val =       0xff  & val >> 24
	    |     0xff00  & val >> 8
	    |   0xff0000  & val << 8
	    | 0xff000000U & val << 24;
#endif
	return val;
}

static u16 load_le16 (const void *src)
{
u16 val
	= *(const u16 *)src;
#if __BYTE_ORDER == __LITTLE_ENDIAN
#else
	val =   0xff & val >> 8
	    | 0xff00 & val << 8;
#endif
	return val;
}

static u24 load_le24 (const void *src)
{
const u8 *p
	= (const u8 *)src;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return p[2]<<16|p[1]<<8|p[0];
#else
	return p[0]<<16|p[1]<<8|p[2];
#endif
}

static void store_be24 (void *dst, u24 val)
{
u8 *p
	= (u8 *)dst;
	p[0] = (u8)(0xff & val >> 16);
	p[1] = (u8)(0xff & val >> 8 );
	p[2] = (u8)(0xff & val      );
}

///////////////////////////////////////////////////////////////
// utility

// lines order
//#define PF_TOPBOTTOM 0 // top -> bottom
#define PF_BOTTOMTOP 1 // bottom -> top (*)cc='BM' format (.bmp)

// endian
//#define PF_RGB888 0 // b.e. (R->G->B)
#define PF_BGR888 2 // l.e. (B->G->R) (*)cc='BM' format (.bmp)

static void _bpp24_get_clipping (
  const void *src_
, u16 src_width, u16 src_height, u16 src_align_bytes
, u1 src_format
, u16 x, u16 y
, u16 width, u16 height, u16 align_bytes
, u24 xy_not_enough_cl24
, void *dst_
)
{
const u5 BPP = 24;

const u8 *src; u8 *dst;
	src = (const u8 *)src_, dst = (u8 *)dst_;
u16 y_end
	= min(src_height, y +height);
u16 x_end
	= min(src_width, x +width);
u16 x_padding_bytes
	= (0 == align_bytes)
	? 0 : PADDING(align_bytes, width * BPP / 8);

unsigned src_opl // bytes per line
	= ALIGN(src_align_bytes, src_width * BPP / 8);
unsigned y_offset;
	if (PF_BOTTOMTOP & src_format)
		y_offset = (src_height -1 -y) * src_opl;
	else
		y_offset = y * src_opl;
u16 x_begin = x, y_begin = y;
	for (; y < y_begin +height; ++y) {
		x = x_begin;
		if (! (y < y_end))
			;
		else if (PF_BGR888 & src_format) {
			for (; x < x_end; ++x) {
u24 rgb24
				= load_le24 (src +y_offset +x * BPP / 8);
				store_be24 (dst, rgb24);
				dst += BPP / 8;
			}
		}
		else /*if (! (PF_BGR888 & src_format))*/ {
			memcpy (dst, src +y_offset,
			        (x_end -x_begin) * BPP / 8);
			dst += (x_end -x_begin) * BPP / 8;
			x = x_end;
		}
u16 i;
		for (; x < x_begin +width; ++x) {
			store_be24 (dst, xy_not_enough_cl24);
			dst += BPP / 8;
		}
		if (0 < x_padding_bytes) {
			memset (dst, 0, x_padding_bytes);
			dst += x_padding_bytes;
		}
		if (PF_BOTTOMTOP & src_format)
			y_offset -= src_opl;
		else
			y_offset += src_opl;
	}
}

///////////////////////////////////////////////////////////////
// singlton memory

typedef struct {
	u16 y_grid, x_grid;
	u8 *image;
} entry_s;

typedef struct {
	entry_s *list;
	unsigned list_max, list_len;
	u8 grid_width, grid_height;
	// streaming work
	u16 x_grid, y_grid;
	u32 image_len;
	unsigned total_received;
	u8 bmphdr[0x36];
	union {
		u8 any;
		struct {
			u8 conflict        :1;
			u8 unsupported_bpp :1;
			u8 padding         :6;
		};
	} header_err;
} gtk2_image_s;

static gtk2_image_s s_singlton = {0};

///////////////////////////////////////////////////////////////
// 

static entry_s *_list_get_last_appended (gtk2_image_s *m_)
{
	if (! (m_->list && 0 < m_->list_len))
		return NULL;
entry_s *retval
	= &m_->list[m_->list_len -1];
	return retval;
}

static entry_s *_list_append (gtk2_image_s *m_, unsigned len)
{
	// prologue
void *ptr; unsigned size, old_max, now_len;
	ptr = m_->list, size = usizeof(entry_s),
	old_max = m_->list_max, now_len = m_->list_len;

	// common
unsigned needed_max = pow2_ge(now_len +len);
	if (old_max < needed_max) {
		if (NULL == ptr)
			ptr = malloc (needed_max * size);
		else
			ptr = realloc (m_->list, needed_max * size);
	}
	now_len += len;

	// epilogue
	m_->list = (entry_s *)ptr, m_->list_len = now_len;
	if (m_->list_max < needed_max)
		m_->list_max = needed_max;

	return _list_get_last_appended (m_);
}

static entry_s *_list_seek (gtk2_image_s *m_,
                            u16 x_grid, u16 y_grid,
                            u16 *img_width_, u16 *img_height_)
{
flow_bug(0 < m_->grid_width && 0 < m_->grid_height)
	if (NULL == m_->list)
		return NULL;

u16 img_height, img_width;
unsigned i;
	for (i = 0; i < m_->list_len; ++i) {
entry_s *p = &m_->list[i];

		// p->y <= y < p->bottom
		if (! (p->y_grid <= y_grid))
			continue;
		img_height = load_le16 (p->image +0x16);
u16 bottom_grid
		= p->y_grid +ALIGN(m_->grid_height, img_height)
		                 / m_->grid_height; // round up
		if (! (y_grid < bottom_grid))
			continue;

		// p->x <= x < p->right
		if (! (p->x_grid <= x_grid))
			continue;
		img_width = load_le16 (p->image +0x12);
u16 right_grid
		= p->x_grid +ALIGN(m_->grid_width, img_width)
		                 / m_->grid_width; // round up
		if (! (x_grid < right_grid))
			continue;

		break; // found
	}
	if (! (i < m_->list_len))
		return NULL; // not found

	if (img_width_)
		*img_width_ = img_width;
	if (img_height_)
		*img_height_ = img_height;
	return &m_->list[i];
}

///////////////////////////////////////////////////////////////
// public

static void _set_grid (u8 width, u8 height)
{
gtk2_image_s *m_ = &s_singlton;

	m_->grid_width = width;
	m_->grid_height = height;
}

static void _setpos (u16 x_grid, u16 y_grid)
{
gtk2_image_s *m_ = &s_singlton;

	m_->image_len = 0;
	m_->total_received = 0;
	m_->header_err.any = 0;

	m_->x_grid = x_grid;
	m_->y_grid = y_grid;
}

static unsigned _write (const void *buf_, unsigned available)
{
gtk2_image_s *m_ = &s_singlton;

const u8 *buf = (const u8 *)buf_, *p = buf;
caller_bug(buf_ && 0 < available)
	if (! (buf_ && 0 < available))
		return 0; // safety, but cannot classify after data end.

u6 end_of_header
	= usizeof(m_->bmphdr);
unsigned needed, receiving; u1 filled_just_now;
	// BITMAP header +00,2'BM' +02,4 length
	filled_just_now = FALSE;
	if (m_->total_received < end_of_header) {
		needed = end_of_header -m_->total_received;
		receiving = min(needed, available);

		memcpy (m_->bmphdr +m_->total_received, p, receiving);
		m_->total_received += receiving;

		p += receiving, available -= receiving;
		if (m_->total_received < end_of_header)
			return (unsigned)(p -buf); // header not filled

		filled_just_now = TRUE;
	}
u32 end_of_body; u16 width, height; u8 bpp;
	end_of_body = load_le32 (m_->bmphdr +0x02);
	width       = load_le16 (m_->bmphdr +0x12);
	height      = load_le16 (m_->bmphdr +0x16);
	bpp         =          *(m_->bmphdr +0x1c);

	if (filled_just_now) {
		// check header error
u32 opl = ALIGN(32, width * bpp) / 8; // octet per line
		if (! (end_of_header +opl * height == end_of_body))
			m_->header_err.conflict = TRUE;
		if (! (24 == bpp))
			m_->header_err.unsupported_bpp = TRUE;
		// cancel new entry if header causes any error(s)
		if (m_->header_err.any) {
			receiving = min(available,
			                end_of_body -m_->total_received);
			m_->total_received += receiving;
			return (unsigned)receiving;
		}

		// create new entry
entry_s *appending
		= _list_append (m_, 1);
		appending->x_grid = m_->x_grid;
		appending->y_grid = m_->y_grid;
		appending->image = (u8 *)malloc (end_of_body);
		// start receiving body
		memcpy (appending->image, m_->bmphdr, end_of_header);
	}

	// BITMAP body
	filled_just_now = FALSE;
	if (m_->total_received < end_of_body) {
		needed = end_of_body -m_->total_received;
		receiving = min(needed, available);

		// streaming if header causes no error
		if (!m_->header_err.any) {
entry_s *appending
			= _list_get_last_appended (m_);
			memcpy (appending->image +m_->total_received, p, receiving);
		}
		m_->total_received += receiving;

		p += receiving, available -= receiving;
		if (m_->total_received < end_of_body)
			return (unsigned)(p -buf);

		filled_just_now = TRUE; // now not handled
	}

	// replying result
	return (unsigned)(p -buf);
}

static bool _tellpos (u16 *x_grid_, u16 *y_grid_,
                      u16 *width_grid, u16 *height_grid)
{
gtk2_image_s *m_ = &s_singlton;

flow_bug(0 < m_->grid_width && 0 < m_->grid_height)

entry_s *p
	= _list_get_last_appended (m_);
	if (NULL == p)
		return false;
u16 img_width, img_height;
	img_width = load_le16 (p->image +0x12);
	img_height = load_le16 (p->image +0x16);

u16 x_grid
//	= p->x / m_->grid_width; // round down NOTE: (*1)
	= p->x_grid;
	if (x_grid_)
		*x_grid_ = x_grid;
	if (y_grid_)
		*y_grid_ = p->y_grid;

u16 x
//	= p->x; NOTE: (*1)
	= p->x_grid * m_->grid_width;
u16 right_grid
	= ALIGN(m_->grid_width, x + img_width)
	      / m_->grid_width; // round up
u16 bottom_grid
	= p->y_grid + ALIGN(m_->grid_height, img_height)
	                  / m_->grid_height; // round up
	if (width_grid)
		*width_grid = right_grid - x_grid;
	if (height_grid)
		*height_grid = bottom_grid - p->y_grid;
	return true;
}

static bool _clip_from (u16 x_grid, u16 y_grid,
                        u24 xy_not_enough_cl24,
                        void *bpp24_image)
{
gtk2_image_s *m_ = &s_singlton;

flow_bug(0 < m_->grid_width && 0 < m_->grid_height)
u16 img_height, img_width;
entry_s *p
	= _list_seek (m_, x_grid, y_grid, &img_width, &img_height);
u1 no_image_on_this_grid
	= (NULL == p) ? TRUE : FALSE;
		if (no_image_on_this_grid)
			return false;

u1 present_or_absent_only
	= (NULL == bpp24_image) ? TRUE : FALSE;
	if (present_or_absent_only)
		return true;

u16 clip_x, clip_y;
	clip_x = (x_grid -p->x_grid) * m_->grid_width;
	clip_y = (y_grid -p->y_grid) * m_->grid_height;
u16 img_align, clip_align;
	_bpp24_get_clipping (p->image +0x36, img_width, img_height,
	                     img_align = 4,
	                     PF_BGR888|PF_BOTTOMTOP, // NOTE: (*2)
	                     clip_x, clip_y,
	                     m_->grid_width, m_->grid_height,
	                     clip_align = 4,
	                     xy_not_enough_cl24,
	                     bpp24_image);
	return true;
}

///////////////////////////////////////////////////////////////
// readability sugar

static const READABILITY_SUGAR_IMAGE_BACKLOG _image_backlog = {
	.set_grid = _set_grid,
	.setpos = _setpos,
	.write = _write,
	.tellpos = _tellpos,
	.clip_from = _clip_from,
};
const READABILITY_SUGAR_IMAGE_BACKLOG *image_backlog = &_image_backlog;

///////////////////////////////////////////////////////////////
// footnote

/* NOTE: (*1)
   If it is supposed to stay X-position from modifying X-grid
   size, refer this comment out.

   NOTE: (*2)
   It causes getting slow, it should be converted into PF_RGB888
   at first loading. */
