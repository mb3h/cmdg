#include <gtk/gtk.h>

#include <stdint.h>
#include "common/pixel.h"
struct gtk2_helper_;
#define gtk_helper_s struct gtk2_helper_
#include "gtk2_helper.hpp"

#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <string.h>
#include <errno.h>
typedef uint32_t u32;
typedef  uint8_t u8;

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

#define ALIGN32(x) (-32 & (x) +32 -1)

///////////////////////////////////////////////////////////////////////////////
// non-state

GtkWindow *gtk_widget_get_root (GtkWidget *widget)
{
	if (NULL == widget)
		return NULL;
	while (1) {
GtkWidget *i;
		if (GTK_IS_MENU(widget)) // (*1)
			i = gtk_menu_get_attach_widget (GTK_MENU(widget)); // = GTK_MENU(widget)->parent_menu_item
		else
			i = gtk_widget_get_parent (widget);
		if (NULL == i)
			break;
		widget = i;
	}
	return GTK_WINDOW(widget);
}

GtkWidget *gtk_widget_get_child (GtkWidget *checked, GType find_type)
{
	if (G_TYPE_CHECK_INSTANCE_TYPE(checked, find_type))
		return GTK_WIDGET(checked);

	if (GTK_IS_NOTEBOOK(checked)) {
gint page_num;
		page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK(checked));
		checked = gtk_notebook_get_nth_page (GTK_NOTEBOOK(checked), page_num);
		return gtk_widget_get_child (checked, find_type);
	}

	if (!GTK_IS_CONTAINER(checked))
		return NULL;
GList *list;
#if GTK_CHECK_VERSION(2,22,0)
	list = gtk_container_get_children (GTK_CONTAINER(checked));
#else
	if (GTK_IS_VBOX(checked)) {
GtkVBox *vbox;
		vbox = GTK_VBOX(checked);
		list = vbox->box->children;
	}
	else
		list = gtk_container_get_children (GTK_CONTAINER(checked));
#endif

	while (list) {
		if (checked = gtk_widget_get_child (GTK_WIDGET(list->data), find_type))
			return checked;
		list = list->next;
	}
	return NULL;
}

void gtk2_helper_argb32_to_gdk_color (u32 argb32, GdkColor *dst)
{
	dst->pixel = argb32;
	dst->red   = 0x101 * (0xff & argb32 >> 16);
	dst->green = 0x101 * (0xff & argb32 >> 8);
	dst->blue  = 0x101 * (0xff & argb32);
}

///////////////////////////////////////////////////////////////////////////////
// private

typedef struct gtk2_helper_ {
	GtkWidget *da;
	GdkDrawable *drawable;
	GdkGC *gc;
	PangoContext *pango;
	GtkStyle *style;
	PangoLayout *layout;
	cairo_t *cr;
} gtk2_helper_s;

///////////////////////////////////////////////////////////////////////////////
// dtor/ctor

static void gtk2_helper_reset (gtk2_helper_s *m_)
{
#ifndef __cplusplus
GtkWidget *da;
	da = m_->da;
	memset (m_, 0, sizeof(gtk2_helper_s));
	m_->da = da;
#else //def __cplusplus
	m_->drawable = NULL;
	m_->gc       = NULL;
	m_->pango    = NULL;
	m_->style    = NULL;
	m_->layout   = NULL;
	m_->cr       = NULL;
#endif // __cplusplus
}

void gtk2_helper_release (struct gtk2_helper *this_)
{
gtk2_helper_s *m_;
	m_ = (gtk2_helper_s *)this_;
	if (m_->layout)
		g_object_unref (m_->layout);
	if (m_->cr)
		cairo_destroy (m_->cr);
	gtk2_helper_reset (m_);
}

#ifdef __cplusplus
void gtk2_helper_dtor (struct gtk2_helper *this_)
{
}
#endif // __cplusplus

