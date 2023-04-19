#ifndef APPCONST_H_INCLUDED__
#define APPCONST_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* Tuning limitation of memory using.
   Oversize image is loaded as clipped without full-size.
	(TODO: clipping not implemented now, oversize image cause BREAKING HEAP.)
 */
#define XPIXELS_MAX 1920
#define YPIXELS_MAX 1200

/* Tuning streaming performance of shell input.
 */
#define READ_SHELL_BUFMAX 0x100000 // 1MB

/* Tuning memory performance, high-responce or minimum-loss.
 */
#define TEXT_CB_MIN 15 // 2^4 -1

/* Tuning performance for each platform GTK+.
   Don't poll input-pipe by non-blocking read() in GTK+ idle time, create
   thread blocking by read(), need libthread.so for calling pthread_create().
 */
//READ_SHELL_THREAD (moved to cmdg.gcc)

/* Tuning performance for each platform GTK+.
   Decide timing to draw dirty.
   If defined, draw dirty after waiting 1/60s period always,
   need gdk_threads_add_timeout() calling.
   If not defined, draw dirty immediatery after waiting GTK+ idle,
   need g_idle_add() and g_timeout_add() calling only.
 */
//#define DRAW_DIRTY_THREAD

/* If DRAW_DIRTY_THREAD defined, this is drawing dirty cycle.
   If not defined, this is interval retrying to dirty draw when unexpected
   something happened.
 */
#define FRAME_MS 16 // [ms]

// app name (*)now only use config filename
#define APPNAME          "cmdg"
#define APPNAME_LEN      4

// config key
#define APPNAME          "cmdg"
#define CFG_SGRCL_PREFIX "sgr_col."
#define CFG_WS_COL       "ws_col"
#define CFG_WS_ROW       "ws_row"
#define CFG_MAX_BACKLOG  "n_max"
#define CFG_FONT         "font_name"
#define CFG_PANGO_UNDRAW "pango_undraw"

// default config (*)use when .conf not found or cannot read
#define DEFAULT_WS_COL 80
#define DEFAULT_WS_ROW 24
#define DEFAULT_BACKLOG 1000
#define DEFAULT_WS_COL_MIN 4
#define DEFAULT_WS_ROW_MIN 1
#define DEFAULT_FONT "Ubuntu Mono 13" // fixed font
#define DEFAULT_PANGO_UNDRAW 1
#define DEFAULT_COLOR_0 0x000000
#define DEFAULT_COLOR_1 0x0000bb
#define DEFAULT_COLOR_2 0x00bb00
#define DEFAULT_COLOR_3 0x00bbbb
#define DEFAULT_COLOR_4 0xbb0000
#define DEFAULT_COLOR_5 0xbb00bb
#define DEFAULT_COLOR_6 0xbbbb00
#define DEFAULT_COLOR_7 0xbbbbbb

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // APPCONST_H_INCLUDED__
