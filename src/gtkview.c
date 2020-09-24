#ifdef WIN32
#else
# define _XOPEN_SOURCE 500 // usleep (>= 500)
# include <features.h>
# include <endian.h>
# include <gtk/gtk.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <malloc.h>
#include <memory.h>
#include <alloca.h>
#include "defs.h"

#include "gtkview.h"
#include "cmdg.h"
#include "lock.h"
#include "conf.h"
#define SSH2_PORT_DEFAULT 22
#include "vt100.h"


typedef uint32_t u32;
typedef uint8_t u8;
typedef uint16_t u16;
#define arraycountof(x) (sizeof(x)/sizeof(x[0]))
#define ALIGN32(x) (-32 & (x) +32 -1)
#define ALIGN4(x) (-4 & (x) +4 -1)
#define ALIGN(x,a) (((x) +(a) -1)/(a)*(a))

#define ATTR_FG(cl) ((7 & (cl)) << ATTR_FG_LSB)
#define ATTR_BG(cl) ((7 & (cl)) << ATTR_BG_LSB)
#define ATTR_DEFAULT (ATTR_FG(7)|ATTR_BG(0))
#define ATTR_DEC(a) (ATTR_DEFAULT ^ (a))
#define ATTR_ENC ATTR_DEC

extern FILE *stderr_old;
///////////////////////////////////////////////////////////////////////////////
// state

#if !defined(READ_SHELL_THREAD) || !defined(DRAW_DIRTY_THREAD)
enum {
# ifndef READ_SHELL_THREAD
	TASK_READ_SHELL = 1,
# endif
# ifndef DRAW_DIRTY_THREAD
	TASK_DRAW_DIRTY = 2,
# endif
};
#endif

typedef struct BPP24__ BPP24;
struct BPP24__ {
	u16 i_node;
	u16 i_matrix;
};

typedef enum mode {
	LALT = 1 << 0,
	RALT = 1 << 1,
	SCROLL = 1 << 2,
} ViewMode;

struct View__ {
	Cmdg *term;
#ifdef READ_SHELL_THREAD
	Lock dirty_lock;
#endif
#if !defined(READ_SHELL_THREAD) || !defined(DRAW_DIRTY_THREAD)
	guint idle_handler_id;
	u32 idle_task;
#endif
	GtkWindow *appframe;
	GtkDrawingArea *view; // use when need to get View faster than g_object_get_data()
	u16 ws_col;
	u16 ws_row;
	u16 ws_col_min;
	u16 ws_row_min;
	u8 lf_width;
	u8 lf_height;

	u8 lf_undraw;
	bool font_is_changed;
	size_t xy_max;
	size_t x32y_max;
	u32 *UTF21s;
	u8 *ATTR8s;
	u32 *DIRTY21s;
	u32 bgr24[8];

	BPP24 *BPP24s;
	IImage images[16];

	ViewMode mode;
	u32 itr_top;
	bool (*write_gtkkey)(guint keyval, bool pressed, void *param);
	void *write_gtkkey_param;

	char title[128];
};

#if !defined(READ_SHELL_THREAD) || !defined(DRAW_DIRTY_THREAD)
# define gtkmain_queue(x) \
	do { \
		if (0 == m_->idle_task) \
			m_->idle_handler_id = g_idle_add (view_on_idle, (gpointer)m_->view); \
		m_->idle_task |= x; \
	} while (0);
#else
# define gtkmain_queue(x) 
#endif

#ifdef DRAW_DIRTY_THREAD
# define gtkmain_queue_draw_dirty() 
#else
# define gtkmain_queue_draw_dirty() gtkmain_queue(TASK_DRAW_DIRTY)
#endif

#ifdef READ_SHELL_THREAD
// TODO: limited lock. now specified area is ignored and all area is locked.
# define lock(x,y) lock_lock (&m_->dirty_lock);
# define unlock(x,y) lock_unlock (&m_->dirty_lock);
# define range_lock(x,y,width,height) lock_lock (&m_->dirty_lock);
# define range_unlock(x,y,width,height) lock_unlock (&m_->dirty_lock);
#else
# define lock(x,y)
# define unlock(x,y)
# define range_lock(x,y,width,height)
# define range_unlock(x,y,width,height)
#endif

#define XY_MAX_DEFAULT 2048 // >= 80x24
#define X32Y_MAX_DEFAULT 128 // >= ALIGN32(80)/32 x24
#define HEIGHT_STATUS_BAR 21
static u16 HEIGHT_MENU_BAR = 0;
static u16 HEIGHT_NOTE_TAG = 0;
static u16 EDGE_NOTEBOOK = 0;

///////////////////////////////////////////////////////////////////////////////
// utility

static void gdk_color_init_bpp24 (unsigned r, unsigned g, unsigned b, GdkColor *dst)
{
	dst->pixel = 0xff0000 & r << 16 | 0xff00 & g << 8 | 0xff & b;
	dst->red   = 0x101 * (0xff & r);
	dst->green = 0x101 * (0xff & g);
	dst->blue  = 0x101 * (0xff & b);
}

// NOTE: thread un-safe.
static const GdkColor *RGBA (unsigned r, unsigned g, unsigned b, unsigned a)
{
static GdkColor colors[16];
static int index = 0;
	index = ++index % 16;
	GdkColor *dst = &colors[index];
	dst->pixel = 0xff0000UL & a << 24 | 0xff0000 & r << 16 | 0xff00 & g << 8 | 0xff & b;
	dst->red   = 0x101 * (0xff & r);
	dst->green = 0x101 * (0xff & g);
	dst->blue  = 0x101 * (0xff & b);
	return dst;
}
#define RGB(r,g,b) RGBA(r,g,b,0)

