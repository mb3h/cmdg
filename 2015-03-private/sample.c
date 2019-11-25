#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h> // _setmode
#include <fcntl.h> // O_BINARY

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
typedef DWORD uint32_t;
typedef BYTE uint8_t;
typedef WORD uint16_t;

#define __BYTE_ORDER 1234
#pragma warning (disable:4127)
#else
#include <stdint.h>
#include <endian.h>
#endif

#include <stdio.h>
#include <memory.h>
#include <malloc.h>

typedef uint32_t u32;
typedef uint8_t u8;
typedef uint16_t u16;

static void store_le32 (void *p, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u32 *)p = val;
#else
	*(u32 *)p = 0xFF000000UL & val << 24 | 0xFF0000 & val << 8 | 0xFF00 & val >> 8 | 0xFF & val >> 24;
#endif
}

static void store_le16 (void *p, u16 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u16 *)p = val;
#else
	*(u16 *)p = 0xFF00 & val << 8 | 0xFF & val >> 8;
#endif
}

int main (int argc, const char *argv[])
{
	int width, height;
	int padding;
	size_t cbUsed, cbLine;
	u8 header[0x36], *lpvBlue00to, *lpvBlue99to, *bpp24;
#ifdef WIN32
	BOOL bBinModeOld;
#endif
	u8 r, g, b;
	int x, y;

#ifdef WIN32 // avoid C4100
	argc; argv;
#endif

	width = 512;
	height = 128;
	padding = 3 - (3 * width + 3) % 4;
	cbLine = 3 * width + padding;
	cbUsed = 0x36 + cbLine * height;
	lpvBlue00to = lpvBlue99to = NULL;
	do {
		memset (header, 0, sizeof(header));
		store_le16 (header +0x00, 'M'<<8|'B');
		store_le32 (header +0x02, sizeof(header) + height * cbLine);
		store_le32 (header +0x0A, sizeof(header));
		store_le32 (header +0x0E, 0x28); // sizeof(BITMAPINFOHEADER)
		store_le32 (header +0x12, (u32)width);
		store_le32 (header +0x16, (u32)height);
		store_le16 (header +0x1A, 1);
		store_le16 (header +0x1C, 24);
		store_le32 (header +0x22, (u32)(height * cbLine));

		if (NULL == (lpvBlue00to = (u8 *)malloc (cbUsed - sizeof(header)))
		 || NULL == (lpvBlue99to = (u8 *)malloc (cbUsed - sizeof(header)))
		)
			return -1;
		memset (lpvBlue00to, 0, cbUsed - sizeof(header));
		memset (lpvBlue99to, 0, cbUsed - sizeof(header));
		for (b = 0; b < 8; ++b)
			for (g = 0; g < 8; ++g)
				for (r = 0; r < 8; ++r)
					for (y = 1; y < 16; ++y)
						for (x = 0; x < 15; ++x) {
							if (b < 4)
								bpp24 = &lpvBlue00to[cbLine * (16 * (7 - r) +y) + 3 * (16 * (8 * b + g) +x)];
							else
								bpp24 = &lpvBlue99to[cbLine * (16 * (7 - r) +y) + 3 * (16 * (8 * (b -4) + g) +x)];
							*bpp24++ = (u8)((0 == b) ? 0x00 : (b * 2 +1) * 0x11);
							*bpp24++ = (u8)((0 == g) ? 0x00 : (g * 2 +1) * 0x11);
							*bpp24++ = (u8)((0 == r) ? 0x00 : (r * 2 +1) * 0x11);
						}

		fprintf (stdout, "RGB=000000 .. FFFF77" "\n");
#ifdef WIN32
		fflush (stdout); // may be need
		bBinModeOld = _setmode (_fileno (stdout), O_BINARY);
#endif
		fwrite ("\033\033", 1, 2, stdout);
		fwrite (&cbUsed, 1, sizeof(cbUsed), stdout);
		fwrite (header, 1, sizeof(header), stdout);
		fwrite (lpvBlue00to, 1, cbUsed - sizeof(header), stdout);
#ifdef WIN32
		fflush (stdout); // must be need
		_setmode (_fileno (stdout), bBinModeOld);
#endif
		fprintf (stdout, "R=00" "\n");

		fprintf (stdout, "RGB=000099 .. FFFFFF" "\n");
#ifdef WIN32
		fflush (stdout); // may be need
		bBinModeOld = _setmode (_fileno (stdout), O_BINARY);
#endif
		fwrite ("\033\033", 1, 2, stdout);
		fwrite (&cbUsed, 1, sizeof(cbUsed), stdout);
		fwrite (header, 1, sizeof(header), stdout);
		fwrite (lpvBlue99to, 1, cbUsed - sizeof(header), stdout);
#ifdef WIN32
		fflush (stdout); // must be need
		_setmode (_fileno (stdout), bBinModeOld);
#endif
		fprintf (stdout, "R=00" "\n");
	} while (0);

	if (lpvBlue00to) {
		free (lpvBlue00to);
		lpvBlue00to = NULL;
	}
	if (lpvBlue99to) {
		free (lpvBlue99to);
		lpvBlue99to = NULL;
	}
	return 0;
}
