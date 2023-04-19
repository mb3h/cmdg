#include <gtk/gtk.h>

#include <stdint.h>
#define VRAM_s void
#define idle_event_s void
#define key_event_s void
#define parser_event_s void
#define mediator_s void
#include "mediator.hpp"

#include "gtk2view.hpp"
typedef struct gtk2view gtk2view_s;
#include "TVRAM.hpp"
typedef struct TVRAM TVRAM_s;
#include "GVRAM.hpp"
typedef struct GVRAM GVRAM_s;
#include "ptmx.hpp"
#include "log.h"
#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <memory.h>
#include <errno.h> // EINVAL
#include <getopt.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

#include "config.hpp"
#include "appconst.h"

///////////////////////////////////////////////////////////////////////////////
// command line parameters

static void usage ()
{
echo ("usage: [[-p|--port=]<number(1-65535)>] [[-k|--ppk=]<ppk-path>] <hostname>" "\n");
}

typedef struct {
	const char *hostname;
	const char *ppk_path;
	int port;
} opts_s;

static bool _parse_options (int argc, char *const argv[], opts_s *dst)
{
static struct option loptions[] = {
	  { "port", required_argument, 0, 'p' }
	, { "ppk",  required_argument, 0, 'k' }
	, { "help",       no_argument, 0, 'h' }
};

BUG(dst && 0 < argc && argv)
	memset (dst, 0, sizeof(opts_s));
int c;
	while (1) {
		if (-1 == (c = getopt_long (argc, argv, "p:k:", loptions, NULL)))
			break;
		switch (c) {
		case 'p':
			dst->port = atoi (optarg);
			if (dst->port < 1 || 65535 < dst->port) {
echo ("'" VTRR "%s" VTO "': invalid port." "\n", optarg);
				return -1;
			}
			break;
		case 'k':
			dst->ppk_path = optarg;
			break;
		case 'h':
			usage ();
			exit (EXIT_SUCCESS);
			//break;
		case '?':
			break;
		default:
echo ("?? getopt returned character code 0%o ??" "\n", c);
			return false;
		}
	}
	if (! (optind < argc)) {
		usage ();
		return false;
	}
	dst->hostname = argv[optind++];
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// entry point

int main (int argc, char *argv[])
{
// command line parse
opts_s opt;
	if (! (true == _parse_options (argc, argv, &opt)))
		exit (EXIT_FAILURE);

// GTK+ setting
int gtkc; char *gtkv_[1], **gtkv = gtkv_;
	gtkc = 0, gtkv[gtkc++] = argv[0]; // PENDING: support GTK+ option
	gtk_init (&gtkc, &gtkv);
	config_warmup ();

struct config_reader_i *cfg; struct config *cfg_this_;
	cfg_this_ = config_get_singlton (), cfg = config_query_config_reader_i (cfg_this_);
u16 conf_ws_col, conf_ws_row;
	conf_ws_col = (u16)cfg->read_u (cfg_this_, CFG_WS_COL, DEFAULT_WS_COL);
	conf_ws_row = (u16)cfg->read_u (cfg_this_, CFG_WS_ROW, DEFAULT_WS_ROW);
//	conf_n_max = (u32)config_read_u (cfg_this_, CFG_MAX_BACKLOG, DEFAULT_BACKLOG);

// VRAM setup
GVRAM_s bg_this_; struct VRAM_i *bg;
	GVRAM_ctor (&bg_this_, sizeof(bg_this_), conf_ws_col, conf_ws_row);
	GVRAM_reset_dirty (&bg_this_);
	bg = GVRAM_query_VRAM_i (&bg_this_);
TVRAM_s fg_this_; struct VRAM_i *fg;
	TVRAM_ctor (&fg_this_, sizeof(fg_this_), conf_ws_col, conf_ws_row);
	TVRAM_reset_dirty (&fg_this_);
	fg = TVRAM_query_VRAM_i (&fg_this_);

// GTK window
gtk2view_s v, *gtk2 = &v;
	gtk2view_ctor (gtk2, sizeof(gtk2view_s));
GtkWidget *da;
	if (NULL == (da = GTK_WIDGET(gtk2view_init (gtk2, fg, &fg_this_, bg, &bg_this_))))
		return -1;

char conf_font_name[64]; u8 conf_pango_undraw;
	cfg->read_s (cfg_this_, CFG_FONT, DEFAULT_FONT, conf_font_name, sizeof(conf_font_name) -1);
	conf_pango_undraw = (u8)cfg->read_u (cfg_this_, CFG_PANGO_UNDRAW, DEFAULT_PANGO_UNDRAW);
	if (false == gtk2view_set_font (da, conf_font_name, conf_pango_undraw)) {
		if (EINVAL == errno)
echo ("'%s' invalid font name." "\n", conf_font_name);
	}
	gtk2view_size_change (da, conf_ws_col, conf_ws_row);
	gtk2view_show_all (da);
	gtk2view_post_dirty (da);
	gtk2view_grid_change (da, DEFAULT_WS_COL_MIN, DEFAULT_WS_ROW_MIN);

struct mediator test;
	mediator_ctor (&test, sizeof(test), da, fg, &fg_this_, bg, &bg_this_);

	do {
		if (! mediator_connect (&test
			, opt.hostname, opt.port, opt.ppk_path
			, conf_ws_col, conf_ws_row
		))
			break;
// ptmx(bash) stdout/stderr pumper setup
struct idle_event_i *idle_event_handler;
		idle_event_handler = mediator_query_idle_event_i (&test);
		gtk2view_set_idle_event (da, idle_event_handler, &test);
// ptmx(bash) stdin sender setup
struct key_event_i *key_event_handler;
		key_event_handler = mediator_query_key_event_i (&test);
		gtk2view_set_key_event (da, key_event_handler, &test);

		gtk_main ();
	} while (0);

	gtk2view_release (gtk2);
	GVRAM_release (&bg_this_);
	TVRAM_release (&fg_this_);
	mediator_release (&test);

	config_cleanup ();
	return 0;
}
