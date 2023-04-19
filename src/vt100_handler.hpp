#ifndef VT100_HANDLER_H_INCLUDED__
#define VT100_HANDLER_H_INCLUDED__

#include "common/pixel.h"
#include "iface/VRAM.hpp"
#include "iface/parser_event.hpp"

struct vt100_cursor_state {
	uint8_t is_show :1;
	uint8_t reserved:7;
};
struct vt100_handler {
#ifdef __x86_64__
	uint8_t priv[302+2]; // gcc's value to x86_64
#else //def __i386__
	uint8_t priv[262+2]; // gcc's value to i386
#endif
};
void vt100_handler_ctor (
	  struct vt100_handler *this_, unsigned cb
	, struct VRAM_i *vram, VRAM_s *vram_this_
	, struct VRAM_i *bg, VRAM_s *bg_this_
	, struct parser_event_i *handler, parser_event_s *handler_this_
);
void vt100_handler_release (struct vt100_handler *this_);
int vt100_handler_request (struct vt100_handler *this_, const char *buf, unsigned cb);
void vt100_handler_cursor_state (struct vt100_handler *this_, struct vt100_cursor_state cs);

#endif // VT100_HANDLER_H_INCLUDED__
