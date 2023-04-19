#include <gtk/gtk.h>

#include <stdint.h>
#include "common/pixel.h"
#define VRAM_s void
#define idle_event_s void
#define key_event_s void
#include "gtk2view.hpp"

#include "portab.h"
#include "unicode.h"
#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <string.h>
#include <errno.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;
typedef  int32_t s32;

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) < (b) ? (b) : (a))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define arraycountof(x) (sizeof(x)/(sizeof((x)[0])))

#define swap(a, b, type__) do { type__ t__; t__ = a, a = b, b = t__; } while (0)
#define ALIGN32(x) (-32 & (x) +32 -1)
#define PADDING32(x) (ALIGN32(x) - (x))

#define IDLE_INTERVAL_MS FRAME_MS
#define MONO(w) argb32_to_gdk_color (RGB24(w,w,w))
// configuration
#define FRAME_MS 16 // [ms]
#define XY_MAX_DEFAULT 2048 // >= 80x24
#define X32Y_MAX_DEFAULT 128 // >= ALIGN32(80)/32 x24
#define TAB_BG MONO(192)
#define MENU_BG argb32_to_gdk_color (RGB24(60,59,55))
#define HEIGHT_STATUS_BAR 21
static u16 HEIGHT_MENU_BAR = 0;
static u16 HEIGHT_NOTE_TAG = 0;
static u16 EDGE_NOTEBOOK = 0;

#ifndef __cplusplus
# define GDKHINTCONST 
# define GTKCSTR 
# define GTKFN
#else //def __cplusplus
// need for C++ (*)[-fpermissive] error caused. Test: g++4.6.3 / gtk+2.24.10
# define GDKHINTCONST (GdkWindowHints)
# define GTKCSTR (gchar *) // 'const' not typed in <gtk/gtkitemfactory.h> (for old C ?)
# define GTKFN (GtkItemFactoryCallback)
#endif // __cplusplus

#define ARGB32(a, r, g, b) (0xff0000UL & (a) << 24 | RGB24(r, g, b))

struct gtk2_helper;
#define gtk_helper_s struct gtk2_helper
#include "gtk2_helper.hpp"
typedef struct gtk2_helper gtk2_helper_s;

///////////////////////////////////////////////////////////////////////////////
// non-state

// NOTE: thread un-safe.
static const GdkColor *argb32_to_gdk_color (u32 argb32)
{
static GdkColor colors[16];
static int index = 0;
	index = ++index % 16;
	gtk2_helper_argb32_to_gdk_color (argb32, &colors[index]);
	return &colors[index];
}

///////////////////////////////////////////////////////////////////////////////

#include "lock.hpp"
typedef struct lock lock_s;
struct gdk2_helper;
#define gdk_helper_s struct gdk2_helper
#include "gdk2_helper.hpp"
typedef struct gdk2_helper gdk2_helper_s;

typedef struct gtk2view_ {
	u8 pending_task;
	u8 lf_width;
	u8 lf_height;
	u8 lf_undraw;
	u32 font_is_changed :1;
	u32 padding_4       :31;
	char font_name[64];
	u16 xy_max; // (*2)
	u16 x32y_max; // (*3)
	u16 ws_col;
	u16 ws_row;
	struct VRAM_i *fg;
	VRAM_s *fg_this_;
	struct VRAM_i *bg;
	VRAM_s *bg_this_;

	struct idle_event_i *idle_handler;
	idle_event_s *idle_handler_this_;

	struct key_event_i *key_handler;
	key_event_s *key_handler_this_;

	struct gdk_helper_i *gdh;
	gdk2_helper_s gdh_this_;

	u8 *paint_rgb888;
	u32 cb_paint_rgb888;

	guint idle_id;
	guint timer_id;

	lock_s idle_control;
#ifdef __x86_64__
	u8 padding_220[4];
#else //def __i386__
#endif
} gtk2view_s;

#ifdef READ_SHELL_THREAD
#define _lock_cleanup() lock_dtor(&m_->idle_control);
#define _lock_warmup() lock_ctor(&m_->idle_control, sizeof(m_->idle_control));
#define _lock() lock_lock(&m_->idle_control);
#define _unlock() lock_unlock(&m_->idle_control);
#else
#define _lock_cleanup()
#define _lock_warmup()
#define _lock()
#define _unlock()
#endif

