/* TODO:
	(*)b:bash v:vim l:less
	b - - BEL
	b - - BS
	b - - CSI 2@
	b - - CSI 22P
	b - - OSC 0;<str>
	    ...
	- - l ESC M
	- v - ESC =
	- v - ESC >
	- v - CSI 8C
	- v l CSI H|<y>;<x>H
	- v - CSI 2J
	- v l CSI K
	- v l CSI m|[1|27|34|3246]m
	- v - CSI 1;24r
	- v - CSI >c
	- v - CSI ? [1|12|25|1049][hl]
 */

#include <stdint.h>
#define parser_event_s void
#define VRAM_s void
#include "vt100_handler.hpp"

#include <wchar.h> // wchar_t
#include "unicode.h"
struct bmp_helper;
#define img_helper_s struct bmp_helper
#include "bmp_helper.hpp"
typedef struct bmp_helper bmp_helper_s;
#include "log.h"
#include "assert.h"

#include <stdbool.h>
#include <stdio.h> // sprintf
#include <stdlib.h> // exit
#include <memory.h>
#include <alloca.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define arraycountof(x) (sizeof(x)/(sizeof((x)[0])))

#define swap(a, b, type__) do { type__ t__; t__ = a, a = b, b = t__; } while (0)
#define HIDIV(x,a) (((x) +(a) -1)/(a))
#define HIALIGN(a,x) (HIDIV(x,a)*(a))
#define ALIGN HIALIGN

#include "config.hpp"
#include "appconst.h"
///////////////////////////////////////////////////////////////////////////////
// non-state

static u32 o_rgb8[256 +1];
static u32 xterm_256color (u16 n)
{
u8 r, g, b;
	if (n < 8) {
		r = (1 & n) ? 187 : 0, g = (2 & n) ? 187 : 0, b = (4 & n) ? 187 : 0;
	}
	else if (n < 16) {
		r = (1 & n) ? 255 : 85, g = (2 & n) ? 255 : 85, b = (4 & n) ? 255 : 85;
	}
	else if (n < 232) {
		n -= 16;
		r = n / 36, g = (n / 6) % 6, b = n % 6;
		r = (r) ? r * 40 +55 : 0, g = (g) ? g * 40 +55 : 0, b = (b) ? b * 40 +55 : 0;
	}
	else if (n < 256) {
		n -= 232;
		r = g = b = n * 10 +8;
	}
	else /*if (256 == n)*/ {
		r = b = 0, g = 255;
	}
	return RGB24(r, g, b);
}

static void vt100_handler_read_config ()
{
static const u32 default_rgb24[8] = {
	  DEFAULT_COLOR_0, DEFAULT_COLOR_1, DEFAULT_COLOR_2, DEFAULT_COLOR_3
	, DEFAULT_COLOR_4, DEFAULT_COLOR_5, DEFAULT_COLOR_6, DEFAULT_COLOR_7
};

struct config_reader_i *cfg; struct config *cfg_this_;
	cfg_this_ = config_get_singlton (), cfg = config_query_config_reader_i (cfg_this_);
unsigned n;
	for (n = 0; n < 8; ++n) {
char key[9 +1];
		sprintf (key, CFG_SGRCL_PREFIX "%d", n);
		o_rgb8[n] = cfg->read_rgb24 (cfg_this_, key, default_rgb24[n]);
	}
}

///////////////////////////////////////////////////////////////////////////////
// private

struct vt100_handler_;
#define vt100_event_s struct vt100_handler_
#include "vt100_parser.hpp"
typedef struct vt100_parser vt100_parser_s;
typedef struct vt100_handler_ {
	vt100_parser_s escseq;
	struct {
		u32 parse_shift  : 2;
		u32 escseq       : 2;
		u32 utf8         : 2;
		u32 is_extscrn   : 1;
		u32 is_csrshow   : 1;
		u32 reserved     : 8;
		u32 utf21        :16;
	} state;
	struct {
		u32 fg_rgb8          :8;
		u32 bg_rgb8          :8;
		u32 fg_bold          :1;
		u32 reverse          :1;
		u32 fg_italic        :1;
		u32 fg_strikethrough :1;
		u32 reserved         :12;
	} sgr;
	struct VRAM_i *bg;
	struct VRAM *bg_this_;
	struct VRAM_i *fg;
	struct VRAM *fg_this_;
	struct parser_event_i *handler;
	parser_event_s *handler_this_;
	struct img_helper_i *bh;
	bmp_helper_s bh_this_;
	u16 ws_col, ws_row;
	u16 x, y;
	u16 bm;
	u16 aux_lf;
	u16 x_stdscrn, y_stdscrn;
	u32 cb_fg_stdscrn;
	u32 cb_bg_stdscrn;
	void *fg_stdscrn;
	void *bg_stdscrn;
	u32 bmp_size, bmp_got;
	u8 bmphdr[sizeofBMPHDR];
#ifdef __x86_64__
	u8 padding_302[2];
#else //def __i386__
	u8 padding_262[2];
#endif
} vt100_handler_s;

