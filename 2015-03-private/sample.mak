tgtfname = sample

# must need -defaultlib. If seek order is libcpmt(d) > libcmt(d), ASSERT happened on 'delete'.
# (because) It seems 'operator delete' of libcpmtd version is elder from libcmt version, _free_dbg() argument is _NORMAL_BLOCK fixed. (watched on VC6)

!IFNDEF DEBUG # DEBUG build ON
optrc = -d "NDEBUG"
optcl = $(optcl) -O2 -MT -DNDEBUG -FAcs
optlink = $(optlink) -map -defaultlib:LIBCMT
outfname = $(tgtfname).exe
!ELSE
optrc = -d "_DEBUG"
optcl = $(optcl) -Od -MTd -D_DEBUG
optlink = $(optlink) -debug -defaultlib:LIBCMTD
outfname = $(tgtfname)D.exe
!ENDIF

#optcl = -D_WINDOWS $(optcl)
#subsys = windows
optcl = -D_CONSOLE $(optcl)
subsys = console,4.10

depends = .obj
target = $(target) \
    $(tgtfname).c

optcl = -TP $(optcl)

.exe: $(depends)
    link -nologo -release -subsystem:$(subsys) -machine:ix86 \
    -out:$(outfname) \
    $(optlink) \
    kernel32.lib \
    $(tgtfname).obj

.obj:
    cl -nologo -c -W4 \
    -DWIN32 \
    $(optcl) -EHsc \
    $(target)

clean:
    -@del/q $(tgtfname).obj        2> NUL

cleand:
    -@del/q $(tgtfname)D.exe       2> NUL
    -@del/q $(tgtfname)D.pdb       2> NUL
    -@del/q $(tgtfname).map        2> NUL
    -@del/q $(tgtfname).ncb        2> NUL
    -@del/q $(tgtfname).opt        2> NUL
    -@del/q $(tgtfname).aps        2> NUL
    -@del/q $(tgtfname).cod        2> NUL
#    -@rmdir/s/q Debug
#    -@rmdir/s/q Release
