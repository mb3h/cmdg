/* queue

Overview:
      FIFO block-queue implementation as fast as possible.
     (automatic stretch)

Design-policy:
   1. speed performance
      Support direct reading interfaces without depending 'memcpy()' as possible.

      -> _peek(), _remove(), _ctor(), _resize(), _margin_clear()

   2. flexibility choice of when to terminate memory blocks.

      -> _set_boundary(), _write(), _peek()

   3. (read only) access to last-in block and now-entrying block.

      -> _peek() [n = -2, -1]

   4. security data leak avoiding

      -> _ctor(), _resize(), _peek(), _margin_clear(), _dtor()

Internal-behavior:
   (a) normal _peek()
     +0     +read_end +write_end    +size                              +(size+margin)
      |  empty  |   data   |   empty  |               (unused)               |
                ->|  cb  |<-
               _peek() retval

   (a) boundary _peek()
     +0   +write_end   +read_end    +size       +(size+margin_used)    +(size+margin)
      |  data2 |   empty   |   data1  | (part of data2) |      (unused)      |
                               ->|          cb          |<-
                              _peek() retval
 */

#include <stdint.h>
#include "queue.hpp"
#include "ringbuf.hpp"
#include "portab.h"

#include <stdlib.h> // exit
#include <memory.h>
#include <malloc.h>
typedef uint32_t u32;
typedef  uint8_t u8;

#include "assert.h"

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

///////////////////////////////////////////////////////////////////////////////
// build settings

#define INITIAL_RESERVED_BLOCKS 16

///////////////////////////////////////////////////////////////////////////////
/* readability (with keeping conflict avoided)

Trade-off:
   - function-name searching
 */
#define _dtor         queue_dtor
#define _ctor         queue_ctor
#define _write        queue_write
#define _write_u8     queue_write_u8
#define _write_be32   queue_write_be32
#define _set_boundary queue_set_boundary
#define _peek         queue_peek
#define _remove       queue_remove

#define _resize       queue_resize
#define _margin_clear queue_margin_clear

#define SECURITY_ERASE RINGBUF_SECURITY_ERASE

///////////////////////////////////////////////////////////////////////////////
// control

// refer to private members of superclass
#define INHERITENCE__
#include "ringbuf.cpp"
#undef INHERITENCE__

typedef struct {
	ringbuf_s super;
	unsigned *lengths;
	unsigned lengths_used;
	unsigned lengths_size;
	unsigned margin_used;
	unsigned margin;
} queue_s;

///////////////////////////////////////////////////////////////////////////////
// internal

static void _resize (queue_s *aux, unsigned new_size, unsigned new_margin)
{
BUG(aux)
struct ringbuf *super_; ringbuf_s *m_;
	super_ = (struct ringbuf *)(m_ = &aux->super);

BUG(m_->size <= new_size && aux->margin <= new_margin)
	if (m_->size == new_size && aux->margin == new_margin)
		return; // resize not needed

u8 *old_data, *new_data;
	if (SECURITY_ERASE & m_->flags) {
		old_data = m_->data;
		new_data = (u8 *)malloc (new_size + new_margin);
	}
	else {
		new_data = (u8 *)realloc (m_->data, new_size + new_margin);
		old_data = new_data;
	}

	if (0 < aux->margin_used)
		memmove (new_data +new_size, old_data +m_->size, aux->margin_used);
unsigned old_size = m_->size;
	ringbuf_resize (super_, new_data, new_size);

	if (SECURITY_ERASE & m_->flags) {
		memset (old_data, 0, old_size +aux->margin_used);
		free (old_data);
	}
	aux->margin = new_margin;
}

static void _margin_clear (queue_s *aux)
{
BUG(aux)

	if (! (0 < aux->margin_used)) return;
ringbuf_s *m_;
	m_ = &aux->super;
	if (SECURITY_ERASE & m_->flags)
		memset (m_->data + m_->size, 0, aux->margin_used); // security erase
	aux->margin_used = 0;
}

///////////////////////////////////////////////////////////////////////////////
// public

void _write (struct queue *this_, const void *val, unsigned cb)
{
queue_s *aux;
	aux = (queue_s *)this_;
BUG(aux)
struct ringbuf *super_; ringbuf_s *m_;
	super_ = (struct ringbuf *)(m_ = &aux->super);

unsigned empty;
	if ((empty = ringbuf_get_empty (super_)) < cb) {
unsigned new_size;
		new_size = m_->size * 2;
		while (! (m_->size -1 -empty + cb <= new_size -1))
			new_size *= 2;
		_resize (aux, new_size, aux->margin);
	}

	ringbuf_write (super_, val, cb);
	aux->lengths[aux->lengths_used -1] += cb; // current block expand
}

// (*1) empty block cannot defined.
void _set_boundary (struct queue *this_)
{
queue_s *aux;
	aux = (queue_s *)this_;
BUG(aux)

	if (0 == aux->lengths[aux->lengths_used -1])
		return; // (*1)

	if (! (aux->lengths_used < aux->lengths_size))
		aux->lengths = (unsigned *)realloc (aux->lengths, sizeof(unsigned) * (aux->lengths_size *= 2));
	aux->lengths[aux->lengths_used++] = 0;
}

