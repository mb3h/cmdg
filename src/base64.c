#include <stdio.h> // FILE
#include "defs.h"
#include "base64.h"
#include "vt100.h"
extern FILE *stderr_old;

typedef uint32_t u32;
typedef  uint8_t u8;

size_t base64_decode (const char *src, void *dst_, size_t cb)
{
u8 *dst, *end;
	dst = (u8 *)dst_, end = dst + cb;
	while (dst < end) {
u32 u;
		u = 0;
int n;
		for (n = 0; n < 4; ++n) {
			u <<= 6;
char c;
			c = *src++;
			if ('A' <= c && c <= 'Z')
				c -= 'A';
			else if ('a' <= c && c <= 'z')
				c -= 'a' -26;
			else if ('0' <= c && c <= '9')
				c -= '0' -52;
			else if ('+' == c)
				c = 62;
			else if ('/' == c)
				c = 63;
			//else if ('=' == c)
			else {
				u <<= (3 -n) * 6;
				break;
			}
			u |= (u8)c;
		}
int i;
		for (i = 2; 3 -i < n && dst < end; --i)
			*dst++ = (u8)(0xff & u >> i * 8);
		if (n < 4)
			break;
	}
	return dst - (u8 *)dst_;
}
