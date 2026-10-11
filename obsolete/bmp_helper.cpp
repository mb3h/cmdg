
#include <stdint.h>
#include "common/pixel.h"
#define img_helper_s void // now all non-state.
#include "bmp_helper.hpp"

#include "core/pixel_stream_calc_dirty.h"
#include "core/bmp_rawdata_clip_range_fit.h"
#include "portab.h"
#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <memory.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

#define ALIGN32(x) (-32 & (x) +32 -1)
#define PADDING32(x) (ALIGN32(x) - (x))
#define PADDING(a,x) ((a) -1 - ((x) +(a) -1)%(a))

///////////////////////////////////////////////////////////////////////////////
// recycle

static bool bmp_header_parse (const void *src, enum pixel_format *ppf, u16 *pwidth, u16 *pheight, u32 *prawdata_bytes)
{
ASSERTE(src)
const u8 *bmphdr;
	bmphdr = (const u8 *)src;
u32 bytes, width, height;
u16 bpp;
	bytes  = load_le32 (bmphdr +0x02); // BITMAPFILEHEADER::bfType
	width  = load_le32 (bmphdr +0x12); // BITMAPINFOHEADER::biWidth
	height = load_le32 (bmphdr +0x16); // BITMAPINFOHEADER::biHeight
	bpp    = load_le16 (bmphdr +0x1c); // BITMAPINFOHEADER::biBitCount
	if (0 == bytes && 0 < width && 0 < height) // (*a)
		bytes = ALIGN32(bpp * width)/8 * height + sizeofBMPHDR;

	if (ppf) *ppf = (1 == bpp) ? PF_RGB1 : (24 == bpp) ? PF_BGR888 : PF_UNKNOWN;
	if (pwidth) *pwidth = (u16)min(0xFFFF, width);
	if (pheight) *pheight = (u16)min(0xFFFF, height);
	if (prawdata_bytes) *prawdata_bytes = bytes - min(bytes, sizeofBMPHDR);

	if (bytes < sizeofBMPHDR)
		return false; // invalid [+02,4].
	if (! (0 == bpp % 8 && 0 < bpp && bpp <= 32 || 1 == bpp))
		return false;
	if (0xFFFF < width || 0xFFFF < height)
		return false;
	return true;
}

