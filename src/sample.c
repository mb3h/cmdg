#ifdef WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <io.h> // _setmode
# include <fcntl.h> // O_BINARY

# define __LITTLE_ENDIAN 1234
# define __BIG_ENDIAN 4321
typedef DWORD uint32_t;
typedef  BYTE  uint8_t;
typedef  WORD uint16_t;

# define __BYTE_ORDER 1234
# pragma warning (disable:4127)
#else
# define _XOPEN_SOURCE 500 // usleep (>= 500) clock_gettime (_POSIX_C_SOURCE >= 199309L)
# include <features.h>
# include <errno.h>
# include <time.h>
# include <stdint.h>
# include <stddef.h>
# include <endian.h>
# include <unistd.h>
//# define DEFAULT_SPEED_FROM_TERMIOS
# ifdef DEFAULT_SPEED_FROM_TERMIOS
#  include <termios.h> // cfgetospeed
# endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <malloc.h>
#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

typedef uint32_t u32;
typedef  uint8_t u8;
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

#ifdef WIN32
static void msleep (u32 ms)
{
	Sleep (ms);
}
static void get_boottime (u32 *now)
{
	*now = GetTickCount ();
}
static u32 get_past_msec (const u32 *base)
{
	return GetTickCount () - *base;
}
#else // unix family
static void msleep (u32 ms)
{
	usleep (ms * 1000);
}
static void get_boottime (struct timespec *now)
{
	clock_gettime (CLOCK_BOOTTIME, now);
}
static u32 get_past_msec (const struct timespec *base)
{
	u32 retval;
	struct timespec now;

	clock_gettime (CLOCK_BOOTTIME, &now);
	retval = now.tv_sec - base->tv_sec;
	if (now.tv_nsec < base->tv_nsec)
		retval = (retval -1) * 1000 + (now.tv_nsec +1000000000 - base->tv_nsec) / 1000000;
	else
		retval = retval * 1000 + (now.tv_nsec - base->tv_nsec) / 1000000;
	return retval;
}
# ifdef DEFAULT_SPEED_FROM_TERMIOS
static int speed_to_baud (speed_t speed)
{
	switch (speed) {
	case B0      : return 0      ;
	case B50     : return 50     ;
	case B75     : return 75     ;
	case B110    : return 110    ;
	case B134    : return 134    ;
	case B150    : return 150    ;
	case B200    : return 200    ;
	case B300    : return 300    ;
	case B600    : return 600    ;
	case B1200   : return 1200   ;
	case B1800   : return 1800   ;
	case B2400   : return 2400   ;
	case B4800   : return 4800   ;
	case B9600   : return 9600   ; //   1200[byte/s]
	case B19200  : return 19200  ; //   2400[byte/s]
	case B38400  : return 38400  ; //   4800[byte/s]
	case B57600  : return 57600  ; //   9600[byte/s]
	case B115200 : return 115200 ; //  14400[byte/s]
	case B230400 : return 230400 ; //  28800[byte/s]
	case B460800 : return 460800 ; //  57600[byte/s]
	case B500000 : return 500000 ; //  62500[byte/s]
	case B576000 : return 576000 ; //  72000[byte/s]
	case B921600 : return 921600 ; // 115200[byte/s]
	case B1000000: return 1000000; // 125000[byte/s]
	case B1152000: return 1152000; // 144000[byte/s]
	case B1500000: return 1500000; // 187500[byte/s]
	case B2000000: return 2000000; // 250000[byte/s]
	case B2500000: return 2500000; // 312500[byte/s]
	case B3000000: return 3000000; // 375000[byte/s]
	case B3500000: return 3500000; // 437500[byte/s]
	case B4000000: return 4000000; // 500000[byte/s]
	}
	return -1;
}
# endif
#endif

int main (int argc, const char *argv[])
{
	int width, height;
	int padding;
	size_t cbUsed, cbLine, sent, sendable;
	u8 header[0x36], *lpvBlue00to, *lpvBlue99to, *bpp24;
	int obps;
	u32 ms;
#ifdef WIN32
	BOOL bBinModeOld;
	u32 base;
#else // unix family
# ifdef DEFAULT_SPEED_FROM_TERMIOS
	speed_t ospeed;
	struct termios tio;
# endif
	struct timespec base;
#endif
	u8 r, g, b;
	int x, y;

	obps = 0;
#ifndef WIN32 // unix family
# ifdef DEFAULT_SPEED_FROM_TERMIOS
	if (-1 == tcgetattr (STDOUT_FILENO, &tio)) {
fprintf (stderr, "!tcgetattr() @err=%d'%s'" "\n", errno, strerror (errno));
		return -1;
	}
	obps = speed_to_baud (ospeed = cfgetospeed (&tio));
# endif
#endif
	if (2 < argc && 0 == strcmp ("-b", argv[1]))
		obps = atoi (argv[2]);
	else if (2 < argc && 0 == strcmp ("-B", argv[1]))
		obps = atoi (argv[2]) * 8;

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
//		fwrite (&cbUsed, 1, sizeof(cbUsed), stdout);
		fwrite (header, 1, sizeof(header), stdout);
		get_boottime (&base);
		sent = 0;
		while (sizeof(header) + sent < cbUsed) {
			sendable = cbUsed;
			if (0 < obps) {
				ms = get_past_msec (&base);
				sendable = min(ms * (unsigned)obps / 8000, cbUsed);
			}
			if (sizeof(header) + sent < sendable)
				sent += fwrite (lpvBlue00to + sent, 1, sendable - (sizeof(header) + sent), stdout);
			if (0 < obps && sizeof(header) + sent < cbUsed)
				msleep (1);
		}
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
//		fwrite (&cbUsed, 1, sizeof(cbUsed), stdout);
		fwrite (header, 1, sizeof(header), stdout);
		get_boottime (&base);
		sent = 0;
		while (sizeof(header) + sent < cbUsed) {
			sendable = cbUsed;
			if (0 < obps) {
				ms = get_past_msec (&base);
				sendable = min(ms * (unsigned)obps / 8000, cbUsed);
			}
			if (sizeof(header) + sent < sendable)
				sent += fwrite (lpvBlue99to + sent, 1, sendable - (sizeof(header) + sent), stdout);
			if (0 < obps && sizeof(header) + sent < cbUsed)
				msleep (1);
		}
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
