#ifndef CONF_H_INCLUDED__
#define CONF_H_INCLUDED__

#define CONF_FONT_NAME "font_name"
#define CONF_WS_WIDTH  "ws_width"
#define CONF_WS_HEIGHT "ws_height"
#define CONF_WS_X      "ws_x"
#define CONF_WS_Y      "ws_y"

/* Readability sugar.
   Never use 'READABILITY_SUGAR_CONF' directly,
   only use 'conf'. */
typedef struct {
	void (*dtor) ();
	unsigned (*read_uint) (const char *key, unsigned err_uint);
	const char *(*read_cstr) (const char *key, const char *err_cstr);
} READABILITY_SUGAR_CONF;
extern const READABILITY_SUGAR_CONF *conf;

#endif //ndef CONF_H_INCLUDED__
