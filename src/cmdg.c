//#include <features.h>
//#include <endian.h>
#include <stdint.h>

#include "gdk2_keyin.h"
#include "pseudo_vt100.h"
#include "image_backlog.h"
#include "conf.h"

#include <gtk/gtk.h>
#include "gtk2_windowing.h"
#include "on_ptout.h"
#include "gtk2_font.h"
#ifdef FALSE
# undef FALSE // avoid bad effect from glib version
#endif
#ifdef TRUE
# undef TRUE // avoid bad effect from glib version
#endif

#include <stdbool.h>
#include <stdlib.h> // exit
#include <memory.h>
#include <errno.h> // EINVAL
#include <getopt.h>
typedef uint32_t u32;
typedef  uint8_t u8, u1;
typedef uint16_t u16, u9, u10, u12;
typedef uint64_t u64;

#define RGB24(r,g,b) (0xff0000 & (r) << 16 \
                    | 0x00ff00 & (g) << 8 \
                    | 0x0000ff & (b))
#define MONO(w) RGB24(w,w,w)

#define FALSE 0
#define TRUE  1
///////////////////////////////////////////////////////////////
// configuration

/* settings when FONTNAME configuration not found */
#define DEFAULT_FONT_NAME "Ubuntu Mono 12"

/* settings when WIDTH x HEIGHT configuration not found */
#define FAKE_LF_WIDTH  8
#define FAKE_LF_HEIGHT 16
#define DEFAULT_WS_COL 80
#define DEFAULT_WS_ROW 24
#define DEFAULT_WS_X 0
#define DEFAULT_WS_Y 0

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

static void _on_gui_resized (u12 ws_width, u12 ws_height)
{
u10 ws_col
	= ws_width / gtk2_font->width();
u9 ws_row
	= ws_height / gtk2_font->height();

u9 ws_row_allowed
	= on_ptout->set_frontend_size (ws_col, ws_row);
	pseudo_vt100->modifyws (ws_col, ws_row_allowed);
}

///////////////////////////////////////////////////////////////
// entry point

int main (int argc, char *argv[])
{
	// GTK+ initialize
int gtkc; char *gtkv_[1], **gtkv = gtkv_;
	gtkc = 0, gtkv[gtkc++] = argv[0]; // PENDING: (*1)
	gtk_init (&gtkc, &gtkv);

	// configuration
unsigned u, err_uint;

	u = conf->read_uint (CONF_WS_WIDTH, err_uint = 0);
u12 ws_width
	= (0 < u && u < (1 << 12)) ? (u12)u
	                           : DEFAULT_WS_COL * FAKE_LF_WIDTH;

	u = conf->read_uint (CONF_WS_HEIGHT, err_uint = 0);
u12 ws_height
	= (0 < u && u < (1 << 12)) ? (u12)u
	                           : DEFAULT_WS_ROW * FAKE_LF_HEIGHT;

	u = conf->read_uint (CONF_WS_X, err_uint = 0);
u12 ws_x
	= (0 <= u && u < (1 << 12)) ? (u12)u
	                            : DEFAULT_WS_X;

	u = conf->read_uint (CONF_WS_Y, err_uint = 0);
u12 ws_y
	= (0 <= u && u < (1 << 12)) ? (u12)u
	                            : DEFAULT_WS_Y;

	// GTK window
bool ok
	= gtk2_windowing->create_gui (gdk2_keyin->on_key,
	                              _on_gui_resized,
	                              ws_x, ws_y);
	if (!ok)
		return -1;

GtkDrawingArea *da
	= gtk2_windowing->show_gui (ws_width, ws_height,
	                            MONO(0)
);

	// font
const char *err_cstr;
const char *font_name
	= conf->read_cstr (CONF_FONT_NAME, err_cstr = DEFAULT_FONT_NAME);
	gtk2_font->load_pango (font_name, GTK_WIDGET(da));
u10 ws_col, ws_row;
	ws_col = ws_width / gtk2_font->width();
	ws_row = ws_height / gtk2_font->height();
	image_backlog->set_grid (gtk2_font->width(),
	                         gtk2_font->height());

bool acceptLFasCRLF;
	on_ptout->ctor (gtk2_windowing->on_backend_changed,
	                acceptLFasCRLF = true);
	on_ptout->set_frontend_size (ws_col, ws_row);

bool allowLFtoCRLF;
	pseudo_vt100->connect (ws_col, ws_row,
	                       allowLFtoCRLF = false);
	do {
		gtk_main ();
	} while (0);

	conf->dtor ();
	return 0;
}

///////////////////////////////////////////////////////////////
// footnote

/* PENDING: (*1)
   omitting GTK+ option supoprt.
 */
