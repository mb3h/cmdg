#ifndef GTK_HELPER_H_INCLUDED__
#define GTK_HELPER_H_INCLUDED__

struct gtk_helper_i {
	_Bool (*set_font) (gtk_helper_s *this_,
		  const char *font_name
		, uint32_t bg_argb32, uint8_t *lf_width
		, uint8_t *lf_ascent, uint8_t *lf_descent
		);
	void (*draw_text) (gtk_helper_s *this_,
		  int x, int y
		, const char *text, int length
		, uint32_t fg_argb32, uint32_t bg_argb32
		);
	void (*fill_rectangle) (gtk_helper_s *this_,
		  int x, int y
		, int width, int height
		, uint32_t argb32
		);
	void (*paint_data) (gtk_helper_s *this_,
		  int x, int y
		, int width, int height
		, enum pixel_format pf
		, const void *data
		);
};

#endif // GTK_HELPER_H_INCLUDED__
