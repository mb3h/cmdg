#define _POSIX_C_SOURCE 200809L /* POSIX.1-2008 strdup vdprintf
#define _POSIX_C_SOURCE 200112L  * POSIX.1-2001 strtoul readlink
#define _POSIX_C_SOURCE 199309L  * POSIX.1b-1993 nanosleep */
//#define _FILE_OFFSET_BITS 64
#include <features.h>
#include <endian.h>
#include <stdint.h>

#include "helper/pow2_ge.h"

#include <gtk/gtk.h>
#include "gtk2_font.h"
#ifdef FALSE
# undef FALSE // avoid bad effect from glib version
#endif
#ifdef TRUE
# undef TRUE // avoid bad effect from glib version
#endif

#include <stdbool.h>
#include <stdio.h>
typedef uint32_t u32, u21, u24;
typedef  uint8_t u8, u1;
typedef uint16_t u16, u9, u12, u10;
typedef uint64_t u64;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define ALIGN32(x) (-32 & (x) +32 -1)
#define PADDING(a,x)    ((a) -1 - ((x) +(a) -1) % (a))
#define ALIGN(a,x) ((x) +(a) -1 - ((x) +(a) -1) % (a))
#define RGB24(r,g,b) (0xff0000 & (r) << 16 \
                      | 0xff00 & (g) << 8  \
                        | 0xff & (b)       )
#define MONO(w) RGB24(w,w,w)

#define FALSE 0
#define TRUE  1
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

#define caller_bug     assert
#define unexpected     assert
#define attention_spec assert

///////////////////////////////////////////////////////////////
// can't recovery

#define FATAL_ABORT exit (-1)

///////////////////////////////////////////////////////////////
// endian portability

static void store_le32 (void *dst, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
#else
	val =       0xff  & val >> 24
	    |     0xff00  & val >> 8
	    |   0xff0000  & val << 8
	    | 0xff000000U & val << 24;
#endif
	*(u32 *)dst = val;
}

static void store_be24 (void *dst, u24 val)
{
u8 *p
	= (u8 *)dst;
	*(p +0) = (u8)(0xff & val >> 16);
	*(p +1) = (u8)(0xff & val >>  8);
	*(p +2) = (u8)(0xff & val      );
}

///////////////////////////////////////////////////////////////
// helper

static void _gdk_color_from_rgb24 (GdkColor *dst, u24 rgb24)
{
	dst->pixel = rgb24;
	dst->red   = 0x101 * (0xff & rgb24 >> 16);
	dst->green = 0x101 * (0xff & rgb24 >>  8);
	dst->blue  = 0x101 * (0xff & rgb24      );
}

static cairo_t *_create_cairo (GdkWindow *window)
{
caller_bug(window)

cairo_t *cr
	= gdk_cairo_create (GDK_DRAWABLE(window));
	return cr;
}

///////////////////////////////////////////////////////////////
// utility

static u32 utf8from (u21 unicode)
{
u32 utf8;
	if (unicode < 0x80) // 7bit (ANSI)
		utf8 = unicode;
	else if (unicode < 0x1000) // 12bit
		utf8  = (0xC0 | 0x3F & unicode >> 6)
		      | (0x80 | 0x3F & unicode     ) << 8;
	else if (unicode < 0x10000) // 16bit
		utf8 = (0xE0 | 0x0F & unicode >> 12)
		     | (0x80 | 0x3F & unicode >> 6 ) << 8
		     | (0x80 | 0x3F & unicode      ) << 16;
	else /*if (unicode < 0x200000)*/ // 21bit
		utf8 = (0xF0 | 0x07 & unicode >> 16)
		     | (0x80 | 0x0F & unicode >> 12) << 8
		     | (0x80 | 0x3F & unicode >> 6 ) << 16 
		     | (0x80 | 0x3F & unicode      ) << 24;
	return utf8;
}

static void _bpp24from1 (void *dst_,
                         u24 fgcl24, u24 bgcl24,
                         unsigned width, unsigned pad_align,
                         const u8 *src)
{
u8 *dst, *end;
	dst = (u8 *)dst_, end = (u8 *)dst_ + width * (24 / 8);
u8 mask = 0x80;
	for (; dst < end; dst += 24 / 8) {
		store_be24 (dst, (mask & *src) ? fgcl24 : bgcl24);
		mask >>= 1;
		if (0x00 == mask)
			mask = 0x80, ++src;
	}
unsigned padding_len = 0;
	if (0 < pad_align)
		padding_len = PADDING(pad_align, width * (24 / 8));
	if (0 < padding_len)
		memset (dst, 0, padding_len);
}