///////////////////////////////////////////////////////////////////////////////
// helper

static void vt100_handler_erase (vt100_handler_s *m_, u16 y, u16 x, u16 width)
{
u32 *utf21s; u64 *rgb24attr4s;
	utf21s = (u32 *)alloca (width * sizeof(u32));
	rgb24attr4s = (u64 *)alloca (width * sizeof(u64));
ASSERTE(utf21s && rgb24attr4s)
u16 i;
	for (i = 0; i < width; ++i) {
		utf21s[i] = ' ';
		rgb24attr4s[i] = ATTR_FG(o_rgb8[m_->sgr.fg_rgb8])|ATTR_BG(o_rgb8[m_->sgr.bg_rgb8]);
	}
	m_->fg->write_text (m_->fg_this_, x, y, width, utf21s, rgb24attr4s, 0);
}

static void vt100_handler_rollup (vt100_handler_s *m_, u16 n_rollup)
{
	if (0 == n_rollup)
		return;
	while (0 < n_rollup--) {
		m_->bg->rollup (m_->bg_this_, 1, o_rgb8[m_->sgr.bg_rgb8]);
		m_->fg->rollup (m_->fg_this_, 1, o_rgb8[m_->sgr.bg_rgb8]);
	}
	m_->handler->dirty_notify (m_->handler_this_);
}

static void vt100_handler_line_feed (vt100_handler_s *m_)
{
u16 n_csrdown, n_rollup;
	n_csrdown = min(1 + m_->aux_lf, m_->ws_row -1 - m_->y),
	m_->y += n_csrdown;
	n_rollup = max(n_csrdown, 1 + m_->aux_lf) - n_csrdown;
	m_->aux_lf = 0;
	if (0 < n_rollup)
		vt100_handler_rollup (m_, n_rollup);
}

static void vt100_handler_line_back (vt100_handler_s *m_, u16 n)
{
	if (n < m_->y +1) {
		m_->y -= n;
		return;
	}
	n -= m_->y, m_->y = 0;
	do {
		m_->bg->rolldown (m_->bg_this_, 1, o_rgb8[m_->sgr.bg_rgb8]);
		m_->fg->rolldown (m_->fg_this_, 1, o_rgb8[m_->sgr.bg_rgb8]);
	} while (0 < --n);
	m_->handler->dirty_notify (m_->handler_this_);
}

///////////////////////////////////////////////////////////////////////////////
// CSI

// CSI [Ps] @
   // insert <Ps> spaces on cursor position (defaut:1)
static void on_ich (vt100_handler_s *m_, u16 n)
{
	if (! (m_->x < m_->ws_col && m_->y < m_->ws_row))
		return;
u16 space;
	space = min(m_->x + n, m_->ws_col) - m_->x;
	if (! (0 < space))
		return;
u16 shift;
	shift = m_->ws_col - (m_->x + space);
	if (0 < shift)
		m_->fg->copy (m_->fg_this_, m_->x, m_->y, shift, 1, m_->x + space, m_->y);
	vt100_handler_erase (m_, m_->y, m_->x + shift, space);
	m_->handler->dirty_notify (m_->handler_this_);
}

// CSI [Ps] A
   // move cursor up to <Ps> line(s) (default:1)
static void on_cuu (vt100_handler_s *m_, u16 n)
{
	m_->y -= min(n, m_->y);
}

// CSI [Ps] C
   // move cursor right to <Ps> column(s) (default:1)
static void on_cuf (vt100_handler_s *m_, u16 n)
{
	m_->x = min(m_->x +n, m_->ws_col -1);
}

// CSI [Ps1];[Ps2] H
   // move cursor to <Ps1> lines <Ps2> column(s) (default:1)
static void on_cup (vt100_handler_s *m_, u16 y, u16 x)
{
	m_->y = min(max(1, y), m_->ws_row) -1;
	m_->x = min(max(1, x), m_->ws_col) -1;
}

