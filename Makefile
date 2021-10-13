
MODULES = modules/afb/afb-qjs.so

ALL = $(MODULES) afb-jscli

all: $(ALL)

DESTDIR ?=
PREFIX ?= /usr/local

CFLAGS += -g

DATADIR ?= $(DESTDIR)$(PREFIX)/share
LIBDIR ?= $(DESTDIR)$(PREFIX)/share

RPATH = $(DESTDIR)$(PREFIX)/$$LIB:$$ORIGIN

modules/afb/afb-qjs.so: modules/afb/afb-qjs.c modules/afb/afbwsj1-qjs.c modules/afb/afbwsapi-qjs.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $^ -lafbcli -lsystemd

quickjs/libquickjs.a:
	$(MAKE) -C quickjs prefix=$(PREFIX) CONFIG_BIGNUM= libquickjs.a

quickjs/.obj/repl.o:
	$(MAKE) -C quickjs prefix=$(PREFIX) CONFIG_BIGNUM= .obj/repl.o

afb-jscli: afb-jscli.c quickjs/libquickjs.a
	$(CC) $(CFLAGS) -rdynamic -o $@ $^ -lm -ldl -lpthread

clean:
	rm -f $(ALL)
	$(MAKE) -C quickjs clean
