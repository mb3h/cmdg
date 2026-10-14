//#include <features.h>
//#include <endian.h>
#include <stdint.h>

#include "memory_usage.h"
#include "helper/pow2_ge.h"

#include <stdio.h>
#include <stdlib.h> // exit
#include <memory.h>
#include <malloc.h>

typedef  uint8_t u8, u1;
typedef uint64_t u64;

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

#define FALSE 0
#define TRUE  1
///////////////////////////////////////////////////////////////
// constants

#define INITIAL_RESERVED 128

///////////////////////////////////////////////////////////////
// VT100

#define VTO  "\033[0m"
#define VTRR "\033[1;31m"
#define VTGG "\033[1;32m"
#define VTYY "\033[1;33m"
#define VTBB "\033[1;34m"
#define VTMM "\033[1;35m"
#define VTCC "\033[1;36m"

///////////////////////////////////////////////////////////////
// assertion

#define assert(expr) \
	do { \
		if (! (expr)) { \
printf (VTRR "ASSERT" VTO ": " #expr " [%s]" "\n", __func__); \
			exit (-1); \
		} \
	} while (0);

#define caller_bug  assert
#define   flow_bug  assert

///////////////////////////////////////////////////////////////
// singlton memory

typedef struct {
	void *ptr;
	unsigned n;
} ITEM;

typedef struct {
	ITEM *list;
	unsigned list_reserved;
	unsigned list_len;
	unsigned total_n;
#ifdef __i386__
//	u8 align4_padding[];
#else //def __x86_64__
	u8 align8_padding[7];
#endif
} log_usage_s;

static log_usage_s s_textlog_singlton = {0};
static log_usage_s s_imagelog_singlton = {0};

///////////////////////////////////////////////////////////////

static unsigned _binary_seek (const ITEM *a, unsigned n, void *ptr)
{
	if (! (a && 0 < n))
		return 0;

unsigned i, ii;
	i = pow2_ge(n) / 2, ii = i;
u1 more_than = FALSE;
	do {
		ii /= 2;
		if (a[i].ptr == ptr)
			return i;
		more_than
		 = (a[i].ptr < ptr) ? TRUE : FALSE;
		if (!more_than)
			i -= ii;
		else /*if (more_than)*/ {
			if (i +1 == n)
				return i +1;
			while (! (i +ii < n))
				ii /= 2;
			i += ii;
		}
	} while (0 < ii);

	if (more_than)
		++i;
	else if (/*!more_than &&*/ 1 == i && ptr <= a[0].ptr)
		--i;
	return i;
}

static unsigned _get (log_usage_s *m_)
{
	return m_->total_n;
}

static void _add (log_usage_s *m_,
                  void *ptr, unsigned n)
{
unsigned index
	= _binary_seek (m_->list, m_->list_len, ptr);

#if 1 // bugcheck
u1 greater_than_previous
	= (0 == index ||
	   m_->list[index -1].ptr < ptr) ? TRUE : FALSE;
u1 less_than_here
	= (m_->list_len == index ||
	   ptr < m_->list[index].ptr) ? TRUE : FALSE;
flow_bug(greater_than_previous && less_than_here)
#endif

	// caller bug recovery (but old entry lost)
u1 is_found
	= (index < m_->list_len &&
	    ptr == m_->list[index].ptr) ? TRUE : FALSE;
caller_bug(!is_found)
	if (is_found) {
		m_->list[index].n   = n;
		return;
	}

	if (! (m_->list_len < m_->list_reserved)) {
		// expanding size
		m_->list_reserved = (0 == m_->list_reserved)
		                  ? INITIAL_RESERVED
		                  : 2 * m_->list_reserved;
		// heap request
unsigned cb
		= m_->list_reserved * usizeof(ITEM);
		m_->list = (NULL == m_->list)
		         ? (ITEM *)malloc (cb)
		         : (ITEM *)realloc (m_->list, cb);
	}

	// expand list
	if (index < m_->list_len)
		memmove (&m_->list[index +1],
		         &m_->list[index],
		         (m_->list_len -index) * sizeof(ITEM));
	++m_->list_len;

	// new entry
	m_->list[index].ptr = ptr;
	m_->list[index].n   = n;

	// add sum
	m_->total_n += n;
}

static void _remove (log_usage_s *m_, void *ptr)
{
unsigned index
	= _binary_seek (m_->list, m_->list_len, ptr);
u1 is_found
	= (index < m_->list_len &&
	   ptr == m_->list[index].ptr) ? TRUE : FALSE;
caller_bug(is_found)
	if (!is_found)
		return;

	// subtract sum
flow_bug(m_->list[index].n <= m_->total_n)
	m_->total_n -= m_->list[index].n;

	// shrink list
	if (index +1 < m_->list_len)
		memmove (&m_->list[index],
		         &m_->list[index +1],
		         (m_->list_len -(index +1)) * sizeof(ITEM));
	--m_->list_len;
}

///////////////////////////////////////////////////////////////
// readability sugar

static unsigned _textlog_get ()
{ return _get (&s_textlog_singlton); }
static unsigned _imagelog_get ()
{ return _get (&s_imagelog_singlton); }

static void _textlog_add (void *ptr, unsigned n)
{ return _add (&s_textlog_singlton, ptr, n); }
static void _imagelog_add (void *ptr, unsigned n)
{ return _add (&s_imagelog_singlton, ptr, n); }

static void _textlog_remove (void *ptr)
{ return _remove (&s_textlog_singlton, ptr); }
static void _imagelog_remove (void *ptr)
{ return _remove (&s_imagelog_singlton, ptr); }

static const READABILITY_SUGAR_TEXTLOG_USAGE _textlog_usage = {
	.get = _textlog_get,
	.add = _textlog_add,
	.remove = _textlog_remove,
};
const READABILITY_SUGAR_TEXTLOG_USAGE *textlog_usage = &_textlog_usage;

static const READABILITY_SUGAR_IMAGELOG_USAGE _imagelog_usage = {
	.get = _imagelog_get,
	.add = _imagelog_add,
	.remove = _imagelog_remove,
};
const READABILITY_SUGAR_IMAGELOG_USAGE *imagelog_usage = &_imagelog_usage;
