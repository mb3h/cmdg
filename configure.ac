#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([cmdg], [0.00])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/vt100.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_ARG_WITH([ssh],
  [AS_HELP_STRING([--without-ssh],
                  [build as not SSH client but simple PTS client.])],
  [],
  [with_ssh=yes])

AM_CONDITIONAL([USE_SSH],[test xyes = "x$with_ssh"])

ifdef([AM_PATH_GTK_2_0],[AM_PATH_GTK_2_0([2.0.0], [gtk=2], [])],
[AC_WARNING([generating configure script without GTK 2 autodetection])])

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h fcntl.h malloc.h memory.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/time.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
TARGET='
gdk_cairo_create
gdk_cairo_set_source_pixbuf
gdk_cairo_set_source_color
gdk_cairo_rectangle
gdk_draw_layout
gdk_gc_set_foreground
gdk_pixbuf_new_from_data
gdk_threads_add_timeout
cairo_destroy
cairo_fill
cairo_paint
pango_layout_new
pango_layout_set_font_description
pango_layout_set_text
pango_layout_set_attributes
pango_attr_list_unref
pango_attr_list_new
pango_attr_list_insert
pango_attr_background_new
pango_font_description_free
pango_font_description_from_string
pango_font_context_get_language
pango_font_context_get_metrics
pango_font_metrics_unref
pango_font_metrics_get_approximate_digit_width
pango_font_metrics_get_ascent
pango_font_metrics_get_descent
g_idle_add
g_object_unref
g_object_get_data
g_object_set_data
g_signal_connect
g_source_remove
g_timeout_add
gtk_init
gtk_main_quit
gtk_widget_destroy
gtk_widget_get_pango_context
gtk_widget_get_window
gtk_widget_modify_bg
gtk_widget_modify_font
gtk_widget_set_size_request
gtk_widget_show
gtk_widget_show_all
gtk_window_new
gtk_window_set_title
gtk_window_set_geometory_hints
gtk_window_set_policy
gtk_accel_group_new
gtk_box_pack_end
gtk_box_pack_start
gtk_container_add
gtk_container_get_children
gtk_drawing_area_new
gtk_hbox_new
gtk_item_factory_new
gtk_item_factory_create_items
gtk_item_factory_get_widget
gtk_image_new_from_stock
gtk_label_new
gtk_notebook_new
gtk_notebook_append_page
gtk_notebook_get_current_page
gtk_notebook_get_nth_page
gtk_notebook_popup_enable
gtk_notebook_set_scrollable
gtk_notebook_set_show_border
gtk_notebook_set_tab_pos
gtk_vbox_new
'
CFLAGS="$CFLAGS $GTK_CFLAGS"
LIBS="$LIBS $GTK_LIBS"
#echo -e "CFLAGS: $CFLAGS"
#echo -e "LIBS: $LIBS"
AC_CHECK_FUNCS([clock_gettime dup2 gettimeofday memchr memmove memset socket strchr strerror $TARGET])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT

if test ! xno = x"$with_ssh" ; then
	echo -e '\033@<:@1;31mWARNING\033@<:@0m:'
	echo -e 'In this version, original aes.c sha1.c sha2.c is \033@<:@1;31mnot implemented\033@<:@0m.'
	echo -e "Please build pts-mode by \033@<:@1;31m'--without-ssh' option\033@<:@0m, or \033@<:@1;31mprepare"
	echo -e 'aes.c sha1.c sha2.c\033@<:@0m fitting to aes.h sha1.h sha2.h interface.'
	echo -e 'It is hard to upload temporarily implements calling present AES SHA'
	echo -e 'libraries \033@<:@1;31mfor keeping MIT-license\033@<:@0m, because most of these libraries'
	echo -e 'seem to choose licence other than MIT-license.'
fi
