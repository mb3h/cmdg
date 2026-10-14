#ifndef GTK2_FONT_H_INCLUDED__
#define GTK2_FONT_H_INCLUDED__

//#include <gtk/gtk.h> // cairo_t GdkWindow ...

enum GTK2_FONT_TYPE {
	INSTALLED = 0,
	CUSTOM,
};
/* Readability sugar.
   Never use 'READABILITY_SUGAR_GTK2_FONT' directly,
   only use 'gtk2_font'. */
typedef struct {
//	void (*ctor) (/*u24*/ uint32_t bgcl/*, GtkDrawingArea *da*/);
	_Bool (*load) (const char *font_path);
	void (*load_pango) (const char *font_name, GtkWidget *widget);
	enum GTK2_FONT_TYPE (*get_type) ();
	void (*put_box) (/*u10*/ uint16_t col, /*u9*/ uint16_t pt_row,
	                 /*u24*/ uint32_t rgb24, cairo_t *cr);
	void (*put_bpp24) (/*u10*/ uint16_t col, /*u9*/ uint16_t pt_row,
	                   const void *rgb888_img, cairo_t *cr);
	void (*put) (/*u10*/ uint16_t col, /*u9*/ uint16_t pt_row,
	             /*u24*/ uint32_t fgcl24,
	             /*u24*/ uint32_t bgcl24,
	             /*u21*/ uint32_t uni21, cairo_t *cr);
	void (*put_pango) (/*u10*/ uint16_t col, /*u9*/ uint16_t pt_row,
	                   /*u24*/ uint32_t fgcl24,
	                   /*u24*/ uint32_t bgcl24,
	                   /*u1*/ uint8_t bold, /*u1*/ uint8_t underline,
	                   /*u21*/ uint32_t uni21,
	                   GdkWindow *window, GdkGC *fgcl_gc,
	                   cairo_t *cr);
	uint8_t (*width) ();
	uint8_t (*height) ();
} READABILITY_SUGAR_GTK2_FONT;
extern const READABILITY_SUGAR_GTK2_FONT *gtk2_font;

// helper export
typedef struct {
	int approximate_digit_width;
	int height;
	int ascent;
	int descent;
} FONT_SIZE;
PangoLayout *gtk_widget_set_pango_font (GtkWidget *widget,
                                        const char *name,
                                        FONT_SIZE *font);

#endif //ndef GTK2_FONT_H_INCLUDED__
