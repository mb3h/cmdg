#include <stdint.h>

#include <stdio.h>
#include <stdlib.h> // atoi exit
#include <errno.h>
#include <memory.h>
#include <unistd.h> // read STDIN_FILENO
typedef uint32_t u32, u24;
typedef  uint8_t u8, u1;
typedef uint16_t u16;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define swap(a, b, type__) \
	do { \
		type__ t__; \
		t__ = a, a = b, b = t__; \
	} while (0)

#define PADDING(a,x)    ((a) -1 - ((x) +(a) -1) % (a))
#define ALIGN(a,x) ((x) +(a) -1 - ((x) +(a) -1) % (a))
#define RGB24(r,g,b) (0xff0000 & (r) << 16 | 0xff00 & (g) << 8 | 0xff & (b))

#define FALSE 0
#define TRUE  1
///////////////////////////////////////////////////////////////
// configuration

// "Ubuntu Mono 12"
#define SUPPOSED_FONT_WIDTH  9
#define SUPPOSED_FONT_HEIGHT (15 +4)
//#define PIXEL_MAG_X 16
//#define PIXEL_MAG_Y PIXEL_MAG_X
#define PIXEL_MAG_X (2 * SUPPOSED_FONT_WIDTH)
#define PIXEL_MAG_Y      SUPPOSED_FONT_HEIGHT

#define DEFAULT_GRID_RGB24 RGB24(0,255,0)
#define    OVER_SIZE_RGB24 RGB24(0,0,0)

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

#define caller_bug  assert

///////////////////////////////////////////////////////////////
// endian portability

static void store_le32 (void *p, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
#else
	val = 0xff000000U & val << 24
	    | 0x00ff0000  & val << 8
	    | 0x0000ff00  & val >> 8
	    | 0x000000ff  & val >> 24;
#endif
	*(u32 *)p = val;
}

static void store_le16 (void *p, u16 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
#else
	val = 0xff00 & val << 8
	    | 0x00ff & val >> 8;
#endif
	*(u16 *)p = val;
}

static u16 load_le16 (const void *src)
{
u16 val
	= *(const u16 *)src;
#if __BYTE_ORDER == __LITTLE_ENDIAN
#else
	val = 0xff00 & val << 8
	    | 0x00ff & val >> 8;
#endif
	return val;
}

static u24 load_le24 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return 0xffffff & *(const u32 *)src;
#else
const u8 *p
	= (const u8 *)src;
	return p[0]<<16|p[1]<<8|p[2];
#endif
}

static u24 load_be24 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
const u8 *p
	= (const u8 *)src;
	return p[0]<<16|p[1]<<8|p[2];
#else
	return 0xffffff & *(const u32 *)src;
#endif
}

static void store_le24 (void *dst, u24 val)
{
u8 *p
	= (u8 *)dst;
	p[0] = (u8)(0xff & val      );
	p[1] = (u8)(0xff & val >> 8 );
	p[2] = (u8)(0xff & val >> 16);
}

static void store_be24 (void *dst, u24 val)
{
u8 *p
	= (u8 *)dst;
	p[0] = (u8)(0xff & val >> 16);
	p[1] = (u8)(0xff & val >> 8 );
	p[2] = (u8)(0xff & val      );
}

///////////////////////////////////////////////////////////////
// helper

unsigned pow2_ge (unsigned n)
{
caller_bug(n <= (1U << 31)) // safety on unsigned == uint32_t
	if (0 == n) return 0;

unsigned r = 1;
	while (r < n)
		r <<= 1;
	return r;
}

///////////////////////////////////////////////////////////////
// local