///////////////////////////////////////////////////////////////
// singlton memory

#define CUSTOM_FONT_WIDTH  8  // PENDING: (*1)
#define CUSTOM_FONT_HEIGHT 16 // PENDING: (*1)

typedef struct {
	u8 *rgb888; // expanding work
	u32 rgb888_max;
	u32 fgcl24     :24;
	u32 fgcl24_pad :8;

	u8 lf_width, lf_height;
	u1 use_custom_font;
	u8 align8_pad[5];

	// installed font
	PangoLayout *layout;
	// custom font (out of pango)
#define WIDTH  CUSTOM_FONT_WIDTH
#define HEIGHT CUSTOM_FONT_HEIGHT
	u8 font[256 * ALIGN(8, WIDTH)/8 * HEIGHT];
#undef WIDTH
#undef HEIGHT
} gtk2_font_s;

static gtk2_font_s s_singlton = {0};

///////////////////////////////////////////////////////////////
// <-> gtk2_windowing.o

static void _load_pango_font (const char *font_name,
                              GtkWidget *widget)
{
gtk2_font_s *m_ = &s_singlton;

	// allocate font description
PangoFontDescription *font_desc
	= pango_font_description_from_string (font_name);
unexpected(font_desc)

	// mention pango context
PangoContext *pango
	= gtk_widget_get_pango_context (widget);

	// font metrics
unexpected(pango)
PangoLanguage *lang
	= pango_context_get_language (pango);
unexpected(lang)
PangoFontMetrics *mtx
	= pango_context_get_metrics (pango, font_desc, lang);
unexpected(mtx)

int approximate_digit_width
	= pango_font_metrics_get_approximate_digit_width (mtx);
int height
	= pango_font_metrics_get_height (mtx);
int ascent
	= pango_font_metrics_get_ascent (mtx);
int descent
	= pango_font_metrics_get_descent (mtx);

	// (convert from glyph units to pixels)
	approximate_digit_width = PANGO_PIXELS(approximate_digit_width);
	height = PANGO_PIXELS(height);
	ascent = PANGO_PIXELS(ascent);
	descent = PANGO_PIXELS(descent);

	// apply font
	gtk_widget_modify_font (widget, font_desc);
attention_spec(widget->style->font_desc != font_desc)

	// layout object for drawing
	if (m_->layout) {
		g_object_unref (m_->layout);
		m_->layout = NULL;
	}
	m_->layout = pango_layout_new (pango);
unexpected(m_->layout)
	pango_layout_set_font_description (m_->layout,
	                                   widget->style->font_desc);

	// release font description
	pango_font_description_free (font_desc);

	// drawing control
	m_->use_custom_font = FALSE;
	m_->lf_width = (u8)approximate_digit_width;
	m_->lf_height = (u8)(max(height, ascent +descent)); // NOTE: (*2)
}

static char *_workdir_plus_dup (char sep, const char *relpath)
{
char workdir_path[6 +5 +4 +1 +16]; // 5=[0-65535] 16=margin
	sprintf (workdir_path, "/proc/%d/cwd", getpid ());

unsigned n
	= strlen (workdir_path) +1 +strlen (relpath) +1;
	n = pow2_ge(n);
char *path_buf
	= (char *)malloc (n);
unsigned workdir_len;

	while (1) {
int result
		= readlink (workdir_path, path_buf, n);
unexpected(0 <= result)
		if (-1 == result)
			FATAL_ABORT; // PENDING: detail message
		workdir_len = (unsigned)min(0, result); // safety

u1 not_truncated
		= (workdir_len < n) ? TRUE : FALSE;
		if (not_truncated)
			break; // success (not truncated)
		n *= 2;
		path_buf = (char *)realloc (path_buf, n);
	}

	path_buf[workdir_len++] = '/';
	strcpy (path_buf +workdir_len, relpath);

	return path_buf;
}

