#define _XOPEN_SOURCE 600 // SUSv3 (includes POSIX.1-2001) wcwidth
#include <features.h>

#include <stdint.h>
#include <wchar.h> // wchar_t wcwidth
#include "unicode.h"

#include "vt100.h"
#include "portab.h"

#include <stdbool.h>
typedef uint32_t u32;
typedef uint16_t u16;

///////////////////////////////////////////////////////////////////////////////
// non-state

int wcwidth_bugfix (wchar_t c)
{
	if (0x00D7 == c // ×
	 || 0x203B == c // ※
	 || 0x2160 <= c && c < 0x216A // ⅠⅡⅢⅣⅤⅥⅦⅧⅨⅩ
	 || 0x2170 <= c && c < 0x217A // ⅰⅱⅲⅳⅴⅵⅶⅷⅸⅹ
	 || 0x2190 <= c && c < 0x2194 // ←↑→↓
	 || 0x25A0 <= c && c < 0x25A2 // ■□
	 || 0x25B2 <= c && c < 0x25B4 // ▲△
	 || 0x25C6 <= c && c < 0x25C8 // ◆◇
	 || 0x25CB == c || 0x25CE == c || 0x25CF == c // ○◎●
	 || 0x2605 <= c && c < 0x2607 // ★☆
	   )
		return 2;
	return wcwidth (c);
}

u16 utf8to16 (u32 u)
{
	if (u < 0x100)
		return (u16)u;
	else if (u < 0x10000)
		return (u16)(0x0FC0 & u >> 2 | 0x003F & u);
	else /*if (u < 0x1000000)*/
		return (u16)(0xF000 & u >> 4 | 0x0FC0 & u >> 2 | 0x003F & u);
}
