//#include <features.h>
//#include <endian.h>
#include <stdint.h>

#include "vt100_parser.h"
#include "image_backlog.h"
#include "bpp1.h"
#include "bpp8.h"
#include "bpp16.h"
#include "bpp32.h"
#include "helper/pow2_ge.h"

#include <gtk/gtk.h>
#include "on_ptout.h"
#include "gtk2_font.h"
#ifdef FALSE
# undef FALSE // avoid bad effect from glib version
#endif
#ifdef TRUE
# undef TRUE // avoid bad effect from glib version
#endif

#include <wchar.h>
#include "unicode.h" // wcwidth_bugfix

#include <stdbool.h>
#include <stdio.h> // sprintf
#include <stdlib.h> // exit
#include <memory.h>
#include <malloc.h>
typedef uint32_t u32, u21, u24;
typedef  uint8_t u8, u1, u2, u3;
typedef uint16_t u16, u9, u10, u12, u14;
typedef uint64_t u64;
typedef  int16_t s10;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define arraycountof(x) (sizeof(x)/(sizeof((x)[0])))

#define swap(a, b, type__) do { type__ t__; t__ = a, a = b, b = t__; } while (0)
#define RGB24(r,g,b) (0xff0000 & (r) << 16 | 0xff00 & (g) << 8 | 0xff & (b))
#define ALIGN(a,x) ((x)+(a)-1 - ((x)+(a)-1)%(a))
#define RING_ADD(law,a,b) /* MUST (a < law && b < law) */ \
	((a) < (law)-(b) ? (a)+(b) : (a)+(b)-(law))
#define RING_SUB(law,a,b) /* MUST (a < law && b < law) */ \
	((a) < (b) ? (a)-(b)+(law) : (a)-(b))

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
#define no_memory   assert
#define unexpected  assert

///////////////////////////////////////////////////////////////
// endian portability