// CSI [Ps] J
   // clear screen (default:0)
   // Ps = 0 from cursor position to console right-bottom
   //      1 from console left-top to cursor position
   //      2 all of screen
static void on_ed (vt100_handler_s *m_, u16 n)
{
	if (2 == n) {
		m_->fg->clear (m_->fg_this_, o_rgb8[m_->sgr.bg_rgb8]);
		m_->bg->clear (m_->bg_this_, o_rgb8[m_->sgr.bg_rgb8]);
		m_->handler->dirty_notify (m_->handler_this_);
		return;
	}
	else if (1 == n) {
		// TODO:
	}
	else /*if (0 == n)*/ {
		// TODO:
	}
	if (! (m_->x < m_->ws_col && m_->y < m_->ws_row))
		return;
}

// CSI [Ps] K
   // erase line (default:0)
   // Ps = 0 from cursor position to line tail
   //      1 from line head to cursor position
   //      2 all of line
static void on_el (vt100_handler_s *m_, u16 n)
{
	if (! (m_->x < m_->ws_col && m_->y < m_->ws_row))
		return;
u16 left, right;
	if (1 == n)
		left = 0, right = m_->x;
	else if (2 == n)
		left = 0, right = m_->ws_col;
	else /*if (0 == n)*/
		left = m_->x, right = m_->ws_col;
	if (! (left < right))
		return;
	vt100_handler_erase (m_, m_->y, left, right - left);
	m_->handler->dirty_notify (m_->handler_this_);
}

// CSI [Ps] L
   // insert blank <Ps> line(s) before cursor line (default:1)
static void on_il (vt100_handler_s *m_, u16 n)
{
u16 bm;
// WARNING: Is this correct specification ?
	bm = (0 < m_->bm) ? min(m_->bm, m_->ws_row) : m_->ws_row;
	if (! (m_->x < m_->ws_col && m_->y < bm))
		return;
u16 space;
	space = min(m_->y + n, bm -1) - m_->y;
u16 shift;
	shift = bm - (m_->y + space);
	m_->fg->copy (m_->fg_this_, 0, m_->y, m_->ws_col, shift, 0, m_->y + space);
u16 y;
	for (y = m_->y; y < m_->y + space; ++y)
		vt100_handler_erase (m_, y, 0, m_->ws_col);
	m_->handler->dirty_notify (m_->handler_this_);
}

// CSI [Ps] M
   // erase <Ps> line(s) after cursor line (default:1)
static void on_dl (vt100_handler_s *m_, u16 n)
{
	if (! (m_->x < m_->ws_col && m_->y < m_->ws_row))
		return;
u16 cut;
	cut = min(m_->y + n, m_->ws_row) - m_->y;
	if (! (0 < cut))
		return;
u16 shift;
	shift = m_->ws_row - (m_->y + cut);
	if (0 < shift)
		m_->fg->copy (m_->fg_this_, 0, m_->y + cut, m_->ws_col, shift, 0, m_->y);
u16 y;
	for (y = m_->y + shift; y < m_->y + shift + cut; ++y)
		vt100_handler_erase (m_, y, 0, m_->ws_col);
	m_->handler->dirty_notify (m_->handler_this_);
}

// CSI [Ps] P
   // erase <Ps> character(s) at cursor position (default:1)
static void on_dch (vt100_handler_s *m_, u16 n)
{
	if (! (m_->x < m_->ws_col && m_->y < m_->ws_row))
		return;
u16 cut;
	cut = min(m_->x + n, m_->ws_col) - m_->x;
	if (! (0 < cut))
		return;
u16 shift;
	shift = m_->ws_col - (m_->x + cut);
	if (0 < shift)
		m_->fg->copy (m_->fg_this_, m_->x + cut, m_->y, shift, 1, m_->x, m_->y);
	vt100_handler_erase (m_, m_->y, m_->x + shift, cut);
	m_->handler->dirty_notify (m_->handler_this_);
}

// CSI 0m
static void on_sgr_none (vt100_handler_s *m_)
{
	m_->sgr.fg_rgb8 = 7;
	m_->sgr.bg_rgb8 = 0;
	m_->sgr.fg_bold = m_->sgr.reverse = 0;
}

// CSI 1|22m
static void on_sgr_bold (vt100_handler_s *m_, bool is_enabled)
{
	m_->sgr.fg_bold = (is_enabled) ? 1 : 0;
}

