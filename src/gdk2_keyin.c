#define _POSIX_C_SOURCE 200809L /* POSIX.1-2008 dprintf
#define _POSIX_C_SOURCE 200112L    POSIX.1-2001 strtoul
#define _POSIX_C_SOURCE 199309L    POSIX.1b-1993 nanosleep */
//#define _FILE_OFFSET_BITS 64
#include <features.h>
#include <endian.h>
#include <stdint.h>

#include "gdk2_keyin.h"
#include "pseudo_vt100.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // exit
#include <unistd.h> // STDERR_FILENO
typedef uint32_t u32;
typedef  uint8_t u8, u1;
typedef uint16_t u16;

#define usizeof(x) (unsigned)sizeof(x)
#define arraycountof(x) (sizeof(x)/(sizeof((x)[0])))

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

#define unexpected assert
#define caller_bug assert

///////////////////////////////////////////////////////////////
// endian portability

static void store_le32 (void *dst, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
#else
	val =       0xff  & val >> 24
	    |     0xff00  & val >> 8
	    |   0xff0000  & val << 8
	    | 0xff000000U & val << 24;
#endif
	*(u32 *)dst = val;
}

///////////////////////////////////////////////////////////////

#define LALT  1
#define RALT  2
#define LCTRL 4
#define RCTRL 8

static u32 _parse_gdk2key (u16 keycode, u1 is_keydown, u8 *keyshift)
{
caller_bug(keyshift)

u8 hi8
	= /*0xff & */keycode >> 8;
unexpected(0x00 == hi8 || 0xff == hi8)

	if (0x00 == hi8) {
		// Alt + <anykey>
		if ((LALT|RALT) & *keyshift) {
			//*keyshift &= ~(LALT|RALT); // unshift Alt
			return 0; // state changed only
		}
u8 ansi8;
		// Ctrl + <anykey>
		if ((LCTRL|RCTRL) & *keyshift) {
			if (! ('a' <= keycode && keycode <= 'z'))
				return 0; // unknown, do nothing
			ansi8 = (u8)(keycode -'a' +1);
		}
		// <full key>
		else
			ansi8 = (u8)keycode;

		if (!is_keydown)
			return 0; // key up, do nothing
		return ansi8;
	}

//assert(0xff == hi8)
u8 low8 = 0xff & keycode;
u8 mask;
	switch (low8) {
	// meta key
	case 0xE9: case 0xEA: case 0xE3: case 0xE4:
		mask =   (0xE9 == low8) ?   LALT
		     :   (0xEA == low8) ?   RALT
		     :   (0xE3 == low8) ?   LCTRL
		     : /*(0xE4 == low8) ?*/ RCTRL
		     ;
		if (is_keydown)
			*keyshift |= mask;
		else
			*keyshift &= ~mask;
		return 0; // state changed only

	// BS TAB ENTER ESC
	case 0x08:
	case 0x09:
	case 0x0D:
	case 0x1B:
		if (!is_keydown)
			return 0; // key up, do nothing
		return low8; // bypass

	// others
	default:
		break;
	}

	// convert into vt100 escape sequence
static const struct {
	u8 low8;
	u32 vt100;
} t[] = {
	  { 0x51, '\033' | '[' << 8 | 'D' << 16 } //  left arrow key
	, { 0x52, '\033' | '[' << 8 | 'A' << 16 } //    up arrow key
	, { 0x53, '\033' | '[' << 8 | 'C' << 16 } // right arrow key
	, { 0x54, '\033' | '[' << 8 | 'B' << 16 } //  down arrow key
	, { 0xff, '\033' | '[' << 8 | '3' << 16 | '\176' << 24 } // DEL
};
unsigned i;
	for (i = 0; i < arraycountof(t); ++i)
		if (t[i].low8 == low8) {
			if (!is_keydown)
				return 0; // key up, do nothing
			return t[i].vt100;
		}
	return 0; // igonre keycode, do nothing
}

///////////////////////////////////////////////////////////////
// singlton memory

typedef struct {
	u8 keyshift;
#ifdef __i386__
//	u8 align4_padding[];
#else //def __x86_64__
	u8 align8_padding[7];
#endif
} gdk2key_s;

static gdk2key_s s_singlton = {0};

///////////////////////////////////////////////////////////////

static bool _on_key_gdk2 (u16 keycode, u1 is_keydown)
{
gdk2key_s *m_ = &s_singlton;

u32 vt100_le
	= _parse_gdk2key (keycode, is_keydown, &m_->keyshift);
u1 keyup_or_metakey_or_outofVT100
	= (0 == vt100_le) ? TRUE : FALSE;
	if (keyup_or_metakey_or_outofVT100)
		return false;

char vt100_s[4];
	store_le32 (vt100_s, vt100_le);
unsigned vt100_n
	= (vt100_le < 0x100    ) ? 1 :
	  (vt100_le < 0x10000  ) ? 2 :
	  (vt100_le < 0x1000000) ? 3 : 4;
int sent
	= pseudo_vt100->write_input (vt100_s, vt100_n);
	if (sent < 1)
		return false;

	return true;
}

///////////////////////////////////////////////////////////////
// readability sugar

static const READABILITY_SUGAR_GDK2_KEYIN _gdk2_keyin = {
	.on_key = _on_key_gdk2
};
const READABILITY_SUGAR_GDK2_KEYIN *gdk2_keyin = &_gdk2_keyin;