static void store_le16 (void *dst, u16 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
#else
	val =   0xff & val >> 8
	    | 0xff00 & val << 8;
#endif
	*(u16 *)dst = val;
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

///////////////////////////////////////////////////////////////
// singlton memory

typedef union {
	struct {
		u8 fgcl3     :3;
		u8 bold      :1;
		u8 bgcl3     :3;
		u8 sgr2_used :1;
	};
	u8 packed;
} SGR1;
typedef union {
	struct {
		u8 underline     :1;
		u8 fgcl_outofVGA :1;
		u8 bgcl_outofVGA :1;
		u8 padding       :5;
	};
	u8 packed;
} SGR2;

typedef struct {
	u8 bold      :1;
	u8 underline :1;
	u8 padding   :6;
} SGR;

typedef struct {
	void (*on_changed) (u12 px, u12 py,
	                    u12 width, u12 height);
	struct {
		u32 need_XXXXXX       :1;
		u32 need_XXXX         :1;
		u32 need_XX           :1;
		u32 error             :1;
		u32 reserved          :12;
		u32 b20to6_of_utf21   :15;
		u32 reserved2         :1;
	} utf8;
	struct {
		u32 fgcl3or24     :24;
		u32 fgcl_outofVGA :1;
		u32 padding1      :3;
		u32 bold          :1;
		u32 underline     :1;
		u32 italic        :1;
		u32 strikethrough :1;
		u32 bgcl3or24     :24;
		u32 bgcl_outofVGA :1;
		u32 padding2      :6;
		u32 reverse       :1;
	} sgr;
	u32 acceptLFasCRLF            :1;
	u32 is_parsing_escseq         :1;
	u32 is_parsing_embedded_image :1;
	u32 is_just_one_after_esc     :1;
	u32 is_extscrn                :1;
	u32 is_csrshow                :1;
	u32 reserved                  :26;
	u9  ws_row;
	u10 ws_col;
#define PT_Y_MAX ((1U << 9) -1)
	u9  cursor_y; // 1920 (display) / 16(font) = 7bits (+margin x4 = 2bits)
	u10 cursor_x; // 1920 (display) / 8 (font) = 8bits (+margin x4 = 2bits)
	u16 bottom_margin;
	u9  backup_stdscrn_cursor_y;
	u10 backup_stdscrn_cursor_x;
	u32 stdscrn_max;
	void *stdscrn;
	struct {
		bpp1 showmask;
		bpp8 b7to0;
		bpp16 b20to8;
		bpp8 sgr;
		bpp8 sgr2;
		bpp21 fgcl3to24;
		bpp21 bgcl3to24;
	} backlog;
	struct {
		bpp1 showmask;
		bpp8 b7to0;
		bpp16 b20to8;
		bpp8 sgr;
		bpp8 sgr2;
		bpp21 fgcl3to24;
		bpp21 bgcl3to24;
	} frontend;
	u16 backlog_y_end;
	u16 backlog_y_of_pt_top;
	u9 frontend_y_of_pt_top;
	void *bpp24_clip;
	unsigned bpp24_clip_max;
#ifdef __i386__
//	u8 align4_padding[];
#else //def __x86_64__
//	u8 align8_padding[2];
#endif
} on_ptout_s;
static on_ptout_s s_singlton = {0};

//#define BACKLOG_Y_MAX ((1U << 16) -1)
#define FRONTEND_Y_SIZE ((1U << 9) -1) // might round up
#define FRONTEND_Y_RINGLAW (FRONTEND_Y_SIZE +1)
static u9 _frontend_y_from (u9 pt_y)
{
on_ptout_s *m_ = &s_singlton;

flow_bug(m_->frontend_y_of_pt_top < FRONTEND_Y_RINGLAW)

u9 frontend_y
	= RING_ADD(FRONTEND_Y_RINGLAW,
	           m_->frontend_y_of_pt_top, pt_y);
	return frontend_y;
}
static u16 _backlog_y_from (u9 pt_y)
{
on_ptout_s *m_ = &s_singlton;

	return m_->backlog_y_of_pt_top +pt_y;
}

static s10 _pt_y_from (u16 backlog_y)
{
on_ptout_s *m_ = &s_singlton;

u1 out_of_range
	= (backlog_y < m_->backlog_y_of_pt_top
	|| m_->backlog_y_of_pt_top +m_->ws_row -1 < backlog_y)
	? TRUE : FALSE;
	if (out_of_range)
		return -1;

u9 pt_y
	= backlog_y -m_->backlog_y_of_pt_top;
	return pt_y;
}

///////////////////////////////////////////////////////////////
// core manipulating

static u9 _set_frontend_size (u10 new_ws_col, u9 new_ws_row_unsafe)
{
on_ptout_s *m_ = &s_singlton;

u9 new_ws_row
	= min(new_ws_row_unsafe, FRONTEND_Y_SIZE); // safety

u1 while_scrolling // NOTE: probably logic not fit supposed design.
	= (m_->cursor_y < m_->ws_row) ? FALSE : TRUE;
u9 pt_y;
	// extend screen never uses backlog
	if (m_->is_extscrn)
		;
	// adding height has to fit cursor-Y not on back-scrolling
	else if (m_->ws_row < new_ws_row
	      && !while_scrolling) {
u9 delta_y
		= new_ws_row -m_->ws_row;
		delta_y = min(delta_y,
		              m_->backlog_y_of_pt_top -m_->backlog_y_end);
		m_->frontend_y_of_pt_top = RING_SUB(FRONTEND_Y_RINGLAW,
		                                    m_->frontend_y_of_pt_top,
		                                    delta_y);
		m_->backlog_y_of_pt_top -= delta_y;
		m_->cursor_y += delta_y;
	}
	// shrinking height has to fit cursor-Y not on back-scrolling
	else if (new_ws_row < m_->ws_row
	      && !while_scrolling) {
u9 new_cursor_y
		= min(m_->cursor_y, new_ws_row -1);
u9 delta_y
		= m_->cursor_y -new_cursor_y;
		m_->frontend_y_of_pt_top = RING_ADD(FRONTEND_Y_RINGLAW,
		                                    m_->frontend_y_of_pt_top,
		                                    delta_y);
		m_->backlog_y_of_pt_top += delta_y;
		m_->cursor_y -= delta_y;
	}

	m_->ws_col = new_ws_col;
	m_->ws_row = new_ws_row;

	bpp1_resize (&m_->frontend.showmask,
	             new_ws_col, FRONTEND_Y_RINGLAW,
	             0);
	bpp8_resize (&m_->frontend.sgr2,
	             new_ws_col, FRONTEND_Y_RINGLAW,
	             0);
	bpp21_resize (&m_->frontend.bgcl3to24,
	              new_ws_col, FRONTEND_Y_RINGLAW,
	              0x000000);
	bpp21_resize (&m_->frontend.fgcl3to24,
	              new_ws_col, FRONTEND_Y_RINGLAW,
	              0x000000);
	bpp8_resize (&m_->frontend.sgr,
	             new_ws_col, FRONTEND_Y_RINGLAW,
	             0);
	bpp8_resize (&m_->frontend.b7to0,
	             new_ws_col, FRONTEND_Y_RINGLAW,
	             0x00);
	bpp16_resize (&m_->frontend.b20to8,
	              new_ws_col, FRONTEND_Y_RINGLAW,
	              0x0000);

#if 1 // TODO: move to supposed position _ctor()
	bpp1_resize (&m_->backlog.showmask,
	             new_ws_col, FRONTEND_Y_RINGLAW,
	             0);
	bpp8_resize (&m_->backlog.sgr2,
	             new_ws_col, FRONTEND_Y_RINGLAW,
	             0);
	bpp21_resize (&m_->backlog.bgcl3to24,
	              new_ws_col, FRONTEND_Y_RINGLAW,
	              0x000000);
	bpp21_resize (&m_->backlog.fgcl3to24,
	              new_ws_col, FRONTEND_Y_RINGLAW,
	              0x000000);
	bpp8_resize (&m_->backlog.sgr,
	             new_ws_col, FRONTEND_Y_RINGLAW,
	             0);
	bpp8_resize (&m_->backlog.b7to0,
	             new_ws_col, FRONTEND_Y_RINGLAW,
	             0x00);
	bpp16_resize (&m_->backlog.b20to8,
	              new_ws_col, FRONTEND_Y_RINGLAW,
	              0x0000);
#endif

	return new_ws_row;
}

// NOTE: temporary, same as qterminal-1.4.0
static const u24 o_rgb3to24[2][8] = {{
	0x000000, 0xb21818, 0x18b218, 0xb26818,
	0x1818b2, 0xb218b2, 0x18b2b2, 0xb2b2b2,
}, {
	0x000000, 0xff5454, 0x54ff54, 0xffff54,
	0x5454ff, 0xff54ff, 0x54ffff, 0xffffff,
}};

static u24 _rgb24_from (u24 rgb3or24, u1 outofVGA, u1 bold)
{
on_ptout_s *m_ = &s_singlton;

	if (outofVGA)
		return rgb3or24;
u3 rgb3
	= (u3)min(rgb3or24, 7);
u24 rgb24
	= o_rgb3to24[bold][rgb3];
	return rgb24;
}

static u21 _get_text (u10 x, u16 backlog_y,
                      u24 *fgcl24, u24 *bgcl24, SGR *sgr)
{
on_ptout_s *m_ = &s_singlton;

s10 pt_y
	= _pt_y_from (backlog_y);
unexpected(! (-1 == pt_y))
u9 frontend_y
	= _frontend_y_from ((u9)pt_y);
u21 unicode
	=  bpp8_get (&m_->frontend.b7to0 , x, frontend_y)
	| bpp16_get (&m_->frontend.b20to8, x, frontend_y) << 8;
	if (NULL == fgcl24 && NULL == bgcl24 && NULL == sgr)
		return unicode;

SGR1 sgr_; SGR2 sgr2 = {0};
	sgr_.packed = bpp8_get (&m_->frontend.sgr, x, frontend_y);
	if (sgr_.sgr2_used)
		sgr2.packed = bpp8_get (&m_->frontend.sgr2, x, frontend_y);
	if (sgr) {
		sgr->bold      = sgr_.bold;
		sgr->underline = sgr2.underline;
	}
u24 fgcl24_, bgcl24_;
	if (!sgr2.fgcl_outofVGA)
		fgcl24_ = o_rgb3to24[sgr_.bold][sgr_.fgcl3];
	else {
		fgcl24_ = 0x800000 & sgr_.fgcl3 << (23 -0) // R.b7
		        | 0x008000 & sgr_.fgcl3 << (15 -1) // G.b7
		        | 0x000080 & sgr_.fgcl3 << ( 7 -2); // B.b7
u21 fgcl3to24
		= bpp21_get (&m_->frontend.fgcl3to24, x, frontend_y);
		fgcl24_ |= 0x7f0000 & fgcl3to24 << 2 // R.b6-b0
		        |  0x007f00 & fgcl3to24 << 1 // G.b6-b0
		        |  0x00007f & fgcl3to24; // B.b6-b0
	}
	if (!sgr2.bgcl_outofVGA)
		bgcl24_ = o_rgb3to24[0][sgr_.bgcl3];
	else {
		bgcl24_ = 0x800000 & sgr_.bgcl3 << (23 -0) // R.b7
		        | 0x008000 & sgr_.bgcl3 << (15 -1) // G.b7
		        | 0x000080 & sgr_.bgcl3 << ( 7 -2); // B.b7
u21 bgcl3to24
		= bpp21_get (&m_->frontend.bgcl3to24, x, frontend_y);
		bgcl24_ |= 0x7f0000 & bgcl3to24 << 2 // R.b6-b0
		        |  0x007f00 & bgcl3to24 << 1 // G.b6-b0
		        |  0x00007f & bgcl3to24; // B.b6-b0
	}
//	if (sgr_.reverse)
//		swap(sgr_.fgcl3, sgr_.bgcl3, u3);
	if (fgcl24)
		*fgcl24 = fgcl24_;
	if (bgcl24)
		*bgcl24 = bgcl24_;
	return unicode;
}

static void _set_text (u10 x, u9 pt_y, u21 unicode)
{
on_ptout_s *m_ = &s_singlton;

SGR2 sgr2 = {0}; // padding safety
	sgr2.underline = m_->sgr.underline;
SGR1 sgr = {0}; // padding safety
	sgr.sgr2_used = FALSE;
u21 fgcl3to24;
	if (!m_->sgr.fgcl_outofVGA)
		sgr.fgcl3 = m_->sgr.fgcl3or24;
	else {
		sgr2.fgcl_outofVGA = TRUE;
u24 rgb24 = m_->sgr.fgcl3or24;
		sgr.fgcl3 = 1 & rgb24 >> (23 -0) // R.b7
		          | 2 & rgb24 >> (15 -1) // G.b7
		          | 4 & rgb24 >> ( 7 -2); // B.b7
		fgcl3to24 = 0x1fc000 & rgb24 >> 2 // R.b6-b0
		          | 0x003f80 & rgb24 >> 1 // G.b6-b0
		          | 0x00007f & rgb24; // B.b6-b0
	}
u21 bgcl3to24;
	if (!m_->sgr.bgcl_outofVGA)
		sgr.bgcl3 = m_->sgr.bgcl3or24;
	else {
		sgr2.bgcl_outofVGA = TRUE;
u24 rgb24 = m_->sgr.bgcl3or24;
		sgr.bgcl3 = 1 & rgb24 >> (23 -0) // R.b7
		          | 2 & rgb24 >> (15 -1) // G.b7
		          | 4 & rgb24 >> ( 7 -2); // B.b7
		bgcl3to24 = 0x1fc000 & rgb24 >> 2 // R.b6-b0
		          | 0x003f80 & rgb24 >> 1 // G.b6-b0
		          | 0x00007f & rgb24; // B.b6-b0
	}
	sgr.bold = m_->sgr.bold;
	if (m_->sgr.reverse)
		swap (sgr.fgcl3, sgr.bgcl3, u3);
	if (sgr2.packed)
		sgr.sgr2_used = TRUE;

u9 frontend_y
	= _frontend_y_from (pt_y);
	bpp16_set (&m_->frontend.b20to8,
	           m_->cursor_x, frontend_y, 1, 1,
	           0x1fff & unicode >> 8);
	 bpp8_set (&m_->frontend.b7to0,
	           m_->cursor_x, frontend_y, 1, 1,
	           0xff & unicode);
	 bpp8_set (&m_->frontend.sgr,
	           m_->cursor_x, frontend_y, 1, 1,
	           sgr.packed);
	if (sgr2.packed)
		bpp8_set (&m_->frontend.sgr2,
		          m_->cursor_x, frontend_y, 1, 1,
		          sgr2.packed);
	if (sgr2.fgcl_outofVGA)
		bpp21_set (&m_->frontend.fgcl3to24,
		           m_->cursor_x, frontend_y, 1, 1,
		           fgcl3to24);
	if (sgr2.bgcl_outofVGA)
		bpp21_set (&m_->frontend.bgcl3to24,
		           m_->cursor_x, frontend_y, 1, 1,
		           bgcl3to24);
}

static void _copy (u10 x, u9 pt_y, u10 width, u9 height,
                   u10 src_x, u9 src_pt_y)
{
on_ptout_s *m_ = &s_singlton;

	// safety
	width = min(    x +width, m_->ws_col) -    x;
	width = min(src_x +width, m_->ws_col) -src_x;
	height = min(    pt_y +height, m_->ws_row) -    pt_y;
	height = min(src_pt_y +height, m_->ws_row) -src_pt_y;

u9 frontend_y
	= _frontend_y_from (pt_y);
u9 src_frontend_y
	= _frontend_y_from (src_pt_y);
u9 rounded_up_period1
	= min(    frontend_y +height, FRONTEND_Y_RINGLAW)
	-         frontend_y;
u9 rounded_up_period2
	= min(src_frontend_y +height, FRONTEND_Y_RINGLAW)
	-     src_frontend_y;
	if (rounded_up_period2 < rounded_up_period1)
		swap(rounded_up_period1, rounded_up_period2, u9);
u9 sep[3]
	= { rounded_up_period1,
	    rounded_up_period2 -rounded_up_period1,
	    height -rounded_up_period2 };
unsigned i;
	for (i = 0; i < 3; ++i) {
		if (0 == sep[i])
			break;
		bpp16_copy (&m_->frontend.b20to8,
		            x, frontend_y, width, sep[i],
		            src_x, src_frontend_y);
		 bpp8_copy (&m_->frontend.b7to0,
		            x, frontend_y, width, sep[i],
		            src_x, src_frontend_y);
		 bpp8_copy (&m_->frontend.sgr,
		            x, frontend_y, width, sep[i],
		            src_x, src_frontend_y);
		bpp21_copy (&m_->frontend.fgcl3to24,
		            x, frontend_y, width, sep[i],
		            src_x, src_frontend_y);
		bpp21_copy (&m_->frontend.bgcl3to24,
		            x, frontend_y, width, sep[i],
		            src_x, src_frontend_y);
		 bpp8_copy (&m_->frontend.sgr2,
		            x, frontend_y, width, sep[i],
		            src_x, src_frontend_y);
		 bpp1_copy (&m_->frontend.showmask,
		            x, frontend_y, width, sep[i],
		            src_x, src_frontend_y);
		src_frontend_y = RING_ADD(FRONTEND_Y_RINGLAW, sep[i],
		                          src_frontend_y);
		    frontend_y = RING_ADD(FRONTEND_Y_RINGLAW, sep[i],
		                              frontend_y);
	}
}
static void _erase (u10 x, u9 pt_y, u10 width, u9 height)
{
on_ptout_s *m_ = &s_singlton;

u9 frontend_y
	= _frontend_y_from (pt_y);
u9 rounded_up_period
	= min(frontend_y +height, FRONTEND_Y_RINGLAW) - frontend_y;
u9 sep[2]
	= { rounded_up_period, height -rounded_up_period };
unsigned i;
	for (i = 0; i < 2; ++i) {
		if (0 == sep[i])
			break;
		bpp16_set (&m_->frontend.b20to8,
		           x, frontend_y, width, sep[i],
		           0x0000);
		 bpp8_set (&m_->frontend.b7to0,
		           x, frontend_y, width, sep[i],
		           ' ');
		 bpp8_set (&m_->frontend.sgr,
		           x, frontend_y, width, sep[i],
		           0);
		bpp21_set (&m_->frontend.fgcl3to24,
		           x, frontend_y, width, sep[i],
		           0);
		bpp21_set (&m_->frontend.bgcl3to24,
		           x, frontend_y, width, sep[i],
		           0);
		 bpp8_set (&m_->frontend.sgr2,
		           x, frontend_y, width, sep[i],
		           0);
		 bpp1_set (&m_->frontend.showmask,
		           x, frontend_y, width, sep[i],
		           0);
		frontend_y = RING_ADD(FRONTEND_Y_RINGLAW, sep[i],
		                      frontend_y);
	}
}

static bool _cursor_up (u9 n)
{
on_ptout_s *m_ = &s_singlton;

u1 is_rolled_down = FALSE;
	if (n < m_->cursor_y +1) {
		m_->cursor_y -= n;
		return is_rolled_down;
	}

	is_rolled_down = TRUE;
	n -= m_->cursor_y, m_->cursor_y = 0;
	_copy (0, n, m_->ws_col, m_->ws_row -n, 0, 0);
	_erase (0, 0, m_->ws_col, n);
	return is_rolled_down;
}

static void _rollup (u16 n_rollup)
{
on_ptout_s *m_ = &s_singlton;

	if (0 == n_rollup)
		return;
unexpected(1 == n_rollup)
	n_rollup = 1;

flow_bug(m_->backlog_y_end <= m_->backlog_y_of_pt_top)
u9 hided_frontend
	= (u9)(m_->backlog_y_of_pt_top -m_->backlog_y_end);
	
u9 expandable
	= FRONTEND_Y_SIZE -(hided_frontend +m_->ws_row);
u9 send_to_backlog
	= n_rollup -min(n_rollup, expandable);
	if (0 < send_to_backlog) {
		// TODO: _move_frontend_to_backlog ()
		m_->backlog_y_end += send_to_backlog;
u9 y_begin, y_end;
		y_begin = RING_SUB(FRONTEND_Y_RINGLAW,
		                   m_->frontend_y_of_pt_top, hided_frontend);
		y_end   = RING_ADD(FRONTEND_Y_RINGLAW,
		                   y_begin, send_to_backlog);
		if (y_end < y_begin) {
			bpp1_set (&m_->frontend.showmask,
			          0, 0, m_->ws_col, y_end,
			          0);
			y_end = FRONTEND_Y_RINGLAW;
		}
		bpp1_set (&m_->frontend.showmask,
		          0, y_begin,
		          m_->ws_col, y_end -y_begin,
		          0);
	}
	m_->frontend_y_of_pt_top = RING_ADD(FRONTEND_Y_RINGLAW,
	                                    m_->frontend_y_of_pt_top,
	                                    n_rollup);
	m_->backlog_y_of_pt_top += n_rollup;
}

static u1 _line_feed ()
{
on_ptout_s *m_ = &s_singlton;

u9 have_to = 1;
u9 cursor_y_max
	= m_->ws_row -1 -m_->bottom_margin;
flow_bug(m_->cursor_y <= cursor_y_max)
u9 able_to
	= cursor_y_max - m_->cursor_y;

u1 is_rolled_up = FALSE;
	m_->cursor_y += min(have_to, able_to);
	if (! (able_to < have_to))
		return is_rolled_up;

	is_rolled_up = TRUE;
u9 needed
	= have_to -able_to;
	if (m_->is_extscrn) {
		_copy (0, 0, m_->ws_col, cursor_y_max +1 -needed,
		       0, needed);
		_erase (0, cursor_y_max +1 -needed, m_->ws_col, needed);
		return is_rolled_up;
	}

	_rollup (have_to -able_to);
	return is_rolled_up;
}

static unsigned _save_frontend (void *dst_)
{
on_ptout_s *m_ = &s_singlton;

u8 *dst = (u8 *)dst_;
u9 frontend_y
	= _frontend_y_from (0);
unsigned used = 0;

	if (dst)
		store_le16 (dst +used, m_->ws_col);
	used += usizeof(u16);
	if (dst)
		store_le16 (dst +used, m_->ws_row);
	used += usizeof(u16);
	if (dst)
		store_le16 (dst +used, m_->frontend_y_of_pt_top);
	used += usizeof(u16);
u9 hided_frontend
	= m_->backlog_y_of_pt_top -m_->backlog_y_end;
	if (dst)
		store_le16 (dst +used, hided_frontend);
	used += usizeof(u16);

	if (dst)
		bpp16_read (&m_->frontend.b20to8,
		            0, 0, m_->ws_col, FRONTEND_Y_RINGLAW,
		            dst +used);
	used += usizeof(u16) * m_->ws_col * FRONTEND_Y_RINGLAW;
	if (dst)
		bpp8_read (&m_->frontend.b7to0,
		           0, 0, m_->ws_col, FRONTEND_Y_RINGLAW,
		           dst +used);
	used += m_->ws_col * FRONTEND_Y_RINGLAW;
	if (dst)
		bpp8_read (&m_->frontend.sgr,
		           0, 0, m_->ws_col, FRONTEND_Y_RINGLAW,
		           dst +used);
	used += m_->ws_col * FRONTEND_Y_RINGLAW;
	if (dst)
		bpp21_read (&m_->frontend.fgcl3to24,
		            0, 0, m_->ws_col, FRONTEND_Y_RINGLAW,
		            dst +used);
	used += usizeof(u21) * m_->ws_col * FRONTEND_Y_RINGLAW;
	if (dst)
		bpp21_read (&m_->frontend.bgcl3to24,
		            0, 0, m_->ws_col, FRONTEND_Y_RINGLAW,
		            dst +used);
	used += usizeof(u21) * m_->ws_col * FRONTEND_Y_RINGLAW;
	if (dst)
		bpp8_read (&m_->frontend.sgr2,
		           0, 0, m_->ws_col, FRONTEND_Y_RINGLAW,
		           dst +used);
	used += m_->ws_col * FRONTEND_Y_RINGLAW;
	if (dst)
		bpp1_read (&m_->frontend.showmask,
		           0, 0, m_->ws_col, FRONTEND_Y_RINGLAW,
		           dst +used);
	// TODO: (*1)
//	used += ALIGN(8, m_->ws_col) / 8 * FRONTEND_Y_RINGLAW;
	used +=          m_->ws_col      * FRONTEND_Y_RINGLAW;

	return used;
}

static void _load_frontend (const void *src_)
{
on_ptout_s *m_ = &s_singlton;

const u8 *src = (const u8 *)src_;
u9 frontend_y
	= _frontend_y_from (0);
unsigned used = 0;

u10 ws_col; u9 ws_row;
	ws_col = load_le16 (src +used);
	used += usizeof(u16);
	ws_row = load_le16 (src +used);
	used += usizeof(u16);

u9 frontend_y_of_pt_top
	= load_le16 (src +used);
	used += usizeof(u16);
u9 hided_frontend
	= load_le16 (src +used);
	used += usizeof(u16);

	bpp16_write (&m_->frontend.b20to8,
	             0, 0, ws_col, FRONTEND_Y_RINGLAW,
	             src +used);
	used += usizeof(u16) * ws_col * FRONTEND_Y_RINGLAW;
	bpp8_write (&m_->frontend.b7to0,
	            0, 0, ws_col, FRONTEND_Y_RINGLAW,
	            src +used);
	used += ws_col * FRONTEND_Y_RINGLAW;
	bpp8_write (&m_->frontend.sgr,
	            0, 0, ws_col, FRONTEND_Y_RINGLAW,
	            src +used);
	used += ws_col * FRONTEND_Y_RINGLAW;
	bpp21_write (&m_->frontend.fgcl3to24,
	             0, 0, ws_col, FRONTEND_Y_RINGLAW,
	             src +used);
	used += usizeof(u21) * ws_col * FRONTEND_Y_RINGLAW;
	bpp21_write (&m_->frontend.bgcl3to24,
	             0, 0, ws_col, FRONTEND_Y_RINGLAW,
	             src +used);
	used += usizeof(u21) * ws_col * FRONTEND_Y_RINGLAW;
	bpp8_write (&m_->frontend.sgr2,
	            0, 0, ws_col, FRONTEND_Y_RINGLAW,
	            src +used);
	used += ws_col * FRONTEND_Y_RINGLAW;
	bpp1_write (&m_->frontend.showmask,
	            0, 0, ws_col, FRONTEND_Y_RINGLAW,
	            src +used);
	// TODO: (*1)
//	used += ALIGN(8, ws_col) / 8 * FRONTEND_Y_RINGLAW;
	used +=          ws_col      * FRONTEND_Y_RINGLAW;

	if (ws_row < m_->ws_row)
		_erase (0, ws_row,
		        ws_col, m_->ws_row -ws_row);
	if (ws_col < m_->ws_col)
		_erase (ws_col, 0,
		        m_->ws_col -ws_col, max(ws_row, m_->ws_row));

	m_->frontend_y_of_pt_top = frontend_y_of_pt_top;
	m_->backlog_y_of_pt_top = m_->backlog_y_end +hided_frontend;
	// frontend size is shrinked than saved size?
	if (m_->ws_row < ws_row) {
u9 shrinking
		= ws_row - m_->ws_row;
		m_->frontend_y_of_pt_top = RING_ADD(FRONTEND_Y_RINGLAW,
		                                    m_->frontend_y_of_pt_top,
		                                    shrinking);
		; /* TODO: fit cursor position to new frontend size
		           if it's needed. */
		m_->backlog_y_of_pt_top += shrinking;
	}
}

///////////////////////////////////////////////////////////////
// CSI
static void _on_changed (u10 x, u9 pt_y, u10 cols, u9 rows);

// CSI [Ps] @
   // insert <Ps> spaces on cursor position (defaut:1)
void on_csi_ich (u10 n)
{
on_ptout_s *m_ = &s_singlton;

flow_bug(m_->cursor_y < m_->ws_row)
	if (! (0 < n && m_->cursor_x < m_->ws_col))
		return;
u10 insert
	= min(m_->cursor_x + n, m_->ws_col) - m_->cursor_x;
u10 moving
	= m_->ws_col - (m_->cursor_x + insert);
	if (0 < moving)
		_copy (m_->cursor_x +insert, m_->cursor_y, moving, 1, m_->cursor_x, m_->cursor_y);
	_erase (m_->cursor_x, m_->cursor_y, insert, 1);
	_on_changed (m_->cursor_x, m_->cursor_y, insert +moving, 1);
}

// CSI [Ps] A
   // move cursor up to <Ps> line(s) (default:1)
void on_csi_cuu (u9 n)
{
on_ptout_s *m_ = &s_singlton;

	m_->cursor_y -= min(n, m_->cursor_y);
}

// CSI [Ps] C
   // move cursor right to <Ps> column(s) (default:1)
void on_csi_cuf (u10 n)
{
on_ptout_s *m_ = &s_singlton;

	m_->cursor_x = min(m_->cursor_x +n, m_->ws_col -1);
}

// CSI [Ps1];[Ps2] H
   // move cursor to <Ps1> lines <Ps2> column(s) (default:1)
void on_csi_cup (u9 y, u10 x)
{
on_ptout_s *m_ = &s_singlton;

	m_->cursor_x = min(max(1, x) -1, m_->ws_col -1);
u9 cursor_y_min
	= 0; // PENDING: +m_->top_margin
u9 cursor_y_max
	= m_->ws_row -1 -m_->bottom_margin;
	m_->cursor_y = min(max(1 +cursor_y_min, y) -1, cursor_y_max);
}

// CSI [Ps] J
   // clear screen (default:0)
   // Ps = 0 from cursor position to console right-bottom
   //      1 from console left-top to cursor position
   //      2 all of screen
void on_csi_ed (u2 n)
{
on_ptout_s *m_ = &s_singlton;

	if (2 == n) {
		// TODO:
		return;
	}
	else if (1 == n) {
		// TODO:
	}
	else /*if (0 == n)*/ {
		// TODO:
	}
	if (! (m_->cursor_x < m_->ws_col && m_->cursor_y < m_->ws_row))
		return;
}

// CSI [Ps] K
   // erase line (default:0)
   // Ps = 0 from cursor position to line tail
   //      1 from line head to cursor position
   //      2 all of line
void on_csi_el (u2 n)
{
on_ptout_s *m_ = &s_singlton;

	if (! (m_->cursor_x < m_->ws_col
	    && m_->cursor_y < m_->ws_row))
		return;
u10 x, right;
	if (1 == n)
		x = 0, right = m_->cursor_x;
	else if (2 == n)
		x = 0, right = m_->ws_col;
	else /*if (0 == n)*/
		x = m_->cursor_x, right = m_->ws_col;
	if (! (x < right))
		return;
	_erase (x, m_->cursor_y, right - x, 1);
	_on_changed (x, m_->cursor_y, right - x, 1);
}

// CSI [Ps] L
   // insert blank <Ps> line(s) before cursor line (default:1)
void on_csi_il (u9 n)
{
on_ptout_s *m_ = &s_singlton;

u1 is_omitted
	= (0 == n) ? TRUE : FALSE;
	n = (is_omitted) ? 1 : n;

u1 is_invalid_cursor
	= !(m_->cursor_x < m_->ws_col
	 && m_->cursor_y < m_->ws_row) ? TRUE : FALSE;
	if (is_invalid_cursor)
		return;

u1 is_excessive_margin
	= !(m_->bottom_margin < m_->ws_row) ? TRUE : FALSE;
u9 available_bottom
	= (is_excessive_margin) ? 1 : m_->ws_row -m_->bottom_margin;

	n = min(m_->cursor_y +n, available_bottom) -m_->cursor_y;
	if (m_->cursor_y +n < available_bottom)
		_copy (0, m_->cursor_y +n,
		       m_->ws_col, available_bottom -(m_->cursor_y +n),
		       0, m_->cursor_y);
	_erase (0, m_->cursor_y, m_->ws_col, n);
	_on_changed (0, m_->cursor_y,
	             m_->ws_col, available_bottom -m_->cursor_y);
}

// CSI [Ps] M
   // cut <Ps> line(s) after cursor line (default:1)
void on_csi_dl (u9 n)
{
on_ptout_s *m_ = &s_singlton;

u1 is_omitted
	= (0 == n) ? TRUE : FALSE;
	n = (is_omitted) ? 1 : n;

u1 is_invalid_cursor
	= !(m_->cursor_x < m_->ws_col
	 && m_->cursor_y < m_->ws_row) ? TRUE : FALSE;
	if (is_invalid_cursor)
		return;

u1 is_excessive_margin
	= !(m_->bottom_margin < m_->ws_row) ? TRUE : FALSE;
u9 available_bottom
	= (is_excessive_margin) ? 1 : m_->ws_row -m_->bottom_margin;

	n = min(m_->cursor_y +n, available_bottom) -m_->cursor_y;
	if (m_->cursor_y +n < available_bottom)
		_copy (0, m_->cursor_y,
		       m_->ws_col, (available_bottom -n) -m_->cursor_y,
		       0, m_->cursor_y +n);
	_erase (0, available_bottom -n, m_->ws_col, n);
	_on_changed (0, m_->cursor_y,
	             m_->ws_col, available_bottom -m_->cursor_y);
}

// CSI [Ps] P
   // erase <Ps> character(s) at cursor position (default:1)
void on_csi_dch (u10 n)
{
on_ptout_s *m_ = &s_singlton;

	if (! (0 < n
	    && m_->cursor_x < m_->ws_col
	    && m_->cursor_y < m_->ws_row))
		return;
	n = min(n, m_->ws_col -m_->cursor_x);

u10 moving_n
	= m_->ws_col - (m_->cursor_x + n);
	if (0 < moving_n) {
		_copy (m_->cursor_x, m_->cursor_y, moving_n, 1,
		       m_->cursor_x +n, m_->cursor_y);
	}
	_erase (m_->cursor_x + moving_n, m_->cursor_y, n, 1);
	_on_changed (m_->cursor_x, m_->cursor_y, moving_n +n, 1);
}

// CSI 0m
void on_csi_sgr_none ()
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.fgcl_outofVGA = FALSE, m_->sgr.fgcl3or24 = 7;
	m_->sgr.bgcl_outofVGA = FALSE, m_->sgr.bgcl3or24 = 0;
	m_->sgr.strikethrough =
	m_->sgr.italic =
	m_->sgr.underline =
	m_->sgr.reverse =
	m_->sgr.bold = 0;
}

