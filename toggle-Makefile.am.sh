#!/bin/sh
if test -f src/Makefile.am~pts-mode ; then
	echo '\033[1;33mpts-build\033[0m.'
	mv -i src/Makefile.am src/Makefile.am~ssh-mode
	mv -i src/Makefile.am~pts-mode src/Makefile.am
else
	echo '\033[1;32mssh-build\033[0m.'
	mv -i src/Makefile.am src/Makefile.am~pts-mode
	mv -i src/Makefile.am~ssh-mode src/Makefile.am
fi
