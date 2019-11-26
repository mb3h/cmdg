# cmdg: Extended VT100 Emurator

## Overview

![](https://raw.githubusercontent.com/mb3h/cmdg/master/2015-03-private/sample.png)

This is VT100 emurator which support embbed PNG/BMP/Sixel graphic output to console.
(Implements based on private code written old for Windows)

Main purpose is to assist writing small graphical tool enough command-line tool extend,
when too exaggerate to treat by HTML and browser, or write dedicated GUI tool.
(I used for seeing address in movie file and monitoring remote server screen)


## TODO priority / progress

BEGINNING
- [x] commit old version (Windows binary only) and sample, old design model(.svg).
- [x] commit new design model. (append UTF-8 support, remove DBCS and much different from old version.)
- [ ] i386-gnu-linux/GTK2 base implementation (not as SSH2 remote, as simple pts implement).
- [ ] 'bash' shell behavior debug (LANG=C 7bits code and basic escape-sequence).
- [ ] embbed BMP binary support.
- [ ] embbed PNG binary support.
- [ ] embbed Sixel text support. (not movie, but only picture)
- [ ] SSH2 remote.
- [ ] 'Vim' editor behavior debug.
- [ ] 'man' command behavior debug.
- [ ] LANG=UTF-8 behavior debug.

ADVANCED
- [ ] Windows build support (not MSVC, but MinGW supposed).
- [ ] embbed JPEG binary support.
- [ ] make embbed-in / embbed-out trigger editable. (by .conf file)
- [ ] graphic transfer without embbed support. (*1)
- [ ] ./configure script support.

PENDING
- [ ] iOS build support and application release. (*2)
- [ ] linux system without X-Window support. (*3)


## Donation

If you think may do it, giving donation would be helpful for keeping my development activity. 
(Off course, I cannot please anything to persons who are not interested in, don't feel possibility, or don't get useful by my works ...)

[![](https://raw.githubusercontent.com/mb3h/cmdg/master/0.004BTC.png)](bitcoin:3KVrnSbvZx7x8qHnz6na7XUtT3HADUZszZ?amount=0.004)

Thanks your will and mind.

[![](https://raw.githubusercontent.com/mb3h/cmdg/master/0.0264BTC.png)](bitcoin:3KVrnSbvZx7x8qHnz6na7XUtT3HADUZszZ?amount=0.0264)

Thanks, it should create time till the end of the year. (Unless meet unexpected problem ...)

[![](https://raw.githubusercontent.com/mb3h/cmdg/master/0.17424BTC.png)](bitcoin:3KVrnSbvZx7x8qHnz6na7XUtT3HADUZszZ?amount=0.17424)

Thanks your investment. If modifying TODO priority, implement new feature, etc., something is, please send mail and say it.

bitcoin:3KVrnSbvZx7x8qHnz6na7XUtT3HADUZszZ


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

README Updated on:2019-11-25
