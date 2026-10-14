# cmdg: Extended VT100 Emurator

## Overview

![](https://raw.githubusercontent.com/mb3h/cmdg/master/snapshot-1.png)

![](https://raw.githubusercontent.com/mb3h/cmdg/master/snapshot-2.png)

![](https://raw.githubusercontent.com/mb3h/cmdg/master/snapshot-3.png)

This is VT100 emurator, where it expands escape sequence for drawing not only text but also image.
(Implements based on private code written old for Windows)

Main purpose is to assist writing small graphical tool enough command-line tool extend.
It helps the case that you want to run on smaller workingset than using HTML, need more intaractive input or find more advantage in keyboard using.
(I used for seeing address in movie file and monitoring remote server screen)


## Getting Started

```bash
# renew suitable './configure'
aclocal ; autoheader ; automake -a -c ; autoconf

# create Makefile
./configure

# build
make

# startup
src/cmdg
```


## TODO

CONSOLE MANAGEMENT
- [x] status bar: show memory usage
- [ ] inifinte backlog (as much as memory limited)
- [ ] save/load backlog
- [ ] swap out backlog (between storage and memory)

DRAWING FLEXIBILITY
- [ ] allow getting current font size (share memory or socket, supply sample code not under AGPL3.0 constraint)
- [ ] expand fixed image format (PNG etc.)
- [ ] add variable image format (custom: 88ALU, V9938 etc.)
- [ ] allow modifying escape sequence for entering image-out

FAIL-SAFE
- [ ] add how to send image data not embedding (shared memory, socket etc.)

PLATFORM EXPANDING
- [ ] GTK+3.0 support

REMOTING
- [ ] SSH2 support (temporary disabled now version)


## Lastly

Thanks a lot achievement of Graphviz dot tools, Vim, and more.

## Notes

README Updated on:2026-10-11
