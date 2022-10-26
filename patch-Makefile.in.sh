#!/bin/sh
# TEST: automake-1.16.1 + vim-8.1.2269
vim -n -E \
	-c'/^#cmdg_CFLAGS/,/conn\.h/s/^#/@USE_SSH_TRUE@' \
	-c'/^cmdg_CFLAGS/,/ptmx\.h/s/^/@USE_SSH_FALSE@' \
	-cwq \
	src/Makefile.in
