/* ringbuf

Overview:
      Basic FIFO queue implemented without 'realloc()'.

Purpose:
   1. speed performance
      Support direct reading interfaces without 'memcpy()'.
      Support direct writing interfaces without 'memcpy()'.

      -> _peek(), _remove(), _enter_appending(), _leave_appending()
                           , _enter_modifying(), _leave_modifying()

	2. stability assist (for long time execution)
      Only core implements of queue are supplied, never used 'realloc()', 'malloc()' and 'free()'.
      Data memory is given by client
     (not limited to the heap, stack frames and static memory are also acceptable)

      -> _ctor(), _resize()

   3. security data leak avoiding

      -> _ctor(), _resize(), _remove(), _read(), _dtor()

Internal-state:
   (a) normal
     +0     +read_end +write_end    +size
      |  empty  |   data   |   empty  |

   (b) split (cross upper boundary)
     +0   +write_end   +read_end    +size
      |  data2 |   empty   |   data1  |

Note:
    maximum data size is '.size' minus 1.
   (because it means no data when '.read_end' equals '.write_end')
 */

# ifndef INHERITENCE__

#include <stdint.h>
#include "ringbuf.hpp"

#include <stdlib.h> // exit
#include <string.h>
#include <memory.h>
#include <stdbool.h>
typedef  uint8_t u8;

#include "assert.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define ustrlen(x) (unsigned)strlen(x) // part of 64bit supports.
///////////////////////////////////////////////////////////////////////////////
/* readability (with keeping conflict avoided)

Trade-off:
   - function-name searching
 */
#define _dtor          ringbuf_dtor
#define _ctor          ringbuf_ctor
#define _get_empty     ringbuf_get_empty
#define _write         ringbuf_write
#define _peek          ringbuf_peek
#define _read          ringbuf_read
#define _remove        ringbuf_remove
#define _resize        ringbuf_resize
#define _enter_modifying ringbuf_enter_modifying
#define _leave_modifying ringbuf_leave_modifying
#define _enter_appending ringbuf_enter_appending
#define _leave_appending ringbuf_leave_appending

#define SECURITY_ERASE RINGBUF_SECURITY_ERASE

///////////////////////////////////////////////////////////////////////////////
// internal

static void *memmove_with_erase (void *dst_, void *src_, size_t n)
{
BUG(dst_ && src_ && !(dst_ == src_))
	if (! (dst_ && src_ && !(dst_ == src_) && 0 < n))
		return dst_; // invalid argument
u8 *dst, *src;
#ifdef __cplusplus
	dst = (u8 *)dst_; src = const_cast<u8 *>(src_);
#else
	dst = (u8 *)dst_; src = (u8 *)src_;
#endif

	memmove (dst, src, n);
u8 *p, *q;
	if (dst < src) {
		p = max(src, dst +n);
		memset (p, 0, src +n -p); // zero clear
	}
	else /*if (src < dst) */ {
		q = min(src +n, dst);
		memset (src, 0, q -src); // zero clear
	}

	return dst_;
}

# endif //ndef INHERITENCE__
///////////////////////////////////////////////////////////////////////////////
// control

typedef struct {
	uint8_t *data;
	unsigned size;
	unsigned read_end;
	unsigned write_end;
	int flags;
} ringbuf_s;

# ifndef INHERITENCE__
///////////////////////////////////////////////////////////////////////////////
// public

unsigned _get_empty (struct ringbuf *this_)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)

unsigned nodata_len
	= (m_->write_end < m_->read_end)
	?           m_->read_end -m_->write_end
	: m_->size +m_->read_end -m_->write_end;
BUG(0 < nodata_len)
	return nodata_len -1; // cf)Note
}

unsigned _write (struct ringbuf *this_, const void *src_, unsigned cb)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_ && src_ && 0 < cb)
	if (! (m_ && src_ && 0 < cb))
		return 0;
const u8 *src;
	src = (const u8 *)src_;

