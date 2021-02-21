
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
		rm -f *.o *.exe data_glade.c

solar.exe: solar.o data_glade.o Makefile
		$(COMPILER) solar.o data_glade.o $(CFLAGS) `pkg-config --libs gtk+-3.0` -lm -o $@

data_glade.c: data.xml data.glade theme.css solar_main.glade
		glib-compile-resources data.xml --target data_glade.c --generate-source

%.exe: %.o Makefile
		$(COMPILER) $< $(CFLAGS) `pkg-config --libs gtk+-3.0` -lm -o $@

%.o: %.c Makefile
		$(COMPILER) -g -Wall $(CFLAGS) `pkg-config --cflags gtk+-3.0` -c $< -o $@