// CSI 3|23m
static void on_sgr_italic (vt100_handler_s *m_, bool is_enabled)
{
	m_->sgr.fg_italic = (is_enabled) ? 1 : 0;
}

// CSI 7|27m
static void on_sgr_reverse (vt100_handler_s *m_, bool is_enabled)
{
	m_->sgr.reverse = (is_enabled) ? 1 : 0;
}

// CSI 9|29m
static void on_sgr_strikethrough (vt100_handler_s *m_, bool is_enabled)
{
	m_->sgr.fg_strikethrough = (is_enabled) ? 1 : 0;
}

// CSI 3[0-7]m 38;5;[0-255]m
static void on_sgr_fgcl (vt100_handler_s *m_, u8 cl)
{
	m_->sgr.fg_rgb8 = cl;
}

// TODO: near 8bit-color -> 24bit-color (fg_rgb8 -> fg_rgb24)
// CSI 3[0-7]m 38;2;[0-255]m
static void on_sgr_fgcl_raw (vt100_handler_s *m_, u32 cl)
{
u8 r, g, b;
	r = 255 & cl >> 16, g = 255 & cl >> 8, b = 255 & cl;
	r = (15 < r) ? (r -16) / 40 : 0, g = (15 < g) ? (g -16) / 40 : 0, b = (15 < b) ? (b -16) / 40 : 0;
	m_->sgr.fg_rgb8 = 16 + r * 36 + g * 6 + b;
}

// CSI 4[0-7]m 48;5;[0-255]m
static void on_sgr_bgcl (vt100_handler_s *m_, u8 cl)
{
	m_->sgr.bg_rgb8 = cl;
}

// TODO: near 8bit-color -> 24bit-color (bg_rgb8 -> bg_rgb24)
// CSI 4[0-7]m 48;2;[0-255]m
static void on_sgr_bgcl_raw (vt100_handler_s *m_, u32 cl)
{
u8 r, g, b;
	r = 255 & cl >> 16, g = 255 & cl >> 8, b = 255 & cl;
	r = (15 < r) ? (r -16) / 40 : 0, g = (15 < g) ? (g -16) / 40 : 0, b = (15 < b) ? (b -16) / 40 : 0;
	m_->sgr.bg_rgb8 = 16 + r * 36 + g * 6 + b;
}

// CSI [Ps] n
   // announce console status
   // Ps = 6 announce cursor position (reply:CPR)
   // PENDING: <Ps> not 6
static void on_dsr (vt100_handler_s *m_, u8 n)
{
	// TODO:
}

// CSI [Ps1];[Ps2] r
   // set top/bottom margin (scroll region)
   // Ps1 line position of top margin (default:1)
   // Ps2 line position of bottom margin (default:1)
static void on_dec_stbm (vt100_handler_s *m_, u16 tm, u16 bm)
{
	m_->bm = bm;
}

// CSI [Ps1(1-23)];[Ps2];[Ps3] t
   // Ps1 = 22 save window title to stack
   // Ps2 = 0,1,2 save window title
static bool on_window_title_save (vt100_handler_s *m_, u8 index)
{
	if (2 < index)
		return false;
	// TODO:
	return true;
}
   // Ps1 = 23 load window title from stack
   // Ps2 = 0,1,2 load window title
static bool on_window_title_load (vt100_handler_s *m_, u8 index)
{
	if (2 < index)
		return false;
	// TODO:
	return true;
}
   // window manipulation
static void on_dec_slpp (vt100_handler_s *m_, u8 op_id, u8 aux1, u8 aux2)
{
	if (22 == op_id)
		on_window_title_save (m_, aux1);
	else if (23 == op_id)
		on_window_title_load (m_, aux1);
}

// CSI > <Ps> c
   // announce second console features (default:0)
static void on_da2 (vt100_handler_s *m_, u16 n)
{
}

