#ifndef CONF_H_INCLUDED__
#define CONF_H_INCLUDED__

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

typedef struct Config__ Config;
struct Config__ {
	uint16_t ws_col;
	uint16_t ws_row;
	// terminal
	uint32_t n_max;
	// view
	uint16_t ws_col_min;
	uint16_t ws_row_min;
	uint32_t bgr24[8];
	uint8_t pango_undraw;
	char font_name[64];
};

bool config_get (Config *conf);

#endif // CONF_H_INCLUDED__
