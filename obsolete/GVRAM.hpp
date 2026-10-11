#ifndef GVRAM_H_INCLUDED__
#define GVRAM_H_INCLUDED__

#include "common/pixel.h"
#include "iface/VRAM.hpp"

struct GVRAM {
#ifdef __x86_64__
	uint8_t priv[136]; // gcc's value to x86_64
#else //def __i386__
	uint8_t priv[84]; // gcc's value to i386
#endif
};
void GVRAM_ctor (struct GVRAM *this_, unsigned cb, uint16_t ws_col, uint16_t ws_row);
void GVRAM_release (struct GVRAM *this_);
void GVRAM_reset_dirty (struct GVRAM *this_);
struct VRAM_i *GVRAM_query_VRAM_i (struct GVRAM *this_);

#endif // GVRAM_H_INCLUDED__
