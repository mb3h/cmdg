#include <stdint.h>
#include "base64.h"

#include <stdio.h>
#include <stdlib.h> // exit

typedef uint32_t u32;
typedef  uint8_t u8;

unsigned base64_decode (const char *src, void *dst_, unsigned cb)
{
unsigned retval; u8 *dst;
	retval = 0, dst = (u8 *)dst_;
	while (!dst || retval < cb) {
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
		for (i = 2; 3 -i < n; --i, ++retval) {
			if (!dst)
				continue;
			if (! (retval < cb))
				return 0; // ERROR: buffer overflow
			*(dst +retval) = (u8)(0xff & u >> i * 8);
		}
		if (n < 4)
			break;
	}
	return retval;
}
