#ifndef GDK2_KEYIN_H_INCLUDED__
#define GDK2_KEYIN_H_INCLUDED__

/* Readability sugar.
   Never use 'READABILITY_SUGAR_GDK2_KEYIN' directly,
   only use 'gdk2_keyin'. */
typedef struct {
	_Bool (*on_key) (uint16_t keycode, /*u1*/ uint8_t is_keydown);
} READABILITY_SUGAR_GDK2_KEYIN;
extern const READABILITY_SUGAR_GDK2_KEYIN *gdk2_keyin;

#endif //ndef GDK2_KEYIN_H_INCLUDED__
