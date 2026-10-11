#define _XOPEN_SOURCE 600 // SUSv3 (includes POSIX.1-2001) wcwidth
#include <features.h>
#include <stdint.h>

#include <wchar.h> // wchar_t wcwidth
#include "unicode.h"

typedef uint32_t u32, u21;

///////////////////////////////////////////////////////////////

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

u21 u21from (u32 utf8)
{
u21 unicode;
	if (utf8 < 0x100)
		unicode = (u21)utf8;
	else if (utf8 < 0x10000)
		unicode = (u21)(0xFC0 & utf8 >>  8 -6
		              | 0x03F & utf8);
	else if (utf8 < 0x1000000)
		unicode = (u21)(0xF000 & utf8 >> 16 -12
		              | 0x0FC0 & utf8 >>  8 -6
		              | 0x003F & utf8);
	else // NOTE: not tested
		unicode = (u21)(0x70000 & utf8 >> 24 -16
		              | 0x0F000 & utf8 >> 16 -12
		              | 0x00FC0 & utf8 >>  8 -6
		              | 0x0003F & utf8);
	return unicode;
}
