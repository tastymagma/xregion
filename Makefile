include config.mk


xregion: xregion.o
	$(CC) $? $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $?

.PHONY: clean install uninstall

clean:
	rm -f *.o xregion

install: xregion
	install -m 755 xregion $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/xregion