static GQuark quark_cmdg_view = 0;

#define NOTIFY_IDLE_TO_CLIENT 1
#define DRAW_DIRTY_TASK       2

///////////////////////////////////////////////////////////////////////////////
// dtor / ctor

static void gtk2view_reset (gtk2view_s *m_)
{
#ifndef __cplusplus
	memset (m_, 0, sizeof(gtk2view_s));
#else //def __cplusplus
	m_->pending_task       = 0;
	m_->idle_id            = 0;
	m_->timer_id           = 0;
	m_->lf_width           = 0;
	m_->lf_height          = 0;
	m_->lf_undraw          = 0;
	m_->font_is_changed    = 0; // FALSE
	m_->font_name[0]       = '\0';
	m_->xy_max             = 0;
	m_->x32y_max           = 0;
	m_->ws_col             = 0;
	m_->ws_row             = 0;
	m_->fg                 = NULL;
	m_->fg_this_           = NULL;
	m_->idle_handler       = NULL;
	m_->idle_handler_this_ = NULL;
	m_->key_handler        = NULL;
	m_->key_handler_this_  = NULL;
#endif // __cplusplus
}

void gtk2view_release (struct gtk2view *this_)
{
gtk2view_s *m_;
	m_ = (gtk2view_s *)this_;

_lock_cleanup()
	gdk2_helper_release (&m_->gdh_this_);
	if (m_->paint_rgb888)
		free (m_->paint_rgb888);
	gtk2view_reset (m_);
}

#ifdef __cplusplus
void gtk2view_dtor (struct gtk2view *this_)
{
}
#endif // __cplusplus

