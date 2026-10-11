#ifndef VRAM_H_INCLUDED__
#define VRAM_H_INCLUDED__

struct VRAM_i {
	void (* write_image_start) (VRAM_s *this_, uint16_t cx, uint16_t cy, uint16_t width, uint16_t height, enum pixel_format pf);
	void (* write_image) (VRAM_s *this_, const void *rawdata, unsigned cb);
	void (* write_text) (VRAM_s *this_, uint16_t cx, uint16_t cy, uint16_t cols, const uint32_t *utf21s, const uint64_t *rgb24attr4s, uint64_t rgb24attr4s_mask);
	void (* clear) (VRAM_s *this_, uint32_t bg_rgb24);
	void (* copy) (VRAM_s *this_, uint16_t cx, uint16_t cy, uint16_t cols, uint16_t rows, uint16_t dst_x, uint16_t dst_y);
	void (* rollup) (VRAM_s *this_, uint16_t n, uint32_t bg_rgb24);
	void (* rolldown) (VRAM_s *this_, uint16_t n, uint32_t bg_rgb24);
	void (* set_grid) (VRAM_s *this_, uint16_t lf_width, uint16_t lf_height);
	void (* resize) (VRAM_s *this_, uint16_t ws_col, uint16_t ws_row);

	uint32_t (* dirty_lock) (VRAM_s *this_, uint16_t cx, uint16_t cy, uint32_t mask);
	uint32_t (* check_transparent) (VRAM_s *this_, uint16_t cx, uint16_t cy, uint32_t mask);
	uint32_t (* read_image) (VRAM_s *this_, uint16_t cx, uint16_t cy, enum pixel_format pf, void *data, uint32_t cb);
	void (* read_text) (VRAM_s *this_, uint16_t cx, uint16_t cy, uint32_t mask, uint32_t *utf21s, uint64_t *rgb24attr4s);
	void (* dirty_unlock) (VRAM_s *this_, uint16_t cx, uint16_t cy, uint32_t mask);
	void (* set_dirty_rect) (VRAM_s *this_, uint16_t cx, uint16_t cy, uint16_t cols, uint16_t rows);

	uint32_t (* save) (VRAM_s *this_, void *buf);
	void (* load) (VRAM_s *this_, const void *buf);

	uint16_t (* get_ws_col) (VRAM_s *this_);
	uint16_t (* get_ws_row) (VRAM_s *this_);
	uint16_t (* get_grid_width) (VRAM_s *this_);
	uint16_t (* get_grid_height) (VRAM_s *this_);
};

#define RGB24(r,g,b) (0xff0000 & (r) << 16 | 0xff00 & (g) << 8 | 0xff & (b))
#define ATTR_FG(cl) ((uint64_t)(0xffffff & (cl)) << ATTR_FG_LSB)
#define ATTR_BG(cl) ((uint64_t)(0xffffff & (cl)) << ATTR_BG_LSB)

enum {
	ATTR_FG_LSB     = 0,
	ATTR_FG_B       = (0xff << ATTR_FG_LSB +0),
	ATTR_FG_G       = (0xff << ATTR_FG_LSB +8),
	ATTR_FG_R       = (0xff << ATTR_FG_LSB +16),
	ATTR_FG_MASK    = (ATTR_FG_R | ATTR_FG_G | ATTR_FG_B),
	ATTR_REV_MASK   = 1 << 24,
//	ATTR_BLINK_MASK = 1 << 25,
//	ATTR_ULINE_MASK = 1 << 26,
//	ATTR_BOLD_MASK  = 1 << 27,
	ATTR_BG_LSB     = 32,
	ATTR_BG_B       = ((uint64_t)0xff << ATTR_BG_LSB +0),
	ATTR_BG_G       = ((uint64_t)0xff << ATTR_BG_LSB +8),
	ATTR_BG_R       = ((uint64_t)0xff << ATTR_BG_LSB +16),
	ATTR_BG_MASK    = (ATTR_BG_R | ATTR_BG_G | ATTR_BG_B),
};

#endif // VRAM_H_INCLUDED__
