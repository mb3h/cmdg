#ifndef PIXEL_STREAM_CALC_DIRTY_H_INCLUDED__
#define PIXEL_STREAM_CALC_DIRTY_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum dirty_type {
	CHANGED = 0,
	FINISHED = 1
};

struct cell_dirty {
	uint16_t lfWidth;  // [in]
	uint16_t lfHeight; // [in]
	uint16_t skip;     // [out] (< width_cols)
	uint16_t count;    // [out] (<= width_cols * height_rows)
};

_Bool pixel_stream_calc_dirty (
	  uint8_t bpp
	, uint16_t width
	, uint16_t height
	, uint8_t right_skip_bits
	, uint8_t top_padding_pixels
	, uint32_t begin
	, uint32_t end
	, enum dirty_type dirty_type
	, struct cell_dirty *result
);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // PIXEL_STREAM_CALC_DIRTY_H_INCLUDED__