#define MONO(w) RGB(w,w,w)
#define BLACK MONO(0)
#define WHITE MONO(255)
#define MENU_BG RGB(60,59,55)

#define DRAWING_AREA(root) widget_get_child (root, GTK_TYPE_DRAWING_AREA)
#define MENU_BAR(root) widget_get_child (root, GTK_TYPE_MENU_BAR)
#define NOTEBOOK(root) widget_get_child (root, GTK_TYPE_NOTEBOOK)
static GtkWidget *widget_get_child (GtkWidget *checked, GType find_type)
{
	if (G_TYPE_CHECK_INSTANCE_TYPE(checked, find_type))
		return GTK_WIDGET(checked);

	if (GTK_IS_NOTEBOOK(checked)) {
gint page_num;
		page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK(checked));
		checked = gtk_notebook_get_nth_page (GTK_NOTEBOOK(checked), page_num);
		return widget_get_child (checked, find_type);
	}

	if (!GTK_IS_CONTAINER(checked))
		return NULL;
GList *list;
#if GTK_CHECK_VERSION(2,22,0)
	list = gtk_container_get_children (GTK_CONTAINER(checked));
#else
	if (GTK_IS_VBOX(checked)) {
GtkVBox *vbox;
		vbox = GTK_VBOX(checked);
		list = vbox->box->children;
	}
	else
		list = gtk_container_get_children (GTK_CONTAINER(checked));
#endif

	while (list) {
		if (checked = widget_get_child (GTK_WIDGET(list->data), find_type))
			return checked;
		list = list->next;
	}
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// image

static u16 images_get_blank (View *m_)
{
static u16 i_node = 0; // TODO
	if (! (++i_node < arraycountof(m_->images)))
		i_node = 1;
	return i_node;
}

u16 image_entry (View *m_, u16 i_node, void *image, IImage *i)
{
	if (! (i_node < arraycountof(m_->images)))
		return 0;
	if (! (0 < i_node))
		i_node = images_get_blank (m_);
range_lock(0,0, m_->ws_col,m_->ws_row)
	m_->images[i_node] = *i;
	m_->images[i_node].this_ = image;
range_unlock(0,0, m_->ws_col,m_->ws_row)
	return i_node;
}

static void image_div_draw (IImage *i, int width, int height, u16 i_matrix, GdkDrawable *drawable, int left, int top)
{
	GdkRectangle area = { left, top, width, height };

u8 *rgba = NULL;
cairo_t *cr;
	do {
		if (NULL == (cr = gdk_cairo_create (drawable)))
			break;
int n_channels;
		if (NULL == i || !(3 == (n_channels = i->get_n_channels (i->this_)) || 4 == n_channels))
			break;
		if (NULL == (rgba = (u8 *)malloc (width * height * ALIGN4(width * n_channels))))
			break;
		i->get_div_pixels (i->this_, width, height, i_matrix, rgba);
GdkPixbuf *pixbuf;
		if (NULL == (pixbuf = gdk_pixbuf_new_from_data ((guchar *)rgba, GDK_COLORSPACE_RGB, (4 == n_channels ? TRUE : FALSE), 8, width, height, ALIGN4(width * n_channels), NULL, NULL)))
			break;
		gdk_cairo_set_source_pixbuf (cr, pixbuf, left, top);
		g_object_unref (pixbuf);
		cairo_paint (cr);
	} while (0);
	if (rgba)
		free (rgba);
	if (cr)
		cairo_destroy (cr);
}

///////////////////////////////////////////////////////////////////////////////
// event

static void menu_on_quit (gpointer data, guint action, GtkWidget *widget)
{
	// TODO
}

// GDK_DESTROY(1)
static void frame_on_destroy (GtkObject *object, gpointer data)
{
	View *m_;
	m_ = (View *)data;

#if !defined(READ_SHELL_THREAD) || !defined(DRAW_DIRTY_THREAD)
	m_->idle_task = 0;
#endif
	gtk_main_quit ();
}

// GDK_CONFIGURE(13)
static gint frame_on_configure (GtkWidget *appframe, GdkEventConfigure *event, gpointer data)
{
GtkWidget *view;
	view = DRAWING_AREA(appframe);
View *m_;
	m_ = (View *)g_object_get_data (G_OBJECT(view), "state");
	if (! (appframe->allocation.height == event->height))
		gtk_widget_set_size_request (view, view->allocation.width, event->height - (HEIGHT_MENU_BAR +HEIGHT_NOTE_TAG +EDGE_NOTEBOOK +HEIGHT_STATUS_BAR));

	return FALSE;
}

static gint view_on_configure (GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
View *m_;
	m_ = (View *)g_object_get_data (G_OBJECT(widget), "state");
	//view_set_dirty (m_, 0, 0, m_->ws_col, m_->ws_row);
	return TRUE;
}

// GDK_EXPOSE(2)
static gint view_on_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
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

///////////////////////////////////////////////////////////////////////////////
// view

static void view_dtor (View *m_)
{
	int n;

	if (m_->BPP24s)
		free (m_->BPP24s);
	if (m_->UTF21s)
		free (m_->UTF21s);
	if (m_->ATTR8s)
		free (m_->ATTR8s);
	if (m_->DIRTY21s)
		free (m_->DIRTY21s);
#ifdef READ_SHELL_THREAD
	lock_dtor (&m_->dirty_lock);
#endif
	free (m_);
}

