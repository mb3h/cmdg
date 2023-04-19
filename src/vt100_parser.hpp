#ifndef VT100_PARSER_H_INCLUDED__
#define VT100_PARSER_H_INCLUDED__

#include "iface/vt100_event.hpp"

struct vt100_parser {
#ifdef __x86_64__
	uint8_t priv[40 +88]; // gcc's value to x86_64
#else //def __i386__
	uint8_t priv[28 +88]; // gcc's value to i386
#endif
};
void vt100_parser_release (struct vt100_parser *this_);
void vt100_parser_ctor (struct vt100_parser *this_, unsigned cb, struct vt100_event_i *iface, vt100_event_s *param);
void vt100_parser_reset (struct vt100_parser *this_);
unsigned vt100_parser_get_resume (struct vt100_parser *this_);
int vt100_parser_request (struct vt100_parser *this_, const void *buf_, unsigned len);

#endif // VT100_PARSER_H_INCLUDED__
