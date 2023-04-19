#ifndef GDK_HELPER_H_INCLUDED__
#define GDK_HELPER_H_INCLUDED__

struct gdk_helper_i {
	int32_t (*keycode_to_vt100) (gdk_helper_s *this_,
		GdkEventKey *event
		);
	_Bool (*is_redraw_expose) (gdk_helper_s *this_,
		const GdkRectangle *r
		);
};

#endif // GDK_HELPER_H_INCLUDED__