unsigned empty
	= _get_empty (this_);
	if (0 == (cb = min(cb, empty)))
		return 0;

	if (m_->write_end < m_->read_end) {
		memcpy (m_->data +m_->write_end, src, cb);
		m_->write_end += cb;
	}
	else if (m_->write_end +cb < m_->size) {
		memcpy (m_->data +m_->write_end, src, cb);
		m_->write_end += cb;
	}
	else /*if (! (m_->write_end +cb < m_->size))*/ {
unsigned split
		= m_->size -m_->write_end;
		memcpy (m_->data +m_->write_end, src, split);
		if (split < cb)
			memcpy (m_->data +0, src +split, cb -split);
		m_->write_end = cb -split;
	}
	return cb;
}

// NOTE: (*1) *pos1 not written when *len1 = 0.
static u8 *_peek_internal (struct ringbuf *this_
, int len
, unsigned *datalen1
, unsigned *datapos1
, unsigned *datalen2
)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)

unsigned data_len
	= (m_->write_end < m_->read_end)
	? m_->size +m_->write_end -m_->read_end
	:           m_->write_end -m_->read_end;
BUG(data_len < m_->size) // cf) Note

unsigned cb, len1, pos1, len2;
	if (! (len < 0)) {
		cb = (unsigned)len, pos1 = m_->read_end;
		if (0 == (cb = min(cb, data_len)))
			len1 = len2 = 0;
		else if (! (m_->size < m_->read_end +cb))
			len1 = cb, len2 = 0;
		else /*if (m_->size < m_->read_end +cb)*/
			len1 = m_->size -m_->read_end, len2 = cb -len1;
	}
	else /*if (len < 0) */ {
		cb = (unsigned)-len;
		if (0 == (cb = min(cb, data_len)))
			len2 = len1 = 0;
		else if (! (m_->write_end < cb))
			len2 = 0, len1 = cb, pos1 = m_->write_end -cb;
		else /*if (m_->write_end < cb)*/
			len2 = m_->write_end, len1 = cb -m_->write_end, pos1 = m_->size -len1;
	}

	if (datalen1) *datalen1 = len1;
	if (datapos1 && 0 < len1) *datapos1 = pos1; // (*1)
	if (datalen2) *datalen2 = len2;
	if (! (0 < len1 +len2))
		return NULL;
	return m_->data;
}

// @param len  0 means maximum buffer length.
const u8 *_peek (struct ringbuf *this_
, int len
, unsigned *datalen1
, unsigned *datapos1
, unsigned *datalen2
)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)
	if (0 == len)
		len = (int)m_->size;
	return _peek_internal (this_, len, datalen1, datapos1, datalen2);
}

unsigned _read (struct ringbuf *this_, void *dst_, unsigned cb)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)

BUG(0 < cb)
u8 *dst;
	dst = (u8 *)dst_;
unsigned got[2], pos[2], i;
	pos[1] = 0; // const
ASSERTE(0 <= (int)cb)
	if (!_peek (this_, (int)cb, &got[0], &pos[0], &got[1]))
		return 0;
BUG(0 < got[0])
	for (i = 0; i < 2 && 0 < got[i]; ++i) {
BUG(m_->read_end == pos[i])
		if (dst) {
			memcpy (dst, m_->data + pos[i], got[i]);
			dst += got[i];
		}
		if (SECURITY_ERASE & m_->flags)
			memset (m_->data + pos[i], 0, got[i]); // security erase
		if (! ((m_->read_end += got[i]) < m_->size))
			m_->read_end = 0;
	}
	
	return got[0] + got[1];
}

unsigned _remove (struct ringbuf *this_, unsigned cb)
{
	return _read (this_, NULL, cb); // syntax sugar
}

// NOTE: (*1) safe cast that accepts rewritable memory
//            but never rewrite the memory.
bool _resize (struct ringbuf *this_, void *new_data_, unsigned new_size)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)

	if (new_size == m_->size && new_data_ == m_->data)
		return true; // anything not needed
	if (! (m_->size <= new_size))
		return false; // PENDING: shorten support
u8 *new_data;
	new_data = (u8 *)new_data_;