/* @param n = 0:first block
             -1:last unfreezed(now modifying) block
             -2:last freezed block (*)only when last unfreezed block is empty.
 */
const u8 *_peek (struct queue *this_, int n, unsigned *cb)
{
queue_s *aux;
	aux = (queue_s *)this_;
BUG(aux)

BUG(0 == n || -1 == n || -2 == n)
BUG(0 < aux->lengths_used)
int len;
	if (0 == n && 1 < aux->lengths_used) // [0] must freezed(set boundary) block.
		len = aux->lengths[0];
	else if (-1 == n && 0 < aux->lengths[aux->lengths_used -1]) // [-1] must unfreezed block.
		len = -aux->lengths[aux->lengths_used -1];
	else if (-2 == n && 1 < aux->lengths_used && 0 == aux->lengths[aux->lengths_used -1])
		len = -aux->lengths[aux->lengths_used -2];
	else {
		if (cb)
			*cb = 0;
		return NULL; // matched block not found.
	}
BUG(len < 0 || 0 < len)

	_margin_clear (aux);
struct ringbuf *super_; ringbuf_s *m_;
	super_ = (struct ringbuf *)(m_ = &aux->super);

unsigned len1, pos1, len2;
	ringbuf_peek (super_, len, &len1, &pos1, &len2);
BUG(len1 +len2 == (len < 0 ? -len : len))
	if (0 == len2) {
		if (cb)
			*cb = len1;
		return m_->data + pos1;
	}
	if (aux->margin < len2) {
unsigned new_margin
		= aux->margin * 2;
		while (new_margin < len2)
			new_margin *= 2;
void *new_data, *old_data;
		if (SECURITY_ERASE & m_->flags) {
			old_data = m_->data;
			new_data = malloc (m_->size + new_margin);
		}
		else {
			m_->data = realloc (m_->data, m_->size + new_margin);
			new_data = m_->data;
		}
		ringbuf_resize (super_, new_data, m_->size);
		if (SECURITY_ERASE & m_->flags)
			free (old_data);
		aux->margin = new_margin;
	}
BUG(pos1 +len1 == m_->size)	
	memcpy (m_->data +m_->size, m_->data +0, len2);
	aux->margin_used = len2;
	if (cb)
		*cb = len1 +len2;
	return m_->data + pos1;
}
unsigned _remove (struct queue *this_, unsigned cb)
{
queue_s *aux;
	aux = (queue_s *)this_;
BUG(aux)
struct ringbuf *super_; ringbuf_s *m_;
	super_ = (struct ringbuf *)(m_ = &aux->super);

unsigned removed, retval, i;
BUG(0 < cb)
	retval = removed = ringbuf_remove (super_, cb);
	for (i = 0; i +1 < aux->lengths_used && !(removed < aux->lengths[i]); ++i) {
		removed -= aux->lengths[i];
		aux->lengths[i] = 0;
	}
	if (0 < removed && i < aux->lengths_used && 0 < aux->lengths[i]) {
BUG(removed < aux->lengths[i])
		aux->lengths[i] -= removed;
		removed = 0;
	}
BUG(0 == removed) // invalid return value of rigbuf_remove()
//BUG(i < aux->lengths_used) // delete up to latest unterminated block
	aux->lengths_used -= i;
	if (0 < aux->lengths_used)
		memmove (&aux->lengths[0], &aux->lengths[i], sizeof(aux->lengths[0]) * aux->lengths_used);
	_margin_clear (aux);

	return retval;
}

void _write_u8 (struct queue *this_, u8 val)
{
	_write (this_, &val, 1);
}

void _write_be32 (struct queue *this_, u32 val)
{
u8 bytes[4];
	store_be32 (bytes, val);
	_write (this_, &bytes, 4);
}

///////////////////////////////////////////////////////////////////////////////
// destructor / constructor

void _dtor (struct queue *this_)
{
queue_s *aux;
	aux = (queue_s *)this_;
BUG(aux)

	_margin_clear (aux);
struct ringbuf *super_; ringbuf_s *m_;
	super_ = (struct ringbuf *)(m_ = &aux->super);
	ringbuf_dtor (super_);
	free (aux->lengths);
	memset (aux, 0, sizeof(queue_s));
}

void _ctor (struct queue *this_, unsigned cb, unsigned size, unsigned margin, int flags)
{
#ifndef __cplusplus // for C++ convert tool
BUGc(sizeof(queue_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(queue_s), cb)
#endif //ndef __cplusplus
queue_s *aux;
	aux = (queue_s *)this_;
BUG(aux)

void *buf
	= malloc (size + margin); // cf) Internal-behavior:
unsigned *lengths
	= (unsigned *)malloc (sizeof(unsigned) * INITIAL_RESERVED_BLOCKS);
ASSERTE(buf && lengths)

	ringbuf_ctor (
		  (struct ringbuf *)&aux->super, usizeof(aux->super)
		, buf, size, flags);
	aux->margin       = margin;
	aux->margin_used  = 0;
	aux->lengths      = lengths;
	aux->lengths_size = INITIAL_RESERVED_BLOCKS;
	aux->lengths_used = 0;
	aux->lengths[aux->lengths_used++] = 0;
}
