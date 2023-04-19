#include <stdint.h>
#include "pixel_stream_calc_dirty.h"

#include <stdbool.h>

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
#define min(a, b) ((a) < (b) ? (a) : (b))
#define arraycountof(x) (sizeof(x)/(sizeof((x)[0])))
#define HIDIV(x,a) (((x) +(a) -1)/(a))

///////////////////////////////////////////////////////////////////////////////

bool pixel_stream_calc_dirty (
	  u8 bpp, u16 width, u16 height
	, u8 right_skip_bits
	, u8 top_padding_pixels
	, u32 begin, u32 end
	, enum dirty_type dirty_type
	, struct cell_dirty *result
)
{
	if (! (0 < bpp && 0 < width && 0 < height && result && 1 < result->lfWidth && 1 < result->lfHeight))
		return false; // parameter bug
	if (0 < (bpp * width + right_skip_bits) % 8)
		return false; // padding-bits unmatch byte boundary.
	result->skip = result->count = 0;
	if (! (begin < end))
		return true; // nothing dirty

u16 lfWidth, lfHeight;
	lfWidth = result->lfWidth, lfHeight = result->lfHeight;
unsigned bpl, pad;
	bpl = (bpp * width + right_skip_bits) / 8;
	pad = right_skip_bits / 8;
unsigned left, top, right, bottom;
	top    = (begin +pad) / bpl   , top    += top_padding_pixels;
	left   = (begin +pad) % bpl   , left   = left  * 8 / bpp;
	bottom = (end     -1) / bpl +1, bottom += top_padding_pixels;
	right  = (end     -1) % bpl +1, right  = right * 8 / bpp, right = min(right, width);
	if (! (top +1 < bottom || top +1 == bottom && left < right))
		return true; // nothing dirty
u16 skip_cols, count_cells;
	{
u16 width_cols;
		width_cols = HIDIV(width, lfWidth);
		count_cells = width_cols * (HIDIV(bottom, lfHeight) - top / lfHeight);
		// adjust end line
u16 lack_cols;
		lack_cols = (right < width) ? width_cols - (right / lfWidth) : 0;
		lack_cols -= (FINISHED & dirty_type || right == width || (0 == right % lfWidth && 1 == bottom % lfHeight)) ? 0 : 1;
		if (FINISHED & dirty_type && (bottom -1) % lfHeight < lfHeight -1)
			count_cells -= width_cols, lack_cols = 0;
		else if (0 < lack_cols)
			count_cells -= lack_cols;
		// adjust begin line
		skip_cols = left / lfWidth;
		if (FINISHED & dirty_type && top % lfHeight < lfHeight -1)
			skip_cols = 0;
		else if (0 < skip_cols)
			count_cells -= min(skip_cols, count_cells);
	}
	result->skip = skip_cols, result->count = count_cells;
	return true;
}