// CSI 1|22m
void on_csi_sgr_bold (bool is_enabled)
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.bold = (is_enabled) ? 1 : 0;
}

// CSI 3|23m
void on_csi_sgr_italic (bool is_enabled)
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.italic = (is_enabled) ? 1 : 0;
}

// CSI 4|24m
void on_csi_sgr_underline (bool is_enabled)
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.underline = (is_enabled) ? 1 : 0;
}

// CSI 7|27m
void on_csi_sgr_reverse (bool is_enabled)
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.reverse = (is_enabled) ? 1 : 0;
}

// CSI 9|29m
void on_csi_sgr_strikethrough (bool is_enabled)
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.strikethrough = (is_enabled) ? 1 : 0;
}

// CSI 3[0-7]m 38;5;[0-255]m
void on_csi_sgr_fgcl3 (u3 cl)
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.fgcl_outofVGA = FALSE;
	m_->sgr.fgcl3or24 = cl;
}

// CSI 3[0-7]m 38;2;[0-255]m
void on_csi_sgr_fgcl24 (u24 cl)
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.fgcl_outofVGA = TRUE;
	m_->sgr.fgcl3or24 = cl;
}

// CSI 4[0-7]m 48;5;[0-255]m
void on_csi_sgr_bgcl3 (u3 cl)
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.bgcl_outofVGA = FALSE;
	m_->sgr.bgcl3or24 = cl;
}