//#define PF_TOPBOTTOM 0 // data order = top -> bottom line
#define PF_BOTTOMTOP 1 // data order = bottom -> top line (*).bmp cc='BM'
//#define PF_RGB888 0 // = RGB24(b.e.)
#define PF_BGR888 2 // = RGB24(l.e.) (*).bmp cc='BM'
static void _bpp24_get_clipping (
  const void *src_
, u16 src_width, u16 src_height, u16 src_align_bytes
, u1 src_format
, u16 x, u16 y
, u16 width, u16 height, u16 align_bytes
, u24 xy_not_enough_cl24
, void *dst_
)
{
static const u8 BPP = 24;
const u8 *src; u8 *dst;
	src = (const u8 *)src_, dst = (u8 *)dst_;
u16 y_end
	= min(src_height, y +height);
u16 x_end
	= min(src_width, x +width);
u16 x_padding_bytes
	= (0 == align_bytes) ? 0 : PADDING(align_bytes, width * BPP/8);

unsigned src_bpl // bytes per line
	= ALIGN(src_align_bytes, src_width * BPP/8);
unsigned y_offset;
	if (PF_BOTTOMTOP & src_format)
		y_offset = (src_height -1 -y) * src_bpl;
	else
		y_offset = y * src_bpl;
u16 x_begin = x, y_begin = y;
	for (; y < y_begin +height; ++y) {
		x = x_begin;
		if (! (y < y_end))
			;
		else if (PF_BGR888 & src_format) {
			for (; x < x_end; ++x) {
u24 rgb24
				= load_le24 (src +y_offset +x *BPP/8);
				store_be24 (dst, rgb24);
				dst += BPP/8;
			}
		}
		else /*if (! (PF_BGR888 & src_format))*/ {
			memcpy (dst, src +y_offset, (x_end -x_begin) * BPP/8);
			dst += (x_end -x_begin) * BPP/8;
			x = x_end;
		}
u16 i;
		for (; x < x_begin +width; ++x) {
			store_be24 (dst, xy_not_enough_cl24);
			dst += BPP/8;
		}
		if (0 < x_padding_bytes) {
			memset (dst, 0, x_padding_bytes);
			dst += x_padding_bytes;
		}
		if (PF_BOTTOMTOP & src_format)
			y_offset -= src_bpl;
		else
			y_offset += src_bpl;
	}
}

static void _bpp24_fill_rect (
  void *dst_
, u16 dst_width, u16 dst_height, u16 dst_align_bytes
, u1 dst_format
, u16 x, u16 y, u16 width, u16 height
, u24 rgb24)
{
static const u8 BPP = 24;
u8 *dst = (u8 *)dst_;

caller_bug(0 < width)
caller_bug(0 < height)
caller_bug(x < dst_width)
caller_bug(y < dst_height)
	if (! (0 < width && 0 < height &&
	       x < dst_width && y < dst_height))
		return; // safety

u16 x_end
	= min(x +width, dst_width);
u16 y_end
	= min(y +height, dst_height);
	if (PF_BOTTOMTOP & dst_format) {
		y     = dst_height -y    ;
		y_end = dst_height -y_end;
		swap(y, y_end, u16);
	}
u8 rgb888[3];
	if (PF_BGR888 & dst_format)
		store_le24 (rgb888, rgb24);
	else
		store_be24 (rgb888, rgb24);
unsigned dst_x_padding
	= PADDING(dst_align_bytes, dst_width * BPP/8);
unsigned first_delta
	= y * (dst_width * BPP/8 +dst_x_padding)
	+ x * BPP/8;
unsigned next_delta
	= (dst_width -x_end) * BPP/8 + dst_x_padding
	+ x * BPP/8;

	dst += first_delta;
u16 x_begin = x;
	for (; y < y_end; ++y) {
		x = x_begin;
		for (; x < x_end; ++x) {
			memcpy (dst, rgb888, BPP/8);
			dst += BPP/8;
		}
		dst += next_delta;
	}
}

///////////////////////////////////////////////////////////////
// entry point