static void on_xt_extscrn_open (vt100_handler_s *m_)
{
	if (m_->state.is_extscrn)
		return;
	m_->state.is_extscrn = 1;
	m_->x_stdscrn = m_->x;
	m_->y_stdscrn = m_->y;
u32 cb;
	if (m_->cb_fg_stdscrn < (cb = m_->fg->save (m_->fg_this_, NULL))) {
		m_->fg_stdscrn = (NULL == m_->fg_stdscrn) ? malloc (cb) : realloc (m_->fg_stdscrn, cb);
ASSERTE(m_->fg_stdscrn)
		m_->cb_fg_stdscrn = cb;
	}
	if (m_->cb_bg_stdscrn < (cb = m_->bg->save (m_->bg_this_, NULL))) {
		m_->bg_stdscrn = (NULL == m_->bg_stdscrn) ? malloc (cb) : realloc (m_->bg_stdscrn, cb);
ASSERTE(m_->bg_stdscrn)
		m_->cb_bg_stdscrn = cb;
	}
struct vt100_cursor_state cs;
u8 is_csrshow;
	if (1 == (is_csrshow = m_->state.is_csrshow))
		cs.is_show = 0, vt100_handler_cursor_state ((struct vt100_handler *)m_, cs);
	m_->bg->save (m_->bg_this_, m_->bg_stdscrn);
	m_->fg->save (m_->fg_this_, m_->fg_stdscrn);
	if (1 == is_csrshow)
		cs.is_show = 1, vt100_handler_cursor_state ((struct vt100_handler *)m_, cs);
}

static void on_xt_extscrn_close (vt100_handler_s *m_)
{
	if (!m_->state.is_extscrn)
		return;
struct vt100_cursor_state cs;
u8 is_csrshow;
	if (1 == (is_csrshow = m_->state.is_csrshow))
		cs.is_show = 0, vt100_handler_cursor_state ((struct vt100_handler *)m_, cs);
	m_->fg->load (m_->fg_this_, m_->fg_stdscrn);
	m_->bg->load (m_->bg_this_, m_->bg_stdscrn);
	m_->x = m_->x_stdscrn;
	m_->y = m_->y_stdscrn;
	if (1 == is_csrshow)
		cs.is_show = 1, vt100_handler_cursor_state ((struct vt100_handler *)m_, cs);
	m_->handler->dirty_notify (m_->handler_this_);
	m_->state.is_extscrn = 0;
}

// CSI ? <Pm> h
   // enable DEC/xterm extend mode
// CSI ? <Pm> l
   // disable DEC/xterm extend mode
static void on_dec_exm (vt100_handler_s *m_, u16 n, bool is_enabled)
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
			on_xt_extscrn_open (m_);
		else
			on_xt_extscrn_close (m_);
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////
// OSC

// OSC [0-2];<title> BEL
static void on_title_chg (vt100_handler_s *m_, const char *title)
{
	// TODO:
}

// OSC 10;? BEL
static void on_vtwin_fgcl_query (vt100_handler_s *m_)
{
	// TODO:
}

// OSC 11;? BEL
static void on_vtwin_bgcl_query (vt100_handler_s *m_)
{
	// TODO:
}

///////////////////////////////////////////////////////////////////////////////
// ESC

// ESC M
   // move cursor up to 1 line
static void on_ri (vt100_handler_s *m_)
{
	vt100_handler_line_back (m_, 1);
}

// ESC =
   // shift to application key-pad mode
static void on_dec_kpam (vt100_handler_s *m_)
{
}

// ESC >
   // shift to normal key-pad mode
static void on_dec_kpnm (vt100_handler_s *m_)
{
}

///////////////////////////////////////////////////////////////////////////////

static unsigned on_bmp (vt100_handler_s *m_, const void *buf, unsigned len)
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// internal