static View *view_ctor (u32 *bgr24, u16 ws_col_min, u16 ws_row_min)
{
	View *m_;

bool unexpected;
	unexpected = true;
	do {
		if (NULL == (m_ = (View *)malloc (sizeof(View))))
			break;
		memset (m_, 0, sizeof(View));
		m_->itr_top = ITR_CUR;
		// SGR (CSI '3[0-7]m') color mapping
		if (bgr24)
			memcpy (m_->bgr24, bgr24, sizeof(m_->bgr24));
		else {
int i;
			for (i = 0; i < 8; ++i)
				m_->bgr24[i] = (1 & i ? 255 : 0) | (2 & i ? 255 : 0) << 8 | (4 & i ? 255 : 0) << 16;
		}
		m_->ws_col_min = ws_col_min;
		m_->ws_row_min = ws_row_min;
#ifdef READ_SHELL_THREAD
		lock_ctor (&m_->dirty_lock);
#endif
		unexpected = false;
	} while (0);
	if (unexpected) {
		if (m_)
			view_dtor (m_);
		return NULL;
	}

	return m_;
}
void view_connect_gtkkey_receiver (View *m_, bool (*lpfn)(guint keyval, bool pressed, void *param), void *param)
{
	m_->write_gtkkey = lpfn;
	m_->write_gtkkey_param = param;
}

void view_set_title (View *m_, const char *title)
{
	gtk_window_set_title (m_->appframe, title);
	strncpy (m_->title, title, arraycountof(m_->title) -1);
	m_->title[arraycountof(m_->title) -1] = '\0'; // safety
}

u16 view_get_cols (View *m_)
{
	return m_->ws_col;
}

u16 view_get_rows (View *m_)
{
	return m_->ws_row;
}

u8 font_get_width (View *m_)
{
	return m_->lf_width;
}

u8 font_get_height (View *m_)
{
	return m_->lf_height;
}

void view_on_shell_disconnect (View *m_)
{
	gtk_widget_destroy (GTK_WIDGET(m_->appframe));
}

///////////////////////////////////////////////////////////////////////////////

/*
 */
static gboolean view_draw_dirty (gpointer data)
{
	View *m_;
	GtkDrawingArea *view;
	GdkDrawable *drawable;

bool unexpected;
	unexpected = true;
	do {
		if (NULL == (view = GTK_DRAWING_AREA(data))
		 || NULL == (drawable = GDK_DRAWABLE(gtk_widget_get_window (GTK_WIDGET(view))))
		 || NULL == (m_ = (View *)g_object_get_data (G_OBJECT(view), "state"))
		 )
			break;
		unexpected = false;
	} while (0);
	if (unexpected) {
#ifndef DRAW_DIRTY_THREAD
		g_timeout_add (FRAME_MS, view_draw_dirty, (gpointer)data);
		return G_SOURCE_REMOVE;
#else
		return G_SOURCE_CONTINUE;
#endif
	}
GdkGC *gc;
PangoContext *ctx;
GtkStyle *style;
PangoLayout *layout;
cairo_t *cr;
	gc = NULL;
	ctx = NULL;
	layout = NULL;
	cr = NULL;
int n, i;
u32 dw;
	for (n = 0; n < ALIGN32(m_->ws_col)/32 * m_->ws_row; ++n) {
		if (0 == (dw = m_->DIRTY21s[n]))
			continue;
int x, y;
		y = (n * 32) / ALIGN32(m_->ws_col);
		x = (n * 32) % ALIGN32(m_->ws_col);
		for (i = 0; i < 32 && x < m_->ws_col; ++i, ++x) {
			if (! (1 << i & dw))
				continue;
			// draw one matrix
			if (NULL == ctx) {
				if (! ((ctx = gtk_widget_get_pango_context (GTK_WIDGET(view)))
				    && (style = GTK_WIDGET(view)->style)
					 && (layout = pango_layout_new (ctx))
					 && (gc = GTK_WIDGET(view)->style->text_gc[GTK_WIDGET(view)->state])
				   )  )
					break;
				pango_layout_set_font_description (layout, style->font_desc);
				if (0 < m_->lf_undraw && NULL == cr)
					cr = gdk_cairo_create (drawable);
			}
BPP24 g;
u32 u21;
char c;
u8 a;
lock(x,y)
			g = m_->BPP24s[y * m_->ws_col + x];
			u21 = m_->UTF21s[y * m_->ws_col + x];
			c = (0 == u21) ? ' ' : (char)(u8)u21; // TODO UTF-8 support
			a = ATTR_DEC(m_->ATTR8s[y * m_->ws_col + x]);
			m_->DIRTY21s[n] &= ~(1 << i);
unlock(x,y)
			if (0 < g.i_node) {
				image_div_draw (&m_->images[g.i_node], m_->lf_width, m_->lf_height, g.i_matrix, drawable, x * m_->lf_width, y * m_->lf_height);
				continue;
			}
			pango_layout_set_text (layout, &c, 1);
u8 bgr8;
			bgr8 = 7 & a >> ATTR_FG_LSB;
const GdkColor *fgcl, *bgcl, *tmpcl;
			fgcl = RGB(0xff & m_->bgr24[bgr8], 0xff & m_->bgr24[bgr8] >> 8, 0xff & m_->bgr24[bgr8] >> 16);
			bgr8 = 7 & a >> ATTR_BG_LSB;
			bgcl = RGB(0xff & m_->bgr24[bgr8], 0xff & m_->bgr24[bgr8] >> 8, 0xff & m_->bgr24[bgr8] >> 16);
			if (ATTR_REV_MASK & a) {
				tmpcl = fgcl;
				fgcl = bgcl;
				bgcl = tmpcl;
			}
			gdk_gc_set_foreground (gc, fgcl);
PangoAttrList *list;
			if (list = pango_attr_list_new ()) {
PangoAttribute *attr;
				attr = pango_attr_background_new (bgcl->red, bgcl->green, bgcl->blue);
				pango_attr_list_insert (list, attr);
				pango_layout_set_attributes (layout, list);
				pango_attr_list_unref (list);
			}
			else
fprintf (stderr, VTYY "WARN" VTO " !pango_attr_list_new()" "\n");
			gdk_draw_layout (drawable, gc, x * m_->lf_width, y * m_->lf_height, layout);
			if (0 < m_->lf_undraw && cr) {
GdkRectangle rc;
				rc.x = x * m_->lf_width;
				rc.y = (y +1) * m_->lf_height - m_->lf_undraw;
				rc.width = m_->lf_width;
				rc.height = m_->lf_undraw;
				gdk_cairo_set_source_color (cr, bgcl);
				gdk_cairo_rectangle (cr, &rc);
				cairo_fill (cr);
			}
		}
	}
	if (layout)
		g_object_unref (layout);
	if (cr)
		cairo_destroy (cr);
#ifdef DRAW_DIRTY_THREAD
	return G_SOURCE_CONTINUE;
#else
	return G_SOURCE_REMOVE;
#endif
}