// CSI 4[0-7]m 48;2;[0-255]m
void on_csi_sgr_bgcl24 (u24 cl)
{
on_ptout_s *m_ = &s_singlton;

	m_->sgr.bgcl_outofVGA = TRUE;
	m_->sgr.bgcl3or24 = cl;
}

// CSI [Ps] n
   // announce console status
   // Ps = 6 announce cursor position (reply:CPR)
   // PENDING: <Ps> not 6
void on_csi_dsr (u3 n)
{
on_ptout_s *m_ = &s_singlton;

	// TODO:
}

// CSI [Ps1];[Ps2] r
   // set top/bottom margin (scroll region)
   // Ps1 line position of top margin (default:1)
   // Ps2 line position of bottom margin (default:ws_row)
void on_csi_dec_stbm (u9 tm, u9 bm_line)
{
on_ptout_s *m_ = &s_singlton;

u1 is_omitted
	= (0 == bm_line) ? TRUE : FALSE;
	if (is_omitted) {
		m_->bottom_margin = 0;
		return;
	}
	bm_line = min(max(1, bm_line), m_->ws_row);
u9 pt_y_max
	= m_->ws_row -1;
	m_->bottom_margin = pt_y_max -(bm_line -1);
}

// CSI [Ps1(1-23)];[Ps2];[Ps3] t
   // Ps1 = 22 save window title to stack
   // Ps2 = 0,1,2 save window title
