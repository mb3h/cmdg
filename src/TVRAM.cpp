
#include <stdint.h>
#include <wchar.h> // wchar_t
struct TVRAM_;
#define VRAM_s struct TVRAM_
#include "TVRAM.hpp"

#include "lock.hpp"
typedef struct lock lock_s;
#include "core/lsb0_bit_helper.h"
#include "unicode.h"
#include "portab.h"
#include "assert.h"

#include <stddef.h> // offsetof
#include <stdbool.h>
#include <stdlib.h> // exit
#include <memory.h>
#include <alloca.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define bitsizeof(x) (unsigned)(sizeof(x) * 8)

#define HIDIV(x,a) (((x) +(a) -1)/(a))
#define HIALIGN(a,x) (HIDIV(x,a)*(a))
#define LOALIGN(a,x) ((x)/(a)*(a))
#define ALIGN HIALIGN

///////////////////////////////////////////////////////////////////////////////
// private

typedef struct TVRAM_ {
	u32 *DIRTY21s;
	u32 *UTF21s;
	u64 *RGB24ATTR1s;
	u16 ws_col, ws_row;
#ifdef __x86_64__
	u8 padding_28[4];
#endif
#ifdef READ_SHELL_THREAD
	lock_s thread_lock; // must be last member. cf)_reset()
#endif
} TVRAM_s;

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
// ctor/dtor

static void TVRAM_reset (TVRAM_s *m_)
{
#ifndef __cplusplus
# ifdef READ_SHELL_THREAD
	memset (m_, 0, offsetof(TVRAM_s, thread_lock));
# else
	memset (m_, 0, sizeof(TVRAM_s));
# endif
#else //def __cplusplus
	m_->ws_col = m_->ws_row = 0;
	m_->DIRTY21s    = NULL;
	m_->UTF21s      = NULL;
	m_->RGB24ATTR1s = NULL;
#endif // __cplusplus
}

void TVRAM_release (struct TVRAM *this_)
{
TVRAM_s *m_;
	m_ = (TVRAM_s *)this_;
_lock_cleanup()
	if (m_->DIRTY21s)
		free (m_->DIRTY21s);
	if (m_->UTF21s)
		free (m_->UTF21s);
	if (m_->RGB24ATTR1s)
		free (m_->RGB24ATTR1s);
	TVRAM_reset (m_);
}

#ifdef __cplusplus
void TVRAM_dtor (struct TVRAM *this_)
{
}
#endif // __cplusplus