#if !defined(READ_SHELL_THREAD) || !defined(DRAW_DIRTY_THREAD)
/* NOTE:
	(*1) Primarily should wait output-pipe of shell, should not
	     block UI-thread (idle-callback of GTK+) but select()
	     cannot work to file descriptor made by posix_openpt().
	     So that unavoidably poll by usleep() and non-blocking
	     read(). (this problem should be happened only developping
	     version before SSH will be implemented.)
 */
#define GTK_IDLE_INTERVAL 16 // (*1)
static gboolean view_on_idle (gpointer data)
{
	View *m_;

	if (NULL == (m_ = (View *)g_object_get_data (G_OBJECT(data), "state")))
		return G_SOURCE_REMOVE;
gboolean retval;
	retval = G_SOURCE_REMOVE;
# ifndef READ_SHELL_THREAD
	if (TASK_READ_SHELL & m_->idle_task) {
ssize_t nread;
		while (! ((nread = cmdg_read_to_view (m_->term)) < -1 || -1 == nread && EWOULDBLOCK == errno || 0 == nread))
			if (-1 == nread && EIO == errno) {
				view_on_shell_disconnect (m_);
				m_->idle_task = 0;
				break;
			}
	}
# endif
# ifndef DRAW_DIRTY_THREAD
	if (TASK_DRAW_DIRTY & m_->idle_task) {
		if (G_SOURCE_REMOVE == view_draw_dirty (data))
			m_->idle_task &= ~TASK_DRAW_DIRTY;
		else
			retval = G_SOURCE_CONTINUE;
	}
# endif
# ifndef READ_SHELL_THREAD
	if (TASK_READ_SHELL & m_->idle_task) {
		usleep (GTK_IDLE_INTERVAL * 1000); // (*1)
		retval = G_SOURCE_CONTINUE;
	}
# endif
	if (G_SOURCE_REMOVE == retval)
		m_->idle_handler_id = 0;
	return retval;
}
#endif

void view_set_dirty (View *m_, u16 x, u16 y, u16 n_cols, u16 n_rows)
{
u16 left, top;
	left = x;
range_lock(left,top, n_cols, n_rows)
	for (top = y; y < top + n_rows; ++y)
		for (x = left; x < left + n_cols; ++x) {
int offset;
			offset = y * ALIGN32(m_->ws_col) + x;
			m_->DIRTY21s[offset >> 5] |= 1 << (31 & offset);
		}
range_unlock(left,top, n_cols, n_rows)
gtkmain_queue_draw_dirty()
}

void view_putc (View *m_, u16 x, u16 y, int c)
{
	if (! (x < m_->ws_col && y < m_->ws_row))
		return;
	c = (' ' == c) ? 0x00 : c;
lock(x,y)
BPP24 *g;
u32 *u;
	g = &m_->BPP24s[y * m_->ws_col + x];
	u = &m_->UTF21s[y * m_->ws_col + x];
	if (! ((u8)c == *u && 0 == g->i_node)) {
		*u = (u8)c;
		g->i_node = 0;
int offset;
		offset = y * ALIGN32(m_->ws_col) + x;
		m_->DIRTY21s[offset >> 5] |= 1 << (31 & offset);
	}
unlock(x,y)
gtkmain_queue_draw_dirty()
}

