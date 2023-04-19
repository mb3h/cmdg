#ifndef BMP_RAWDATA_CLIP_RANGE_FIT_H_INCLUDED__
#define BMP_RAWDATA_CLIP_RANGE_FIT_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

_Bool bmp_rawdata_clip_range_fit (
	  uint8_t bpp
	, uint16_t xpixels, uint16_t ypixels
	, uint16_t x, uint16_t y
	, uint8_t dst_bpp
	, uint16_t *width, uint16_t *height // [in/out]
	, uint8_t *bit_padding
	, uint16_t *byte_padding
	, uint16_t *bottom_padding
);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // BMP_RAWDATA_CLIP_RANGE_FIT_H_INCLUDED__
