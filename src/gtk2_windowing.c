//#include <features.h>
//#include <endian.h>
#include <stdint.h>

#include "pseudo_vt100.h"

#include <gtk/gtk.h>
#include "gtk2_windowing.h"
#include "on_ptout.h"
#include "gtk2_font.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <string.h>
#include <errno.h>
typedef uint32_t u32, u24;
typedef  uint8_t u8, u1;
typedef uint16_t u16, u9, u10, u12;
typedef uint64_t u64;
typedef  int32_t s32;

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) < (b) ? (b) : (a))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define arraycountof(x) (sizeof(x)/(sizeof((x)[0])))

#define swap(a, b, type__) do { type__ t__; t__ = a, a = b, b = t__; } while (0)
#define ALIGN32(x) (-32 & (x) +32 -1)
#define PADDING32(x) (ALIGN32(x) - (x))

#define RGB24(r,g,b) (0xff0000 & (r) << 16 | 0xff00 & (g) << 8 | 0xff & (b))
#define MONO(w) RGB24(w,w,w)
///////////////////////////////////////////////////////////////
// configuration

#define IDLE_INTERVAL_MS 16 // [ms] (near 60Hz)
#define MENU_BG RGB24(60,59,55)
#define HEIGHT_STATUS_BAR 21

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

#define unexpected assert
#define caller_bug assert

///////////////////////////////////////////////////////////////
// helper

static void _gdk_color_from_rgb24 (GdkColor *dst, u24 rgb24)
{
	dst->pixel = rgb24;
	dst->red   = 0x101 * (0xff & rgb24 >> 16);
	dst->green = 0x101 * (0xff & rgb24 >>  8);
	dst->blue  = 0x101 * (0xff & rgb24      );
}

static GtkWindow *_gtk_widget_get_root_window (GtkWidget *widget)
{
caller_bug(widget)

GtkWidget *existed = NULL, *new_checked = widget;
	while (new_checked) {
		existed = new_checked;

		if (GTK_IS_MENU(existed)) {
		//	new_checked = GTK_MENU(existed)->parent_menu_item;
GtkMenu *child_menu = GTK_MENU(existed);
			new_checked = gtk_menu_get_attach_widget (child_menu);
		}
		else
			new_checked = gtk_widget_get_parent (existed);
	}

GtkWindow *root_window
	= GTK_WINDOW(existed);
	return root_window;
}

///////////////////////////////////////////////////////////////
// singlton memory

typedef struct {
	bool (*on_key_gdk2) (u16 keycode, u1 is_keydown);
	GtkWindow *app;
	GtkVBox *vbox;
	GtkDrawingArea *da;
	void (*on_gui_resized) (u12 da_width, u12 da_height);
	u16 da_width;
	u16 da_height;
	u8 menubar_height;
	u8 da_sbar_height;
	u8 is_gdk_exposed_app       :1;
	u8 is_gdk_exposed_vbox      :1;
//	u8 is_gdk_exposed_menubar   :1;
	u8 is_gdk_exposed_statusbar :1;
	u8 is_da_resizing_started   :1;
	u8 padding                  :4;
	bool is_waiting_idle;
	bool app_destroyed;
#ifdef __i386__
//	u8 align4_padding[];
#else //def __x86_64__
	u8 align8_padding[7];
#endif
} gtk2_windowing_s;

static gtk2_windowing_s s_singlton = {0};

///////////////////////////////////////////////////////////////
// GTK+ event callback (GTK+ thread)

static void _calc_da_size_supposed_from_vbox (GtkVBox *vbox,
                                              int *true_da_width,
                                              int *true_da_height)
{
gtk2_windowing_s *m_ = &s_singlton;

int vbox_width, vbox_height; // NOTE: (*1)
	vbox_width = gdk_window_get_width (GTK_WIDGET(vbox)->window);
	vbox_height = gdk_window_get_height (GTK_WIDGET(vbox)->window);
int menu_and_sbar_height
	= m_->menubar_height +m_->da_sbar_height;

	if (true_da_width)
		*true_da_width = vbox_width;
	if (true_da_height)
		*true_da_height = vbox_height -menu_and_sbar_height;
}

