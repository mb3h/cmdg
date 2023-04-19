#include <stdint.h>
#include "bmp_rawdata_clip_range_fit.h"

#include <stdbool.h>

typedef  uint8_t u8;
typedef uint16_t u16;
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) < (b) ? (b) : (a))
#define arraycountof(x) (sizeof(x)/(sizeof((x)[0])))
#define ALIGN32(x) (-32 & (x) +32 -1)
#define PADDING(a,x) ((a) -1 - ((x) +(a) -1)%(a))

///////////////////////////////////////////////////////////////////////////////

bool bmp_rawdata_clip_range_fit (
	  u8 bpp
	, u16 xpixels, u16 ypixels
	, u16 x, u16 y
	, u8 dst_bpp
	, u16 *width, u16 *height // [in/out]
	, u8 *bit_padding
	, u16 *byte_padding
	, u16 *bottom_padding
)
{
	if (! ((1 == bpp || 24 == bpp) && 0 < xpixels && 0 < ypixels))
		return false; // parameter bug
	if (! ((1 == dst_bpp || 24 == dst_bpp) && width && 0 < *width && height && 0 < *height))
		return false; // parameter bug
	if (! (bit_padding && byte_padding && bottom_padding))
		return false; // parameter bug

	*bit_padding = 0;
// padding bottom over-range (pixels)
	*bottom_padding = max(min(y, ypixels) + *height, ypixels) - ypixels;
	*height -= *bottom_padding;
// padding right over-range (pixels)
u16 bpl;
	bpl = ALIGN32(dst_bpp * *width) / 8;
	*width -= max(min(x, xpixels) + *width, xpixels) - xpixels;
// padding 32bit-alignment (bits, bytes)
	*bit_padding = PADDING(8, dst_bpp * *width); // byte alignment (1bpp only)
	*byte_padding = bpl - (dst_bpp * *width + *bit_padding) / 8;
	return true;
}
