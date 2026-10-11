#ifndef GDK2_HELPER_H_INCLUDED__
#define GDK2_HELPER_H_INCLUDED__

#include "iface/gdk_helper.hpp"

struct gdk2_helper {
#ifdef __x86_64__
	uint8_t priv[8]; // gcc's value to x86_64
#else
	uint8_t priv[4]; // gcc's value to i386
#endif
};
void gdk2_helper_ctor (struct gdk2_helper *this_, unsigned cb);
void gdk2_helper_release (struct gdk2_helper *this_);
struct gdk_helper_i *gdk2_helper_query_gdk_helper_i (struct gdk2_helper *this_);

#endif // GDK2_HELPER_H_INCLUDED__