#define ATTR_DEFAULT (ATTR_FG(o_rgb8[7])|ATTR_BG(o_rgb8[0]))
#define UTF8_ERROR_REP 0xE296A0 // utf8to16(e296a0)=25a0='■'
static void on_text_utf8 (vt100_handler_s *m_, const char *src, unsigned cb)
{
const char *p, *q, *end;
	p = q = src, end = src + cb;
	for (; q < end; ++q) {
u8 c;
		c = (u8)*q;
bool is_ctl;
		is_ctl = (c < 0x20 || 0x7f == c || 0xfc < c) ? true : false;
		if (!is_ctl) {
u32 u;
			switch (m_->state.utf8) {
			default:
			//case 0:
				if (!(c < 0xE0) && c < 0xF0)
					m_->state.utf21 = c << 8, m_->state.utf8 = 1;
				else if (!(c < 0xC0) && c < 0xE0)
					m_->state.utf21 = c, m_->state.utf8 = 2;
				else
					u = c;
				break;
			case 1:
ASSERTE(!(c < 0x80) && c < 0xC0)
				m_->state.utf21 |= c, m_->state.utf8 = 2;
				break;
			case 2:
ASSERTE(!(c < 0x80) && c < 0xC0)
				u = m_->state.utf21 << 8 | c, m_->state.utf8 = 0;
				break;
			}
			if (0 == m_->state.utf8) {
				u = (0x20 <= u && u < 0x7F || 0xC000 <= u && u < 0xDFC0 || 0xE00000 <= u && u < 0xFCBFC0) ? u : UTF8_ERROR_REP;
u8 fg_rgb8, bg_rgb8;
				fg_rgb8 = m_->sgr.fg_rgb8, bg_rgb8 = m_->sgr.bg_rgb8;
				if (m_->sgr.reverse)
					swap(fg_rgb8, bg_rgb8, u8);
				if (m_->sgr.fg_bold && fg_rgb8 < 8)
					fg_rgb8 += 8;
u64 a;
				a = ATTR_FG(o_rgb8[fg_rgb8])|ATTR_BG(o_rgb8[bg_rgb8]);
				m_->fg->write_text (m_->fg_this_, m_->x, m_->y, 1, &u, &a, 0);
				m_->handler->dirty_notify (m_->handler_this_);
u16 u_width;
				m_->x += (u_width = wcwidth_bugfix (utf8to16 (u)));
				if (! (m_->x < m_->ws_col))
					m_->x = 0, vt100_handler_line_feed (m_);
			}
			if (q +1 < end)
				continue;
			q = end;
		}
		else /*if (is_ctl) */
			m_->state.utf8 = 0;
		if (q < end) {
const char *text;
			switch (*q) {
			case 0x07: text = VTYY "BEL" VTO; break;
			case 0x08: text = VTYY "\\b" VTO;
				--m_->x;
				break;
			case 0x0A: text = VTGG "LF" VTO;
				vt100_handler_line_feed (m_);
				break;
			case 0x0D: text = VTGG "CR" VTO;
				m_->x = 0;
				break;
			default:
#if 0
				sprintf (cstr, VTGG "\\x%02X" VTO, (u8)*q);
				text = cstr;
#endif
				break;
			}
#if 0
echo ("%s", text);
fsync (STDOUT_FILENO);
			if (0x0A == *p)
echo ("\n");
#endif
		}
		else
			--q;
		p = q +1;
	}
}

static unsigned on_escape_sequence (vt100_handler_s *m_, const char *p, unsigned cb)
{
static const u8 SLEEP = 0, RUNNING = 1, END_NOTIFY = 2;
	if (END_NOTIFY == m_->state.escseq) { // TODO: remove END_NOTIFY (*)need changing vt100_parser_request()
		m_->state.escseq = SLEEP;
		return 0;
	}

	if (SLEEP == m_->state.escseq) {
		vt100_parser_reset (&m_->escseq);
		m_->state.escseq = RUNNING; // kick first
	}

int got;
	if (0 < (got = vt100_parser_request (&m_->escseq, p, cb)))
		return (unsigned)got; // correct sequence
	if (0 == got) {
		m_->state.escseq = SLEEP;
		return (unsigned)got; // notify end of sequence
	}
unsigned skip_bytes;
	skip_bytes = vt100_parser_get_resume (&m_->escseq);
ASSERTE(0 <= skip_bytes)

	if (-1 == got && skip_bytes == cb)
		return skip_bytes; // uncompleted (end of buffer)

	m_->state.escseq = (0 == skip_bytes) ? SLEEP : END_NOTIFY;
	return skip_bytes; // uncompleted (invalid multi-byte) or syntax error or ...
}

static unsigned on_bmp_header (vt100_handler_s *m_, const u8 *p, unsigned cb)
{
u8 got;
	got = min(cb, sizeofBMPHDR - m_->bmp_got);
	memcpy (m_->bmphdr + m_->bmp_got, p, got);
	m_->bmp_got += got, p += got;
	if (m_->bmp_got < sizeofBMPHDR)
		return got;
enum pixel_format pf; u16 width, height;
	m_->bh->header_parse (m_->bmphdr, &pf, &width, &height, &m_->bmp_size);
	m_->bmp_size += sizeofBMPHDR;
u8 lf_width, lf_height;
	lf_width = m_->bg->get_grid_width (m_->bg_this_);
	lf_height = m_->bg->get_grid_height (m_->bg_this_);
u16 rows;
	rows = HIDIV(height, lf_height);
	rows = min(rows, m_->ws_row); // PENDING: image over console height

u16 n_rollup;
	if (0 < (n_rollup = max(m_->y + rows, m_->ws_row) - m_->ws_row)) {
		m_->y -= n_rollup;
		vt100_handler_rollup (m_, n_rollup);
	}
	m_->bg->write_image_start (m_->bg_this_, m_->x, m_->y, width, height, pf);

	m_->aux_lf = max(m_->aux_lf, rows -1);
u16 cols;
	cols = HIDIV(width, lf_width);
	cols = min(m_->x + cols, m_->ws_col) - m_->x; // PENDING: image over console width
	m_->x += cols;
	return got;
}