typedef void *(*syntax_sugar)(void *dst, void *src, size_t n);
syntax_sugar security_memmove
	= (SECURITY_ERASE & m_->flags)
	? memmove_with_erase
	: (syntax_sugar)memmove; // (*1)

unsigned len1, pos1, len2, new_pos1;
ASSERTE(0 <= (int)m_->size)
	if (_peek (this_, (int)m_->size, &len1, &pos1, &len2)) {
		new_pos1 = (pos1 +len1 < m_->size) ? pos1 : new_size - len1;
		if (new_data < m_->data) {
			if (0 < len2)
				security_memmove (new_data, m_->data, len2);
			security_memmove (new_data + new_pos1, m_->data + pos1, len1);
		}
		else if (m_->data < new_data) {
			security_memmove (new_data + new_pos1, m_->data + pos1, len1);
			if (0 < len2)
				security_memmove (new_data, m_->data, len2);
		}
		else /*if (new_data == m_->data)*/ {
			if (! (new_pos1 == pos1))
				security_memmove (new_data + new_pos1, m_->data + pos1, len1);
		}
		m_->read_end = new_pos1;
	}

	m_->data = new_data, m_->size = new_size;
	return true;
}

u8 *_enter_appending (struct ringbuf *this_
, unsigned *emptylen1
, unsigned *emptypos1
, unsigned *emptylen2
)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)

unsigned len1, len2;
	if (m_->write_end < m_->read_end)
		len1 = m_->read_end -1 -m_->write_end, len2 = 0;
	else if (0 == m_->read_end)
		len1 = m_->size -1 -m_->write_end, len2 = 0;
	else /*if (0 < m_->read_end)*/
		len1 = m_->size - m_->write_end, len2 = m_->read_end -1;

	if (emptylen1) *emptylen1 = len1;
	if (emptypos1) *emptypos1 = m_->write_end;
	if (emptylen2) *emptylen2 = len2;
	return m_->data;
}
void _leave_appending (struct ringbuf *this_, unsigned add)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)

	if (m_->write_end < m_->read_end) {
BUG(m_->write_end +add < m_->read_end)
		m_->write_end = min(m_->write_end +add, m_->read_end -1);
		return;
	}
BUG(add < m_->size +m_->read_end - m_->write_end)
	if (add < m_->size -m_->write_end)
		m_->write_end += add;
	else /*if (m_->size -m_->write_end <= add)*/
		m_->write_end = min(m_->write_end +add -m_->size, m_->read_end -1);
}

u8 *_enter_modifying (struct ringbuf *this_
, unsigned *datalen1
, unsigned *datapos1
, unsigned *datalen2
)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)

	return _peek_internal (this_, (int)m_->size, datalen1, datapos1, datalen2);
}
void _leave_modifying (struct ringbuf *this_)
{
	// do nothing now
}

///////////////////////////////////////////////////////////////////////////////
// destructor / constructor

void _dtor (struct ringbuf *this_)
{
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)

	if (SECURITY_ERASE & m_->flags) {
		if (m_->write_end < m_->read_end /* && m_->read_end < m_->size */) {
			m_->read_end = 0;
			memset (m_->data + m_->read_end, 0, m_->size - m_->read_end); // security erase
		}
		if (m_->read_end < m_->write_end) {
			m_->read_end = m_->write_end; // safety
			memset (m_->data + m_->read_end, 0, m_->write_end - m_->read_end); // security erase
		}
	}
	memset (m_, 0, sizeof(ringbuf_s));
}

void _ctor (struct ringbuf *this_, unsigned cb, void *buf_, unsigned size, int flags)
{
#ifndef __cplusplus // for C++ convert tool
BUGc(sizeof(ringbuf_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(ringbuf_s), cb)
#endif //ndef __cplusplus
ringbuf_s *m_;
	m_ = (ringbuf_s *)this_;
BUG(m_)
BUG(buf_ && 0 < size)

	memset (m_, 0, sizeof(ringbuf_s));
	m_->data = (u8 *)buf_;
	m_->size = size;
	m_->flags = flags;
}

# endif //ndef INHERITENCE__