u8 bpp_from_pixel_format (enum pixel_format pf)
{
	switch (pf) {
	case PF_RGB888: case PF_BGR888:
		return 24;
	case PF_RGBA8888:
		return 32;
	/*case PF_UNKNOWN:*/ default:
		return 0;
	}
}
static bool bmp_header_init (enum pixel_format pf, u16 width, u16 height, void *dst, unsigned cb)
{
ASSERTE(dst && sizeofBMPHDR <= cb && 0 < width && 0 < height)
	if (! (dst && sizeofBMPHDR <= cb && 0 < width && 0 < height))
		return false;
	memset (dst, 0, sizeofBMPHDR);
u8 bpp; u32 bytes;
	bpp = bpp_from_pixel_format (pf);
	bytes = ALIGN32(bpp * width)/8 * height + sizeofBMPHDR;
	store_le32 ((u8 *)dst +0x02, bytes ); // BITMAPFILEHEADER::bfType
	store_le32 ((u8 *)dst +0x12, width ); // BITMAPINFOHEADER::biWidth
	store_le32 ((u8 *)dst +0x16, height); // BITMAPINFOHEADER::biHeight
	store_le16 ((u8 *)dst +0x1c, bpp   ); // BITMAPINFOHEADER::biBitCount
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// private

typedef void bmp_helper_s; // now all non-state.

static void *bmp_rawdata_clip_sub (const void *src_, enum pixel_format pf, u16 x, enum pixel_format dst_pf, u8 *ppixels_byte, void *dst_)
{
const u8 *src;
	src = (const u8 *)src_;
u8 *dst;
	dst = (u8 *)dst_;
u8 a; u32 rgb24;
	if (PF_BGR888 == pf)
		a = 255, rgb24 = load_le24 (&src[x * 3]);
	else /*if (PF_RGB1 == pf)*/
		a = (src[x / 8] & 128 >> x % 8) ? 255 : 0, rgb24 = (a) ? 0xffffff : 0;
	if (PF_RGB888 == dst_pf)
		store_be24 (dst, (a) ? rgb24 : 0), dst += 3;
	else if (PF_BGR888 == dst_pf)
		store_le24 (dst, (a) ? rgb24 : 0), dst += 3;
	else /*if (PF_RGB1 == pf)*/ {
u8 pixels_byte, not_enough;
		pixels_byte = *ppixels_byte, not_enough = !(128 & pixels_byte);
		pixels_byte <<= 1, pixels_byte |= (a && rgb24) ? 1 : 0;
		if (not_enough) 
			*ppixels_byte = pixels_byte;
		else
			*dst++ = pixels_byte, *ppixels_byte = 1;
	}
	return dst;
}
static bool bmp_rawdata_clip (const void *src_, enum pixel_format pf, u16 xpixels, u16 ypixels, u16 x, u16 y, u16 width, u16 height, enum pixel_format dst_pf, u8 dst_flags, void *dst_, unsigned cb)
{
	if (! ((PF_RGB1 == pf || PF_BGR888 == pf) && 0 < xpixels && 0 < ypixels))
		return false; // parameter bug
	if (! ((PF_RGB1 == dst_pf || PF_BGR888 == dst_pf || PF_RGB888 == dst_pf) && 0 < width && 0 < height))
		return false; // parameter bug
	if (! (src_ && x <= xpixels && y <= ypixels))
		return false; // parameter bug
u16 dst_bpl; u8 *dst;
	dst_bpl = ALIGN32(bpp_from_pixel_format (dst_pf) * width) / 8, dst = (u8 *)dst_;
	if (! (dst_ && cb <= dst_bpl * height))
		return false; // parameter bug
u16 bpl;
	bpl = ALIGN32(bpp_from_pixel_format (pf) * xpixels) / 8;

u8 bit_padding; u16 right_padding, bottom_padding;
	if (!bmp_rawdata_clip_range_fit (bpp_from_pixel_format (pf), xpixels, ypixels, x, y, bpp_from_pixel_format (dst_pf), &width, &height, &bit_padding, &right_padding, &bottom_padding))
		return false;
// PENDING: optimize
	if (0 < bottom_padding && !(BF_TOPDOWN & dst_flags))
		memset (dst, 0, bottom_padding * dst_bpl), dst += bottom_padding * dst_bpl;
u16 i, n;
	for (n = 0; n < height; ++n) {
u16 bmp_y; const u8 *src;
		bmp_y = (BF_TOPDOWN & dst_flags) ? y + n : y + height -1 -n;
		src = (const u8 *)src_ + (ypixels - (bmp_y +1)) * bpl;
u8 pixels_byte;
		for (pixels_byte = 1, i = x; i < x + width; ++i)
			dst = bmp_rawdata_clip_sub (src, pf, i, dst_pf, &pixels_byte, dst);
		if (0 < bit_padding)
			*dst++ = (u8)(pixels_byte << bit_padding);
		if (0 < right_padding)
			memset (dst, 0, right_padding), dst += right_padding;
	}
	if (0 < bottom_padding && BF_TOPDOWN & dst_flags)
		memset (dst, 0, bottom_padding * dst_bpl), dst += bottom_padding * dst_bpl;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// interface

static bool bmp_clip (const void *bmphdr, const void *rawdata, u16 x, u16 y, u16 width, u16 height, enum pixel_format dst_pf, u8 dst_flags, void *dst_, unsigned cb)
{
enum pixel_format pf; u16 xpixels, ypixels;
	if (!bmp_header_parse (bmphdr, &pf, &xpixels, &ypixels, NULL))
		return false;
	return bmp_rawdata_clip (rawdata, pf, xpixels, ypixels, x, y, width, height, dst_pf, dst_flags, dst_, cb);
}

static bool bmp_trim (void *rawdata, uint16_t width, uint16_t height, enum pixel_format pf, uint8_t flags, uint16_t trim_pixels)
{
	if (BF_TOPDOWN & flags && TRIM_TOP & ~flags
	 || BF_TOPDOWN & ~flags && TRIM_TOP & flags)
		return true; // nothing needed
	// PENDING: now not used
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// ctor/dtor

static void bmp_helper_reset (bmp_helper_s *m_)
{
#if 0 // now all non-state
	memset (m_, 0, sizeof(bmp_helper_s));
#endif
}

void bmp_helper_release (bmp_helper_s *m_)
{
	bmp_helper_reset (m_);
}

void bmp_helper_ctor (struct bmp_helper *this_, unsigned cb)
{
#if 0 // now all non-state
BUGc(sizeof(bmp_helper_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(bmp_helper_s), cb)
#endif
bmp_helper_s *m_;
	m_ = (bmp_helper_s *)this_;
	bmp_helper_reset (m_);
}

///////////////////////////////////////////////////////////////////////////////

static struct img_helper_i o_img_helper_i = {
	.release       = bmp_helper_release,

	.header_parse  = bmp_header_parse,
	.clip          = bmp_clip,
	.trim          = bmp_trim,
	.header_init   = bmp_header_init,
};
struct img_helper_i *bmp_helper_query_img_helper_i (struct bmp_helper *this_) { return &o_img_helper_i; }
