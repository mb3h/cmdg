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
typedef    int int32_t;
typedef  short int16_t;

# define __BYTE_ORDER 1234
# pragma warning (disable:4127)
#else
# define _XOPEN_SOURCE 500 // usleep (>= 500) clock_gettime (_POSIX_C_SOURCE >= 199309L)
# include <features.h>
# include <errno.h>
# include <time.h>
# include <stdint.h>
# include <stdbool.h>
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
typedef int32_t s32;
typedef int16_t s16;

#define arraycountof(x) (sizeof(x)/sizeof((x)[0]))
#define RUPDIV(a, value) (((value) + (a) -1) / (a))
#define PADDING(a, value) ((a) -1 - ((value) * (a) + (a) -1) % (a))
#define RGB(r,g,b) (0xff0000 & (r) << 16 | 0xff00 & (g) << 8 | 0xff & (b))

static void store_le32 (void *p, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u32 *)p = val;
#else
	*(u32 *)p = 0xff000000UL & val << 24 | 0xff0000 & val << 8 | 0xff00 & val >> 8 | 0xff & val >> 24;
#endif
}
static void store_be32 (void *p, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u32 *)p = 0xff000000UL & val << 24 | 0xff0000 & val << 8 | 0xff00 & val >> 8 | 0xff & val >> 24;
#else
	*(u32 *)p = val;
#endif
}
static u32 load_be32 (const void *src_)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
u32 val;
	val = *(const u32 *)src_;
	return 0xff & val >> 24 | 0xff00 & val >> 8 | 0xff0000 & val << 8 | 0xff000000UL & val << 24;
#else
	return *(const u32 *)src_;
#endif
}
static void store_le16 (void *p, u16 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u16 *)p = val;
#else
	*(u16 *)p = 0xff00 & val << 8 | 0xff & val >> 8;
