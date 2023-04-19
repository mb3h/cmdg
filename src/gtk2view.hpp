#ifndef GTK2VIEW_H_INCLUDED__
#define GTK2VIEW_H_INCLUDED__

#include "common/pixel.h"
#include "iface/VRAM.hpp"
#include "iface/idle_event.hpp"
#include "iface/key_event.hpp"

struct gtk2view {
#ifdef __x86_64__
	uint8_t priv[220+4]; // gcc's value to x86_64
#else // __i386__
	uint8_t priv[160]; // gcc's value to i386
#endif
};
void gtk2view_ctor (struct gtk2view *this_, unsigned cb);
void gtk2view_release (struct gtk2view *this_);
GtkDrawingArea *gtk2view_init (struct gtk2view *this_, struct VRAM_i *fg, VRAM_s *fg_this_, struct VRAM_i *bg, VRAM_s *bg_this_);
_Bool gtk2view_set_font (GtkWidget *da, const char *font_name, uint8_t pango_undraw);
void gtk2view_size_change (GtkWidget *da, uint16_t ws_col, uint16_t ws_row);
void gtk2view_grid_change (GtkWidget *da, uint16_t ws_col_min, uint16_t ws_row_min);
void gtk2view_show_all (GtkWidget *da);
void gtk2view_post_dirty (GtkWidget *da);
void gtk2view_set_idle_event (GtkWidget *da, struct idle_event_i *handler, idle_event_s *handler_this_);
void gtk2view_set_key_event (GtkWidget *da, struct key_event_i *handler, key_event_s *handler_this_);

#endif // GTK2VIEW_H_INCLUDED__
