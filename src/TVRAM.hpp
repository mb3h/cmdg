#ifndef TVRAM_H_INCLUDED__
#define TVRAM_H_INCLUDED__

#include "common/pixel.h"
#include "iface/VRAM.hpp"

struct TVRAM {
#ifdef __x86_64__
	uint8_t priv[72]; // gcc's value to x86_64
#else
	uint8_t priv[40]; // gcc's value to i386
#endif
};
void TVRAM_ctor (struct TVRAM *this_, unsigned cb, uint16_t ws_col, uint16_t ws_row);
void TVRAM_release (struct TVRAM *this_);
void TVRAM_reset_dirty (struct TVRAM *this_);
struct VRAM_i *TVRAM_query_VRAM_i (struct TVRAM *this_);

#endif // TVRAM_H_INCLUDED__