static gboolean _on_idle (gpointer da)
{
gtk2_windowing_s *m_ = &s_singlton;

	if (m_->is_gdk_exposed_app
	 || m_->is_gdk_exposed_vbox
//	 || m_->is_gdk_exposed_menubar
	 || m_->is_gdk_exposed_statusbar) {
int true_da_width, true_da_height;
	_calc_da_size_supposed_from_vbox (m_->vbox, &true_da_width, &true_da_height);
u1 not_fitted_vbox = !(true_da_width == m_->da_width
                    && true_da_height == m_->da_height)
                    ? 1 : 0;
		if (not_fitted_vbox) {
			if (!m_->is_da_resizing_started) {
				gtk_widget_set_size_request (GTK_WIDGET(m_->da), true_da_width, true_da_height);
				m_->is_da_resizing_started = 1;
			}
		}
	}
	m_->is_gdk_exposed_app       =
	m_->is_gdk_exposed_vbox      =
//	m_->is_gdk_exposed_menubar   =
	m_->is_gdk_exposed_statusbar = 0;

//_lock()
char buf[1024]; int got;
	while (1) {
		got = pseudo_vt100->read_output (buf, usizeof(buf));
		if (! (0 < got))
			break;
		on_ptout->write_output (buf, got);
	}
bool end_of_ptio_reading
	= (-1 == got && EIO == errno) ? true : false;
bool needed_to_wait
	= (-1 == got && EWOULDBLOCK == errno) ? true : false;
unexpected(end_of_ptio_reading || needed_to_wait)

	if (end_of_ptio_reading && !m_->app_destroyed) {
		m_->app_destroyed = true;
		gtk_widget_destroy (GTK_WIDGET(m_->app));
	}

	m_->is_waiting_idle = false;
//_unlock()

	return G_SOURCE_REMOVE;
}

static gboolean _on_timer (gpointer da)
{
gtk2_windowing_s *m_ = &s_singlton;

	if (!m_->is_waiting_idle) {
//_lock()
bool is_success = false;
	if (0 < g_idle_add (_on_idle, (gpointer)da))
		is_success = true;
unexpected(is_success)
	if (is_success)
		m_->is_waiting_idle = true;
//_unlock()
	}
	return G_SOURCE_CONTINUE;
}

static GtkMenuBar *_get_menubar (GtkWindow *app)
{
	// GtkWindow -> (children) -> GtkVBox
GList *root_widgets
	= gtk_container_get_children (GTK_CONTAINER(app));
unexpected(GTK_IS_VBOX(root_widgets->data))

	// GtkVBox -> (children) -> GtkMenuBar
GtkVBox *vbox
	= GTK_VBOX(root_widgets->data);
GList *vbox_widgets
	= gtk_container_get_children (GTK_CONTAINER(vbox));
unexpected(GTK_IS_MENU_BAR(vbox_widgets->data))

	return GTK_MENU_BAR(vbox_widgets->data);
}

// GDK_CONFIGURE(13)
static void _on_configure (GtkWidget *app, GdkEventConfigure *event, gpointer data)
{
gint x, y;
	gtk_window_get_position (GTK_WINDOW(app),
	                         &x, &y); // ATTENTION: (*2)
gtk2_windowing_s *m_ = &s_singlton;

GtkMenuBar *menubar
	= _get_menubar (GTK_WINDOW(app));
bool just_after_show_all
	= (0 == m_->menubar_height) ? true : false;
	if (just_after_show_all)
		m_->menubar_height 
		= gdk_window_get_height (GTK_WIDGET(menubar)->window);
}

// GDK_DESTROY(1)
static void _on_destroy (GtkWidget *app, gpointer data)
{
	gtk_main_quit ();
}

// GDK_EXPOSE(2)
/* return TRUE  (>= GTK+3.0: = GDK_EVENT_STOP)
   return FALSE (>= GTK+3.0: = GDK_EVENT_PROPAGATE) */
