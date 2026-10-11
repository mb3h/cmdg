#ifndef IMG_HELPER_H_INCLUDED__
#define IMG_HELPER_H_INCLUDED__

#define BF_TOPDOWN  0x01

#define TRIM_MASK  0x06
#define TRIM_TOP   0x02
//#define TRIM_RIGHT 0x04 // PENDING: now not used

struct img_helper_i {
	void (*release) (img_helper_s *this_);

	_Bool (*header_parse) (
		  const void *src
		, enum pixel_format *ppf
		, uint16_t *pwidth
		, uint16_t *pheight
		, uint32_t *prawdata_bytes
		);
	_Bool (*clip) (
		  const void *bmphdr
		, const void *rawdata
		, uint16_t left, uint16_t top
		, uint16_t width, uint16_t height
		, enum pixel_format pf
		, uint8_t dst_flags
		, void *dst_, unsigned cb
		);
	_Bool (*trim) (
		  void *rawdata
		, uint16_t width, uint16_t height
		, enum pixel_format pf
		, uint8_t flags
		, uint16_t trim_pixels
		);
	_Bool (*header_init) (
		  enum pixel_format pf
		, uint16_t width
		, uint16_t height
		, void *dst
		, unsigned cb
		);
};

#endif // IMG_HELPER_H_INCLUDED__
