
#include <stdint.h>
#include "common/pixel.h"
struct GVRAM_;
#define VRAM_s struct GVRAM_
#include "GVRAM.hpp"

#define img_helper_s void
#include "bmp_helper.hpp"
typedef struct bmp_helper bmp_helper_s;

#include "lock.hpp"
typedef struct lock lock_s;
#include "core/lsb0_bit_helper.h"
#include "portab.h"
#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdbool.h>
#include <stddef.h> // offsetof
#include <stdlib.h> // exit
#include <memory.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define bitsizeof(x) (unsigned)(sizeof(x) * 8)
#define arraycountof(x) (sizeof(x)/(sizeof((x)[0])))

#define HIDIV(x,a) (((x) +(a) -1)/(a))
#define HIALIGN(a,x) (HIDIV(x,a)*(a))
#define ALIGN HIALIGN
#define ALIGN32(x) (-32 & (x) +32 -1)

#define MAX_IMAGE (1 << 16) // = 65536

typedef struct {
	u32 width         :12; // 0..4095(= 0xFFF)
	u32 height        :12; // 0..4095(= 0xFFF)
	u32 pf            : 8;
	u32 rawdata_maxcb :26; // 0..67092479(= 0x3FF8003)(= 32bpp x 4095 * 4095)
	u32 reserved_7    : 6;
	void *rawdata;
} IMAGE_s;

typedef struct GVRAM_ {
	u16 ws_col, ws_row;
	u8 lf_width, lf_height;
	u8 padding_6[2];
	struct {
		u32 total_got   :25; // 1920 x 1280 x BGRA32 = 9.8MB (24bit)
		u32 reserved_11 : 7;
		u32 size        :25;
		u32 reserved_15 : 7;
		u16 cx, cy;
		u16 img_id;
		u8 padding_22[2];
	} now_loading;
	u32 *DIRTYs, *VLINKs, *HLINKs; // [ALIGN32(ws_col)/32 * ws_row]
	u16 *IDs;                      // [ws_col * ws_row]
	u32 *id2BLANKs;                // [ALIGN32(MAX_IMAGE)/32]
	IMAGE_s *id2IMAGEs;            // [len_IMAGEs]
	u16 len_IMAGEs;
#ifdef __x86_64__
		u8 padding_74[6];
#else //def __i386__
		u8 padding_50[2];
#endif
	struct img_helper_i *bh;
	bmp_helper_s bh_this_;
	lock_s thread_lock; // must be last member. cf)_reset()
} GVRAM_s;

#ifdef READ_SHELL_THREAD
#define _lock_cleanup() lock_dtor(&m_->thread_lock);
#define _lock_warmup() lock_ctor(&m_->thread_lock, sizeof(m_->thread_lock));
#define _lock() lock_lock(&m_->thread_lock);
#define _unlock() lock_unlock(&m_->thread_lock);
#else
#define _lock_cleanup()
#define _lock_warmup()
#define _lock()
#define _unlock()
#endif

///////////////////////////////////////////////////////////////////////////////
// interface

static u32 GVRAM_check_transparent (GVRAM_s *m_, u16 cx, u16 cy, u32 mask)
{
	if (0 == mask)
		return 0;
unsigned bitwidth; u8 work[sizeof(mask)];
	store_le32 (work, mask);
	bitwidth = 1 + lsb0_bitrchr (work, 1, bitsizeof(mask));
u32 retval; u16 x0;
	retval = 0, x0 = cx, cx = min(cx + bitwidth, m_->ws_col);
	while (x0 < cx) {
		--cx, retval <<= 1;
		if (! (1 & mask >> cx - x0))
			continue;
unsigned bits, i;
		bits = bitsizeof(m_->HLINKs[0]), i = cy * ALIGN(bits, m_->ws_col) + cx;
		if (1 & m_->HLINKs[i / bits] >> i % bits)
			continue;
		bits = bitsizeof(m_->VLINKs[0]), i = cy * ALIGN(bits, m_->ws_col) + cx;
		if (1 & m_->VLINKs[i / bits] >> i % bits)
			continue;
		if (0 < m_->IDs[cy * m_->ws_col + cx])
			continue;
		retval |= 1;
	}
	return retval;
}

