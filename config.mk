PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

CFLAGS = -c -std=c99 -Wall -Wno-comment

CFLAGS += $(shell pkg-config --cflags xcb xcb-cursor)
LDFLAGS += $(shell pkg-config --libs xcb xcb-cursor)

CC ?= clang
