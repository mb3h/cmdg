#define _XOPEN_SOURCE 500 // usleep (>= 500)
#include <features.h>
#include <stdio.h>
#include <memory.h>
#include "defs.h"
#include <unistd.h> // ssize_t
#include <sys/select.h> // fd_set

#include "fakeconn.h"

#define VT100BUF_MAX 40
typedef struct vt100out vt100out_s;
struct vt100out {
	char buf[VT100BUF_MAX];
	size_t begin;
	size_t end;
};

static ssize_t vt100out_fifo_write (void *this_, const void *buf_, size_t len)
{
vt100out_s *m_;
	m_ = (vt100out_s *)this_;
const char *buf;
	buf = (const char *)buf_;
size_t i;
	for (i = 0; i < len; ++i) {
		m_->buf[m_->end++] = *buf++;
		if (! (m_->end < VT100BUF_MAX))
			m_->end = 0;
	}
	return (ssize_t)len;
}

static ssize_t vt100out_fifo_read (void *this_, void *buf_, size_t len)
{
vt100out_s *m_;
	m_ = (vt100out_s *)this_;
char *buf;
	buf = (char *)buf_;
	while (m_->end == m_->begin)
		usleep (16 * 1000);
	for (; 0 < len && !(m_->end == m_->begin); --len) {
		*buf++ = m_->buf[m_->begin++];
		if (! (m_->begin < VT100BUF_MAX))
			m_->begin = 0;
	}
	return buf - (const char *)buf_;
}

static bool vt100out_fifo_connect (void *this_, uint16_t ws_col, uint16_t ws_row) { return true; }

static void vt100out_fifo_dtor (void *this_)
{
vt100out_s *m_;
	m_ = (vt100out_s *)this_;
	memset (m_, 0, sizeof(vt100out_s));
}

static struct conn_i vt100out;
struct conn_i *vt100out_fifo_ctor (void *this_, size_t cb)
{
	if (cb < sizeof(vt100out_s))
		return NULL;
vt100out_s *m_;
	m_ = (vt100out_s *)this_;
	memset (m_, 0, sizeof(vt100out_s));
	return &vt100out;
}

static struct conn_i vt100out = {
	vt100out_fifo_dtor,
	vt100out_fifo_connect,
	vt100out_fifo_write,
	vt100out_fifo_read,
};