struct image_attr {
	const void *rawdata;
	u16 width, height;
	u16 x0, y0;
	enum pixel_format pf;
};
static u16 GVRAM_attr_from_grid_position (GVRAM_s *m_, u16 cx, u16 cy, struct image_attr *a)
{
	if (! (m_ && cx < m_->ws_col && cy < m_->ws_row))
		return 0; // invalid parameter
	// top-left image seek
u16 x0, y0; unsigned bitwidth, bpr; // bits per row
	bitwidth = bitsizeof(m_->HLINKs[0]), bpr = ALIGN(bitwidth, m_->ws_col);
	x0 = (u16)lsb0_bitrchr (&m_->HLINKs[cy * bpr / bitwidth], 0, cx);
	bitwidth = bitsizeof(m_->VLINKs[0]), bpr = ALIGN(bitwidth, m_->ws_col);
	for (y0 = cy; 0 < y0; --y0)
		if (! (1 & m_->VLINKs[(y0 * bpr + x0) / bitwidth] >> x0 % bitwidth))
			break;
	// base image information
u16 img_id;
	if (0 == (img_id = m_->IDs[y0 * m_->ws_col + x0]))
		return 0;
IMAGE_s *src;
	src = &m_->id2IMAGEs[img_id];
	if (a)
		a->pf = src->pf, a->width = src->width, a->height = src->height, a->x0 = x0, a->y0 = y0;
	return img_id;
}

///////////////////////////////////////////////////////////////////////////////
// DIRTY

static u32 *GVRAM_dirty_ref (GVRAM_s *m_, u16 cx, u16 cy, u8 *bpos)
{
	if (! (cx < m_->ws_col && cy < m_->ws_row))
		return NULL;
u32 *p;
	p = m_->DIRTYs;
unsigned i;
	i = cy * ALIGN(bitsizeof(p[0]), m_->ws_col) + cx;
	if (bpos)
		*bpos = (u8)(i % bitsizeof(p[0]));
	return &p[i / bitsizeof(p[0])];
}

static void GVRAM_set_dirty_rect (GVRAM_s *m_, u16 cx, u16 cy, u16 cols, u16 rows)
{
	cx = min(cx, m_->ws_col);
	cy = min(cy, m_->ws_row);
u16 cx_end, cy_end;
	cx_end = (0 == cols) ? m_->ws_col : min(cx + cols, m_->ws_col);
	cy_end = (0 == rows) ? m_->ws_row : min(cy + rows, m_->ws_row);
	for (; cy < cy_end; ++cy) {
u32 *p;
		p = GVRAM_dirty_ref (m_, 0, cy, NULL);
		lsb0_bitset (p, cx, 1, cx_end - cx);
	}
}

///////////////////////////////////////////////////////////////////////////////
// ctor/dtor

static void GVRAM_reset (GVRAM_s *m_)
{
#ifndef __cplusplus
	memset (m_, 0, offsetof(GVRAM_s, thread_lock));
#else //def __cplusplus
	// TODO:
#endif // __cplusplus
}

void GVRAM_release (struct GVRAM *this_)
{
GVRAM_s *m_;
	m_ = (GVRAM_s *)this_;
_lock_cleanup()
	GVRAM_reset (m_);
}

#ifdef __cplusplus
void GVRAM_dtor (struct GVRAM *this_)
{
}
#endif // __cplusplus

