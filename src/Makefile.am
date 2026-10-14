bin_PROGRAMS = cmdg
# use not cmdg_CPPFLAGS but AM_CPPFLAGS, because definition of
# simple '.c.o' '.cpp.o' is expand into waste many 'XXX.o'.
AM_CPPFLAGS = -std=c99 \
              -Wno-deprecated-declarations \
              -Wno-unused-result \
              -fdiagnostics-color=always \
              @GTK_CFLAGS@ \
              -DNODEBUG=
cmdg_LDADD = @GTK_LIBS@ -lpthread
cmdg_CFLAGS = 
cmdg_SOURCES = cmdg.c appconst.h \
               conf.c \
               conf.h \
               gdk2_keyin.c pseudo_vt100.c gtk2_windowing.c \
               gdk2_keyin.h pseudo_vt100.h gtk2_windowing.h \
               on_ptout.c vt100_parser.c \
               on_ptout.h vt100_parser.h \
               gtk2_font.c image_backlog.c \
               gtk2_font.h image_backlog.h \
               bpp1.c bpp8.c bpp16.c bpp32.c \
               bpp1.h bpp8.h bpp16.h bpp32.h \
               unicode.c memory_usage.c helper/pow2_ge.c \
               unicode.h memory_usage.h helper/pow2_ge.h
