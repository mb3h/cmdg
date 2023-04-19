bin_PROGRAMS = cmdg
# use not cmdg_CPPFLAGS but AM_CPPFLAGS, because definition of
# simple '.c.o' '.cpp.o' is expand into waste many 'XXX.o'.
AM_CPPFLAGS = -std=c99 \
              -Wno-deprecated-declarations \
              -fdiagnostics-color=always \
              -x c \
              @GTK_CFLAGS@ \
              -DNODEBUG=
cmdg_LDADD = @GTK_LIBS@ -lpthread
cmdg_CFLAGS = 
cmdg_SOURCES = cmdg.cpp appconst.h \
               config.cpp gtk2view.cpp mediator.cpp vt100_handler.cpp vt100_parser.cpp TVRAM.cpp GVRAM.cpp \
               config.hpp gtk2view.hpp mediator.hpp vt100_handler.hpp vt100_parser.hpp TVRAM.hpp GVRAM.hpp \
               gdk2_helper.cpp gtk2_helper.cpp bmp_helper.cpp unicode.c \
               gdk2_helper.hpp gtk2_helper.hpp bmp_helper.hpp unicode.h \
               core/pixel_stream_calc_dirty.c core/bmp_rawdata_clip_range_fit.c core/lsb0_bit_helper.c \
               core/pixel_stream_calc_dirty.h core/bmp_rawdata_clip_range_fit.h core/lsb0_bit_helper.h \
               ssh2const.h \
               ssh2.cpp queue.cpp ringbuf.cpp mpint.c intv.cpp sha2.cpp sha1.cpp aes.cpp base64.c rand.c \
               ssh2.hpp queue.hpp ringbuf.hpp mpint.h intv.hpp sha2.hpp sha1.hpp aes.hpp base64.h rand.h \
               cipher_helper.c cipher_factory.cpp mac_factory.cpp \
               cipher_helper.h cipher_factory.hpp mac_factory.hpp \
               puttykey.c \
               puttykey.h \
               lock.cpp portab.c log.c \
               lock.hpp portab.h log.h assert.h vt100.h
