# okay so i've flattened out the build stuff here.
# this is really for the best since so many things depend
# on each other now.. --androsyn

AUTOMAKE_OPTIONS = foreign subdir-objects 

prefix		= @prefix@
libcoredir 	= @libdir@
modulesdir	= @moduledir@/autoload
coredir		= @moduledir@
servicesdir	= @moduledir@/autoload

AM_CFLAGS = $(WARNFLAGS)

ircd_SOURCES = main.c
bin_PROGRAMS = ircd

if MINGW
EXTRA_FLAGS = -no-undefined -Wl,--enable-runtime-pseudo-reloc -export-symbols-regex '*'
endif

if !STATIC_MODULES

SUBDIRS = libratbox libltdl src modules extensions tools doc help bandb ssld

ircd_LDADD = libratbox/src/libratbox.la src/libcore.la $(LIBLTDL)
ircd_LDFLAGS = $(EXTRA_FLAGS) -dlopen self

else

SUBDIRS = libratbox libltdl modules src tools doc help bandb ssld
ircd_LDADD = libratbox/src/libratbox.la modules/libmodules.la src/libcore.la modules/static_modules.o $(LIBLTDL) $(DLOPEN)

endif

BUILT_SOURCES = include/serno.h
nodist_include_HEADERS = include/serno.h

include/serno.h:
	@if [ -d .hg ] ; then \
		revh=`hg parents --template '{date|shortdate}_{node|short}' 2>/dev/null | sed -e s/-//g -e s/_/-/`;\
		[ -z "$$revh" ] || echo '#define SERNO "'$$revh'"' >include/serno.h ; \
	fi
	@[ -f include/serno.h ] || echo '#define SERNO "unknown"' >include/serno.h