#endif
}
static void store_le24 (void *p_, u32 val)
{
u8 *p;
	p = (u8 *)p_;
	*p++ = 0xff & val;
	*p++ = 0xff & val >> 8;
	*p++ = 0xff & val >> 16;
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

static bool bitblt_nor (size_t width, size_t height, const void *bpp1_src_, u32 rgb, void *bpp24_dst_, size_t cb_dst)
{
	if (! (0 < width && 0 < height && bpp1_src_ && bpp24_dst_ && 0 < cb_dst))
		return false;
const u8 *bpp1_src;
	bpp1_src = (const u8 *)bpp1_src_;
u8 *bpp24_dst;
	bpp24_dst = (u8 *)bpp24_dst_;
size_t opl_src, opl_dst; // octet(byte) per line
	opl_src = RUPDIV(32, width) * 4;
	opl_dst = width * 3 + PADDING(4, width * 3);
	if (cb_dst < opl_dst * height)
		return false;
size_t y, x;
	for (y = 0; y < height; ++y)
		for (x = 0; x < width; x += 32) {
u32 dw;
			dw = load_be32 (bpp1_src + y * opl_src + x / 8);
size_t i;
			for (i = 0; i < 32; ++i) {
				if (! (1 & dw >> 31 -i))
					continue;
				store_le24 (bpp24_dst + (height -1 - y) * opl_dst + (x +i) * 3, rgb);
			}
		}
	return true;
}
static bool enum_arch (size_t resolution_radian, u16 width,
                       u16 height, u16 x_begin, u16 y_begin,
                       s16 x_center, s16 y_center, void *bpp1_dst_,
                       size_t cb_dst,
                       bool (*lpfn)(u16 x, u16 y, void *bpp1_dst,
                                    size_t opl_dst, void *param),
                       void *param
                       )
{
	if (! (0 < width && 0 < height && x_begin < width && y_begin < height && bpp1_dst_ && 0 < cb_dst))
		return false;
u8 *bpp1_dst;
	bpp1_dst = (u8 *)bpp1_dst_;
size_t opl_dst; // octet(byte) per line
	opl_dst = RUPDIV(32, width) * 4;
s32 x, y;
	x = (s32)x_begin - (s32)x_center;
	y = (s32)y_center - (s32)y_begin;
	x <<= resolution_radian;
	y <<= resolution_radian;
s32 x_bmp_prev, y_bmp_prev;
	x_bmp_prev = y_bmp_prev = -1;
bool is_abort;
	is_abort = false;
size_t i;
	for (i = 0; !is_abort && i < 1 << resolution_radian -1; ++i) {
		// dot pixel
s32 x_bmp, y_bmp;
		x_bmp = x_center + (x >> resolution_radian);
		y_bmp = y_center - (y >> resolution_radian);
		do {
			if (x_bmp_prev == x_bmp && y_bmp_prev == y_bmp)
				break;
			if (! (0 < x_bmp +1 && x_bmp < width && 0 < y_bmp +1 && y_bmp < height))
				break;
			if (NULL == lpfn)
				bpp1_dst[(u16)y_bmp * opl_dst + (u16)x_bmp / 8] |= 1 << 7 - (u16)x_bmp % 8;
			else
				is_abort = lpfn ((u16)x_bmp, (u16)y_bmp, bpp1_dst, opl_dst, param) ? false : true;
			x_bmp_prev = x_bmp, y_bmp_prev = y_bmp;
		} while (0);
		// move next (right circular)
s32 t;
		t = y - (x >> resolution_radian);
		x = x + (y >> resolution_radian);
		y = t;
	}
	return true;
}
struct arch_with_inpaint_param {
	u16 width;
	u16 height;
	u16 y_bmp_prev;
};
static bool arch_with_inpaint (u16 x_bmp, u16 y_bmp, void *bpp1_dst_, size_t opl_dst, void *param)
{
u8 *bpp1_dst;
	bpp1_dst = (u8 *)bpp1_dst_;
struct arch_with_inpaint_param *m_;
	m_ = (struct arch_with_inpaint_param *)param;
u8 *left, *p;
	p = (left = bpp1_dst + y_bmp * opl_dst) + (x_bmp / 32) * 4;
	do {
u32 dw;
		dw = load_be32 (p);
		if (! (m_->y_bmp_prev < y_bmp)) {
			store_be32 (p, dw | 1 << 31 - x_bmp % 32);
			break;
		}
u32 mask;
		mask = (u32)~0 << 31 - x_bmp % 32;
		if (mask & dw) {
size_t i;
			for (i = x_bmp % 32; 0 < i; --i)
				if (1 & dw >> 31 -i)
					break;
			mask &= (u32)~0 >> i;
			store_be32 (p, mask | dw);
			break;
		}
		store_be32 (p, mask | dw);
		for (; left < p; p -= 4) {
			if (dw = load_be32 (p -4)) {
size_t i;
				for (i = 31; 0 < i; --i)
					if (1 & dw >> 31 -i)
						break;
				store_be32 (p -4, (u32)~0 >> i | dw);
				break;
			}
			*(u32 *)(p -4) = (u32)~0;
		}
	} while (0);
	m_->y_bmp_prev = y_bmp;
	if (x_bmp == m_->width -1) {
		for (++y_bmp; y_bmp < m_->height; ++y_bmp)
			memset (bpp1_dst + y_bmp * opl_dst, 0xff, RUPDIV(32, m_->width) * 4);
		return false;
	}
	return true;
}
typedef struct {
	s16 x_center;
	s16 y_center;
	u16 x_begin;
	u16 y_begin;
	u32 rgb;
} draw_rainbow_s;
static bool draw_rainbow (u16 width, u16 height, draw_rainbow_s *t, size_t t_len, void *bpp24_dst_, size_t cb_dst, size_t bpp_dst)
{
	if (! (0 < width && 0 < height && t && 0 < t_len && bpp24_dst_ && 24 == bpp_dst))
		return false;
size_t opp_dst, opl_dst; // octet(byte) per pixel/line
	opp_dst = bpp_dst / 8;
	opl_dst = width * opp_dst + PADDING(4, width * opp_dst);
	if (cb_dst < opl_dst * height)
		return false;
u8 *bpp24_dst;
	bpp24_dst = (u8 *)bpp24_dst_;
size_t cb_tmp;
u8 *bpp1_tmp;
	if (NULL == (bpp1_tmp = (u8 *)malloc (cb_tmp = RUPDIV(32, width) * 4 * height)))
		return false;
	do {
		memset (bpp24_dst, 0, cb_dst);
struct arch_with_inpaint_param u, *m_ = &u;
		m_->width = width;
		m_->height = height;
size_t i;
		for (i = 0; i < t_len; ++i) {
			m_->y_bmp_prev = height;
			memset (bpp1_tmp, 0, cb_tmp);
			enum_arch (11, width, height, t[i].x_begin, t[i].y_begin, t[i].x_center, t[i].y_center, bpp1_tmp, cb_tmp, arch_with_inpaint, m_);
			bitblt_nor (width, height, bpp1_tmp, t[i].rgb, bpp24_dst, cb_dst);
		}
	} while (0);
	free (bpp1_tmp);
	return true;
}
int main (int argc, const char *argv[])
{
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

int obps;
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

u16 width, height;
	width = 512, height = 128;
draw_rainbow_s t[] = {
	{ 256, 1280, 0,  34, RGB(0xff,   0,0x44) },
	{ 256, 1280, 0,  41, RGB(0xff,0x3f,0x4c) },
	{ 256, 1280, 0,  48, RGB(0xff,0x7f,0x55) },
	{ 256, 1280, 0,  55, RGB(0xff,0xbf,0x5d) },
	{ 256, 1280, 0,  62, RGB(0xff,0xff,0x66) },
	{ 256, 1280, 0,  69, RGB(0x7f,0xff,0x6e) },
	{ 256, 1280, 0,  76, RGB(   0,0xff,0x77) },
	{ 256, 1280, 0,  83, RGB(   0,0x90,0x9f) },
	{ 256, 1280, 0,  90, RGB(   0,0x22,0xcc) },
	{ 256, 1280, 0,  98, RGB(0x3f,0x33,0xcc) },
	{ 256, 1280, 0, 105, RGB(0x7f,0x44,0xcc) },
	{ 256, 1280, 0, 113, RGB(0xbf,0x55,0xcc) },
	{ 256, 1280, 0, 120, RGB(0xff,0x66,0xcc) },
	{ 256, 1280, 0, 127, RGB(   0,   0,   0) }
};
size_t bpp, opp, opl;
	bpp = 24;
	opl = width * (opp = bpp / 8) + PADDING(4, width * opp);
size_t cb_lpv;
	cb_lpv = opl * height;
u8 *lpv;
	lpv = NULL;
	do {
u8 header[0x36];
		memset (header, 0, sizeof(header));
		store_le16 (header +0x00, 'M'<<8|'B');
		store_le32 (header +0x02, sizeof(header) + cb_lpv);
		store_le32 (header +0x0A, sizeof(header));
		store_le32 (header +0x0E, 0x28); // sizeof(BITMAPINFOHEADER)
		store_le32 (header +0x12, width);
		store_le32 (header +0x16, height);
		store_le16 (header +0x1A, 1);
		store_le16 (header +0x1C, bpp);
		store_le32 (header +0x22, cb_lpv);

		if (NULL == (lpv = (u8 *)malloc (cb_lpv)))
			return -1;
		draw_rainbow (width, height, t, arraycountof(t), lpv, cb_lpv, 24);

//		fprintf (stdout, "RGB=000000 .. FFFF77" "\n");
#ifdef WIN32
		fflush (stdout); // may be need
		bBinModeOld = _setmode (_fileno (stdout), O_BINARY);
#endif
		fwrite ("\033\033", 1, 2, stdout);
		fwrite (header, 1, sizeof(header), stdout);
		get_boottime (&base);
size_t sent;
		sent = 0;
		while (sent < cb_lpv) {
size_t sendable;
			sendable = sizeof(header) + cb_lpv;
			if (0 < obps) {
u32 ms;
				ms = get_past_msec (&base);
				sendable = min(ms * (unsigned)obps / 8000, sizeof(header) + cb_lpv);
			}
			if (sizeof(header) + sent < sendable)
				sent += fwrite (lpv + sent, 1, sendable - (sizeof(header) + sent), stdout);
			if (0 < obps && sent < cb_lpv)
				msleep (1);
		}
#ifdef WIN32
		fflush (stdout); // must be need
		_setmode (_fileno (stdout), bBinModeOld);
#endif
		fprintf (stdout, /*"R=00"*/ "\n");
	} while (0);

	if (lpv) {
		free (lpv);
		lpv = NULL;
	}
	return 0;
}