static unsigned on_bmp_body (vt100_handler_s *m_, const u8 *p, unsigned cb)
{
u32 got;
	if (0 == (got = min(cb, m_->bmp_size - m_->bmp_got)))
		return 0;
u16 width, height;
	m_->bh->header_parse (m_->bmphdr, NULL, &width, &height, NULL);
	// TODO:
	m_->bg->write_image (m_->bg_this_, p, got);
	m_->bmp_got += got;
	return got;
}

///////////////////////////////////////////////////////////////////////////////
// method

int vt100_handler_request (struct vt100_handler *this_, const char *buf, unsigned cb)
{
static const u8 UTF8 = 0, ESCAPE_FOUND = 1, ESCAPE_SEQUENCE = 2, BITMAP_ACCEPT = 3;
vt100_handler_s *m_;
	m_ = (vt100_handler_s *)this_;
const char *p, *end;
	p = buf, end = buf + cb;
	while (p < end) {
const char *q;
u32 got;
		switch (m_->state.parse_shift) {
		default:
		//case UTF8:
			if (NULL == (q = (const char *)memchr (p, '\033', end - p)))
				q = end;
			if (p < q)
				on_text_utf8 (m_, p, q - p);
			if ((p = q) < end)
				++p, m_->state.parse_shift = ESCAPE_FOUND;
			break;
		case ESCAPE_FOUND:
			if ('\033' == *p) {
				++p, m_->state.parse_shift = BITMAP_ACCEPT, m_->bmp_got = 0;
				break;
			}
			m_->state.parse_shift = ESCAPE_SEQUENCE;
			// fall through
		case ESCAPE_SEQUENCE:
			for (got = 1/*guard*/; 0 < got && p < end; p += got)
				got = on_escape_sequence (m_, p, end - p);
			if (0 == got)
				m_->state.parse_shift = UTF8;
			break;
		case BITMAP_ACCEPT:
			if (m_->bmp_got < sizeofBMPHDR)
				p += on_bmp_header (m_, (const u8 *)p, end - p);
			for (got = 1 /*guard*/; 0 < got && p < end; p += got)
				got = on_bmp_body (m_, (const u8 *)p, end - p);
			if (0 == got)
				m_->state.parse_shift = UTF8;
			break;
		}
	}
	return (int)(p - buf);
}

void vt100_handler_cursor_state (struct vt100_handler *this_, struct vt100_cursor_state cs)
{
vt100_handler_s *m_;
	m_ = (vt100_handler_s *)this_;
u32 rgb24;
	rgb24 = (cs.is_show) ? o_rgb8[256] : o_rgb8[0];
u64 a;
	a = ATTR_BG(rgb24);
	m_->fg->write_text (m_->fg_this_, m_->x, m_->y, 1, NULL, &a, ATTR_BG_MASK);
	m_->state.is_csrshow = cs.is_show;
	m_->handler->dirty_notify (m_->handler_this_);
}

///////////////////////////////////////////////////////////////////////////////
// ctor/dtor

static void vt100_handler_reset (vt100_handler_s *m_)
{
	vt100_parser_reset (&m_->escseq);
	memset (&m_->state, 0, sizeof(m_->state));
	memset (&m_->sgr, 0, sizeof(m_->sgr));
	m_->bg = NULL, m_->bg_this_ = NULL;
	m_->fg = NULL, m_->fg_this_ = NULL;
	m_->handler = NULL;
	m_->ws_col = m_->ws_row = 0;
	m_->x = m_->y = 0;
	m_->bm = 0;
	m_->aux_lf = 0;
	m_->fg_stdscrn = NULL;
	m_->bg_stdscrn = NULL;
	m_->cb_fg_stdscrn = 0;
	m_->x_stdscrn = m_->y_stdscrn = 0;
int n;
	for (n = 0; n < arraycountof(o_rgb8); ++n)
		o_rgb8[n] = xterm_256color (n);
}