static bool on_window_title_save (on_ptout_s *m_, u8 index)
{
	if (2 < index)
		return false;
	// TODO:
	return true;
}
   // Ps1 = 23 load window title from stack
   // Ps2 = 0,1,2 load window title
static bool on_window_title_load (on_ptout_s *m_, u8 index)
{
	if (2 < index)
		return false;
	// TODO:
	return true;
}
   // window manipulation
void on_csi_dec_slpp (u8 op_id, u8 aux1, u8 aux2)
{
on_ptout_s *m_ = &s_singlton;

	if (22 == op_id)
		on_window_title_save (m_, aux1);
	else if (23 == op_id)
		on_window_title_load (m_, aux1);
}

// CSI > <Ps> c
   // announce second console features (default:0)
void on_csi_da2 (u16 n)
{
on_ptout_s *m_ = &s_singlton;

	// PENDING:
}

// CSI > <Ps1>;<Ps2> m
   // Xterm Extention Sequence? (Vim sending found)
void on_csi_xtmodkeys (uint8_t n1, uint8_t n2)
{
on_ptout_s *m_ = &s_singlton;

	// PENDING:
}

static void _xt_extscrn_open ()
{
on_ptout_s *m_ = &s_singlton;

	if (m_->is_extscrn)
		return;

unsigned cb
	= _save_frontend (NULL);
	if (m_->stdscrn_max < pow2_ge(cb)) {
		m_->stdscrn = (NULL == m_->stdscrn)
		            ? malloc (pow2_ge(cb))
		            : realloc (m_->stdscrn, pow2_ge(cb));
no_memory(m_->stdscrn)
		m_->stdscrn_max = pow2_ge(cb);
	}
	_save_frontend (m_->stdscrn);
	m_->backup_stdscrn_cursor_x = m_->cursor_x;
	m_->backup_stdscrn_cursor_y = m_->cursor_y;

	_erase (0, 0, m_->ws_col, m_->ws_row);
	m_->cursor_x = m_->cursor_y = 0;

	m_->is_extscrn = 1;
	_on_changed (0, 0, 0, 0);
}

