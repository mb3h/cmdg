#include <gtk/gtk.h>

#include <stdint.h>
struct gdk2_helper_;
#define gdk_helper_s struct gdk2_helper_
#include "gdk2_helper.hpp"

#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <string.h>
#include <errno.h>
typedef  uint8_t u8;
typedef  int32_t s32;

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

#define LALT  1
#define RALT  2
#define LCTRL 4
#define RCTRL 8

typedef struct gdk2_helper_ {
	u8 keyshift;
#ifdef __x86_64__
	u8 padding_1[7];
#else //def __i386__
	u8 padding_1[3];
#endif
} gdk2_helper_s;

///////////////////////////////////////////////////////////////////////////////
// dtor/ctor

static void gdk2_helper_reset (gdk2_helper_s *m_)
{
#ifndef __cplusplus
	memset (m_, 0, sizeof(gdk2_helper_s));
#else //def __cplusplus
	m_->keyshift = 0;
#endif // __cplusplus
}

void gdk2_helper_release (struct gdk2_helper *this_)
{
gdk2_helper_s *m_;
	m_ = (gdk2_helper_s *)this_;
	gdk2_helper_reset (m_);
}

#ifdef __cplusplus
void gdk2_helper_dtor (struct gdk2_helper *this_)
{
}
#endif // __cplusplus

void gdk2_helper_ctor (struct gdk2_helper *this_, unsigned cb)
{
#ifndef __cplusplus
BUGc(sizeof(gdk2_helper_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(gdk2_helper_s), cb)
#endif //ndef __cplusplus
gdk2_helper_s *m_;
	m_ = (gdk2_helper_s *)this_;
	gdk2_helper_reset (m_);
}

///////////////////////////////////////////////////////////////////////////////
// interface

static s32 gdk2_helper_keycode_to_vt100 (gdk2_helper_s *m_, GdkEventKey *event)
{
static const struct {
	u8 gdk;
	s32 vt100;
} *p, conv[] = {
	  { 0x51, '\033' | '[' << 8 | 'D' << 16 } //  left arrow key
	, { 0x52, '\033' | '[' << 8 | 'A' << 16 } //    up arrow key
	, { 0x53, '\033' | '[' << 8 | 'C' << 16 } // right arrow key
	, { 0x54, '\033' | '[' << 8 | 'B' << 16 } //  down arrow key
	, { 0xff, '\033' | '[' << 8 | '3' << 16 | '\176' << 24 } // DEL
	, { 0, 0 } // safety
};

guint keycode;
	if (0xffff < (keycode = event->keyval))
		return -1; // unexpected
u8 lo, hi;
	lo = 0xff & keycode,
	hi = /*0xff & */keycode >> 8;
bool keydown;
	keydown = (GDK_KEY_PRESS == event->type) ? true : false;
	if (0xff == hi) {
u8 mask;
		// meta key
		switch (lo) {
		case 0xE9: case 0xEA: case 0xE3: case 0xE4:
			mask = (0xE9 == lo) ? LALT
			     : (0xEA == lo) ? RALT
			     : (0xE3 == lo) ? LCTRL
			     :                RCTRL
			     ;
			if (keydown)
				m_->keyshift |= mask;
			else
				m_->keyshift &= ~mask;
			return 0;
		}
		switch (lo) {
		// forward
		case 0x08: // BS
		case 0x09: // TAB
		case 0x0D: // ENTER
		case 0x1B: // ESC
			break;
		// replace as vt100 escape sequence
		default:
			for (p = conv; 0 < p->vt100; ++p)
				if (p->gdk == lo)
					break;
			lo = p->vt100; // TODO:
			break;
		}
		// ignored
		if (0 == lo)
			return 0;
	}
	else if (0x00 == hi) {
		// Alt + <anykey>
		if ((LALT|RALT) & m_->keyshift) {
			m_->keyshift &= ~(LALT|RALT); // unshift Alt
			return 0; // ignored
		}
		// Ctrl + <anykey>
		if ((LCTRL|RCTRL) & m_->keyshift && 'a' <= lo && lo <= 'z')
			lo -= 'a' -1;
		// <full key>
	}
	else
		return -1; // not 00XX FFXX [unexpected]

	// key up
	if (!keydown)
		return 0;
	return lo;
}

static bool gdk2_helper_is_redraw_expose (gdk2_helper_s *m_, const GdkRectangle *r)
{
/* happen: GTK+ 2.24.32-4ubuntu4
           Ubuntu 20.04.3 (Lubntu Desktop)
   note:   window edge activate/deactivate (?)
 */
	if (2 == r->width || 6 == r->width || 6 == r->height)
		return false;
	return true;
}

///////////////////////////////////////////////////////////////////////////////

static struct gdk_helper_i o_gdk_helper_i = {
	.keycode_to_vt100 = gdk2_helper_keycode_to_vt100,
	.is_redraw_expose = gdk2_helper_is_redraw_expose,
};
struct gdk_helper_i *gdk2_helper_query_gdk_helper_i (struct gdk2_helper *this_) { return &o_gdk_helper_i; }