static bool _load_outside_font (const char *path)
{
gtk2_font_s *m_ = &s_singlton;
caller_bug(path)
	m_->use_custom_font = TRUE;
	m_->lf_width  = CUSTOM_FONT_WIDTH ; // PENDING: (*1)
	m_->lf_height = CUSTOM_FONT_HEIGHT; // PENDING: (*1)

u1 has_waste_prefix
	= ('.' == path[0] && '/' == path[1]) ? TRUE : FALSE;
	if (has_waste_prefix)
		path += 2;
char *path_buf;

u1 is_full_path
	= ('/' == path[0]) ? TRUE : FALSE;
	if (is_full_path)
		path_buf = strdup (path);
	else
		path_buf = _workdir_plus_dup ('/', path);
unexpected(path_buf)

u1 is_reading_okay = FALSE;
	do {
FILE *fp
		= fopen (path_buf, "rb");
		if (NULL == fp)
			break;
		fseek (fp, +0x800, SEEK_SET);
		fread (m_->font, 1, sizeof(m_->font), fp);
		fclose (fp);

		is_reading_okay = TRUE;
	} while (0);

	free (path_buf);
	return (is_reading_okay) ? true : false;
}

static enum GTK2_FONT_TYPE _get_type ()
{
gtk2_font_s *m_ = &s_singlton;

enum GTK2_FONT_TYPE retval
	= INSTALLED;
	if (m_->use_custom_font)
		retval = CUSTOM;
	return retval;
}

static u8 _get_font_width ()
{
gtk2_font_s *m_ = &s_singlton;
	return m_->lf_width;
}

static u8 _get_font_height ()
{
gtk2_font_s *m_ = &s_singlton;
	return m_->lf_height;
}

static const u8 *_get_bpp1_font (u21 utf21,
                                 u8 *lf_width, u8 *lf_height)
{
gtk2_font_s *m_ = &s_singlton;

	if (!m_->use_custom_font)
		return NULL;

	if (! (0x20 <= utf21 && utf21 < 0x7f))
		return NULL;
#define WIDTH  CUSTOM_FONT_WIDTH
#define HEIGHT CUSTOM_FONT_HEIGHT
	if (lf_width)
		*lf_width = WIDTH;
	if (lf_height)
		*lf_height = HEIGHT;
		
	return &m_->font[utf21 * ALIGN(8, WIDTH)/8 * HEIGHT];
#undef WIDTH
#undef HEIGHT
}

///////////////////////////////////////////////////////////////
// 

static void _cairo_put_box (u10 col, u9 pt_row, u24 rgb24,
                            cairo_t *cr)
{
gtk2_font_s *m_ = &s_singlton;

GdkColor cl;
	_gdk_color_from_rgb24 (&cl, rgb24);
	gdk_cairo_set_source_color (cr, &cl);

GdkRectangle rc = {
	.x = col * m_->lf_width, .y = pt_row * m_->lf_height,
	.width  = m_->lf_width, .height = m_->lf_height,
};
	gdk_cairo_rectangle (cr, &rc);
	cairo_fill (cr);
}

static void _cairo_put_bpp24 (u10 col, u9 pt_row,
                              const void *rgb888_img, cairo_t *cr)
{
gtk2_font_s *m_ = &s_singlton;
caller_bug(cr)

const unsigned BPP = 24;
	// request to cairo PENDING: (*3)
gboolean has_alpha = FALSE; // RGB888(FALSE) or RGBA8888(TRUE)
GdkPixbuf *pixbuf
	= gdk_pixbuf_new_from_data (
	    (guchar *)rgb888_img,
	    GDK_COLORSPACE_RGB, has_alpha, 8,
	    m_->lf_width, m_->lf_height,
	    ALIGN32(m_->lf_width * BPP) / 8,
	    NULL, NULL);
	if (NULL == pixbuf)
		return; // GDK error
	gdk_cairo_set_source_pixbuf (cr, pixbuf, col * m_->lf_width,
	                             pt_row * m_->lf_height);
	g_object_unref (pixbuf);
	cairo_paint (cr);
}