void gtk2view_ctor (struct gtk2view *this_, unsigned cb)
{
#ifndef __cplusplus
BUGc(sizeof(gtk2view_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(gtk2view_s), cb)
#endif //ndef __cplusplus
gtk2view_s *m_;
	m_ = (gtk2view_s *)this_;
	gtk2view_reset (m_);
	m_->cb_paint_rgb888 = ALIGN32(16 * 24) / 8 * 32; // start by 16x32.
	m_->paint_rgb888 = (u8 *)malloc (m_->cb_paint_rgb888);
	gdk2_helper_ctor (&m_->gdh_this_, sizeof(m_->gdh_this_));
	m_->gdh = gdk2_helper_query_gdk_helper_i (&m_->gdh_this_);
_lock_warmup()
}

///////////////////////////////////////////////////////////////////////////////
// setter / getter

static void gtk2view_set_control (gtk2view_s *m_, GtkWidget *da)
{
	if (0 == quark_cmdg_view)
		quark_cmdg_view = g_quark_from_static_string ("cmdg-view");
ASSERTE(0 < quark_cmdg_view)
	g_object_set_qdata (G_OBJECT(da), quark_cmdg_view, (gpointer)m_);
}

static gtk2view_s *gtk2view_get_control (GtkWidget *da)
{
gtk2view_s *m_;
	m_ = (gtk2view_s *)g_object_get_qdata (G_OBJECT(da), quark_cmdg_view);
ASSERTE(m_)
	return m_;
}

void gtk2view_set_idle_event (GtkWidget *da, struct idle_event_i *handler, idle_event_s *handler_this_)
{
gtk2view_s *m_;
	m_ = gtk2view_get_control (da);
	m_->idle_handler = handler;
	m_->idle_handler_this_ = handler_this_;
}

void gtk2view_set_key_event (GtkWidget *da, struct key_event_i *handler, key_event_s *handler_this_)
{
gtk2view_s *m_;
	m_ = gtk2view_get_control (da);
	m_->key_handler = handler;
	m_->key_handler_this_ = handler_this_;
}

static void gtk2view_set_vram (gtk2view_s *m_, struct VRAM_i *fg, VRAM_s *fg_this_, struct VRAM_i *bg, VRAM_s *bg_this_)
{
	m_->fg = fg, m_->fg_this_ = fg_this_;
	m_->bg = bg, m_->bg_this_ = bg_this_;
}

///////////////////////////////////////////////////////////////////////////////
// pixel drawing

static void draw_bg_dirty32 (gtk2view_s *m_, u32 bg_dirty32, struct gtk_helper_i *gh, gtk_helper_s *gh_this_, int x, int y)
{
u32 cb_need;
	cb_need = ALIGN32(m_->lf_width * 24) / 8 * m_->lf_height;
	while (m_->cb_paint_rgb888 < cb_need)
		m_->paint_rgb888 = (u8 *)realloc (m_->paint_rgb888, m_->cb_paint_rgb888 *= 2);

u8 i;
	for (i = 0; i < 32 && x < m_->ws_col; ++i, ++x) {
		if (! (1 << i & bg_dirty32))
			continue;
u32 offset, got;
		for (offset = 0, got = 1/*dummy*/; offset < cb_need && 0 < got; offset += got)
			got = m_->bg->read_image (m_->bg_this_, x, y, PF_RGB888, m_->paint_rgb888 +offset, cb_need -offset);
		gh->paint_data (gh_this_, x * m_->lf_width, y * m_->lf_height, m_->lf_width, m_->lf_height, PF_RGB888, m_->paint_rgb888);
	}
}

static void draw_fg_utf8 (gtk2view_s *m_, u16 x, u16 y, u32 u, u64 a, struct gtk_helper_i *gh, gtk_helper_s *gh_this_)
{
	u = (u < 0x100) ? u << 24 : (u < 0x10000) ? u << 16 : (u < 0x1000000) ? u << 8 : u;
char utf8[4 +1];
	store_be32 (utf8, u);
	utf8[4] = '\0'; // no works now (for future)
u32 fg_argb32, bg_argb32;
	fg_argb32 = (u32)(0xffffff & a >> ATTR_FG_LSB);
	bg_argb32 = (u32)(0xffffff & a >> ATTR_BG_LSB);
	if (ATTR_REV_MASK & a)
		swap (fg_argb32, bg_argb32, u32);
	gh->draw_text (gh_this_,
		  x * m_->lf_width
		, y * m_->lf_height
		, utf8
		, strlen (utf8)
		, fg_argb32
		, bg_argb32
		);
	if (0 == m_->lf_undraw)
		return;
	gh->fill_rectangle (gh_this_,
		  x * m_->lf_width
		, (y +1) * m_->lf_height - m_->lf_undraw
		, m_->lf_width
		, m_->lf_undraw
		, bg_argb32
		);
}

// PENDING: Unicode-17..21 support
#define ALLOW_OVERDRAW 0
#define DENY_OVERDRAW  1
static unsigned draw_fg_dirty32 (gtk2view_s *m_, u32 fg_dirty32, struct gtk_helper_i *gh, gtk_helper_s *gh_this_, int x, int y, int flags)
{
unsigned enabled;
	enabled = (u32)-1 >> max(m_->ws_col, x +32) - m_->ws_col;
int i;
	for (i = 0; i < 32 && x < m_->ws_col; ++i, ++x) {
		if (! (1 << i & fg_dirty32))
			continue;
		// draw one matrix
u32 u;
u64 a;
		m_->fg->read_text (m_->fg_this_, x, y, 1, &u, &a);
		if (0 == u)
			u = ' ';
int char_width;
		char_width = wcwidth_bugfix (utf8to16 (u));
		if (1 < char_width && 31 == i && DENY_OVERDRAW & flags)
			break;
		draw_fg_utf8 (m_, x, y, u, a, gh, gh_this_);
		if (2 == char_width)
			++i, ++x;
	}
	return enabled & ~((u32)-1 >> 32 - min(i, 32));
}

static bool gtk2view_draw_dirty (GtkWidget *da)
{
gtk2view_s *m_;
	m_ = gtk2view_get_control (da);
gtk2_helper_s gh_this_;
	gtk2_helper_ctor (&gh_this_, sizeof(gh_this_), da);
struct gtk_helper_i *gh;
	gh = gtk2_helper_query_gtk_helper_i (&gh_this_);
u32 fg_leftover32; u32 u; u64 a;
int n;
	for (n = 0; n < ALIGN32(m_->ws_col)/32 * m_->ws_row; ++n) {
int x, y;
		y = (n * 32) / ALIGN32(m_->ws_col);
		x = (n * 32) % ALIGN32(m_->ws_col);
		if (0 == x)
			fg_leftover32 = 0;
u32 bg_dirty32;
		if (fg_leftover32) {
			m_->bg->dirty_lock (m_->bg_this_, x, y, 1);
#if 0 // need nothing until support transparent bg-color text
			draw_bg_dirty32 (m_, 1, gh, &gh_this_, x, y);
#endif
			m_->bg->dirty_unlock (m_->bg_this_, x, y, 1);

			m_->fg->dirty_lock (m_->fg_this_, x -1, y, 1);
			draw_fg_utf8 (m_, x -1, y, u, a, gh, &gh_this_);
			m_->fg->dirty_unlock (m_->fg_this_, x -1, y, 1);
		}
u32 fg_dirty32;
		bg_dirty32 = m_->bg->dirty_lock (m_->bg_this_, x, y, 0);
		fg_dirty32 = m_->fg->dirty_lock (m_->fg_this_, x, y, 0);
		if (fg_leftover32) {
			fg_dirty32 &= ~1;
			fg_leftover32 = 0;
		}
u32 any_dirty32, bg_trans32, fg_trans32;
		any_dirty32 = fg_dirty32 | bg_dirty32;
		bg_trans32 = m_->bg->check_transparent (m_->bg_this_, x, y, any_dirty32);
		fg_trans32 = m_->fg->check_transparent (m_->fg_this_, x, y, any_dirty32);
		if (any_dirty32 & ~bg_trans32 & fg_trans32)
			draw_bg_dirty32 (m_, any_dirty32 & ~bg_trans32 & fg_trans32, gh, &gh_this_, x, y);
		m_->bg->dirty_unlock (m_->bg_this_, x, y, any_dirty32);

		fg_leftover32 = 0;
		if (any_dirty32 & (bg_trans32 | ~fg_trans32))
			fg_leftover32 = draw_fg_dirty32 (m_, any_dirty32 & (bg_trans32 | ~fg_trans32), gh, &gh_this_, x, y, DENY_OVERDRAW);
		if (fg_leftover32)
			m_->fg->read_text (m_->fg_this_, x +31, y, 1, &u, &a);
		m_->fg->dirty_unlock (m_->fg_this_, x, y, any_dirty32 ^ fg_leftover32);
	}
	gtk2_helper_release (&gh_this_);
	return true;
}
#if 0 //def DRAW_DIRTY_THREAD
static gboolean gtk2view_draw_dirty_thunk (gpointer da)
{
	gtk2view_draw_dirty (GTK_WIDGET(da));
	return G_SOURCE_CONTINUE;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// broadcast

static void gtk2view_con_resize (gtk2view_s *m_, u16 ws_col, u16 ws_row)
{
#if 0 // TODO: adjust to new version design
	if (m_->xy_max < ws_row * ws_col) {
unsigned xy_max; // (*2)
		xy_max = (0 < m_->xy_max) ? m_->xy_max : XY_MAX_DEFAULT;
		while (xy_max < ws_col * ws_row)
			xy_max *= 2;
unsigned cb;
//		cb = sizeof(m_->BPP24s[0]) * xy_max;
//		m_->BPP24s = (BPP24 *)(m_->BPP24s ? realloc (m_->BPP24s, cb) : malloc (cb))))
//ASSERTE(m_->BPP24s)
		cb = sizeof(u32) * xy_max;
		m_->UTF21s = (u32 *)(m_->UTF21s ? realloc (m_->UTF21s, cb) : malloc (cb));
ASSERTE(m_->UTF21s)
		cb = sizeof(u64) * xy_max;
		m_->ATTR50s = (u64 *)(m_->ATTR50s ? realloc (m_->ATTR50s, cb) : malloc (cb));
ASSERTE(m_->ATTR50s)
		m_->xy_max = xy_max;
	}
	if (m_->x32y_max < ALIGN32(ws_col)/32 * ws_row) {
unsigned x32y_max; // (*3)
		x32y_max = (0 < m_->x32y_max) ? m_->x32y_max : X32Y_MAX_DEFAULT;
		while (x32y_max < ALIGN32(ws_col)/32 * ws_row)
			x32y_max *= 2;
unsigned cb;
		cb = sizeof(u32) * x32y_max;
		m_->DIRTY21s = (u32 *)(m_->DIRTY21s ? realloc (m_->DIRTY21s, cb) : malloc (cb));
ASSERTE(m_->DIRTY21s)
		m_->x32y_max = x32y_max;
	}
#else
	m_->fg->resize (m_->fg_this_, ws_col, ws_row);
#endif
	m_->ws_col = ws_col;
	m_->ws_row = ws_row;
}

static void gtk2view_con_erase (gtk2view_s *m_)
{
#if 0 // TODO: adjust to new version design
unsigned xy;
	xy = m_->ws_row * m_->ws_col;
//	memset (m_->BPP24s, 0, sizeof(m_->BPP24s[0]) * xy);
	memset (m_->UTF21s, 0, sizeof(m_->UTF21s[0]) * xy);
	memset (m_->ATTR50s, 0, sizeof(m_->ATTR50s[0]) * xy);
unsigned x32y;
	x32y = ALIGN32(m_->ws_col)/32 * m_->ws_row;
	memset (m_->DIRTY21s, 0, sizeof(m_->DIRTY21s[0]) * x32y);
#else
//	m_->fg->erase (m_->fg_this_);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// GTK event
static void gtk2view_kick_idle (GtkWidget *da, u8 kick_task);

// GDK_DESTROY(1)
static void frame_on_destroy (GtkObject *object, gpointer data)
{
	gtk_main_quit ();
}

// GDK_EXPOSE(2)
static gint gtk2view_on_expose (GtkWidget *da, GdkEventExpose *event, gpointer data)
{
gtk2view_s *m_;
	m_ = gtk2view_get_control (GTK_WIDGET(da));
	if (!m_->gdh->is_redraw_expose (&m_->gdh_this_, &event->area))
		return TRUE;
// PENDING: minimize dirty (speed optimize)
	m_->bg->set_dirty_rect (m_->bg_this_, 0, 0, 0, 0);
	gtk2view_kick_idle (da, DRAW_DIRTY_TASK);
	return TRUE;
}
static gint statusbar_on_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	cairo_t *cr;

	if (! (event && (cr = gdk_cairo_create (event->window))))
		return TRUE;
	gdk_cairo_set_source_color (cr, MENU_BG);
	gdk_cairo_rectangle (cr, &event->area);
	cairo_fill (cr);
	cairo_destroy (cr);

	return TRUE;
}

static gboolean gtk2view_on_idle (gpointer da)
{
gtk2view_s *m_;
	m_ = gtk2view_get_control (GTK_WIDGET(da));
#if 0 //def DRAW_DIRTY_THREAD
	gtk2view_draw_dirty (GTK_WIDGET(da));
	if (m_->idle_handler && m_->idle_handler->on_idle)
		m_->idle_handler->on_idle (m_->idle_handler_this_);
	return G_SOURCE_CONTINUE;
#else //ndef DRAW_DIRTY_THREAD
_lock()
	if (DRAW_DIRTY_TASK & m_->pending_task && gtk2view_draw_dirty (GTK_WIDGET(da)))
		m_->pending_task &= ~DRAW_DIRTY_TASK;
	if (NOTIFY_IDLE_TO_CLIENT & m_->pending_task) {
		//m_->pending_task = ~NOTIFY_IDLE_TO_CLIENT;
		m_->idle_handler->on_idle (m_->idle_handler_this_);
	}
	m_->idle_id = 0;
_unlock()
	return G_SOURCE_REMOVE;
#endif
}
static void gtk2view_kick_idle (GtkWidget *da, u8 kick_task)
{
gtk2view_s *m_;
	m_ = gtk2view_get_control (da);
	m_->pending_task |= kick_task; // WARNING: take care of reset timing for task lost.
	if (0 == m_->idle_id && 0 < m_->pending_task) {
_lock()
		m_->idle_id = g_idle_add (gtk2view_on_idle, (gpointer)da);
_unlock()
	}
}
static gboolean gtk2view_on_timer (gpointer da)
{
	gtk2view_kick_idle (GTK_WIDGET(da), 0);
	return G_SOURCE_CONTINUE;
}

static void frame_on_size_allocate (GtkWidget *app, GtkAllocation *a)
{
gtk2view_s *m_;
	// get environment constant.
	if (! (0 < HEIGHT_MENU_BAR)) {
GtkWidget *menu;
		menu = gtk_widget_get_child (app, GTK_TYPE_MENU_BAR);
		HEIGHT_MENU_BAR = menu->allocation.height;
	}
GtkWidget *da;
	da = gtk_widget_get_child (app, GTK_TYPE_DRAWING_AREA);
	if (! (0 < EDGE_NOTEBOOK && 0 < HEIGHT_NOTE_TAG)) {
GtkWidget *notebook;
		notebook = gtk_widget_get_child (app, GTK_TYPE_NOTEBOOK);
		if (! (0 < EDGE_NOTEBOOK)) {
			EDGE_NOTEBOOK = (notebook->allocation.width - da->allocation.width) / 2;
		}
		if (! (0 < HEIGHT_NOTE_TAG)) {
			HEIGHT_NOTE_TAG = notebook->allocation.height - (da->allocation.height +EDGE_NOTEBOOK);
		}
	}
	// 
	m_ = gtk2view_get_control (da);
// {THUNK-PASS} da
u16 ws_col, ws_row;
	ws_col = da->allocation.width / m_->lf_width;
	ws_row = da->allocation.height / m_->lf_height;
static const int RESIZE = 1, ERASE = 2;
int what_needed;
	what_needed  = 0;
	if (! (ws_col == m_->ws_col && ws_row == m_->ws_row)) {
		what_needed |= RESIZE | ERASE;
	}
	if (m_->font_is_changed) {
		what_needed |= ERASE;
		m_->font_is_changed = 0; // FALSE
	}

	if (RESIZE & what_needed)
		gtk2view_con_resize (m_, ws_col, ws_row);
	if (ERASE & what_needed)
		gtk2view_con_erase (m_);
}

// GDK_KEY_PRESS(8) GDK_KEY_RELEASE(9)
static gint frame_on_key_event (GtkWidget *app, GdkEventKey *event, gpointer data)
{
gtk2view_s *m_;
GtkWidget *da;
	da = gtk_widget_get_child (GTK_WIDGET(app), GTK_TYPE_DRAWING_AREA);
	m_ = gtk2view_get_control (da);
s32 vt100;
	if (-1 == (vt100 = m_->gdh->keycode_to_vt100 (&m_->gdh_this_, event)))
		return FALSE;
	if (0 == vt100)
		return FALSE; // TODO: inspect detail of 'META key' on GTK.
char cccc[4];
	store_le32 (cccc, (u32)vt100);
unsigned cb;
	cb = (vt100 < 0x100) ? 1 : (vt100 < 0x10000) ? 2 : (vt100 < 0x1000000) ? 3 : 4;
int sent;
	if ((sent = m_->key_handler->on_key (m_->key_handler_this_, cccc, cb)) < 1) {
		return FALSE;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// GTK event (UI frontend)

static void menu_on_test (gpointer data, guint action, GtkWidget *menu_item)
{
GtkWindow *app;
	app = gtk_widget_get_root (menu_item);
GtkWidget *da;
	da = gtk_widget_get_child (GTK_WIDGET(app), GTK_TYPE_DRAWING_AREA);
// {THUNK-PASS} da
//gtkmain_queue_draw_dirty()
	gtk2view_kick_idle (da, DRAW_DIRTY_TASK);
}

static void menu_on_quit (gpointer data, guint action, GtkWidget *menu_item)
{
GtkWindow *app;

	app = gtk_widget_get_root (menu_item);
	gtk_widget_destroy (GTK_WIDGET(app));
}

///////////////////////////////////////////////////////////////////////////////
// interface

void gtk2view_post_dirty (GtkWidget *da)
{
//gtkmain_queue_draw_dirty()
	gtk2view_kick_idle (da, DRAW_DIRTY_TASK);
}

///////////////////////////////////////////////////////////////////////////////
// interface (forwarder)

void gtk2view_size_change (GtkWidget *da, u16 ws_col, u16 ws_row)
{
gtk2view_s *m_;
	m_ = gtk2view_get_control (da);
	gtk_widget_set_size_request (da, ws_col * m_->lf_width, ws_row * m_->lf_height);
}

bool gtk2view_set_font (GtkWidget *da, const char *font_name, u8 pango_undraw)
{
gtk2view_s *m_;
	m_ = gtk2view_get_control (da);
u8 lf_width, lf_ascent, lf_descent;
gtk2_helper_s gh_this_;
	gtk2_helper_ctor (&gh_this_, sizeof(gh_this_), da);
struct gtk_helper_i *gh;
	gh = gtk2_helper_query_gtk_helper_i (&gh_this_);
bool is_changed;
	is_changed = gh->set_font (&gh_this_, font_name, RGB24(0,0,0), &lf_width, &lf_ascent, &lf_descent) ? 1 : 0; // TRUE : FALSE
	gtk2_helper_release (&gh_this_);
	if (!is_changed)
		return false;
	m_->lf_width = lf_width;
	m_->lf_height = lf_ascent + lf_descent;
	m_->lf_undraw = pango_undraw;
	strncpy (m_->font_name, font_name, sizeof(m_->font_name) -1);
	m_->font_name[sizeof(m_->font_name) -1] = '\0';

	m_->bg->set_grid (m_->bg_this_, m_->lf_width, m_->lf_height);

	m_->font_is_changed = 1; // TRUE
	return true;
}

void gtk2view_grid_change (GtkWidget *da, u16 ws_col_min, u16 ws_row_min)
{
GtkWindow *app;
	app = gtk_widget_get_root (da);
gtk2view_s *m_;
	m_ = gtk2view_get_control (da);
GdkGeometry hints;
		hints.base_width = 2 * EDGE_NOTEBOOK;
		hints.base_height = HEIGHT_MENU_BAR +HEIGHT_NOTE_TAG +EDGE_NOTEBOOK +HEIGHT_STATUS_BAR;
		hints.width_inc = m_->lf_width;
		hints.height_inc = m_->lf_height;
		hints.min_width = hints.base_width + hints.width_inc * ws_col_min;
		hints.min_height = hints.base_height + hints.height_inc * ws_row_min;
		gtk_window_set_geometry_hints (
			app,
			GTK_WIDGET(app),
			&hints,
			GDKHINTCONST(GDK_HINT_RESIZE_INC | GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE)
			);
}

void gtk2view_show_all (GtkWidget *da)
{
GtkWindow *app;
	app = gtk_widget_get_root (da);
	gtk_widget_show_all (GTK_WIDGET(app));
}

///////////////////////////////////////////////////////////////////////////////
// setup

GtkDrawingArea *gtk2view_init (struct gtk2view *this_, struct VRAM_i *fg, VRAM_s *fg_this_, struct VRAM_i *bg, VRAM_s *bg_this_)
{
gtk2view_s *m_;
	m_ = (gtk2view_s *)this_;
	gtk2view_set_vram (m_, fg, fg_this_, bg, bg_this_);
GtkWidget *app;
	if (NULL == (app = gtk_window_new (GTK_WINDOW_TOPLEVEL)))
		return NULL;
	gtk_widget_modify_bg (app, GTK_STATE_NORMAL, MENU_BG); // outside of GtkNotebook
	gtk_window_set_policy (GTK_WINDOW(app), TRUE, TRUE, TRUE);
	g_signal_connect (app, "destroy", G_CALLBACK(frame_on_destroy), NULL);
	g_signal_connect (app, "size_allocate", G_CALLBACK(frame_on_size_allocate), NULL);
//	g_signal_connect (app, "configure_event", G_CALLBACK(frame_on_configure), NULL);
	g_signal_connect (app, "key_press_event", G_CALLBACK(frame_on_key_event), NULL);
	g_signal_connect (app, "key_release_event", G_CALLBACK(frame_on_key_event), NULL);

	// GtkBox(V) (1/3) GtkMenuBar
# if 1
GtkItemFactoryEntry menu_items[] = {
		// .path .accelerator .callback .callback_action .item_type .extra_data
		  { GTKCSTR"/_File", NULL, NULL, 0, GTKCSTR"<Branch>" }
		, { GTKCSTR"/File/Quit", GTKCSTR"<CTRL>Q", GTKFN menu_on_quit, 0, GTKCSTR"<StockItem>", GTK_STOCK_QUIT }
// TODO:
//		, { GTKCSTR"/_Edit", NULL, NULL, 0, GTKCSTR"<Branch>" }
//		, { GTKCSTR"/Edit/Scro_ll", NULL, menu_on_scroll, 0, GTKCSTR"<Item>" }
		, { GTKCSTR"/_Options", NULL, NULL, 0, GTKCSTR"<Branch>" }
	};
GtkAccelGroup *accel_group;
	accel_group = gtk_accel_group_new ();
GtkItemFactory *factory;
	factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<Main>", accel_group);
	gtk_item_factory_create_items (factory, arraycountof(menu_items), menu_items, (void *)NULL);
GtkWidget *menu_bar;
	menu_bar = gtk_item_factory_get_widget (factory, "<Main>");
# endif

	// GtkWindow -> GtkBox(V)
GtkWidget *vbox;
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(app), vbox);
	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(menu_bar), FALSE, FALSE, 0);
	gtk_widget_show (GTK_WIDGET(menu_bar));

	// GtkBox(V) (2/3) GtkNotebook
GtkNotebook *notebook;
	notebook = GTK_NOTEBOOK(gtk_notebook_new ());
	gtk_widget_modify_bg (GTK_WIDGET(notebook), GTK_STATE_NORMAL, TAB_BG); // bg of tablabel
	gtk_notebook_set_show_border (notebook, FALSE);
	gtk_notebook_set_tab_pos (notebook, GTK_POS_TOP);
	gtk_notebook_set_scrollable (notebook, TRUE);
	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(notebook), FALSE, FALSE, 0);
	gtk_notebook_popup_enable (notebook);

	// GtkBox(H) (2/3) GtkLabel
GtkWidget *label;
	label = gtk_label_new ("#1"); // TODO SSH local channel ID
	gtk_widget_show (label);
	// GtkBox(H) (1/3) GtkImage
GtkWidget *icon;
	icon = gtk_image_new_from_stock (GTK_STOCK_FILE, GTK_ICON_SIZE_MENU);
	gtk_widget_show (icon);
	// GtkBox(H) (3/3) GtkImage
GtkWidget *icon2;
	icon2 = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_widget_show (icon2);

	// GtkNotebook (1/2) GtkBox(H)
GtkBox *tab;
	tab = GTK_BOX(gtk_hbox_new (FALSE, 0));
	gtk_box_pack_start (tab, icon, FALSE, FALSE, 0);
	gtk_box_pack_start (tab, label, FALSE, FALSE, 0);
	gtk_box_pack_end (tab, icon2, FALSE, FALSE, 0);

	// GtkNotebook (2/2) GtkDrawingArea
GtkWidget *da;
	da = gtk_drawing_area_new ();
ASSERTE(GTK_IS_DRAWING_AREA(da))
	g_signal_connect (da, "expose_event", G_CALLBACK(gtk2view_on_expose), NULL);
//	g_signal_connect (da, "configure_event", G_CALLBACK(gtk2view_on_configure), NULL);
	gtk_widget_show (da);

	gtk_notebook_append_page (notebook, da, GTK_WIDGET(tab));
	gtk_widget_show (GTK_WIDGET(notebook));
	// GtkBox(V) (3/3) GtkDrawingArea
GtkWidget *da_sbar;
	da_sbar = gtk_drawing_area_new ();
ASSERTE(GTK_IS_DRAWING_AREA(da_sbar))
	gtk_widget_set_size_request (da_sbar, 1, HEIGHT_STATUS_BAR);
	gtk_box_pack_end (GTK_BOX(vbox), da_sbar, FALSE, FALSE, 0);
	g_signal_connect (da_sbar, "expose_event", G_CALLBACK(statusbar_on_expose), NULL);
	gtk_widget_show (da_sbar);

	gtk_widget_show (vbox);

	gtk2view_set_control (m_, da);
//#if 0 //def DRAW_DIRTY_THREAD
//	gdk_threads_add_timeout (FRAME_MS, gtk2view_draw_dirty, (gpointer)da);
//#endif
//#if 1 //ndef READ_SHELL_THREAD
//gtkmain_queue(TASK_READ_SHELL)
//#endif
	gtk2view_kick_idle (da, DRAW_DIRTY_TASK|NOTIFY_IDLE_TO_CLIENT);
	gdk_threads_add_timeout (IDLE_INTERVAL_MS, gtk2view_on_timer, (gpointer)da);

	return GTK_DRAWING_AREA(da);
}