static gint _on_expose_app (GtkWidget *app,
                            GdkEventExpose *event,
                            gpointer data)
{
gtk2_windowing_s *m_ = &s_singlton;

	m_->is_gdk_exposed_app = 1;
	return FALSE;
}

static gint _on_expose_vbox (GtkWidget *vbox,
                             GdkEventExpose *event,
                             gpointer data)
{
gtk2_windowing_s *m_ = &s_singlton;

	m_->is_gdk_exposed_vbox = 1;
	return FALSE;
}

static gint _on_expose_menubar (GtkWidget *menubar,
                                GdkEventExpose *event,
                                gpointer data)
{
gtk2_windowing_s *m_ = &s_singlton;

//	m_->is_gdk_exposed_menubar = 1;
	return FALSE;
}

static gint _on_expose_da (GtkWidget *da,
                           GdkEventExpose *event,
                           gpointer data)
{

gtk2_windowing_s *m_ = &s_singlton;

bool first_exposed_after_gtk_widget_show_all
	= (0 == m_->da_width && 0 == m_->da_height) ? true : false;
	if (first_exposed_after_gtk_widget_show_all) {
		m_->da_width = event->area.width;
		m_->da_height = event->area.height;
	}

int true_da_width, true_da_height;
	_calc_da_size_supposed_from_vbox (m_->vbox, &true_da_width, &true_da_height);
u1 not_fitted_vbox = !(true_da_width == m_->da_width
                    && true_da_height == m_->da_height)
                    ? 1 : 0;
u1 is_full_exposed = (0 == event->area.x
                   && 0 == event->area.y
                   && true_da_width == event->area.width
                   && true_da_height == event->area.height)
                   ? 1 : 0;
	if (not_fitted_vbox) {
		// 1st: resizing start -> GTK+
		if (!m_->is_da_resizing_started) {
			gtk_widget_set_size_request (GTK_WIDGET(m_->da), true_da_width, true_da_height);
			m_->is_da_resizing_started = 1;
			return TRUE; // GDK_EVENT_STOP
		}
		// 2nd: ignore exposing by system menu closed
u1 caused_by_system_menu_closing = !is_full_exposed;
		if (caused_by_system_menu_closing)
			return TRUE; // GDK_EVENT_STOP
		// 3rd: resizing completed
u1 da_resizing_finished = is_full_exposed;
		if (da_resizing_finished) {
			m_->is_da_resizing_started = 0;
			if (m_->on_gui_resized)
				m_->on_gui_resized (true_da_width, true_da_height);

unexpected(0 < true_da_width && true_da_width < 0x10000)
unexpected(0 < true_da_height && true_da_height < 0x10000)
			m_->da_width = true_da_width;
			m_->da_height = true_da_height;
			// fall through for drawing to GDK+(cairo)
		}
	}

GdkGC *fgcl_gc
	= da->style->text_gc[da->state];
	on_ptout->draw_to_gdk2 (event->area.x, event->area.y,
	                        event->area.width,
	                        event->area.height,
	                        event->window, fgcl_gc);

	return TRUE; // GDK_EVENT_STOP
}

static gint _on_expose_statusbar (GtkWidget *statusbar,
                                  GdkEventExpose *event,
                                  gpointer data)
{
gtk2_windowing_s *m_ = &s_singlton;

	m_->is_gdk_exposed_statusbar = 1;
	return FALSE; // CHECKED: TRUE also doesn't break event chain
}