void gtk2_helper_ctor (struct gtk2_helper *this_, unsigned cb, GtkWidget *da)
{
#ifndef __cplusplus
BUGc(sizeof(gtk2_helper_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(gtk2_helper_s), cb)
#endif // __cplusplus
gtk2_helper_s *m_;
	m_ = (gtk2_helper_s *)this_;
	m_->da = da;
	gtk2_helper_reset (m_);
}

///////////////////////////////////////////////////////////////////////////////
// getter (GDK/GTK)

static GtkStyle *gtk2_helper_get_gtk_style (gtk2_helper_s *m_)
{
	return m_->da->style;
}

static GdkGC *gtk2_helper_get_gdk_gc (gtk2_helper_s *m_)
{
	if (NULL == m_->gc) {
		m_->gc = m_->da->style->text_gc[m_->da->state];
	}
	return m_->gc;
}

static GdkDrawable *gtk2_helper_get_gdk_drawable (gtk2_helper_s *m_)
{
	if (NULL == m_->drawable) {
GdkWindow *window;
		window = gtk_widget_get_window (m_->da);
		m_->drawable = GDK_DRAWABLE(window);
	}
	return m_->drawable;
}

///////////////////////////////////////////////////////////////////////////////
// getter (cairo)

static cairo_t *gtk2_helper_get_cairo_t (gtk2_helper_s *m_)
{
	if (NULL == m_->cr) {
GdkDrawable *drawable;
		drawable = gtk2_helper_get_gdk_drawable (m_);
		m_->cr = gdk_cairo_create (drawable);
	}
	return m_->cr;
}

///////////////////////////////////////////////////////////////////////////////
// getter (pango)

static PangoContext *gtk2_helper_get_pango_context (gtk2_helper_s *m_)
{
	if (NULL == m_->pango) {
		m_->pango = gtk_widget_get_pango_context (m_->da);
	}
	return m_->pango;
}

static PangoLayout *gtk2_helper_get_pango_layout (gtk2_helper_s *m_)
{
	if (NULL == m_->layout) {
PangoContext *pango;
		pango = gtk2_helper_get_pango_context (m_);
		m_->layout = pango_layout_new (pango);
GtkStyle *style;
		style = gtk2_helper_get_gtk_style (m_);
		pango_layout_set_font_description (m_->layout, style->font_desc);
	}
	return m_->layout;
}

///////////////////////////////////////////////////////////////////////////////
// cairo

static cairo_t *gtk2_helper_cairo_set_source_argb32 (gtk2_helper_s *m_, cairo_t *cr, u32 argb32)
{
	if (NULL == cr)
		cr = gtk2_helper_get_cairo_t (m_);
GdkColor bgcl;
	gtk2_helper_argb32_to_gdk_color (argb32, &bgcl);
	gdk_cairo_set_source_color (cr, &bgcl);
}

static cairo_t *gtk2_helper_cairo_set_rectangle (gtk2_helper_s *m_, cairo_t *cr, int x, int y, int width, int height)
{
	if (NULL == cr)
		cr = gtk2_helper_get_cairo_t (m_);
GdkRectangle rc;
	rc.x = x;
	rc.y = y;
	rc.width = width;
	rc.height = height;
	gdk_cairo_rectangle (cr, &rc);
	return cr;
}

///////////////////////////////////////////////////////////////////////////////
// pango

static PangoLayout *gtk2_helper_layout_set_text (gtk2_helper_s *m_, PangoLayout *layout, const char *text, int length)
{
	if (NULL == layout)
		layout = gtk2_helper_get_pango_layout (m_);
	pango_layout_set_text (layout, text, length);
	return layout;
}

static GdkGC *gtk2_helper_gc_set_foreground (gtk2_helper_s *m_, GdkGC *gc, u32 argb32)
{
	if (NULL == gc)
		gc = gtk2_helper_get_gdk_gc (m_);
GdkColor fgcl;
	gtk2_helper_argb32_to_gdk_color (argb32, &fgcl);
	gdk_gc_set_foreground (gc, &fgcl);
	return gc;
}

static PangoLayout *gtk2_helper_layout_set_attr_background (gtk2_helper_s *m_, PangoLayout *layout, u32 argb32)
{
	if (NULL == layout)
		layout = gtk2_helper_get_pango_layout (m_);
PangoAttrList *list;
	list = pango_attr_list_new ();
ASSERTE(list)
GdkColor bgcl;
	gtk2_helper_argb32_to_gdk_color (argb32, &bgcl);
PangoAttribute *attr;
	attr = pango_attr_background_new (bgcl.red, bgcl.green, bgcl.blue);
ASSERTE(attr)
	pango_attr_list_insert (list, attr);
	pango_layout_set_attributes (layout, list);
	pango_attr_list_unref (list);
	return layout;
}

static bool gtk2_helper_font_get_metrics (GtkWidget *widget, PangoFontDescription *font, u8 *lf_width, u8 *lf_ascent, u8 *lf_descent)
{
PangoFontMetrics *mtx;
	do {
		if (NULL == font) {
GtkRcStyle *rc_style;
			if (NULL == (rc_style = gtk_widget_get_modifier_style (widget)))
				break;
			if (NULL == (font = rc_style->font_desc))
				break;
		}
PangoContext *pango;
		if (NULL == (pango = gtk_widget_get_pango_context (widget)))
			break;
PangoLanguage *lang;
		if (NULL == (lang = pango_context_get_language (pango)))
			break;
		if (NULL == (mtx = pango_context_get_metrics (pango, font, lang)))
			break;
	} while (0);
	if (NULL == mtx) {
		errno = 0; // font metrics cannot retrieved. (pango error)
		return false;
	}

int px;
	if (lf_width)
		*lf_width = (u8)(px = PANGO_PIXELS(pango_font_metrics_get_approximate_digit_width (mtx)));
	if (lf_ascent)
		*lf_ascent = (u8)(px = PANGO_PIXELS(pango_font_metrics_get_ascent (mtx)));
	if (lf_descent)
		*lf_descent = (u8)(px = PANGO_PIXELS(pango_font_metrics_get_descent (mtx)));

	pango_font_metrics_unref (mtx);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// interface

static void gtk2_helper_draw_text (gtk2_helper_s *m_, int x, int y, const char *text, int length, u32 fg_argb32, u32 bg_argb32)
{
PangoLayout *layout;
	layout = gtk2_helper_layout_set_text (m_, NULL, text, length);
	gtk2_helper_layout_set_attr_background (m_, layout, bg_argb32);
GdkGC *gc;
	gc = gtk2_helper_gc_set_foreground (m_, NULL, fg_argb32);
GdkDrawable *drawable;
	drawable = gtk2_helper_get_gdk_drawable (m_);
	gdk_draw_layout (drawable, gc, x, y, layout);
}

static void gtk2_helper_fill_rectangle (gtk2_helper_s *m_, int x, int y, int width, int height, u32 argb32)
{
cairo_t *cr;
	cr = gtk2_helper_cairo_set_rectangle (m_, NULL, x, y, width, height);
	gtk2_helper_cairo_set_source_argb32 (m_, cr, argb32);
	cairo_fill (cr);
}

static void gtk2_helper_paint_data (gtk2_helper_s *m_, int x, int y, int width, int height, enum pixel_format pf, const void *rgb24_le)
{
	if (! (PF_RGB888 == pf || PF_RGBA8888 == pf))
		return;
gboolean has_alpha;
	has_alpha = (PF_RGBA8888 == pf) ? TRUE : FALSE;
cairo_t *cr;
	cr = gtk2_helper_get_cairo_t (m_);
GdkPixbuf *pixbuf;
	if (NULL == (pixbuf = gdk_pixbuf_new_from_data ((guchar *)rgb24_le, GDK_COLORSPACE_RGB, has_alpha, 8, width, height, ALIGN32(width * 24) / 8, NULL, NULL)))
			return;
	gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);
	g_object_unref (pixbuf);
	cairo_paint (cr);
}

static bool gtk2_helper_set_font (gtk2_helper_s *m_, const char *font_name, u32 bg_argb32, u8 *lf_width, u8 *lf_ascent, u8 *lf_descent)
{
PangoFontDescription *font;
	if (NULL == (font = pango_font_description_from_string (font_name))) {
		errno = EINVAL; // invalid font name.
		return false;
	}
	if (! gtk2_helper_font_get_metrics (m_->da, font, lf_width, lf_ascent, lf_descent)) {
		errno = 0; // font metrics cannot retrieved. (pango error)
		return false;
	}
	gtk_widget_modify_font (m_->da, font);
	pango_font_description_free (font);
GdkColor bgcl;
	gtk2_helper_argb32_to_gdk_color (bg_argb32, &bgcl);
	gtk_widget_modify_bg (m_->da, GTK_STATE_NORMAL, &bgcl); // bg of View
	return true;
}

///////////////////////////////////////////////////////////////////////////////

static struct gtk_helper_i o_gtk_helper_i = {
	.set_font       = gtk2_helper_set_font,
	.draw_text      = gtk2_helper_draw_text,
	.fill_rectangle = gtk2_helper_fill_rectangle,
	.paint_data     = gtk2_helper_paint_data,
};
struct gtk_helper_i *gtk2_helper_query_gtk_helper_i (struct gtk2_helper *this_) { return &o_gtk_helper_i; }
