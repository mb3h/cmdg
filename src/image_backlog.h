#ifndef IMAGE_BACKLOG_H_INCLUDED__
#define IMAGE_BACKLOG_H_INCLUDED__

/* Readability sugar.
   Never use 'READABILITY_SUGAR_IMAGE_BACKLOG' directly,
   only use 'image_backlog'. */
typedef struct {
	void (*set_grid) (uint8_t width, uint8_t height);
	void (*setpos) (uint16_t x_grid, uint16_t y_grid);
	unsigned (*write) (const void *buf_, unsigned len);
	_Bool (*tellpos) (uint16_t *x_grid, uint16_t *y_grid,
	                  uint16_t *width_grid, uint16_t *height_grid);
	_Bool (*clip_from) (uint16_t x_grid, uint16_t y_grid,
	                    /*u24*/ uint32_t xy_not_enough_cl24,
	                    void *bpp24_image);
} READABILITY_SUGAR_IMAGE_BACKLOG;
extern const READABILITY_SUGAR_IMAGE_BACKLOG *image_backlog;

#endif //ndef IMAGE_BACKLOG_H_INCLUDED__