u16 image_get_cols (View *m_, u16 i_node)
{
	return ALIGN(m_->images[i_node].get_width (m_->images[i_node].this_), m_->lf_width)/m_->lf_width;
}
u8 image_get_rows (View *m_, u16 i_node)
{
u16 height_aligned;
	height_aligned = ALIGN(m_->images[i_node].get_height (m_->images[i_node].this_), m_->lf_height);
	return (u8)(height_aligned / m_->lf_height);
}
void view_putg (View *m_, u16 x, u16 y, u16 i_node, u16 i_matrix, u16 n_cols, u16 n_rows)
{
	if (! (x < m_->ws_col && y < m_->ws_row))
		return;
u16 img_col, img_row;
	img_col = image_get_cols (m_, i_node);
	img_row = image_get_rows (m_, i_node);
	if (0 == n_cols || img_col < n_cols)
		n_cols = img_col;
	if (0 == n_rows || img_row < n_rows)
		n_rows = img_row;
u16 left, top, right, bottom;
	left = x;
	top = y;
	right = (x + n_cols < m_->ws_col) ? x + n_cols : m_->ws_col;
	bottom = (y + n_rows < m_->ws_row) ? y + n_rows : m_->ws_row;
range_lock(left,top, right - left,bottom - top)
	for (y = top; y < bottom; ++y)
		for (x = left; x < right; ++x) {
			m_->BPP24s[y * m_->ws_col + x].i_node = i_node;
			m_->BPP24s[y * m_->ws_col + x].i_matrix = i_matrix + (y - top) * img_col + (x - left);
int offset;
			offset = y * ALIGN32(m_->ws_col) + x;
			m_->DIRTY21s[offset >> 5] |= 1 << (31 & offset);
		}
range_unlock(left,top, right - left,bottom - top)
gtkmain_queue_draw_dirty()
}

unsigned view_attr (View *m_, u16 x, u16 y, unsigned mask, unsigned set)
{
	if (! (x < m_->ws_col && y < m_->ws_row))
		return 0;
u8 *attr, upd;
	attr = &m_->ATTR8s[y * m_->ws_col + x];
	upd = ATTR_DEC(*attr);
	upd &= ~(u16)mask;
	upd |= (u16)mask & (u16)set;
unsigned old;
	if (upd == (old = ATTR_DEC(*attr)))
		return old;
lock(x,y)
u32 *u;
	if (~ATTR_FG_MASK & (ATTR_ENC(upd) ^ *attr) || *(u = &m_->UTF21s[y * m_->ws_col + x])) {
int offset;
		offset = y * ALIGN32(m_->ws_col) + x;
		m_->DIRTY21s[offset >> 5] |= 1 << (31 & offset);
	}
	*attr = ATTR_ENC(upd);
unlock(x,y)
gtkmain_queue_draw_dirty()
	return old;
}

void view_rolldown (View *m_)
{
BPP24 *g;
	g = m_->BPP24s;
u8 *a;
	a = m_->ATTR8s;
u32 *u, *p, d;
	u = m_->UTF21s;
	p = m_->DIRTY21s;
	d = *p;
range_lock(0,0, m_->ws_col,m_->ws_row -1)
u16 y, x;
	for (y = 0; y < m_->ws_row -1; ++y)
		for (x = 0; x < m_->ws_col; ++x, ++g, ++u, ++a) {
			if (0 < g->i_node || 0 < g[m_->ws_col].i_node || !(*u == u[m_->ws_col]) || ~ATTR_FG_MASK & (*a ^ a[m_->ws_col]) || *u)
				d |= 1 << x % 32;
			if (31 == x % 32 || m_->ws_col -1 == x) {
				*p++ = d;
				d = *p;
			}
		}
	memmove (m_->UTF21s, m_->UTF21s +m_->ws_col, (m_->ws_row -1) * m_->ws_col * sizeof(u32));
	memmove (m_->ATTR8s, m_->ATTR8s +m_->ws_col, (m_->ws_row -1) * m_->ws_col * sizeof(u8));
	memmove (m_->BPP24s, m_->BPP24s +m_->ws_col, (m_->ws_row -1) * m_->ws_col * sizeof(BPP24));
range_unlock(0,0, m_->ws_col,m_->ws_row -1)
//u32 *u;
int n;
range_lock(0,m_->ws_row -1, m_->ws_col,1)
	for (x = 0; x < m_->ws_col; ++x, ++g, ++u, ++a) {
		if (0 < g->i_node || ~ATTR_FG_MASK & *a || *u)
			d |= 1 << x % 32;
		if (31 == x % 32 || m_->ws_col -1 == x) {
			*p++ = d;
			d = *p;
		}
	}
	memset (m_->UTF21s + (m_->ws_row -1) * m_->ws_col, 0, m_->ws_col * sizeof(u32));
	memset (m_->ATTR8s + (m_->ws_row -1) * m_->ws_col, 0, m_->ws_col * sizeof(m_->ATTR8s[0]));
	memset (m_->BPP24s + (m_->ws_row -1) * m_->ws_col, 0, m_->ws_col * sizeof(BPP24));
range_unlock(0,m_->ws_row -1, m_->ws_col,1)
gtkmain_queue_draw_dirty()
}

