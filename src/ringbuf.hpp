#ifndef RINGBUF_HPP_INCLUDED__
#define RINGBUF_HPP_INCLUDED__

#define RINGBUF_SECURITY_ERASE 1
struct ringbuf {
#ifdef __x86_64__
	uint8_t priv[24]; // gcc's value to x86_64
#else // __i386__
	uint8_t priv[20]; // gcc's value to i386
#endif
};

void ringbuf_dtor (struct ringbuf *this_);
void ringbuf_ctor (struct ringbuf *this_, unsigned cb, void *buf_, unsigned size, int flags);

unsigned ringbuf_get_empty (struct ringbuf *this_);
unsigned ringbuf_write (struct ringbuf *this_, const void *src_, unsigned cb);
const uint8_t *ringbuf_peek (
	  struct ringbuf *this_
	, int len
	, unsigned *datalen1
	, unsigned *datapos1
	, unsigned *datalen2
);
unsigned ringbuf_read (struct ringbuf *this_, void *dst_, unsigned cb);
unsigned ringbuf_remove (struct ringbuf *this_, unsigned cb);
_Bool ringbuf_resize (struct ringbuf *this_, void *new_begin_, unsigned new_size);

uint8_t *ringbuf_enter_modifying (
	  struct ringbuf *this_
	, unsigned *datalen1
	, unsigned *datapos1
	, unsigned *datalen2
);
void ringbuf_leave_modifying (struct ringbuf *this_);

uint8_t *ringbuf_enter_appending (
	  struct ringbuf *this_
	, unsigned *emptylen1
	, unsigned *emptypos1
	, unsigned *emptylen2
);
void ringbuf_leave_appending (struct ringbuf *this_, unsigned add);

#endif //nndef RINGBUF_HPP_INCLUDED__