void vt100_handler_release (struct vt100_handler *this_)
{
vt100_handler_s *m_;
	m_ = (vt100_handler_s *)this_;
	if (m_->fg_stdscrn)
		free (m_->fg_stdscrn);
	if (m_->bg_stdscrn)
		free (m_->bg_stdscrn);
	vt100_handler_reset (m_);
	vt100_parser_release (&m_->escseq);
}

#ifdef __cplusplus
void vt100_handler_dtor (struct vt100_handler *this_)
{
}
#endif // __cplusplus

static struct vt100_event_i o_iface = {
	.csi_ich               = on_ich,               // CSI @
	.csi_cuu               = on_cuu,               // CSI A
	.csi_cuf               = on_cuf,               // CSI C
	.csi_cup               = on_cup,               // CSI H
	.csi_ed                = on_ed,                // CSI J
	.csi_el                = on_el,                // CSI K
	.csi_il                = on_il,                // CSI L
	.csi_dl                = on_dl,                // CSI M
	.csi_dch               = on_dch,               // CSI P
	.csi_sgr_none          = on_sgr_none,          // CSI 0m
	.csi_sgr_bold          = on_sgr_bold,          // CSI 1|22m
	.csi_sgr_italic        = on_sgr_italic,        // CSI 3|23m
	.csi_sgr_reverse       = on_sgr_reverse,       // CSI 7|27m
	.csi_sgr_strikethrough = on_sgr_strikethrough, // CSI 9|29m
	.csi_sgr_fgcl          = on_sgr_fgcl,          // CSI 3[0-7]m 38;5;[0-255]m
	.csi_sgr_fgcl_raw      = on_sgr_fgcl_raw,      // CSI 38;2[;:][0-255][;:][0-255][;:][0-255]m
	.csi_sgr_bgcl          = on_sgr_bgcl,          // CSI 4[0-7]m 48;5;[0-255]m
	.csi_sgr_bgcl_raw      = on_sgr_bgcl_raw,      // CSI 48;2[;:][0-255][;:][0-255][;:][0-255]m
	.csi_dsr               = on_dsr,               // CSI 5|6n
	.csi_dec_stbm          = on_dec_stbm,          // CSI r
	.csi_dec_slpp          = on_dec_slpp,          // CSI [1-23];<n>;<n>t
	.csi_da2               = on_da2,               // CSI > c
	.csi_dec_exm           = on_dec_exm,           // CSI ? [1-8200]h
	                                               // CSI ? [1-8200]l
	.osc_title_chg         = on_title_chg,         // OSC [0-2];<title>BEL
	.osc_vtwin_fgcl_query  = on_vtwin_fgcl_query,  // OSC 10;?BEL
	.osc_vtwin_bgcl_query  = on_vtwin_bgcl_query,  // OSC 11;?BEL
	.esc_ri                = on_ri,                // ESC M
	.esc_dec_kpam          = on_dec_kpam,          // ESC =
	.esc_dec_kpnm          = on_dec_kpnm,          // ESC >
	.esc_bmp               = on_bmp,
};
void vt100_handler_ctor (
	  struct vt100_handler *this_, unsigned cb
	, struct VRAM_i *fg, VRAM_s *fg_this_
	, struct VRAM_i *bg, VRAM_s *bg_this_
	, struct parser_event_i *handler, parser_event_s *handler_this_
)
{
#ifndef __cplusplus
BUGc(sizeof(vt100_handler_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(vt100_handler_s), cb)
#endif //ndef __cplusplus
vt100_handler_s *m_;
	m_ = (vt100_handler_s *)this_;
	vt100_parser_ctor (&m_->escseq, sizeof(m_->escseq), &o_iface, m_);
	bmp_helper_ctor (&m_->bh_this_, sizeof(m_->bh_this_));
	vt100_handler_reset (m_);
	vt100_handler_read_config ();
	m_->bh = bmp_helper_query_img_helper_i (&m_->bh_this_);
	on_sgr_none (m_);
	m_->handler = handler;
	m_->handler_this_ = handler_this_;
	m_->bg = bg, m_->bg_this_ = bg_this_;
	m_->fg = fg, m_->fg_this_ = fg_this_;
	m_->ws_col = m_->fg->get_ws_col (m_->fg_this_);
	m_->ws_row = m_->fg->get_ws_row (m_->fg_this_);
}
