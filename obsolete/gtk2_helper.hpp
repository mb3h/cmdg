#ifndef GTK2_HELPER_H_INCLUDED__
#define GTK2_HELPER_H_INCLUDED__

#include "iface/gtk_helper.hpp"

GtkWindow *gtk_widget_get_root (GtkWidget *widget);
GtkWidget *gtk_widget_get_child (GtkWidget *checked
	, GType find_type
	);

struct gtk2_helper {
#ifdef __x86_64__
	uint8_t priv[56]; // gcc's value to x86_64
#else //def __i386__
	uint8_t priv[28]; // gcc's value to i386
#endif
};
void gtk2_helper_release (struct gtk2_helper *this_);
void gtk2_helper_ctor (struct gtk2_helper *this_, unsigned cb, GtkWidget *da);
struct gtk_helper_i *gtk2_helper_query_gtk_helper_i (struct gtk2_helper *this_);
void gtk2_helper_argb32_to_gdk_color (uint32_t argb32, GdkColor *dst);

#endif // GTK2_HELPER_H_INCLUDED__