void GVRAM_ctor (struct GVRAM *this_, unsigned cb, u16 ws_col, u16 ws_row)
{
#ifndef __cplusplus
BUGc(sizeof(GVRAM_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(GVRAM_s), cb)
#endif //ndef __cplusplus
GVRAM_s *m_;
	m_ = (GVRAM_s *)this_;
	GVRAM_reset (m_);
_lock_warmup()
	bmp_helper_ctor (&m_->bh_this_, sizeof(m_->bh_this_));
	m_->bh = bmp_helper_query_img_helper_i (&m_->bh_this_);
	lock_ctor (&m_->thread_lock, sizeof(m_->thread_lock));
	m_->ws_col = ws_col;
	m_->ws_row = ws_row;
	m_->DIRTYs = (u32 *)calloc (ALIGN(bitsizeof(m_->DIRTYs[0]), ws_col) / 8 * ws_row, 1);
	m_->VLINKs = (u32 *)calloc (ALIGN(bitsizeof(m_->VLINKs[0]), ws_col) / 8 * ws_row, 1);
	m_->HLINKs = (u32 *)calloc (ALIGN(bitsizeof(m_->HLINKs[0]), ws_col) / 8 * ws_row, 1);
	m_->IDs = (u16 *)calloc (ws_col * ws_row, sizeof(m_->IDs[0]));
	m_->id2IMAGEs = (IMAGE_s *)malloc (sizeof(IMAGE_s) * (m_->len_IMAGEs = 16)); // init not needed
unsigned size; void *p;
	size = ALIGN(bitsizeof(m_->id2BLANKs[0]), MAX_IMAGE) * sizeof(m_->id2BLANKs[0]);
	m_->id2BLANKs = (u32 *)(p = malloc (size)), memset (p, 0xff, size), *(u32 *)p &= ~1; // #0 always used
}

///////////////////////////////////////////////////////////////////////////////
// IMAGE
// (*1)now not needed because called from vt100_handler thread only.

static void GVRAM_image_destroy (GVRAM_s *m_, u16 img_id)
{
	if (! (0 < img_id && img_id < MAX_IMAGE))
		return;
//_lock() // (*1)
u32 *p;
	p = &m_->id2BLANKs[img_id / bitsizeof(*p)];
	*p |= 1 << img_id % bitsizeof(*p);
IMAGE_s *img;
	img = &m_->id2IMAGEs[img_id];
	if (img->rawdata)
		free (img->rawdata);
	memset (img, 0, sizeof(IMAGE_s));
//_unlock() // (*1)
}

static u16 GVRAM_image_create (GVRAM_s *m_, u16 width, u16 height, enum pixel_format pf)
{
	if (! (width < 0x1000 && height < 0x1000))
		return 0; // too large rectangle
//_lock() // (*1)
	// blank seek
u32 *p, *end;
	p = m_->id2BLANKs, end = p + MAX_IMAGE / bitsizeof(*p);
	while (p < end && 0 == *p)
		++p;
	if (! (p < end)) {
//_unlock() // (*1)
		return 0; // blank not found
	}
u16 i, img_id;
	for (i = 0; i < bitsizeof(*p); ++i)
		if (1 & *p >> i)
			break;
	img_id = (p - m_->id2BLANKs) * bitsizeof(*p) + i;
	// blank -> used
	*p &= ~(1 << i);
	// detail allocate
	if (! (img_id < m_->len_IMAGEs))
		m_->len_IMAGEs *= 2, m_->id2IMAGEs = (IMAGE_s *)realloc (m_->id2IMAGEs, sizeof(IMAGE_s) * m_->len_IMAGEs);
IMAGE_s *img;
	img = &m_->id2IMAGEs[img_id];
	img->pf = pf, img->width = width, img->height = height;
	img->rawdata = (u8 *)malloc (img->rawdata_maxcb = ALIGN32(bpp_from_pixel_format (pf) * width)/8 * height);
//_unlock() // (*1)
	return img_id;
}

///////////////////////////////////////////////////////////////////////////////

static void GVRAM_write_start (GVRAM_s *m_, u16 cx, u16 cy, u16 width, u16 height, enum pixel_format pf)
{
u8 bpp; u32 cb_rawdata; u16 img_id;
	bpp = bpp_from_pixel_format (pf);
	cb_rawdata = ALIGN32(bpp * width)/8 * height;
_lock()
	img_id = GVRAM_image_create (m_, width, height, pf);
ASSERTE(PF_END -1 < 16 && cb_rawdata < 0x2000000)
	m_->now_loading.cx        = cx        ;
	m_->now_loading.cy        = cy        ;
	m_->now_loading.size      = cb_rawdata;
	m_->now_loading.total_got = 0         ;
	m_->now_loading.img_id    = img_id    ;
	m_->IDs[cy * m_->ws_col + cx] = img_id;
unsigned alpr_h, alpr_v; // alpr = array length per row
	alpr_h = HIDIV(m_->ws_col, bitsizeof(m_->HLINKs[0]));
	alpr_v = HIDIV(m_->ws_col, bitsizeof(m_->VLINKs[0]));
u16 cols;
	cols = HIDIV(width, m_->lf_width);
	lsb0_bitset (&m_->HLINKs[cy * alpr_h], cx +1, 1, cols -1);
u16 cy_end;
	cy_end = cy + HIDIV(height, m_->lf_height);
	for (++cy; cy < cy_end; ++cy) {
		lsb0_bitset (&m_->HLINKs[cy * alpr_h], cx +1, 1, cols -1);
		lsb0_bitset (&m_->VLINKs[cy * alpr_v], cx   , 1, cols   );
	}
_unlock()
}
static void GVRAM_write (GVRAM_s *m_, const void *rawdata, unsigned cb)
{
	// streaming image rawdata
IMAGE_s *img; u32 size;
	img = &m_->id2IMAGEs[m_->now_loading.img_id];
	size = ALIGN32(bpp_from_pixel_format (img->pf) * img->width)/8 * img->height;
	memcpy (img->rawdata + m_->now_loading.total_got, rawdata, cb);
	m_->now_loading.total_got += cb;
	if (m_->now_loading.total_got < m_->now_loading.size)
		return;
	// complete
	// TODO: streaming dirty
u16 cols, rows;
	cols = HIDIV(img->width, m_->lf_width);
	rows = HIDIV(img->height, m_->lf_height);
u16 cx, cx_end, cy, cy_end;
	cx = m_->now_loading.cx, cx_end = min(cx + cols, m_->ws_col);
	cy = m_->now_loading.cy, cy_end = min(cy + rows, m_->ws_row);
_lock()
	for (; cy < cy_end; ++cy) {
u32 *p;
		if (NULL == (p = GVRAM_dirty_ref (m_, 0, cy, NULL)))
			continue;
		lsb0_bitset (p, cx, 1, cx_end - cx);
	}
_unlock()
}
static void GVRAM_erase_row (GVRAM_s *m_, u16 cy)
{
u16 bpr; // bits_of_row
	bpr = ALIGN(bitsizeof(m_->DIRTYs[0]), m_->ws_col);
u16 n;
	n = cy * m_->ws_col + 0;
	memset (&m_->IDs[n], 0, m_->ws_col * sizeof(m_->IDs[0]));
	n = cy * bpr + 0;
ASSERTE(sizeof(m_->DIRTYs[0]) == sizeof(m_->VLINKs[0]))
	lsb0_bitset (m_->VLINKs, n, 0, m_->ws_col);
ASSERTE(sizeof(m_->DIRTYs[0]) == sizeof(m_->HLINKs[0]))
	lsb0_bitset (m_->HLINKs, n, 0, m_->ws_col);
	lsb0_bitset (m_->DIRTYs, n, 1, m_->ws_col);
}

static void GVRAM_copy_for_roll (GVRAM_s *m_, u16 cx_src, u16 cy_src, u16 cols, u16 rows, u16 cx_dst, u16 cy_dst)
{
	if (! (cy_src < m_->ws_row && cx_src < m_->ws_col && cx_dst < m_->ws_col && cy_dst < m_->ws_row))
		return;
	if (cx_src == cx_dst && cy_src == cy_dst)
		return;
	cols = min(min(cx_src + cols, m_->ws_col) - cx_src,
	           min(cx_dst + cols, m_->ws_col) - cx_dst);
	rows = min(min(cy_src + rows, m_->ws_col) - cy_src,
	           min(cy_dst + rows, m_->ws_col) - cy_dst);
	if (! (0 < cols && 0 < rows))
		return;
	if (cy_src < cy_dst)
		cy_src += rows -1, cy_dst += rows -1;
u16 bpr; // bits per row
	bpr = ALIGN(bitsizeof(m_->DIRTYs[0]), m_->ws_col);
u16 i;
	for (i = 0; i < rows; ++i) {
u16 offset_src, offset_dst;
		offset_src = cy_src * m_->ws_col + cx_src;
		offset_dst = cy_dst * m_->ws_col + cx_dst;
		memmove (&m_->IDs[offset_dst], &m_->IDs[offset_src], cols * sizeof(m_->IDs[0]));
		offset_src = cy_src * bpr + cx_src;
		offset_dst = cy_dst * bpr + cx_dst;
ASSERTE(sizeof(m_->DIRTYs[0]) == sizeof(m_->VLINKs[0]))
		lsb0_bitmove (m_->VLINKs, offset_dst, m_->VLINKs, offset_src, cols);
ASSERTE(sizeof(m_->DIRTYs[0]) == sizeof(m_->HLINKs[0]))
		lsb0_bitmove (m_->HLINKs, offset_dst, m_->HLINKs, offset_src, cols);
		lsb0_bitset (m_->DIRTYs, offset_dst, 1, cols);
		if (cy_src < cy_dst)
			--cy_src, --cy_dst;
		else
			++cy_dst, ++cy_src;
	}
}

static void GVRAM_rollup (GVRAM_s *m_, u16 n, u32 bg_rgb24)
{
	while (0 < n--) {
_lock()
u16 cx;
		for (cx = 0; cx < m_->ws_row; ++cx) {
u16 img_id;
			if (0 == (img_id = m_->IDs[0 * m_->ws_col + cx]))
				continue;
IMAGE_s *img;
			img = &m_->id2IMAGEs[img_id];
			if (m_->lf_height < img->height) {
				m_->bh->trim (img->rawdata, img->width, img->height, img->pf, TRIM_TOP, m_->lf_height);
				m_->IDs[1 * m_->ws_col + cx] = img_id;
				img->height -= m_->lf_height;
				continue;
			}
			GVRAM_image_destroy (m_, img_id);
		}
		GVRAM_copy_for_roll (m_, 0, 1, m_->ws_col, m_->ws_row -1, 0, 0);
		GVRAM_erase_row (m_, m_->ws_row -1);
_unlock()
	}
}

static void GVRAM_set_grid (GVRAM_s *m_, u16 lf_width, u16 lf_height)
{
	m_->lf_width = lf_width, m_->lf_height = lf_height;
}

static u32 GVRAM_dirty_lock (GVRAM_s *m_, u16 cx, u16 cy, u32 mask)
{
	if (0 == mask)
		mask = (u32)-1;
_lock()
u32 *dirty; u8 bpos;
	dirty = GVRAM_dirty_ref (m_, cx, cy, &bpos);
	if (! (0 == bpos || 0 == (mask & (u32)-1 << bitsizeof(dirty[0]) - bpos)))
		return 0; // refuse ladder of boundary
	return mask & *dirty >> bpos;
}

static u32 GVRAM_read (GVRAM_s *m_, u16 cx, u16 cy, enum pixel_format pf, void *data, u32 cb)
{
u32 cb_need;
	cb_need = ALIGN32(m_->lf_width * 24) / 8 * m_->lf_height;
u8 *p;
	p = (u8 *)data;
	while (cb_need < cb) // waste padding (safety)
		*p++ = 0x00;
u16 img_id; struct image_attr a;
	if (0 == (img_id = GVRAM_attr_from_grid_position (m_, cx, cy, &a)))
		return 0;
IMAGE_s *img;
	img = &m_->id2IMAGEs[img_id];
u8 bmphdr[sizeofBMPHDR];
	m_->bh->header_init (img->pf, img->width, img->height, bmphdr, usizeof(bmphdr));
	m_->bh->clip (bmphdr
		, img->rawdata
		, (cx - a.x0) * m_->lf_width
		, (cy - a.y0) * m_->lf_height
		, m_->lf_width
		, m_->lf_height
		, PF_RGB888
		, BF_TOPDOWN
		, data
		, cb_need);
	return cb;
}

static void GVRAM_dirty_unlock (GVRAM_s *m_, u16 cx, u16 cy, u32 mask)
{
u8 bpos;
u32 *dirty;
	dirty = GVRAM_dirty_ref (m_, cx, cy, &bpos);
ASSERTE(0 == bpos || 0 == (mask & (u32)-1 << bitsizeof(dirty[0]) - bpos))
	*dirty &= ~(mask >> bpos);
_unlock()
}

static const u8 IMAGE_KEEP = 1;
// PENDING: support to specify 'cols' and 'rows' (*)because coding cost to move IDs
static void GVRAM_erase_rect (GVRAM_s *m_, u8 flags, u16 cx_begin, u16 cy_begin/*, u16 cols, u16 rows*/)
{
u16 cy_end, cx_end;
//	cx_end = (0 == cols) ? m_->ws_col : min(cx_begin + cols, m_->ws_col); // PENDING:
	cx_end = m_->ws_col;
//	cy_end = (0 == rows) ? m_->ws_row : min(cy_begin + rows, m_->ws_row); // PENDING:
	cy_end = m_->ws_row;
u16 cx, cy;
	for (cy = cy_end; cy_begin < cy;) {
		--cy;
u32 bpr; // bits per row
		// upper chain of cells
		bpr = ALIGN(bitsizeof(m_->VLINKs[0]), m_->ws_col);
		lsb0_bitset (m_->VLINKs, cy * bpr + cx_begin, 0, cx_end - cx_begin);
		// lefter chain of cells
		bpr = ALIGN(bitsizeof(m_->HLINKs[0]), m_->ws_col);
		lsb0_bitset (m_->HLINKs, cy * bpr + cx_begin, 0, cx_end - cx_begin);
	}
	// ID of cells
	for (cy = cy_begin; cy < cy_end; ++cy) {
u16 *img_id;
		img_id = &m_->IDs[cy * m_->ws_col] + cx_begin;
		for (--img_id, cx = cx_begin; cx < cx_end; ++cx) {
			if (0 == *++img_id)
				continue;
			if (! (IMAGE_KEEP & flags))
				GVRAM_image_destroy (m_, *img_id);
			*img_id = 0;
		}
	}
}

static void GVRAM_clear (GVRAM_s *m_, u32 bg_rgb24)
{
	GVRAM_erase_rect (m_, 0, 0, 0);
	GVRAM_set_dirty_rect (m_, 0, 0, 0, 0);
}

typedef struct vram_export {
	u16 ws_col, ws_row;
	u8 data[1];
} vram_export_s;

static void GVRAM_load (GVRAM_s *m_, const void *buf)
{
vram_export_s *p;
	p = (vram_export_s *)buf;
const u16 *ids; const u32 *vlinks, *hlinks;
	ids = (const u16 *)(p->data +0);
	vlinks = (const u32 *)(ids + p->ws_col * p->ws_row);
	hlinks = (const u32 *)(vlinks + HIDIV(p->ws_col, 32) * p->ws_row);
u16 cols, rows;
	cols = min(p->ws_col, m_->ws_col), rows = min(p->ws_row, m_->ws_row);
u16 cy;
	for (cy = 0; cy < rows; ++cy) {
unsigned src_alpr, dst_alpr; // array length per row
		src_alpr = ALIGN(p->ws_col, bitsizeof(m_->VLINKs[0]));
		dst_alpr = ALIGN(m_->ws_col, bitsizeof(m_->VLINKs[0]));
		lsb0_bitmove (m_->VLINKs, cy * dst_alpr, vlinks, cy * src_alpr, cols);
		src_alpr = ALIGN(p->ws_col, bitsizeof(m_->HLINKs[0]));
		dst_alpr = ALIGN(m_->ws_col, bitsizeof(m_->HLINKs[0]));
		lsb0_bitmove (m_->HLINKs, cy * dst_alpr, hlinks, cy * src_alpr, cols);
		memmove (&m_->IDs[cy * m_->ws_col], &ids[cy * p->ws_col], cols * sizeof(m_->IDs[0]));
	}
	GVRAM_set_dirty_rect (m_, 0, 0, 0, 0);
}

static u32 GVRAM_save (GVRAM_s *m_, void *buf)
{
vram_export_s *p;
	p = (vram_export_s *)buf;
u32 cb_total, bpr; // byte per row
	cb_total = 0;
	// ID of cells
	bpr = sizeof(m_->IDs[0]) * m_->ws_col;
	if (p)
		memcpy (p->data +cb_total, m_->IDs, bpr * m_->ws_row);
	cb_total += bpr * m_->ws_row;
	// upper chain of cells
	bpr = ALIGN(bitsizeof(m_->VLINKs[0]), m_->ws_col) / 8;
	if (p)
		memcpy (p->data +cb_total, m_->VLINKs, bpr * m_->ws_row);
	cb_total += bpr * m_->ws_row;
	// lefter chain of cells
	bpr = ALIGN(bitsizeof(m_->HLINKs[0]), m_->ws_col) / 8;
	if (p)
		memcpy (p->data +cb_total, m_->HLINKs, bpr * m_->ws_row);
	cb_total += bpr * m_->ws_row;

	if (p) {
		GVRAM_erase_rect (m_, IMAGE_KEEP, 0, 0);
		p->ws_col = m_->ws_col;
		p->ws_row = m_->ws_row;
		GVRAM_set_dirty_rect (m_, 0, 0, 0, 0);
	}
	cb_total += offsetof(vram_export_s, data);
	return cb_total;
}

static u16 GVRAM_get_ws_col (GVRAM_s *m_) { return m_->ws_col; }
static u16 GVRAM_get_ws_row (GVRAM_s *m_) { return m_->ws_row; }
static u16 GVRAM_get_grid_width (GVRAM_s *m_) { return m_->lf_width; }
static u16 GVRAM_get_grid_height (GVRAM_s *m_) { return m_->lf_height; }

// TODO:
static void GVRAM_rolldown (GVRAM_s *m_, u16 n, u32 bg_rgb24) {}
static void GVRAM_resize (GVRAM_s *m_, u16 ws_col, u16 ws_row) {}
void GVRAM_reset_dirty (struct GVRAM *this_) {}

// PENDING: cannot implement without deciding to treate of HLINKs[] VLINKs[]
static void GVRAM_copy (GVRAM_s *m_, u16 x_src, u16 y_src, u16 width, u16 height, u16 x_dst, u16 y_dst) {}

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
static void GVRAM_write_text (GVRAM_s *m_, u16 cx, u16 cy, u16 width, const u32 *utf21s, const u64 *rgb24attr4s, u64 rgb24attr4s_mask) {}
static void GVRAM_read_text (GVRAM_s *m_, u16 cx, u16 cy, u32 mask, u32 *utf21s, u64 *rgb24attr4s) {}
#endif //def __cplusplus

static struct VRAM_i o_VRAM_i = {
	.write_image_start = GVRAM_write_start,
	.write_image       = GVRAM_write,
	.clear             = GVRAM_clear,
	.copy              = GVRAM_copy,
	.rollup            = GVRAM_rollup,
	.rolldown          = GVRAM_rolldown,
	.set_grid          = GVRAM_set_grid,
	.resize            = GVRAM_resize,

	.dirty_lock        = GVRAM_dirty_lock,
	.check_transparent = GVRAM_check_transparent,
	.read_image        = GVRAM_read,
	.dirty_unlock      = GVRAM_dirty_unlock,
	.set_dirty_rect    = GVRAM_set_dirty_rect,

	.save              = GVRAM_save,
	.load              = GVRAM_load,

	.get_ws_col        = GVRAM_get_ws_col,
	.get_ws_row        = GVRAM_get_ws_row,
	.get_grid_width    = GVRAM_get_grid_width,
	.get_grid_height   = GVRAM_get_grid_height,

#ifdef __cplusplus
	.write_text        = GVRAM_write_text,
	.read_text         = GVRAM_read_text,
#endif //def __cplusplus
};
struct VRAM_i *GVRAM_query_VRAM_i (struct GVRAM *this_) { return &o_VRAM_i; }
