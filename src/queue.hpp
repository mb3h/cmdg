#ifndef QUEUE_HPP_INCLUDED__
#define QUEUE_HPP_INCLUDED__

struct queue {
#ifdef __x86_64__
	uint8_t priv[24+24]; // gcc's value to x86_64
#else // __i386__
	uint8_t priv[20+20]; // gcc's value to i386
#endif
};
void queue_dtor (struct queue *this_);
void queue_ctor (struct queue *this_, unsigned cb, unsigned size, unsigned margin, int flags);

void queue_write (struct queue *this_, const void *val, unsigned cb);
void queue_write_u8 (struct queue *this_, uint8_t val);
void queue_write_be32 (struct queue *this_, uint32_t val);
void queue_set_boundary (struct queue *this_);
const uint8_t *queue_peek (struct queue *this_, int n, unsigned *cb);
unsigned queue_remove (struct queue *this_, unsigned cb);

#endif //nndef QUEUE_HPP_INCLUDED__