static void _xt_extscrn_close ()
{
on_ptout_s *m_ = &s_singlton;

	if (!m_->is_extscrn)
		return;
	_load_frontend (m_->stdscrn);

	m_->cursor_x = m_->backup_stdscrn_cursor_x;
	m_->cursor_y = m_->backup_stdscrn_cursor_y;

	m_->is_extscrn = 0;
	_on_changed (0, 0, 0, 0);
}

// CSI ? <Pm> h
   // enable DEC/xterm extend mode
// CSI ? <Pm> l
   // disable DEC/xterm extend mode
void on_csi_dec_exm (u14 n, bool is_enabled)
{
	switch (n) {
	//case 1:
		// TODO: set application cursor mode
	case 12:
		// TODO: disable cursor blink
	case 25:
		// TODO: show cursor
		return;
	case 1049:
		// secondary screen buffer
		if (is_enabled)
			_xt_extscrn_open ();
		else
			_xt_extscrn_close ();
		break;
//	case 8200: // TTCTH
	}
}

// CSI ? 4 m
   // ??? (Vim sending found)
void on_vim_unknown_1_csi ()
{
	// PENDING:
}

///////////////////////////////////////////////////////////////
// OSC

// OSC [0-2];<title> BEL
void on_osc_set_window_title (const char *title)
{
	// TODO:
}

// OSC 8;[<id>];[<uri>] ST
void on_osc_hyperlinks (const char *uri)
{
u1 uri_omitted
	= (NULL == uri) ? 1 : 0;
	if (uri_omitted)
		uri = "";
	// TODO:
}

// OSC 10;? BEL
void on_osc_fg_rgb_query ()
{
	// TODO:
}

// OSC 11;? BEL
void on_osc_bg_rgb_query ()
{
	// TODO:
}

///////////////////////////////////////////////////////////////
// ESC

// ESC M
   // move cursor up to 1 line
void on_esc_ri ()
{
on_ptout_s *m_ = &s_singlton;

u1 is_rolled_down
	= _cursor_up (1);
	if (is_rolled_down)
		_on_changed (0, 0, 0, 0);
}

// ESC P <s> ST (DCS: Device Control Sequence)
void on_esc_dcs (const char *s)
{
	// PENDING:
   // Xterm Extention Sequence? (Vim sending found)
}

// ESC =
   // shift to application key-pad mode
void on_esc_dec_kpam ()
{
}

// ESC >
   // shift to normal key-pad mode
void on_esc_dec_kpnm ()
{
}

///////////////////////////////////////////////////////////////
// internal

