INSTALL_BIN	= @INSTALL_PROGRAM@
bindir		= @bindir@

bin_PROGRAMS = mkpasswd viconf
AM_CFLAGS=$(WARNFLAGS)

INCLUDES = $(DEFAULT_INCLUDES) -I../libratbox/include -I.


mkpasswd_SOURCES = mkpasswd.c
mkpasswd_LDADD = ../libratbox/src/libratbox.la

viconf_SOURCES = viconf.c
viconf_LDADD = ../libratbox/src/libratbox.la

install-mkdirs:
	-@if test ! -d $(DESTDIR)$(confdir); then \
		echo "mkdir -p $(confdir)"; \
		mkdir $(DESTDIR)$(confdir); \
	fi

install-genssl: install-mkdirs
	$(INSTALL_BIN) genssl.sh $(DESTDIR)$(bindir)

install: install-mkdirs install-binPROGRAMS install-genssl
