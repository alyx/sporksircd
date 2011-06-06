AUTOMAKE_OPTIONS = foreign

prefix		= @prefix@
libcoredir	= @libdir@

AM_YFLAGS = -d
AM_CFLAGS = $(WARNFLAGS)
INCLUDES = -I../include -I../libratbox/include -I../libltdl @SSL_INCLUDES@

BUILT_SOURCES = version.c

version.c: version.c.SH ../CREDITS
	$(SHELL) ./version.c.SH
	$(CP) version.c version.c.last
                
version.lo: version.c ../include/patchlevel.h ../include/serno.h
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

if MINGW
EXTRA_FLAGS = -no-undefined -Wl,--enable-runtime-pseudo-reloc -export-symbols-regex '*'
endif

libcore_la_SOURCES =			\
	bandbi.c			\
	blacklist.c			\
	cache.c				\
	channel.c			\
	chmode.c			\
	class.c				\
	client.c			\
	extban.c			\
	getopt.c			\
	hash.c				\
	hook.c				\
	hostmask.c			\
	ircd.c				\
	ircd_signal.c			\
	list.c				\
	listener.c			\
	logger.c			\
	match.c				\
	modules.c			\
	monitor.c			\
	newconf.c			\
	numeric.c			\
	operhash.c			\
	packet.c			\
	parse.c				\
	privilege.c			\
	reject.c			\
	res.c				\
	reslib.c			\
	restart.c			\
	s_auth.c			\
	scache.c			\
	s_conf.c			\
	send.c				\
	s_newconf.c			\
	snomask.c			\
	s_serv.c			\
	sslproc.c			\
	substitution.c			\
	supported.c			\
	s_user.c			\
	tgchange.c			\
	whowas.c			\
	version.c			\
	ircd_parser.y			\
	ircd_lexer.l

libcore_LTLIBRARIES = libcore.la
libcore_la_LDFLAGS = $(EXTRA_FLAGS) -avoid-version

if MINGW
libcore_la_DEPENDENCIES = $(LIBLTDL) ../libratbox/src/libratbox.la
libcore_la_LIBADD = ../libratbox/src/libratbox.la $(LIBLTDL) @SSL_LIBS@
else
libcore_la_DEPENDENCIES = ../libratbox/src/libratbox.la
libcore_la_LIBADD = ../libratbox/src/libratbox.la $(LIBLTDL) @SSL_LIBS@
endif


install-exec-hook: install-libcoreLTLIBRARIES
	@RB_RM@ version.c

