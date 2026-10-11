#ifndef ON_PTOUT_H_INCLUDED__
#define ON_PTOUT_H_INCLUDED__

//#include <gtk/gtk.h> // GdkWindow

/* Readability sugar.
   Never use 'READABILITY_SUGAR_ON_PTOUT' directly,
   only use 'on_ptout'. */
typedef struct {
	void (*ctor) (void (*on_changed) (/*u12*/ uint16_t px,
	                                  /*u12*/ uint16_t py,
	                                  /*u12*/ uint16_t width,
	                                  /*u12*/ uint16_t height),
	              _Bool acceptLFasCRLF);
	/*u9*/ uint16_t (*set_frontend_size) (/*u10*/ uint16_t ws_col,
	                                       /*u9*/ uint16_t ws_row);
	int (*write_output) (const char *buf, unsigned cb);
	void (*draw_to_gdk2) (/*u12*/ uint16_t px,
	                      /*u12*/ uint16_t py,
                         /*u12*/ uint16_t width,
	                      /*u12*/ uint16_t height,
	                      GdkWindow *window, GdkGC *fgcl_gc);
} READABILITY_SUGAR_ON_PTOUT;
extern const READABILITY_SUGAR_ON_PTOUT *on_ptout;

#endif // ON_PTOUT_H_INCLUDED__
