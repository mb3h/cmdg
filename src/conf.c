#include <string.h>
#include "defs.h"

#include "conf.h"
typedef uint32_t u32;

//#define CONF_PATH "./cmdg.conf" // TODO
#define WS_COL_DEFAULT 80
#define WS_ROW_DEFAULT 24
#define BACKLOG_DEFAULT 1000
#define WS_COL_MIN 4
#define WS_ROW_MIN 1
#define FONT_DEFAULT "IPAGothic 12" // fixed font [gnome-terminal]
#define PANGO_UNDRAW_DEFAULT 0
///////////////////////////////////////////////////////////////////////////////
// 

static struct {
	const u32 bgr24[8];
} s_ = {
	{ 0, 0xff, 0x00ff00, 0x00ffff, 0xff0000, 0xff00ff, 0xffff00, 0xcfd7d3 }
};

bool config_get (Config *conf)
{
	if (NULL == conf)
		return false;
	// TODO apply 'cmdg.conf'
	conf->ws_col = WS_COL_DEFAULT;
	conf->ws_row = WS_ROW_DEFAULT;
	conf->n_max = BACKLOG_DEFAULT;
	conf->ws_col_min = WS_COL_MIN;
	conf->ws_row_min = WS_ROW_MIN;
	strncpy (conf->font_name, FONT_DEFAULT, sizeof(conf->font_name) -1);
	conf->pango_undraw = PANGO_UNDRAW_DEFAULT;
	memcpy (conf->bgr24,s_.bgr24, sizeof(conf->bgr24));
	return true;
}
