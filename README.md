# cmdg: Extended VT100 Emurator

## Overview

![](https://raw.githubusercontent.com/mb3h/cmdg/master/cmdg.png)

![](https://raw.githubusercontent.com/mb3h/cmdg/master/cmdg.svg)

This is VT100 emurator which support embbed PNG/BMP/Sixel graphic output to console.
(Implements based on private code written old for Windows)

Main purpose is to assist writing small graphical tool enough command-line tool extend,
when too exaggerate to treat by HTML and browser, or write dedicated GUI tool.
(I used for seeing address in movie file and monitoring remote server screen)


## Getting Started

```bash
# renew suitable './configure'
aclocal ; autoheader ; automake -a -c ; autoconf
./patch-Makefile.in.sh

# create Makefile
./configure

# build
make

# startup
src/cmdg
```


## TODO priority / progress

BEGINNING
- [x] commit old version (Windows binary only) and sample, old design model(.svg).
- [x] commit new design model. (append UTF-8 support, remove DBCS and much different from old version.)
- [x] i386-gnu-linux/GTK2 base implementation (not as SSH2 remote, as simple pts implement).
- [x] 'bash' shell behavior debug (LANG=C 7bits code and basic escape-sequence).
- [x] embbed BMP binary support.
- [x] SSH2 remote (ssh-rsa, aes256-ctr and hmac-sha1 limited).
- [ ] Windows build support (MSVC).
- [ ] 'Vim' editor behavior debug.
- [ ] 'man' command behavior debug.
- [ ] LANG=UTF-8 behavior debug.
- [ ] SSH2 remote (algorythm support expand).
- [ ] embbed PNG binary support.
- [ ] embbed Sixel text support. (not movie, but only picture)
- [ ] implement original aes.c sha1.c sha2.c.

ADVANCED
- [ ] Windows build support (MinGW).
- [ ] embbed JPEG binary support.
- [ ] make embbed-in / embbed-out trigger editable. (by .conf file)
- [ ] graphic transfer without embbed support. (*1)
- [x] ./configure script support.

PENDING
- [ ] iOS build support and application release. (*2)
- [ ] linux system without X-Window support. (*3)


## Lastly

Thanks a lot achievement of Graphviz dot tools, Vim, and more.

## Notes

(*1)
For example, embbed minimum graphic start trigger byte and use one TCP send port and few UDP recv port.
(This is variation for avoiding display confusion when outputted on graphic embbed unsupported VT100 emurator or happened network trouble, Trade off few port security)

(*2)
However I have iPad only now. In future, after I can prepare Machintosh and XCode enviorment ...

(*3)
For Example, packaged only ldlinux.sys, syslinux.cfg, vmlinuz and minimized initrd.img, booting without switching /root mount, mini USB bootable system etc.
(I don't examine wheather possible or impossible. Even if on VGA, should use scaledown 8 colors, but feel like needing patch something for libc or kernel bypass ...)

README Updated on:2022-10-26
