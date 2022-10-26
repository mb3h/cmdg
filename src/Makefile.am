bin_PROGRAMS = cmdg
# use not cmdg_CPPFLAGS but AM_CPPFLAGS, because definition of
# simple '.c.o' '.cpp.o' is expand into waste many 'XXX.o'.
AM_CPPFLAGS = -x c -std=c99 -Wno-deprecated-declarations @GTK_CFLAGS@
cmdg_LDADD = @GTK_LIBS@
#cmdg_CFLAGS = 
#cmdg_SOURCES = cmdg.c gtkview.c vt100.c escseq.c lock.c conf.c \
#               cmdg.h gtkview.h vt100.h          lock.h conf.h \
#               iid.h defs.h \
#               ssh2.c aes.c sha2.c sha1.c intv.c rand.c base64.c \
#               ssh2.h aes.h sha2.h sha1.h intv.h rand.h base64.h \
#               iface/conn.h
cmdg_CFLAGS = -DSSH_NOT_AVAILABLE
cmdg_SOURCES = cmdg.c gtkview.c vt100.c escseq.c lock.c conf.c \
               cmdg.h gtkview.h vt100.h          lock.h conf.h \
               iid.h defs.h \
               ptmx.c \
               ptmx.h