enum {
	PCG_ERASE = 1,
	ALL_RESIZE = 2,
};
static bool view_con_resize (View *m_, u16 ws_col, u16 ws_row)
{
	if (m_->xy_max < ws_row * ws_col) {
size_t xy_max;
		xy_max = (0 < m_->xy_max) ? m_->xy_max : XY_MAX_DEFAULT;
		while (xy_max < ws_col * ws_row)
			xy_max *= 2;
size_t cb;
		cb = sizeof(m_->BPP24s[0]) * xy_max;
		if (NULL == (m_->BPP24s = (BPP24 *)(m_->BPP24s ? realloc (m_->BPP24s, cb) : malloc (cb))))
			return false;
		cb = sizeof(u32) * xy_max;
		if (NULL == (m_->UTF21s = (u32 *)(m_->UTF21s ? realloc (m_->UTF21s, cb) : malloc (cb))))
			return false;
		cb = sizeof(u8) * xy_max;
		if (NULL == (m_->ATTR8s = (u8 *)(m_->ATTR8s ? realloc (m_->ATTR8s, cb) : malloc (cb))))
			return false;
		m_->xy_max = xy_max;
	}
	if (m_->x32y_max < ALIGN32(ws_col)/32 * ws_row) {
size_t x32y_max;
		x32y_max = (0 < m_->x32y_max) ? m_->x32y_max : X32Y_MAX_DEFAULT;
		while (x32y_max < ALIGN32(ws_col)/32 * ws_row)
			x32y_max *= 2;
size_t cb;
		cb = sizeof(u32) * x32y_max;
		if (NULL == (m_->DIRTY21s = (u32 *)(m_->DIRTY21s ? realloc (m_->DIRTY21s, cb) : malloc (cb))))
			return false;
		m_->x32y_max = x32y_max;
	}
	m_->ws_col = ws_col;
	m_->ws_row = ws_row;
	return true;
}
static void view_con_erase (View *m_)
{
size_t xy;
	xy = m_->ws_row * m_->ws_col;
	memset (m_->BPP24s, 0, sizeof(m_->BPP24s[0]) * xy);
	memset (m_->UTF21s, 0, sizeof(m_->UTF21s[0]) * xy);
	memset (m_->ATTR8s, 0, sizeof(m_->ATTR8s[0]) * xy);
size_t x32y;
	x32y = ALIGN32(m_->ws_col)/32 * m_->ws_row;
	memset (m_->DIRTY21s, 0, sizeof(m_->DIRTY21s[0]) * x32y);
}
static bool view_font_change (View *m_, const char *font_name, u8 pango_undraw)
{
PangoFontDescription *desc;
	if (NULL == (desc = pango_font_description_from_string (font_name))) {
		errno = EINVAL; // invalid font name.
		return false;
	}

PangoFontMetrics *mtx;
	mtx = NULL;
bool succeeded;
	succeeded = false;
	do {
PangoContext *ctx;
PangoLanguage *lang;
		if (NULL == (ctx = gtk_widget_get_pango_context (GTK_WIDGET(m_->view))))
			break;
		if (NULL == (lang = pango_context_get_language (ctx)))
			break;
		if (NULL == (mtx = pango_context_get_metrics (ctx, desc, lang)))
			break;
		succeeded = true;
	} while (0);

	if (succeeded) {
		gtk_widget_modify_font (GTK_WIDGET(m_->view), desc);
		gtk_widget_modify_bg (GTK_WIDGET(m_->view), GTK_STATE_NORMAL, BLACK); // bg of View
int px;
		m_->lf_width = (u8)(px = PANGO_PIXELS(pango_font_metrics_get_approximate_digit_width (mtx)));
		m_->lf_height = (u8)(px = PANGO_PIXELS(pango_font_metrics_get_ascent (mtx)));
		m_->lf_height += (u8)(px = PANGO_PIXELS(pango_font_metrics_get_descent (mtx)));
		m_->lf_undraw = pango_undraw;
		m_->font_is_changed = true;
	}
	if (mtx)
		pango_font_metrics_unref (mtx);
	pango_font_description_free (desc);


	if (!succeeded) {
		errno = 0; // font metrics cannot retrieved. (pango error)
		return false;
	}
	return true;
}

static void view_grid_change (View *m_)
{
GdkGeometry hints;
		hints.base_width = 2 * EDGE_NOTEBOOK;
		hints.base_height = HEIGHT_MENU_BAR +HEIGHT_NOTE_TAG +EDGE_NOTEBOOK +HEIGHT_STATUS_BAR;
		hints.width_inc = m_->lf_width;
		hints.height_inc = m_->lf_height;
		hints.min_width = hints.base_width + hints.width_inc * m_->ws_col_min;
		hints.min_height = hints.base_height + hints.height_inc * m_->ws_row_min;
		gtk_window_set_geometry_hints (m_->appframe, GTK_WIDGET(m_->appframe), &hints, GDK_HINT_RESIZE_INC | GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);
}

///////////////////////////////////////////////////////////////////////////////
// event

static void menu_on_scroll (gpointer data, guint action, GtkWidget *widget)
{
View *m_;
	m_ = (View *)data;
	m_->mode |= SCROLL;
char *cstr;
	cstr = (char *)alloca (9 + strlen (m_->title) +1);
	strcpy (cstr +9, m_->title);
	memcpy (cstr, "<scroll> ", 9);
	gtk_window_set_title (m_->appframe, cstr);
}

// GDK_KEY_PRESS(8) GDK_KEY_RELEASE(9)
static gint frame_on_key_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
View *m_;
	m_ = (View *)data;

bool pressed;
	pressed = (GDK_KEY_PRESS == event->type) ? true : false;
unsigned hi;
u8 lo;
	if (0xff == (hi = (/*0xff & */event->keyval >> 8))) {
		switch (lo = 0xff & event->keyval) {
		case 0x0d: case 0x1b:
			if (! (SCROLL & m_->mode))
				break;
			if (pressed) {
				m_->mode &= ~SCROLL;
				gtk_window_set_title (m_->appframe, m_->title);
			}
			return TRUE;
		case 0x52: case 0x54: case 0x55: case 0x56: {
			if (! (SCROLL & m_->mode))
				break;
int offset;
			offset = (0x52 == lo) ? -1 : (0x54 == lo) ? 1 : (0x55 == lo) ? -m_->ws_row : m_->ws_row;
			if (pressed)
				m_->itr_top = cmdg_reput (m_->term, m_->itr_top, offset);
			return TRUE;
		}
		case 0xe9: case 0xea: {
u8 mask;
			mask = (0xe9 == lo) ? LALT : RALT;
			m_->mode &= ~mask;
			if (pressed)
				m_->mode |= mask;
			return FALSE;
		}
		default:
			break;
		}
	}
	if (SCROLL & m_->mode)
		return FALSE;
	if (!(ITR_CUR == m_->itr_top) && 0 == ((LALT|RALT) & m_->mode) && pressed) {
u32 visible_top;
		visible_top = itr_seek (m_->term, ITR_CUR, -m_->ws_row +1);
		if (! (itr_find (m_->term, visible_top, m_->ws_row, m_->itr_top) < m_->ws_row))
			cmdg_reput (m_->term, visible_top, 0);
		m_->itr_top = ITR_CUR;
	}

	// Alt + <anykey> not bypass
	if (0x00 == hi && (LALT|RALT) & m_->mode) { // Alt -> <anykey>
		m_->mode &= ~(LALT|RALT);
		return FALSE;
	}
	if ((LALT|RALT) & m_->mode)
		return FALSE;

	if (true == m_->write_gtkkey (event->keyval, (GDK_KEY_PRESS == event->type) ? true : false, m_->write_gtkkey_param))
		return TRUE;

	return FALSE;
}