// GDK_KEY_PRESS(8) GDK_KEY_RELEASE(9)
static gint _on_key (GtkWidget *app,
                     GdkEventKey *event,
                     gpointer data)
{
gtk2_windowing_s *m_ = &s_singlton;

	if (0xffff < event->keyval) {
unexpected(1 == 0)
		return FALSE;
	}
u16 keycode
	= (u16)event->keyval;
	switch (event->type) {
	case GDK_KEY_PRESS:
		if (m_->on_key_gdk2 && m_->on_key_gdk2 (keycode, 1))
			return TRUE;
		break;
	case GDK_KEY_RELEASE:
		if (m_->on_key_gdk2 && m_->on_key_gdk2 (keycode, 0))
			return TRUE;
		break;
	default:
unexpected(2 == 0)
		break;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////
// GTK event (UI frontend)

static void _on_quit_memnu (gpointer data, guint action,
                            GtkWidget *menu_item)
{
unexpected(menu_item)

GtkWindow *app
	= _gtk_widget_get_root_window (menu_item);
	gtk_widget_destroy (GTK_WIDGET(app)); // causes GDK_DESTROY(1)
}

///////////////////////////////////////////////////////////////
// interface


///////////////////////////////////////////////////////////////
// setup

static bool _create_gui_gtk2 (
  bool (*on_key_gdk2) (u16 keycode, u1 is_keydown)
, void (*on_gui_resized) (u12 da_width, u12 da_height)
, u12 ws_x, u12 ws_y
)
{
gtk2_windowing_s *m_ = &s_singlton;

	m_->on_key_gdk2 = on_key_gdk2;
GtkWidget *app;
	if (NULL == (app = gtk_window_new (GTK_WINDOW_TOPLEVEL)))
		return false;
	m_->app = GTK_WINDOW(app);
GdkColor menu_bgcl;
	_gdk_color_from_rgb24 (&menu_bgcl, MENU_BG);
	gtk_widget_modify_bg (app, GTK_STATE_NORMAL, &menu_bgcl);
	gtk_window_set_policy (GTK_WINDOW(app), TRUE, TRUE, TRUE);
	g_signal_connect (app, "destroy",
	                  G_CALLBACK(_on_destroy), NULL);
	g_signal_connect (app, "key_press_event",
	                  G_CALLBACK(_on_key), NULL);
	g_signal_connect (app, "key_release_event",
	                  G_CALLBACK(_on_key), NULL);
	g_signal_connect (app, "configure_event",
	                  G_CALLBACK(_on_configure), NULL);

	// GtkBox(V) (1/3) GtkMenuBar
# if 1
GtkItemFactoryEntry menu_items[] = { {
	/* .path .accelerator .callback .callback_action .item_type
	   .extra_data */
	"/_File", NULL, NULL, 0, "<Branch>"
}, {
	"/File/Quit", "<CTRL>Q", _on_quit_memnu, 0, "<StockItem>",
	GTK_STOCK_QUIT
}, {
	"/_Options", NULL, NULL, 0, "<Branch>"
} };
GtkAccelGroup *accel_group
	= gtk_accel_group_new ();
GtkItemFactory *factory
	= gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<Main>", accel_group);
	gtk_item_factory_create_items (factory, arraycountof(menu_items), menu_items, (void *)NULL);
GtkWidget *menubar
	= gtk_item_factory_get_widget (factory, "<Main>");
# endif

	// GtkWindow -> GtkBox(V)
GtkWidget *vbox
	= gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(app), vbox);
	gtk_box_pack_start (GTK_BOX(vbox), GTK_WIDGET(menubar), FALSE, FALSE, 0);
	gtk_widget_show (GTK_WIDGET(menubar));

	// GtkBox(V) (2/3) GtkDrawingArea
GtkWidget *da
	= gtk_drawing_area_new ();
	g_signal_connect (da       , "expose_event",
	                  G_CALLBACK(_on_expose_da)       , NULL);
	gtk_box_pack_start (GTK_BOX(vbox), da, FALSE, FALSE, 0);
	gtk_widget_show (da);

	// GtkBox(V) (3/3) GtkDrawingArea
GtkWidget *statusbar
	= gtk_drawing_area_new ();
	gtk_widget_set_size_request (statusbar, 1, HEIGHT_STATUS_BAR);
	gtk_box_pack_end (GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);
//	g_signal_connect (statusbar, "expose_event", G_CALLBACK(statusbar_on_expose), NULL);
	gtk_widget_show (statusbar);

	gtk_widget_show (vbox);
	gtk_window_move (GTK_WINDOW(app), ws_x, ws_y);

	g_signal_connect (app      , "expose_event",
	                  G_CALLBACK(_on_expose_app)      , NULL);
	g_signal_connect (vbox     , "expose_event",
	                  G_CALLBACK(_on_expose_vbox)     , NULL);
	g_signal_connect (menubar  , "expose_event",
	                  G_CALLBACK(_on_expose_menubar)  , NULL);
	g_signal_connect (statusbar, "expose_event",
	                  G_CALLBACK(_on_expose_statusbar), NULL);

	gdk_threads_add_timeout (IDLE_INTERVAL_MS, _on_timer, (gpointer)da);

	m_->vbox = GTK_VBOX(vbox);
	m_->da = GTK_DRAWING_AREA(da);
	m_->da_sbar_height = HEIGHT_STATUS_BAR;
	m_->on_gui_resized = on_gui_resized;
	return true;
}

///////////////////////////////////////////////////////////////
// interface (forwarder)

static GtkDrawingArea *_show_gui_gtk2 (u12 da_width, u16 da_height,
                                       u24 bgcl24)
{
gtk2_windowing_s *m_ = &s_singlton;

	gtk_widget_set_size_request (GTK_WIDGET(m_->da),
	                             da_width, da_height); // PENDING: (*3)
	gtk_widget_show_all (GTK_WIDGET(m_->app));

GdkColor bgcl;
	_gdk_color_from_rgb24 (&bgcl, bgcl24);
	gtk_widget_modify_bg (GTK_WIDGET(m_->da), GTK_STATE_NORMAL, &bgcl);

	return m_->da;
}

static void _on_backend_changed (u12 da_x, u12 da_y,
                                 u12 da_width, u12 da_height)
{
gtk2_windowing_s *m_ = &s_singlton;

GdkWindow *window
	= GTK_WIDGET(m_->da)->window;
u12 da_x_end
	= gdk_window_get_width (window);
u12 da_y_end
	= gdk_window_get_height (window);
	if (0 < da_width)
		da_x_end = min(da_x +da_width, da_x_end);
	if (0 < da_height)
		da_y_end = min(da_y +da_height, da_y_end);
GdkRectangle r = {
	.x = da_x, .y = da_y,
	.width = da_x_end -da_x, .height = da_y_end -da_y };
	gdk_window_invalidate_rect (window, &r, FALSE);
}

///////////////////////////////////////////////////////////////
// readability sugar

static const READABILITY_SUGAR_GTK2_WINDOWING _gtk2_windowing = {
	.create_gui = _create_gui_gtk2,
	.show_gui = _show_gui_gtk2,
	.on_backend_changed = _on_backend_changed,
};
const READABILITY_SUGAR_GTK2_WINDOWING *gtk2_windowing = &_gtk2_windowing;

///////////////////////////////////////////////////////////////
// footnote

/* NOTE: (*1)
   Only gtk_widget_get_window() + gdk_window_get_[width|height]()
   returns right size of GtkVBox.

   NG#1: gtk_widget_get_requisition()
   -> returns booted size that ignores all resizing that the
      user gives on GUI.
   NG#2: gtk_widget_get_size_request()
   -> always returns -1x-1 (unknown behavior)

   However, now non-client area (= title-bar and frame-edge
   excluded area) of app(GtkWindow) equals to size of GtkVBox,
   therefore it feels like getting same result even if functions
   are called to GtkWindow not to GtkVBox.

   ATTENTION: (*2)
   This calling gives statusbar(GtkDrawingArea) able to follow
   the resizing with frame-window(GtkWindow) automatically,
   with fitting the bottom.
   I don't know why it gets able to work.

   PENDING: (*3)
   gtk_widget_set_size_request() calling to GtkWindow at this
   timing must be given the hight of GtkMenuBar.
  (That's why we can't get GtkMenuBar's running height before
   GDK_EXPOSE event getting available.)
   This isn't supposed to be, so gtk_widget_set_size_request()
   is called to GtkDrawingArea not to GtkWindow.
 */
