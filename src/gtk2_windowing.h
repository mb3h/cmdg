#ifndef GTK2_WINDOWING_H_INCLUDED__
#define GTK2_WINDOWING_H_INCLUDED__

//#include <gtk/gtk.h> // GtkDrawingArea

/* Readability sugar.
   Never use 'READABILITY_SUGAR_GTK2_WINDOWING' directly,
   only use 'gtk2_windowing'. */
typedef struct {
	_Bool (*create_gui) (
		_Bool (*on_key) (uint16_t keycode,
		                 /*u1*/ uint8_t is_keydown),
		void (*on_gui_resized) (/*u12*/ uint16_t da_width,
		                        /*u12*/ uint16_t da_height),
		/*u12*/ uint16_t ws_x, /*u12*/ uint16_t ws_y,
		/*u6*/ uint8_t statusbar_height
	);
	GtkDrawingArea *(*show_gui) (/*u12*/ uint16_t da_width,
	                             /*u12*/ uint16_t da_height,
	                             /*u24*/ uint32_t bgcl24,
	                             const char *statusbar_font_name);
	void (*on_backend_changed) (/*u12*/ uint16_t x,
	                            /*u12*/ uint16_t y,
	                            /*u12*/ uint16_t width,
	                            /*u12*/ uint16_t height);
} READABILITY_SUGAR_GTK2_WINDOWING;
extern const READABILITY_SUGAR_GTK2_WINDOWING *gtk2_windowing;

#endif //ndef GTK2_WINDOWING_H_INCLUDED__