static void frame_on_size_allocate (GtkWidget *appframe, GtkAllocation *a)
{
	if (! (0 < HEIGHT_MENU_BAR))
		HEIGHT_MENU_BAR = MENU_BAR(appframe)->allocation.height;
	if (! (0 < EDGE_NOTEBOOK))
		EDGE_NOTEBOOK = (NOTEBOOK(appframe)->allocation.width - DRAWING_AREA(appframe)->allocation.width) / 2;
GtkWidget *view;
	view = DRAWING_AREA(appframe);
	if (! (0 < HEIGHT_NOTE_TAG))
		HEIGHT_NOTE_TAG = NOTEBOOK(appframe)->allocation.height - (view->allocation.height +EDGE_NOTEBOOK);
View *m_;
	m_ = (View *)g_object_get_data (G_OBJECT(view), "state");
u16 ws_col, ws_row;
	ws_col = view->allocation.width / m_->lf_width;
	ws_row = view->allocation.height / m_->lf_height;
	if (! (ws_col == m_->ws_col && ws_row == m_->ws_row)) {
		if (!view_con_resize (m_, ws_col, ws_row)) {
fprintf (stderr, VTRR "not enough memory" VTO "." "\n");
			exit (1); // not enough memory.
		}
		view_con_erase (m_);
		m_->itr_top = cmdg_reput (m_->term, ITR_CUR, 0);
	}
	else if (m_->font_is_changed) {
		view_con_erase (m_);
		m_->itr_top = cmdg_reput (m_->term, ITR_CUR, 0);
		m_->font_is_changed = false;
	}
}

///////////////////////////////////////////////////////////////////////////////
// UI Thread

static bool view_init (View *m_, Cmdg *term)
{
#ifdef READ_SHELL_THREAD
#else
	m_->term = term;
#endif

	if (NULL == (m_->appframe = GTK_WINDOW(gtk_window_new (GTK_WINDOW_TOPLEVEL))))
		return false;
	gtk_widget_modify_bg (GTK_WIDGET(m_->appframe), GTK_STATE_NORMAL, MENU_BG); // outside of GtkNotebook
	gtk_window_set_policy (m_->appframe, TRUE, TRUE, TRUE);
	g_signal_connect (m_->appframe, "destroy", G_CALLBACK(frame_on_destroy), m_);
	g_signal_connect (m_->appframe, "key_press_event", G_CALLBACK(frame_on_key_event), m_);
	g_signal_connect (m_->appframe, "key_release_event", G_CALLBACK(frame_on_key_event), m_);
	g_signal_connect (m_->appframe, "size_allocate", G_CALLBACK(frame_on_size_allocate), m_);

	// GtkBox(V) (1/3) GtkMenuBar
# if 1
GtkItemFactoryEntry menu_items[] = {
		  { "/_File", NULL, NULL, 0, "<Branch>" }
		, { "/File/Quit", "<CTRL>Q", menu_on_quit, 0, "<StockItem>", GTK_STOCK_QUIT }
		, { "/_Edit", NULL, NULL, 0, "<Branch>" }
		, { "/Edit/Scro_ll", NULL, menu_on_scroll, 0, "<Item>", m_ }
	};
GtkAccelGroup *accel_group;
	accel_group = gtk_accel_group_new ();
GtkItemFactory *factory, *subfactory;
	factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<Main>", accel_group);
	gtk_item_factory_create_items (factory, arraycountof(menu_items), menu_items, (void *)m_);
GtkWidget *menu_bar;
	menu_bar = gtk_item_factory_get_widget (factory, "<Main>");
# endif

	// GtkWindow -> GtkBox(V)
GtkWidget *vbox;
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(m_->appframe), vbox);
	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(menu_bar), FALSE, FALSE, 0);
	gtk_widget_show (GTK_WIDGET(menu_bar));

	// GtkBox(V) (2/3) GtkNotebook
GtkNotebook *notebook;
	notebook = GTK_NOTEBOOK(gtk_notebook_new ());
	gtk_widget_modify_bg (GTK_WIDGET(notebook), GTK_STATE_NORMAL, MONO(192)); // bg of tablabel
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
GtkDrawingArea *view;
	view = GTK_DRAWING_AREA(gtk_drawing_area_new ());
	g_signal_connect (GTK_WIDGET(view), "expose_event", G_CALLBACK(view_on_expose), NULL);
	g_signal_connect (GTK_WIDGET(view), "configure_event", G_CALLBACK(view_on_configure), NULL);
	gtk_widget_show (GTK_WIDGET(view));

	gtk_notebook_append_page (notebook, GTK_WIDGET(view), GTK_WIDGET(tab));
	gtk_widget_show (GTK_WIDGET(notebook));
	// GtkBox(V) (3/3) GtkDrawingArea
