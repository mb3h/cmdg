#include <gtk/gtk.h>

#include <stdint.h>
#define VRAM_s void
struct mediator_;
#define idle_event_s struct mediator_
#define key_event_s struct mediator_
#define parser_event_s struct mediator_
#include "mediator.hpp"

#ifdef SSH_NOT_AVAILABLE
#include "ptmx.hpp"
#else //ndef SSH_NOT_AVAILABLE
#include "ssh2.hpp"
#endif
#include "log.h"
#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <memory.h>
#include <errno.h> // EIO
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

///////////////////////////////////////////////////////////////////////////////
// private

#include "vt100_handler.hpp"
typedef struct vt100_handler vt100_handler_s;
typedef struct vt100_cursor_state vt100_cursor_state_s;
typedef struct mediator_ {
	GtkWindow *appframe;
	struct conn_i *remote;
#ifdef SSH_NOT_AVAILABLE
	struct ptmx shell;
#else //ndef SSH_NOT_AVAILABLE
	struct ssh2 shell;
#endif
	GtkWidget *da;
	vt100_handler_s recv_parser;
	u8 is_app_closed :1;
	u8 reserved      :7;
} mediator_s;

///////////////////////////////////////////////////////////////////////////////
// trigger event

static int mediator_on_key (mediator_s *m_, const char *input, unsigned cb)
{
	return m_->remote->write (&m_->shell, input, cb);
}
static struct key_event_i o_key_event = {
	.on_key = mediator_on_key,
};
struct key_event_i *mediator_query_key_event_i (struct mediator *this_) { return &o_key_event; }

static bool mediator_on_idle (mediator_s *m_)
{
	// TODO:
char buf[1024];
unsigned got_total;
	got_total = 0;
int got;
vt100_cursor_state_s cs;
	cs.is_show = 1;
	while (0 < (got = m_->remote->read (&m_->shell, buf, sizeof(buf)))) {
		if (0 == got_total) {
			cs.is_show = 0;
			vt100_handler_cursor_state (&m_->recv_parser, cs);
		}
		got_total += got;
		vt100_handler_request (&m_->recv_parser, buf, got);
	}
bool remote_closed;
	remote_closed = false;
	if (-1 == got && EIO == errno)
		remote_closed = true;
	else if (! (-1 == got && EWOULDBLOCK == errno))
		;
	if (0 == cs.is_show) {
		cs.is_show = 1;
		vt100_handler_cursor_state (&m_->recv_parser, cs);
	}
	if (remote_closed && 0 == m_->is_app_closed) {
		m_->is_app_closed = 1;
		gtk_widget_destroy (GTK_WIDGET(m_->appframe));
	}
	return true;
}
static struct idle_event_i o_idle_event = {
	.on_idle = mediator_on_idle,
};
struct idle_event_i *mediator_query_idle_event_i (struct mediator *this_) { return &o_idle_event; }

///////////////////////////////////////////////////////////////////////////////
// test

bool mediator_connect (
	  struct mediator *this_
	, const char *hostname, u16 port, const char *ppk_path
	, u16 ws_col, u16 ws_row
)
{
mediator_s *m_;
	m_ = (mediator_s *)this_;
// ptmx(bash) setup
#ifdef SSH_NOT_AVAILABLE
	m_->remote = ptmx_ctor (&m_->shell, sizeof(m_->shell));
BUG(m_->remote)
#else //ndef SSH_NOT_AVAILABLE
	m_->remote = ssh2_ctor (&m_->shell, sizeof(m_->shell));
BUG(m_->remote)
	ssh2_preconnect (&m_->shell, hostname, port, ppk_path);
#endif

	if (! m_->remote->connect (&m_->shell, ws_col, ws_row)) {
echo (VTRR "cannot connect." VTO "\n");
		return false;
	}
	return true;
}

#include "gtk2view.hpp"
typedef struct gtk2view gtk2view_s;

static void test_post_dirty (GtkWidget *da)
{
	gtk2view_post_dirty (da);
}

void mediator_release (struct mediator *this_)
{
mediator_s *m_;
	m_ = (mediator_s *)this_;
	vt100_handler_release (&m_->recv_parser);
}

#ifdef __cplusplus
void mediator_dtor (struct mediator *this_)
{
}
#endif // __cplusplus

static void mediator_dirty_notify (mediator_s *m_)
{
	test_post_dirty (m_->da);
}
static struct parser_event_i o_parser_event_i = {
	.dirty_notify = mediator_dirty_notify,
};
struct parser_event_i *mediator_query_parser_event_i (struct mediator *this_) { return &o_parser_event_i; }

#define gtk_helper_s void
#include "gtk2_helper.hpp" // gtk_widget_get_root

void mediator_ctor (
	  struct mediator *this_, unsigned cb
	, GtkWidget *da
	, struct VRAM_i *fg, VRAM_s *fg_this_
	, struct VRAM_i *bg, VRAM_s *bg_this_
)
{
mediator_s *m_;
	m_ = (mediator_s *)this_;
#ifndef __cplusplus
BUGc(sizeof(mediator_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(mediator_s), cb)
	memset (m_, 0, sizeof(mediator_s));
#else //def __cplusplus
	m_->appframe = NULL;
	m_->remote   = NULL;
	m_->shell    = NULL;
	m_->is_app_closed = 0;
#endif // __cplusplus
	m_->da     = da;
struct parser_event_i *dirty_event_handler;
	dirty_event_handler = mediator_query_parser_event_i (this_);
	vt100_handler_ctor (&m_->recv_parser, sizeof(m_->recv_parser), fg, fg_this_, bg, bg_this_, dirty_event_handler, m_);
	m_->appframe = gtk_widget_get_root (da);
}
