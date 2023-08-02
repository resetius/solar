All: solar.exe euler.exe verlet.exe

clean:
		rm -f *.o *.exe

solar.exe: solar.o Makefile
		$(CC) $< $(CFLAGS) `pkg-config --libs gtk4,gio-2.0` -lm -o $@

euler.exe: euler.o Makefile
		$(CC) $< $(CFLAGS) -lm -o $@

verlet.exe: verlet.o Makefile
		$(CC) $< $(CFLAGS) -lm -o $@

%.o: %.c Makefile
		$(CC) -g -Wall $(CFLAGS) -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED `pkg-config --cflags gtk4,gio-2.0` -c $< -o $@
