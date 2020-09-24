#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "defs.h"
typedef uint8_t u8;

#include "conf.h"
#ifndef VT100_PARSE_MAXLEN_INIT
# define VT100_PARSE_MAXLEN_INIT 16
#endif
#define VT100_CALLBACK void
#include "vt100.h"
size_t esc_seq_parse (void *state, const char *buf, size_t len, IVt100Action *iface, void *param);
bool esc_seq_parse_init (void *state, size_t size);

struct Vt100__ {
	IVt100Action iface;
	void *param;
	int state;
	size_t len;
	u8 esc_seq_state[128];
};
///////////////////////////////////////////////////////////////////////////////

enum {
	  esc_ = 1
};
void vt100_parse (Vt100 *m_, const char *buf, size_t len)
{
u8 c;
	for (; 0 < len; --len, ++buf) {
		c = *(const u8 *)buf;
size_t used;
		switch (m_->state) {
		case esc_:
			if ((used = esc_seq_parse (m_->esc_seq_state, buf, len, &m_->iface, m_->param)) < len)
				m_->state = 0;
			buf += used;
			len -= used;
			--buf, ++len;
			continue;
		case 0:
			break;
		}
		if (! (c < 0x20 || 0x7e < c)) {
			m_->iface.on_ansi (m_->param, c);
			continue;
		}
		switch (c) {
		case 0x08: // BS
			m_->iface.on_bs (m_->param);
			break;
		case 0x0a: // LF
			m_->iface.on_lf (m_->param);
			break;
		case 0x0d: // CR
			m_->iface.on_cr (m_->param);
			break;
		case 0x1b: // ESC
			m_->state = esc_;
			esc_seq_parse_init (m_->esc_seq_state, sizeof(m_->esc_seq_state));
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// dtor/ctor

void vt100_dtor (Vt100 *m_)
{
	free (m_);
}

Vt100 *vt100_ctor (const IVt100Action *iface, void *param)
{
Vt100 *m_;
	if (NULL == (m_ = (Vt100 *)malloc (sizeof(Vt100))))
		return NULL;
	memset (m_, 0, sizeof(Vt100));

	m_->iface = *iface;
	m_->param = param;
	return m_;
}
