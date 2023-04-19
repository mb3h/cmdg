#ifndef MEDIATOR_H_INCLUDED__
#define MEDIATOR_H_INCLUDED__

#include "common/pixel.h"
#include "iface/VRAM.hpp"
#include "iface/idle_event.hpp"
#include "iface/key_event.hpp"
#include "iface/parser_event.hpp"

struct mediator {
#ifdef __x86_64__
	uint8_t priv[392+3072]; // TODO: cannot set suitable value unless original sha2.c aes.c are implemented.
#else //def __i386__
	uint8_t priv[336+3072]; // TODO: cannot set suitable value unless original sha2.c aes.c are implemented.
#endif
};
void mediator_release (struct mediator *this_);
void mediator_ctor (
	  struct mediator *this_, unsigned cb
	, GtkWidget *da
	, struct VRAM_i *fg, VRAM_s *fg_this_
	, struct VRAM_i *bg, VRAM_s *bg_this_
);
_Bool mediator_connect (
	  struct mediator *this_
	, const char *hostname, uint16_t port, const char *ppk_path
	, uint16_t ws_col, uint16_t ws_row
);
struct idle_event_i *mediator_query_idle_event_i (struct mediator *this_);
struct key_event_i *mediator_query_key_event_i (struct mediator *this_);
struct parser_event_i *mediator_query_parser_event_i (struct mediator *this_);

#endif // MEDIATOR_H_INCLUDED__