// use installed font inside OS
static void _pango_put_font (u10 col, u9 pt_row,
                             u24 fgcl24, u24 bgcl24,
	                          u1 bold, u1 underline,
	                          u21 uni21,
                             GdkWindow *window, GdkGC *fgcl_gc,
                             cairo_t *cr)
{
gtk2_font_s *m_ = &s_singlton;
caller_bug(window)

u1 installed_font_available
	= (m_->layout) ? 1 : 0;
	if (!installed_font_available)
		return;

GdkColor fgcl;
	_gdk_color_from_rgb24 (&fgcl, fgcl24);

	// PangoLayout: character
u32 utf8
	= utf8from (uni21);
char s[4];
	store_le32 (s, utf8);
unsigned utf8_len
	= (0xff000000U & utf8) ? 4
	:   (0xff0000  & utf8) ? 3
	:     (0xff00  & utf8) ? 2 : 1;
	pango_layout_set_text (m_->layout, s, utf8_len);

	// PangoLayout: enter PangoAttrList
PangoAttrList *list
	= pango_attr_list_new ();
unexpected(list)
PangoAttribute *attr;
	// PangoLayout: background color
GdkColor bgcl;
	_gdk_color_from_rgb24 (&bgcl, bgcl24);
	attr = pango_attr_background_new (bgcl.red, bgcl.green,
	                                  bgcl.blue);
unexpected(attr)
	pango_attr_list_insert (list, attr);
	// PangoLayout: bold style
	if (bold) {
		attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
unexpected(attr)
		pango_attr_list_insert (list, attr);
	}
	// PangoLayout: leave PangoAttrList
	pango_layout_set_attributes (m_->layout, list);
	pango_attr_list_unref (list);

	// GdkGC: text color
	gdk_gc_set_foreground (fgcl_gc, &fgcl);

	// gdk: drawing
gint x, y;
	x = col * m_->lf_width, y = pt_row * m_->lf_height;
	gdk_draw_layout (GDK_DRAWABLE(window), fgcl_gc, x, y, m_->layout);

	// cairo: underline overlay NOTE: (*4)
	if (underline) {
		gdk_cairo_set_source_color (cr, &fgcl);
GdkRectangle rc = {
		.x = x, .y = y +m_->lf_height -1,
		.width  = m_->lf_width, .height = 1,
		};
		gdk_cairo_rectangle (cr, &rc);
		cairo_fill (cr);
	}
}

static void _cairo_put_font (u10 col, u9 pt_row,
                             u24 fgcl24, u24 bgcl24,
                             u21 uni21, cairo_t *cr)
{
gtk2_font_s *m_ = &s_singlton;
caller_bug(cr)

	// getting font pattern and size
const u8 *font = NULL; u8 lf_width, lf_height;
	if (m_->use_custom_font)
		font = _get_bpp1_font (uni21, &lf_width, &lf_height);

static u8 giveup[16] = {
	  0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
	, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 
};
	if (NULL == font)
		font = giveup, lf_width = 8, lf_height = 16;

	// work memory
const unsigned BPP = 24;
unsigned /*bytes_per_line*/ bpl = ALIGN32(lf_width * BPP) / 8;
	if (m_->rgb888_max < pow2_ge(bpl * lf_height)) {
		m_->rgb888_max = pow2_ge(bpl * lf_height);
		if (NULL == m_->rgb888)
			m_->rgb888 = (u8 *)malloc (m_->rgb888_max);
		else
			m_->rgb888 = (u8 *)realloc (m_->rgb888, m_->rgb888_max);
	}

	// bits per pixel conversion
	for (unsigned y = 0; y < lf_height; ++y) {
unsigned pad_align;
		_bpp24from1 (m_->rgb888 + y * bpl, fgcl24, bgcl24,
		             lf_width, pad_align = 4,
		             &font[y]);
	}

	_cairo_put_bpp24 (col, pt_row, m_->rgb888, cr);
//	free (m_->rgb888);
}

///////////////////////////////////////////////////////////////
// readability sugar

static const READABILITY_SUGAR_GTK2_FONT _gtk2_font = {
	.get_type = _get_type,
	.load = _load_outside_font,
	.load_pango = _load_pango_font,
	.put_box = _cairo_put_box,
	.put_bpp24 = _cairo_put_bpp24,
	.put = _cairo_put_font,
	.put_pango = _pango_put_font,

	.width = _get_font_width,
	.height = _get_font_height,
};
const READABILITY_SUGAR_GTK2_FONT *gtk2_font = &_gtk2_font;

///////////////////////////////////////////////////////////////
// footnote

/* PENDING: (*1)
   It is supposed to allow any size.
  (calculating from file size or file header of font file)

   NOTE: (*2)
   'ascent + descent' is sometimes over 'height'.
	For example, 18 < 15 +4 is found on 'Ubuntu Mono 12'.

   PENDING: (*3)
   Is fragment calling gdk_pixbuf_new_from_data()
                     / g_object_unref() give slow?
   This isn't made sure.

   NOTE: (*4)
   Using pango_attr_underline_new() can't give fine tuning
   to thinness. If we want to do it, we have to use cairo.
  (Probably, qterminal (LUbuntu) also takes this way. */