int main (int argc, char *argv[])
{
static const u8 BPP = 24;
const char *bmp_path = "stdin";
u1 completed = FALSE;

	// pipe input
static const unsigned CHUNK_SIZE = 0x10000; // 64KB
unsigned fsize = 0, fmax = CHUNK_SIZE;
u8 *file
	= (u8 *)malloc (fmax);
	while (1) {
int r
		= read (STDIN_FILENO, file +fsize, CHUNK_SIZE);
		if (0 == r)
			break; // EOF
		if (r < 0) {
			if (EWOULDBLOCK == errno)
				continue;
fprintf (stderr, "%s: unknown error. @err=%d" "\n",
         bmp_path, errno);
			goto error;
		}
		fsize += r;
		if (! (fmax < fsize +CHUNK_SIZE))
			continue;
		fmax = pow2_ge(fsize +CHUNK_SIZE);
		if (NULL == (file = (u8 *)realloc (file, fmax))) {
fprintf (stderr, "not enough memory." "\n");
			goto error;
		}
	}

	// bmpheader
	if (! ('B' == file[0] && 'M' == file[1] && BPP == file[+0x1c])) {
fprintf (stderr, "%s: input not valid BITMAP." "\n", bmp_path);
		goto error;
	}
u16 bmp_width
	= load_le16 (file +0x12);
u16 bmp_height
	= load_le16 (file +0x16);
unsigned supposed_size
	= 0x36 +ALIGN(4, bmp_width * BPP/8) * bmp_height;
	if (! (supposed_size <= fsize)) {
fprintf (stderr, "%s: %dx%d BITMAP needs %d bytes." "\n",
         bmp_path, bmp_width, bmp_height, supposed_size);
		goto error;
	}

	// cmdline
	if (! (4 < argc)) {
fprintf (stderr, "usage: <width> <height> <left> <top> [<grid-rgb24(hex)>]" "\n");
		return 0;
	}
u16 clip_width, clip_height, clip_x, clip_y;
	clip_width  = (u16)atoi (argv[1]);
	clip_height = (u16)atoi (argv[2]);
	clip_x      = (u16)atoi (argv[3]);
	clip_y      = (u16)atoi (argv[4]);
u24 grid_rgb24
	= DEFAULT_GRID_RGB24;
	if (5 < argc)
		grid_rgb24 = (u24)strtoul (argv[5], NULL, 16);

	// get clipping
u8 *clipped24
	= (u8 *)malloc (ALIGN(1, bmp_width * BPP/8) * bmp_height);
u16 bmp_align, clip_align;
	_bpp24_get_clipping (file +0x36, bmp_width, bmp_height,
	                     bmp_align = 4,
	                     PF_BGR888|PF_BOTTOMTOP,
	                     clip_x, clip_y,
	                     clip_width, clip_height,
	                     clip_align = 1,
	                     OVER_SIZE_RGB24,
	                     clipped24);

	// make BMP (magnify)
u8 bmphdr[0x36] = {0};
	store_le16 (bmphdr +0x00, 'M'<<8|'B');
u16 mag_width, mag_height; u32 mag_len;
	mag_width = PIXEL_MAG_X * clip_width;
	mag_height = PIXEL_MAG_Y * clip_height;
	mag_len = ALIGN(4, mag_width * BPP/8) * mag_height;
	store_le32 (bmphdr +0x02, usizeof(bmphdr) +mag_len);
	store_le32 (bmphdr +0x0A, usizeof(bmphdr));
	store_le32 (bmphdr +0x0E, usizeof(bmphdr) -0x0E);
	store_le32 (bmphdr +0x12, mag_width);
	store_le32 (bmphdr +0x16, mag_height);
	store_le16 (bmphdr +0x1A, 1);
	store_le16 (bmphdr +0x1C, BPP);
	store_le32 (bmphdr +0x22, mag_len);
u8 *mag
	= (u8 *)malloc (mag_len);

	_bpp24_fill_rect (mag, mag_width, mag_height, 4,
	                  PF_BOTTOMTOP|PF_BGR888,
	                  0, 0,
	                  mag_width, mag_height,
	                  grid_rgb24);
u16 x, y;
	for (y = 0; y < clip_height; ++y)
		for (x = 0; x < clip_width; ++x) {
unsigned pickup_addr
			= (y * clip_width +x) * BPP/8;
u24 rgb24
			= load_be24 (clipped24 +pickup_addr);
			_bpp24_fill_rect (mag, mag_width, mag_height, 4,
			                  PF_BOTTOMTOP|PF_BGR888,
			                  x * PIXEL_MAG_X, y * PIXEL_MAG_Y,
			                  PIXEL_MAG_X -1, PIXEL_MAG_Y -1,
			                  rgb24);
		}

	// Y-grid (1/2)
unsigned y_pos_len
	= (clip_y +clip_height -1) < 10   ? 1 :
	  (clip_y +clip_height -1) < 100  ? 2 :
	  (clip_y +clip_height -1) < 1000 ? 3 : 4;
	fprintf (stdout, "\033[%dC"
	, 1 +y_pos_len +1);

	// output
#ifdef WIN32
	fflush (stdout); // may be need
	bBinModeOld = _setmode (_fileno (stdout), O_BINARY);
#endif
	fwrite ("\033\033", 1, 2, stdout);
	fwrite (bmphdr, 1, sizeof(bmphdr), stdout);
	fwrite (mag, 1, mag_len, stdout);
#ifdef WIN32
	fflush (stdout); // must be need
	_setmode (_fileno (stdout), bBinModeOld);
#endif

	// Y-grid (2/2)
	fprintf (stdout, "\r");
	for (y = clip_y; y < clip_y +clip_height; ++y) {
		if (0 == (y -clip_y) % 4)
			fprintf (stdout, "%*d", 1 +y_pos_len, y);
		fprintf (stdout, "\n");
	}
	// X-grid
	fprintf (stdout, "\033[%dC", 1 +y_pos_len +1);
	for (x = clip_x; x < clip_x +clip_width; x += 4)
		fprintf (stdout, "%-*d", 2 * 4, x);
	fprintf (stdout, "\n");

	completed = FALSE;
error:
	if (mag)
		free (mag);
	if (clipped24)
		free (clipped24);
	if (file)
		free (file);

	return (completed) ? -1 : 0;
}
