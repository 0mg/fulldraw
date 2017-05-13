# exe name
all = fulldraw.exe

# Library names
LIBS = kernel32 shell32 user32 gdi32 ole32 gdiplus

# GCC
GCCFLAGS = $(AR:%=-o $@ -DUNICODE -mwindows -nostartfiles -s ${LIBS:%=-l%})
GCRC = $(AR:%=windres)
GCRFLAGS = $(AR:%=-o $*.o)
GCRM = $(AR:%=rm -f)

# VS
_LIBS = $(LIBS) %
_VSLIBS = $(_LIBS: =.lib )
#_VSFLAGS = /DUNICODE /link $(_VSLIBS:%=)
#_VSFLAGS = /DUNICODE /MD /link /ENTRY:__start__ $(_VSLIBS:%=)
_VSFLAGS = /DUNICODE /Ddev /MD /link /ENTRY:__start__ $(_VSLIBS:%=)
_VSRC = rc
_VSRFLAGS = /fo $*.o
_VSRM = del /f
VSFLAGS = $(_VSFLAGS:%=)
VSRC = $(_VSRC:%=)
VSRFLAGS = $(_VSRFLAGS:%=)
VSRM = $(_VSRM:%=)

# GCC or VS
CFLAGS = $(GCCFLAGS) $(VSFLAGS)
RC = $(GCRC) $(VSRC)
RFLAGS = $(GCRFLAGS) $(VSRFLAGS)
RM = $(GCRM) $(VSRM)

# main
.PHONY: all
all: $(all) clean

.SUFFIXES: .exe .o .rc .cpp

$(all):
	$(CC) $*.cpp $(CFLAGS)

clean:
	$(RM) *.obj
