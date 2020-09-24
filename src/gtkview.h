#ifndef GTK_VIEW_H_INCLUDED__
#define GTK_VIEW_H_INCLUDED__

typedef struct decoder_callback__ decoder_callback;
struct decoder_callback__ {
	void *(*malloc) (size_t size);
	void *(*realloc) (void *ptr, size_t size);
	void (*notify_size) (void *this_, uint32_t width, uint32_t height, void *noset_image);
	void (*notify_finish) (void *this_, void *image);
};

typedef struct IImage__ IImage;
struct IImage__ {
	void *this_;
	uint32_t (*get_width) (const void *this_);
	uint32_t (*get_height) (const void *this_);
	int (*get_n_channels) (const void *this_);
	void (*get_div_pixels) (const void *this_, int div_x, int div_y, int get_index, void *rgba);
	bool (*decode_start) (void *state_, size_t state_len);
	size_t (*decode) (void *state_, const void *src_, size_t len, decoder_callback *i, void *i_param);
};

typedef struct View__ View;
void view_connect_gtkkey_receiver (View *m_, bool (*lpfn)(guint keyval, bool pressed, void *param), void *param);
void view_set_title (View *m_, const char *title);
uint8_t font_get_width (View *m_);
uint8_t font_get_height (View *m_);
uint16_t view_get_cols (View *m_);
uint16_t view_get_rows (View *m_);
uint16_t image_entry (View *m_, uint16_t i_node, void *image, IImage *i);
uint16_t image_get_cols (View *m_, uint16_t i_node);
uint8_t image_get_rows (View *m_, uint16_t i_node);
void view_set_dirty (View *m_, uint16_t x, uint16_t y, uint16_t n_cols, uint16_t n_rows);
void view_putc (View *m_, uint16_t x, uint16_t y, int c);
void view_putg (View *m_, uint16_t x, uint16_t y, uint16_t i_node, uint16_t i_matrix, uint16_t n_cols, uint16_t n_rows);
void view_on_shell_disconnect (View *m_);
unsigned view_attr (View *m_, uint16_t x, uint16_t y, unsigned mask, unsigned set);
void view_rolldown (View *m_);

enum {
	ATTR_FG_LSB    = 0,
	ATTR_FG_R      = (1 << ATTR_FG_LSB +0),
	ATTR_FG_G      = (1 << ATTR_FG_LSB +1),
	ATTR_FG_B      = (1 << ATTR_FG_LSB +2),
	ATTR_FG_MASK   = (ATTR_FG_R | ATTR_FG_G | ATTR_FG_B),
	ATTR_BG_LSB    = 3,
	ATTR_BG_R      = (1 << ATTR_BG_LSB +0),
	ATTR_BG_G      = (1 << ATTR_BG_LSB +1),
	ATTR_BG_B      = (1 << ATTR_BG_LSB +2),
	ATTR_BG_MASK   = (ATTR_BG_R | ATTR_BG_G | ATTR_BG_B),
	ATTR_REV_MASK  = 1 << 6,
	ATTR_BOLD_MASK = 1 << 7,
};

#endif // GTK_VIEW_H_INCLUDED__