void TVRAM_ctor (struct TVRAM *this_, unsigned cb, u16 ws_col, u16 ws_row)
{
#ifndef __cplusplus
BUGc(sizeof(TVRAM_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(TVRAM_s), cb)
#endif //ndef __cplusplus
TVRAM_s *m_;
	m_ = (TVRAM_s *)this_;
	TVRAM_reset (m_);
_lock_warmup()
	m_->DIRTY21s    = (u32 *)calloc (ALIGN(bitsizeof(m_->DIRTY21s[0]), ws_col) / 8 * ws_row, 1);
	m_->UTF21s      = (u32 *)calloc (ws_col * ws_row, sizeof(m_->UTF21s     [0]));
	m_->RGB24ATTR1s = (u64 *)calloc (ws_col * ws_row, sizeof(m_->RGB24ATTR1s[0]));
ASSERTE(m_->DIRTY21s && m_->UTF21s && m_->RGB24ATTR1s)
	m_->ws_col = ws_col;
	m_->ws_row = ws_row;
}

///////////////////////////////////////////////////////////////////////////////
// DIRTY

// (*1) (right)MSB ... LSB(left)
static u32 *TVRAM_dirty_ref (TVRAM_s *m_, u16 x, u16 y, u8 *bpos)
{
	if (! (x < m_->ws_col && y < m_->ws_row))
		return NULL;
	if (bpos)
		*bpos = (u8)(x % bitsizeof(m_->DIRTY21s[0])); // (*1)
unsigned bpr; // bit per row
	bpr = ALIGN(bitsizeof(m_->DIRTY21s[0]), m_->ws_col);
	return &m_->DIRTY21s[(y * bpr + x) / bitsizeof(m_->DIRTY21s[0])];
}

// TODO: apply 'rows'
static void TVRAM_set_dirty_rect (TVRAM_s *m_, u16 x, u16 y, u16 cols, u16 rows)
{
	x = min(x, m_->ws_col);
	y = min(y, m_->ws_row);
u16 x_end, y_end;
	x_end = (0 == cols) ? m_->ws_col : min(x + cols, m_->ws_col);
	y_end = (0 == rows) ? m_->ws_row : min(y + rows, m_->ws_row);
	for (; y < y_end; ++y) {
u32 *p;
		p = TVRAM_dirty_ref (m_, 0, y, NULL);
		lsb0_bitset (p, x, 1, x_end - x);
	}
}

void TVRAM_reset_dirty (struct TVRAM *this_)
{
TVRAM_s *m_;
	m_ = (TVRAM_s *)this_;
	memset (m_->DIRTY21s, 0, ALIGN(bitsizeof(m_->DIRTY21s[0]), m_->ws_col) / 8 * m_->ws_row);
}

///////////////////////////////////////////////////////////////////////////////
// interface

static bool mask2width (u16 x, u32 *mask, u16 *cols)
{
u8 tmp[sizeof(*mask)];
	store_le32 (tmp, *mask);
	*cols = (u16)(1 + lsb0_bitrchr (tmp, 1, bitsizeof(*mask)));
	if (! (x % bitsizeof(*mask) + *cols <= bitsizeof(*mask)))
		return false; // alignment over (illegal specification)
	*mask <<= x % bitsizeof(*mask);
	return true;
}

static bool rgb24attr1s_bgcl_equals (u64 rgb24attr1s, u32 bg_rgb24)
{
	if (ATTR_REV_MASK & rgb24attr1s)
		return false;
	if (ATTR_BG_MASK & (ATTR_BG(bg_rgb24) ^ rgb24attr1s))
		return false;
	return true;
}

// PENDING: (*2) support bg colour not RGB(0,0,0)
static u32 TVRAM_check_transparent (TVRAM_s *m_, u16 x, u16 y, u32 mask)
{
	if (0 == mask)
		return 0;
unsigned bitwidth; u8 tmp[sizeof(mask)];
	store_le32 (tmp, mask);
	bitwidth = 1 + lsb0_bitrchr (tmp, 1, bitsizeof(mask));
u32 retval; u16 x0;
	retval = 0, x0 = x, x = min(x + bitwidth, m_->ws_col);
	while (x0 < x) {
		--x, retval <<= 1;
		if (! (1 & mask >> x - x0))
			continue;
u32 u;
		if (u = m_->UTF21s[y * m_->ws_col + x])
			continue;
u64 a;
		a = m_->RGB24ATTR1s[y * m_->ws_col + x];
		if (!rgb24attr1s_bgcl_equals (a, 0)) // (*2)
			continue;
		retval |= 1;
	}
	return retval;
}

static u32 TVRAM_dirty_lock (TVRAM_s *m_, u16 x, u16 y, u32 mask)
{
	if (0 == mask)
		mask = (u32)-1;
_lock()
u32 *dirty; u8 bpos;
	dirty = TVRAM_dirty_ref (m_, x, y, &bpos);
	if (0 < bpos && mask & (u32)-1 << bitsizeof(m_->DIRTY21s[0]) - bpos)
		return 0; // refuse ladder of boundary
	return mask & *dirty >> bpos;
}

static void TVRAM_read (VRAM_s *m_, u16 x, u16 y, u32 mask, u32 *utf21s, u64 *rgb24attr4s)
{
	if (! (y < m_->ws_row))
		return;
u16 cols;
	if (! (true == mask2width (x, &mask, &cols)))
		return;
unsigned n;
	n = y * m_->ws_col +x;
unsigned i;
	for (i = 0; i < cols; ++i) {
		if (0 == (1 & mask >> i + x % bitsizeof(m_->DIRTY21s[0]))) {
			continue;
		}
u32 u;
u64 a;
		u = m_->UTF21s[n +i], a = m_->RGB24ATTR1s[n +i];
		if (utf21s)
			utf21s[i] = u;
		if (rgb24attr4s)
			rgb24attr4s[i] = a;
	}
}
static void TVRAM_dirty_unlock (VRAM_s *m_, u16 x, u16 y, u32 mask)
{
u8 bpos;
u32 *dirty;
	dirty = TVRAM_dirty_ref (m_, x, y, &bpos);
ASSERTE(0 == bpos || 0 == (mask & (u32)-1 << bitsizeof(m_->DIRTY21s[0]) - bpos))
	*dirty &= ~(mask >> bpos);
_unlock()
}

static void TVRAM_erase_rect (TVRAM_s *m_, u32 bg_rgb24, u16 x, u16 y, u16 cols, u16 rows)
{
u32 *u; u64 *a;
	u = m_->UTF21s, a = m_->RGB24ATTR1s;
u16 x_end, y_end, x_begin;
	x_end = (0 == cols) ? m_->ws_col : min(x + cols, m_->ws_col), x_begin = x;
	y_end = (0 == rows) ? m_->ws_row : min(y + rows, m_->ws_row);
	for (; y < y_end; ++y) {
u32 *dirty;
		dirty = TVRAM_dirty_ref (m_, LOALIGN(bitsizeof(m_->DIRTY21s[0]), x), y, NULL);
unsigned wide_erase;
		wide_erase = 0;
u16 x;
		for (x = x_begin; x < x_end; ++x, ++u, ++a) {
			if (1 == wide_erase)
				wide_erase = 0;
			else if (*u) {
				if (2 == wcwidth_bugfix (utf8to16 (*u)) && x +1 < m_->ws_col)
					wide_erase = 1;
			}
			*u = 0, *a = ATTR_BG(bg_rgb24);
			*dirty |= 1 << x % bitsizeof(m_->DIRTY21s[0]);
			if (0 == (x +1) % bitsizeof(m_->DIRTY21s[0]))
				++dirty;
		}
	}
}

static void TVRAM_clear (TVRAM_s *m_, u32 bg_rgb24)
{
	TVRAM_erase_rect (m_, bg_rgb24, 0, 0, 0, 0);
}

static void TVRAM_write (TVRAM_s *m_, u16 x, u16 y, u16 cols, const u32 *utf21s, const u64 *rgb24attr4s, u64 rgb24attr4s_mask)
{
	if (! (y < m_->ws_row))
		return;
unsigned n, i;
	n = y * m_->ws_col + x;
_lock()
	for (i = 0; i < cols && x +i < m_->ws_col; ++i) {
u32 u;
		if (utf21s)
			m_->UTF21s[n +i] = (' ' == (u = utf21s[i]) || 0xe38080 == u) ? 0 : u;
		if (rgb24attr4s)
			if (0 == rgb24attr4s_mask)
				m_->RGB24ATTR1s[n +i] = rgb24attr4s[i];
			else {
u64 mix, old;
				mix = rgb24attr4s[i] ^ (old = m_->RGB24ATTR1s[n +i]);
				mix &= rgb24attr4s_mask;
				m_->RGB24ATTR1s[n +i] = mix ^ old;
			}
u8 bpos;
u32 *dirty;
		dirty = TVRAM_dirty_ref (m_, x +i, y, &bpos);
		*dirty |= 1 << bpos;
	}
_unlock()
}

static void TVRAM_copy (TVRAM_s *m_, u16 x_src, u16 y_src, u16 cols, u16 rows, u16 x_dst, u16 y_dst)
{
	if (! (y_src < m_->ws_row && x_src < m_->ws_col && x_dst < m_->ws_col && y_dst < m_->ws_row))
		return;
	if (x_src == x_dst && y_src == y_dst)
		return;
	cols  = min(min(x_src + cols , m_->ws_col) - x_src,
	            min(x_dst + cols , m_->ws_col) - x_dst);
	rows = min(min(y_src + rows, m_->ws_col) - y_src,
	           min(y_dst + rows, m_->ws_col) - y_dst);
	if (! (0 < cols && 0 < rows))
		return;
	if (y_src < y_dst)
		y_src += rows -1, y_dst += rows -1;
u16 i;
	for (i = 0; i < rows; ++i) {
u16 n_src, n_dst;
		n_dst = y_dst * m_->ws_col + x_dst, n_src = y_src * m_->ws_col + x_src;
		memmove (&m_->UTF21s     [n_dst], &m_->UTF21s     [n_src], cols * sizeof(m_->UTF21s     [0]));
		memmove (&m_->RGB24ATTR1s[n_dst], &m_->RGB24ATTR1s[n_src], cols * sizeof(m_->RGB24ATTR1s[0]));
		TVRAM_set_dirty_rect (m_, x_dst, y_dst, cols, 1);
		if (y_src < y_dst)
			--y_src, --y_dst;
		else
			++y_dst, ++y_src;
	}
}

static void TVRAM_rollup (TVRAM_s *m_, u16 n, u32 bg_rgb24)
{
u32 *utf21s; u64 *rgb24attr4s;
	utf21s = (u32 *)alloca (m_->ws_col * sizeof(u32));
	rgb24attr4s = (u64 *)alloca (m_->ws_col * sizeof(u64));
u16 i;
	for (i = 0; i < m_->ws_col; ++i)
		utf21s[i] = 0, rgb24attr4s[i] = ATTR_BG(bg_rgb24);
	while (0 < n--) {
		TVRAM_copy (m_, 0, 1, m_->ws_col, m_->ws_row -1, 0, 0);
		TVRAM_write (m_, 0, m_->ws_row -1, m_->ws_col, utf21s, rgb24attr4s, 0);
	}
}

static void TVRAM_rolldown (TVRAM_s *m_, u16 n, u32 bg_rgb24)
{
u32 *utf21s; u64 *rgb24attr4s;
	utf21s = (u32 *)alloca (m_->ws_col * sizeof(u32));
	rgb24attr4s = (u64 *)alloca (m_->ws_col * sizeof(u64));
u16 i;
	for (i = 0; i < m_->ws_col; ++i)
		utf21s[i] = 0, rgb24attr4s[i] = ATTR_BG(bg_rgb24);
	while (0 < n--) {
		TVRAM_copy (m_, 0, 0, m_->ws_col, m_->ws_row -1, 0, 1);
		TVRAM_write (m_, 0, 0, m_->ws_col, utf21s, rgb24attr4s, 0);
	}
}

typedef struct vram_export {
	u16 ws_col, ws_row;
	u8 data[1];
} vram_export_s;

static void TVRAM_load (TVRAM_s *m_, const void *buf)
{
vram_export_s *p;
	p = (vram_export_s *)buf;
unsigned elems;
	elems = p->ws_col * p->ws_row;
const u32 *utf21s;
const u64 *rgb24attr1s;
	utf21s = (const u32 *)(p->data +0);
	rgb24attr1s = (const u64 *)(utf21s +elems);
u16 cols;
	cols = min(p->ws_col, m_->ws_col);
u16 y;
	for (y = 0; y < min(p->ws_row, m_->ws_row); ++y) {
		memcpy (m_->UTF21s      + y * m_->ws_col, utf21s      + y * p->ws_col, cols * sizeof(m_->UTF21s     [0]));
		memcpy (m_->RGB24ATTR1s + y * m_->ws_col, rgb24attr1s + y * p->ws_col, cols * sizeof(m_->RGB24ATTR1s[0]));
		TVRAM_set_dirty_rect (m_, 0, y, cols, 1);
	}
}

static u32 TVRAM_save (TVRAM_s *m_, void *buf)
{
vram_export_s *p;
	p = (vram_export_s *)buf;
u32 elems;
	elems = m_->ws_col * m_->ws_row;
u32 cb;
	cb = 0;
	if (p)
		memcpy (p->data +cb, m_->UTF21s, sizeof(m_->UTF21s[0]) * elems);
	cb += sizeof(m_->UTF21s[0]) * elems;
	if (p)
		memcpy (p->data +cb, m_->RGB24ATTR1s, sizeof(m_->RGB24ATTR1s[0]) * elems);
	cb += sizeof(m_->RGB24ATTR1s[0]) * elems;
	if (p) {
		p->ws_col = m_->ws_col;
		p->ws_row = m_->ws_row;
	}
	cb += offsetof(vram_export_s, data);
	return cb;
}

static u16 TVRAM_get_ws_col (TVRAM_s *m_) { return m_->ws_col; }
static u16 TVRAM_get_ws_row (TVRAM_s *m_) { return m_->ws_row; }

// TODO:
static void TVRAM_resize (TVRAM_s *m_, u16 ws_col, u16 ws_row) {}

#ifdef __cplusplus
static u16 TVRAM_get_grid_width (TVRAM_s *m_) { return 0; }
static u16 TVRAM_get_grid_height (TVRAM_s *m_) { return 0; }
static void TVRAM_write_image (TVRAM_s *m_, const void *rawdata, unsigned cb) {}
static u32 TVRAM_read_image (TVRAM_s *m_, u16 x, u16 y, enum pixel_format pf, void *data, u32 cb) { return 0: }
#endif //def __cplusplus

///////////////////////////////////////////////////////////////////////////////

static struct VRAM_i o_VRAM_i = {
	.write_text        = TVRAM_write,
	.clear             = TVRAM_clear,
	.copy              = TVRAM_copy,
	.rollup            = TVRAM_rollup,
	.rolldown          = TVRAM_rolldown,
	.resize            = TVRAM_resize,

	.dirty_lock        = TVRAM_dirty_lock,
	.check_transparent = TVRAM_check_transparent,
	.read_text         = TVRAM_read,
	.dirty_unlock      = TVRAM_dirty_unlock,
	.set_dirty_rect    = TVRAM_set_dirty_rect,

	.save              = TVRAM_save,
	.load              = TVRAM_load,

	.get_ws_col        = TVRAM_get_ws_col,
	.get_ws_row        = TVRAM_get_ws_row,

#ifdef __cplusplus
	.get_grid_width    = TVRAM_get_grid_width,
	.get_grid_height   = TVRAM_get_grid_height,
	.write_image       = TVRAM_write_image,
	.read_image        = TVRAM_read_image,
#endif //def __cplusplus
};
struct VRAM_i *TVRAM_query_VRAM_i (struct TVRAM *this_) { return &o_VRAM_i; }