GtkDrawingArea *da;
	da = GTK_DRAWING_AREA(gtk_drawing_area_new ());
	gtk_widget_set_size_request (GTK_WIDGET(da), 1, HEIGHT_STATUS_BAR);
	gtk_box_pack_end (GTK_BOX(vbox), GTK_WIDGET(da), FALSE, FALSE, 0);
	g_signal_connect (GTK_WIDGET(da), "expose_event", G_CALLBACK(statusbar_on_expose), NULL);
	gtk_widget_show (GTK_WIDGET(da));

	gtk_widget_show (vbox);

	m_->view = view;
	g_object_set_data (G_OBJECT(view), "state", (gpointer)m_);
#ifdef DRAW_DIRTY_THREAD
	gdk_threads_add_timeout (FRAME_MS, view_draw_dirty, (gpointer)view);
#endif
#ifndef READ_SHELL_THREAD
gtkmain_queue(TASK_READ_SHELL)
#endif

	return true;
}

FILE *stderr_old = NULL;
static void stderr_old_log_setup ()
{
	if (NULL == stderr_old) {
		stderr_old = stderr;
		do {
int errfd;
			if (-1 == (errfd = dup (STDERR_FILENO)))
				break;
FILE *errfp;
			if (NULL == (errfp = fdopen (errfd, "a")))
				break;
			stderr_old = errfp;
		} while (0);
		if (stderr == stderr_old)
fprintf (stderr_old, VTRR "WARN" VTO " STDERR cannot duplicated, shell error(s) are outputed to window." "\n");
	}
}

int main (int argc, char *argv[])
{
	stderr_old_log_setup ();
Config conf;
Cmdg *term;
View *m_;
bool unexpected;
char **cmdv;
	cmdv = (char **)alloca (argc * sizeof(char *));
ASSERTE(cmdv)
int cmdc;
	cmdc = 1;
const char *hostname, *ppk_path;
	hostname = NULL;
	ppk_path = "./.ppk";
u16 port;
	port = SSH2_PORT_DEFAULT;
bool is_help;
	is_help = false;
int n;
	for (n = 1; n < argc; ++n) {
const char *token;
		token = argv[n];
const char *param;
		if (! ('-' == token[0]))
			;
		else if ('p' == token[1]) {
			port = 0;
			do {
				if (! (isdigit ((param = token +2)[0]) || n +1 < argc && isdigit ((param = argv[n +1])[0])))
					break;
				if (0xffff < atoi (param))
					break;
				port = (u16)atoi (param);
				if (argv[n +1] == param)
					++n;
			} while (0);
			if (0 < port)
				continue;
			fprintf (stderr, "usage: -p[ ]<port(1-65535)>" "\n");
			return -1;
		}
		else if ('k' == token[1]) {
			ppk_path = NULL;
			do {
				if ('\0' == (param = token +2)[0] && (argc < n +2 || '\0' == (param = argv[n +1])[0]))
					break;
				ppk_path = param;
				if (argv[n +1] == param)
					++n;
			} while (0);
			if (ppk_path)
				continue;
			fprintf (stderr, "usage: -k[ ]<ppk-path>" "\n");
			return -1;
		}
		else if (0 == strcmp ("-help", token +1))
			is_help = true;
		cmdv[cmdc++] = argv[n];
	}
	if (cmdc < 2 || is_help) {
fprintf (stderr, "usage: [-k[ ]<ppk-path>] [-p[ ]<port>] <hostname>" "\n");
		return -1;
	}
	hostname = cmdv[1];
	++cmdv, --cmdc;
	cmdv[0] = argv[0];

	term = NULL;
	m_ = NULL;
	unexpected = true;
	do {
		memset (&conf, 0, sizeof(conf));
		if (false == config_get (&conf))
			break;
		if (NULL == (m_ = view_ctor (conf.bgr24, conf.ws_col_min, conf.ws_row_min)))
			break;
		if (NULL == (term = cmdg_ctor (m_, conf.ws_col, conf.n_max)))
			break;
		gtk_init (&cmdc, &cmdv);
		if (false == view_init (m_, term))
			break;
		if (false == view_font_change (m_, conf.font_name, conf.pango_undraw)) {
			if (EINVAL == errno)
fprintf (stderr, "'%s' invalid font name." "\n", conf.font_name);
			break;
		}
		gtk_widget_set_size_request (GTK_WIDGET(m_->view), conf.ws_col * m_->lf_width, conf.ws_row * m_->lf_height);
		g_signal_connect (GTK_WIDGET(m_->appframe), "configure_event", G_CALLBACK(frame_on_configure), NULL);
		gtk_widget_show_all (GTK_WIDGET(m_->appframe));
		view_grid_change (m_);
ASSERTE(hostname && !('\0' == hostname[0]))
		if (false == cmdg_connect (term, hostname, port, ppk_path))
			break;
		unexpected = false;
	} while (0);
	if (unexpected) {
		if (term)
			cmdg_dtor (term);
		if (m_)
			view_dtor (m_);
		return -1;
	}

	gtk_main ();
	cmdg_dtor (term);
#if !defined(READ_SHELL_THREAD) || !defined(DRAW_DIRTY_THREAD)
	if (m_->idle_handler_id)
		g_source_remove (m_->idle_handler_id);
#endif
	view_dtor (m_);

	return 0;
}
