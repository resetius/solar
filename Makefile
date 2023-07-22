
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
	COMPILER?=gcc
endif
ifeq ($(UNAME_S),Darwin)
	COMPILER?=gcc
endif
ifneq (,$(findstring CYGWIN,$(UNAME_S)))
	COMPILER?=x86_64-w64-mingw32-gcc
	PKG_CONFIG_LIBDIR:=/usr/x86_64-w64-mingw32/sys-root/mingw/lib/pkgconfig
	export PKG_CONFIG_LIBDIR
endif

COMPILER ?= $(CC)

All: solar.exe

clean:
		rm -f *.o *.exe 

solar.exe: solar.o Makefile
		$(COMPILER) solar.o $(CFLAGS) `pkg-config --libs gtk4` -lm -o $@

%.exe: %.o Makefile
		$(COMPILER) $< $(CFLAGS) `pkg-config --libs gtk4` -lm -o $@

%.o: %.c Makefile
		$(COMPILER) -g -Wall $(CFLAGS) -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED `pkg-config --cflags gtk4` -c $< -o $@