#define UTF8_ERROR_REP 0xE296A0 // u21from(e296a0)=0025a0='■'
static void _output_utf8_or_ctrl (on_ptout_s *m_,
                                  const char *src, unsigned cb)
{

u1 entirely_drawing_needed = FALSE;
const char *p, *q, *end;
	p = q = src, end = src + cb;
	for (; q < end; ++q) {
u8 c;
		c = (u8)*q;
bool is_ctl;
		is_ctl = (c < 0x20 || 0x7f == c || 0xfc < c) ? true : false;
		if (!is_ctl) {
u21 uni21;
			// [F]X (XX) XX XX
			if (m_->utf8.need_XXXXXX) {
				if (! (0x80 == (0xc0 & c)))
					m_->utf8.error = 1;
				m_->utf8.b20to6_of_utf21 |= 0xfc0 & c << 6;
				m_->utf8.need_XXXXXX = 0, m_->utf8.need_XXXX = 1;
			}
			// [E]X (XX) XX or [F]X XX (XX) XX
			else if (m_->utf8.need_XXXX) {
				if (! (0x80 == (0xc0 & c)))
					m_->utf8.error = 1;
				m_->utf8.b20to6_of_utf21 |= 0x3f & c;
				m_->utf8.need_XXXX = 0, m_->utf8.need_XX = 1;
			}
			// [CD]X (XX) or [E]X XX (XX) or [F]X XX XX (XX)
			else if (m_->utf8.need_XX) {
				if (! (0x80 == (0xc0 & c)))
					m_->utf8.error = 1;
				uni21 = m_->utf8.b20to6_of_utf21 << 6 | 0x3f & c;
				m_->utf8.need_XX = 0;
			}
			// (INITIAL)
			else {
				// 21bit unicode: (F[0-7]) XX XX XX
				if (0xf0 == (0xf8 & c)) {
					m_->utf8.b20to6_of_utf21 = 0x7000 & c << 12;
					m_->utf8.need_XXXXXX = 1;
				}
				// 16bit unicode: ([E]X) XX XX
				else if (0xe0 == (0xf0 & c)) {
					m_->utf8.b20to6_of_utf21 = 0x3c0 & c << 6;
					m_->utf8.need_XXXX = 1;
				}
				// 12bit unicode: ([CD]X) XX
				else if (0xc0 == (0xe0 & c)) {
					m_->utf8.b20to6_of_utf21 = 0x03f & c;
					m_->utf8.need_XX = 1;
				}
				// 7bit ANSI: [0-7]X
				else if (0x00 == (0x80 & c))
					uni21 = c;
				// (unsupported): [8-B]X or F[8-F]
				else
					m_->utf8.error = 1;
			}

u1 is_utf8_pending
			= (m_->utf8.need_XX
			|| m_->utf8.need_XXXX
			|| m_->utf8.need_XXXXXX) ? TRUE : FALSE;
			if (!is_utf8_pending) {
				if (m_->utf8.error)
					uni21 = UTF8_ERROR_REP, m_->utf8.error = 0;
				_set_text (m_->cursor_x, m_->cursor_y, uni21);
u2 uni21_width;
				if (0xffff < uni21)
					uni21_width = 2; // PENDING: (*2)
				else
				   uni21_width = wcwidth_bugfix ((wchar_t)(u16)uni21);
				_on_changed (m_->cursor_x, m_->cursor_y, uni21_width, 1);
u9 frontend_y
				= _frontend_y_from (m_->cursor_y);
				bpp1_set (&m_->frontend.showmask,
				          m_->cursor_x, frontend_y,
				          min(uni21_width, m_->ws_col -m_->cursor_x), 1,
				          1);
				m_->cursor_x += uni21_width;
				if (! (m_->cursor_x < m_->ws_col)) { // NOTE: (*3)
					m_->cursor_x = 0;
u1 is_rolled_up
					= _line_feed ();
					if (is_rolled_up)
						entirely_drawing_needed = TRUE;
				}
			}
			if (q +1 < end)
				continue;
			q = end;
		}
		else /*if (is_ctl)*/ {
/*input_error(0 == m_->utf8.need_XXXXXX &&
              0 == m_->utf8.need_XXXX &&
              0 == m_->utf8.need_XX)*/
			m_->utf8.need_XXXXXX =
			m_->utf8.need_XXXX   =
			m_->utf8.need_XX     =
			m_->utf8.error       = 0;
		}
		if (q < end) {
			switch (*q) {
			case 0x07:
				break;
			case 0x08:
				--m_->cursor_x;
				break;
			case 0x0A:
				if (m_->acceptLFasCRLF)
					m_->cursor_x = 0; // same as CR
u1 is_rolled_up
				= _line_feed ();
				if (is_rolled_up)
					entirely_drawing_needed = TRUE;
				break;
			case 0x0D:
				m_->cursor_x = 0;
				break;
			default:
				break;
			}
		}
		else
			--q;
		p = q +1;
	}

	if (entirely_drawing_needed)
		_on_changed (0, 0, 0, 0);
}

///////////////////////////////////////////////////////////////
// method

static void _ctor (void (*on_changed) (u12 px, u12 py,
                                       u12 width, u12 height),
                   bool acceptLFasCRLF)
{
on_ptout_s *m_ = &s_singlton;

	m_->on_changed = on_changed;
	m_->acceptLFasCRLF = (acceptLFasCRLF) ? 1 : 0;
}

// streaming output from pt
static int _write_output (const char *buf, unsigned cb)
{
on_ptout_s *m_ = &s_singlton;

	_on_changed (m_->cursor_x, m_->cursor_y, 1, 1);
const char *p, *end;
	p = buf, end = buf + cb;
	while (p < end) {
const char *q;
		// handling each letter or control code
		// until finding ESC(\033) or end of buffer.
		if (! (m_->is_just_one_after_esc
		    || m_->is_parsing_escseq
		    || m_->is_parsing_embedded_image)) {
			q = (const char *)memchr (p, '\033', end - p);
u1 esc_found
			= (q) ? TRUE : FALSE;
			if (!esc_found)
				q = end;
			if (p < q) {
				_output_utf8_or_ctrl (m_, p, q - p);
				p = q;
			}
			if (esc_found)
				++p, m_->is_just_one_after_esc = 1;
			continue;
		}
		// handling escape sequence (for VT100) or embedded image
u1 is_escseq_terminated = 0;
		while (p < end) {
			if (m_->is_just_one_after_esc) {
				m_->is_just_one_after_esc = 0;
				if ('\033' == *p) {
					++p, m_->is_parsing_embedded_image = 1;
u16 backlog_y
					= _backlog_y_from (m_->cursor_y);
					image_backlog->setpos (m_->cursor_x, backlog_y);
					continue; // need to check (p < end)
				}
				m_->is_parsing_escseq = 1;
				escseq_reset ();
			}
			// embedded image
			if (m_->is_parsing_embedded_image) {
unsigned backlog_accepted
				= image_backlog->write (p, (unsigned)(end -p));
				if (0 < backlog_accepted) {
					p += backlog_accepted;
					continue;
				}
				// end of embedded image
				m_->is_parsing_embedded_image = 0;
u16 backlog_y; u9 rows; u10 pt_x, cols;
				image_backlog->tellpos (&pt_x, &backlog_y,
				                        &cols, &rows);
s10 pt_y
				= _pt_y_from (backlog_y);
unexpected(0 <= pt_y)
				_on_changed (pt_x, (u9)pt_y, cols, rows);
				// starting again another ESC sequence?
				if (p < end && '\033' == *p) {
					++p, m_->is_just_one_after_esc = 1;
					continue; // need to check (p < end)
				}
				// back to UTF-8 or CTRL
				is_escseq_terminated = 1;
				break;
			}
flow_bug(m_->is_parsing_escseq)
			// parsing and handling later bytes of escape sequence
			// (*) callback unchangeble named function
			//    'on_esc_dec_kpam' etc.
int parser_accepted
			= escseq_write (p, (unsigned)(end -p));
			// handling failed or paused
			if (-1 == parser_accepted) {
unsigned aborted_len
				= escseq_get_paused_len ();
unexpected(0 < aborted_len)

				p += aborted_len;
				if (! (p < end))
					break; // input not enough
				is_escseq_terminated = 1;
				break; // invalid multi-byte or aborted by '\0'
			}
			// handling succeeded or accomplished
			if (0 <= parser_accepted) {
				p += parser_accepted;
				// skip memchr() if next escape sequence starts at once
				if (0 < parser_accepted && p < end && '\033' == *p) {
					++p, m_->is_just_one_after_esc = 1;
					continue; // need to check (p < end)
				}
				is_escseq_terminated = 1;
				break; // end of input or back to UTF-8 or CTRL
			}
//no_memory(! (-127 == parser_accepted)) // not enough memory
//input_error(! (-2 == parser_accepted)) // syntax error
unexpected(1 == 0) // unexpected error
		}
		if (is_escseq_terminated) {
			escseq_reset ();
			m_->is_parsing_escseq = 0;
		}
	}
	_on_changed (m_->cursor_x, m_->cursor_y, 1, 1);
	return (int)(p - buf);
}

