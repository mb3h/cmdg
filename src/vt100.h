#ifndef VT100_H_INCLUDED__
#define VT100_H_INCLUDED__

#define VT(x) "\033[3" #x "m"
#define VTO   "\033[0m"
#define VTHI  "\033[1m"

#define VTRR VTHI VT(1) // red
#define VTGG VTHI VT(2) // green
#define VTYY VTHI VT(3) // yellow
//#define VTBB VTHI VT(4) // blue
#define VTMM VTHI VT(5) // magenta
#define VTCC VTHI VT(6) // cyan
//#define VTWW VTHI VT(7) // white

#ifndef VT100_CALLBACK
#define VT100_CALLBACK void
#endif

typedef struct IVt100Action__ IVt100Action;
struct IVt100Action__ {
	size_t (*on_bmp) (VT100_CALLBACK *param, const void *buf, size_t len);
	void (*on_bs) (VT100_CALLBACK *param);
	void (*on_lf) (VT100_CALLBACK *param);
	void (*on_cr) (VT100_CALLBACK *param);
	void (*on_cuu) (VT100_CALLBACK *param, int n);
	void (*on_cuf) (VT100_CALLBACK *param, int n);
	void (*on_el) (VT100_CALLBACK *param, int n);
	void (*on_ansi) (VT100_CALLBACK *param, int c);
	void (*on_none_sgr) (VT100_CALLBACK *param);
	void (*on_bold_sgr) (VT100_CALLBACK *param);
	void (*on_fgcl_sgr) (VT100_CALLBACK *param, int cl);
	void (*on_title_chg) (VT100_CALLBACK *param, const char *title);
};

typedef struct Vt100__ Vt100;
void vt100_dtor (Vt100 *m_);
Vt100 *vt100_ctor (const IVt100Action *iface, void *param);
void vt100_parse (Vt100 *m_, const char *buf, size_t len);

#endif // VT100_H_INCLUDED__
