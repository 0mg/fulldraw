# exe name
all = fulldraw.exe

# Library names
LIBS = kernel32 user32 gdi32 gdiplus comdlg32 ucrt

# GCC
GCCFLAGS = $(AR:%=-o $@ -DUNICODE -mwindows -nostartfiles -s ${LIBS:%=-l%})
GCRC = $(AR:%=windres)
GCRFLAGS = $(AR:%=-o $*.o)
GCRM = $(AR:%=rm -f)

# VS
_LIBS = $(LIBS) %
_VSLIBS = $(_LIBS: =.lib )
_VSFLAGS = /utf-8 /DUNICODE /Ddev /O2 /fp:fast /link $(_VSLIBS:%=) /ENTRY:__start__
_VSRC = rc
_VSRFLAGS = /fo $*.o /Ddev
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

$(all): $(all:.exe=.o)
	$(CC) $*.o $*.cpp $(CFLAGS)

$(all:.exe=.o): $(all:.exe=.rc)
	$(RC) $(RFLAGS) $*.rc

clean:
	$(RM) *.obj