static void _on_changed (u10 x, u9 pt_y, u10 cols, u9 rows)
{
on_ptout_s *m_ = &s_singlton;

u12 da_x, da_y, da_width, da_height;
	da_x      =    x * gtk2_font->width();
	da_y      = pt_y * gtk2_font->height();
	da_width  = cols * gtk2_font->width();
	da_height = rows * gtk2_font->height();
	if (m_->on_changed)
		m_->on_changed (da_x, da_y,
		                da_width, da_height);
}

static void _draw_to_gdk2 (u12 px, u12 py, u12 width, u12 height,
                           GdkWindow *window, GdkGC *fgcl_gc)
{
cairo_t *cr
	= gdk_cairo_create (GDK_DRAWABLE(window));

on_ptout_s *m_ = &s_singlton;

u10 x
	= px / gtk2_font->width();
u9 pt_y
	= py / gtk2_font->height();

u1 is_width_omitted
	= (0 == width) ? TRUE : FALSE;
u10 x_end;
	if (is_width_omitted)
		x_end = m_->ws_col;
	else {
		x_end = ALIGN(gtk2_font->width(), px +width)
		            / gtk2_font->width(); // round up
		x_end = min(x_end, m_->ws_col);
	}

u1 is_height_omitted
	= (0 == height) ? TRUE : FALSE;
u9 pt_y_end;
	if (is_height_omitted)
		pt_y_end = m_->ws_row;
	else {
		pt_y_end = ALIGN(gtk2_font->height(), py +height)
		               / gtk2_font->height(); // round up
		pt_y_end = min(pt_y_end, m_->ws_row);
	}

caller_bug(cr)

unsigned needed_clip_tmp
	= ALIGN(4, gtk2_font->width() * 3) * gtk2_font->height();
	if (! (needed_clip_tmp <= m_->bpp24_clip_max)) {
		if (NULL == m_->bpp24_clip)
			m_->bpp24_clip = malloc (pow2_ge(needed_clip_tmp));
		else
			m_->bpp24_clip = realloc (m_->bpp24_clip, pow2_ge(needed_clip_tmp));
		m_->bpp24_clip_max = pow2_ge(needed_clip_tmp);
	}

u1 is_pango_font
	= (INSTALLED == gtk2_font->get_type ()) ? 1 : 0;
u10 x_begin = x;
	for (; pt_y < pt_y_end; ++pt_y) {
		for (x = x_begin; x < x_end; ++x) {
u1 is_cursor_pos
			= (pt_y == m_->cursor_y
			&&    x == m_->cursor_x) ? TRUE : FALSE;
u1 is_text_show = FALSE;
			if (pt_y < FRONTEND_Y_SIZE) // safety
				is_text_show = bpp1_get (&m_->frontend.showmask,
				                         x, _frontend_y_from (pt_y));
static const u24 CURSOR_CL24
			= RGB24(0,255,0);
u16 backlog_y
			= _backlog_y_from (pt_y);
			if (is_text_show) {
u24 fgcl24, bgcl24; SGR sgr; u21 uni21
				= _get_text (x, backlog_y, &fgcl24, &bgcl24, &sgr);
				if (is_cursor_pos) {
					swap(fgcl24, bgcl24, u24);
					if (!m_->is_extscrn)
						bgcl24 = CURSOR_CL24;
				}
				if (is_pango_font)
					gtk2_font->put_pango (x, pt_y, fgcl24, bgcl24,
					                      sgr.bold, sgr.underline,
					                      uni21,
					                      window, fgcl_gc,
					                      cr);
				else
					gtk2_font->put (x, pt_y, fgcl24, bgcl24,
					                uni21, cr);
u16 uni21_width
				= wcwidth_bugfix ((wchar_t)(u16)uni21);
				if (2 == uni21_width)
					++x;
				continue;
			}
bool is_image_exist
			= image_backlog->clip_from (x, backlog_y, 0, NULL);
			if (is_image_exist && !m_->is_extscrn) {
u24 xy_not_enough_cl24 = 0x000000U; // TODO: supposed to apply SGR
				image_backlog->clip_from (x, backlog_y,
				                          xy_not_enough_cl24,
				                          m_->bpp24_clip);
				gtk2_font->put_bpp24 (x, pt_y, m_->bpp24_clip, cr);
				continue;
			}
u1 bold = FALSE; u24 box_cl24;
			if (is_cursor_pos && !m_->is_extscrn)
				box_cl24 = CURSOR_CL24;
			else if (is_cursor_pos /*&& m_->is_extscrn*/)
				box_cl24 = _rgb24_from (m_->sgr.fgcl3or24,
				                        m_->sgr.fgcl_outofVGA, bold);
			else /*if (!is_cursor_pos)*/
				box_cl24 = _rgb24_from (m_->sgr.bgcl3or24,
				                        m_->sgr.bgcl_outofVGA, bold);
			gtk2_font->put_box (x, pt_y, box_cl24, cr);
		}
	}

	cairo_destroy (cr);
}

///////////////////////////////////////////////////////////////
// readability sugar

static const READABILITY_SUGAR_ON_PTOUT _on_ptout = {
	.ctor = _ctor,
	.set_frontend_size = _set_frontend_size,
	.write_output = _write_output,
	.draw_to_gdk2 = _draw_to_gdk2,
};
const READABILITY_SUGAR_ON_PTOUT *on_ptout = &_on_ptout;

///////////////////////////////////////////////////////////////
// footnote

/* TODO: (*1)
   Now "bpp1" is fake codes copied from "bpp8", so supposed
   codes can't be written here.
   If "bpp1" will be given right 1-bit coding, these coding can
   be enabled.

   PENDING: (*2)
   U+010000 - U+1FFFFF is always wide character?

   NOTE: (*3)
   If the following will be broken at right edge of screen,
   the bug will be occured, where cursor turns back left edge
   without feeding line.

     - 'm_->ws_col' management

   Inputing any character at right edge, 1 byte of character and
   2 bytes of " \r" are sent by shell on posix_openpt(), 2 bytes
   of later causes bug. */
