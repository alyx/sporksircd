# XXX fix this for static modules

prefix          = @prefix@
libcoredir      = @libdir@
modulesdir      = @moduledir@/autoload
coredir         = @moduledir@
servicesdir     = @moduledir@/autoload
libmodulesdir   = @moduledir@
extensionsdir	= @moduledir@/unsupported

INCLUDES = -I../include -I../libratbox/include $(INCLTDL)
AM_CFLAGS=$(WARNFLAGS)

SRCS = \
hints.c		\
m_clearchan.c	\
m_forcequit.c

ALL_SRCS = $(SRCS)
S_OBJS = $(ALL_SRCS:.c=.la)
S_CORE_OBJS = $(CORE_SRCS:.c=.la)
ST_OBJS = $(ALL_SRCS:.c=.lo) $(CORE_SRCS:.c=.lo)
SUFFIXES = .c .so .lo .la

# Silent build rubbish --Elizabeth

AM_V_CC = $(am__v_CC_$(V))
am__v_CC_ = $(am__v_CC_$(AM_DEFAULT_VERBOSITY))
am__v_CC_0 = @echo "  CC    " $@;

AM_V_CCLD = $(am__v_CCLD_$(V))
am__v_CCLD_ = $(am__v_CCLD_$(AM_DEFAULT_VERBOSITY))
am__v_CCLD_0 = @echo "  CCLD  " $@;

AM_V_lt = $(am__v_lt_$(V))
am__v_lt_ = $(am__v_lt_$(AM_DEFAULT_VERBOSITY))
am__v_lt_0 = --silent

.c.lo:
	$(AM_V_CC)$(LIBTOOL) $(AM_V_lt) --mode=compile $(CC) -prefer-pic -DHAVE_CONFIG_H $(INCLUDES) $(AM_CPPFLAGS) $(CFLAGS) -c $< -o $@

.lo.la:
	$(AM_V_CCLD)$(LIBTOOL) $(AM_V_lt) --mode=link  $(CC) -rpath $(moduledir) -module -avoid-version $(CFLAGS) -o $@ $< $(LIBS) $(LDFLAGS)

all: $(S_OBJS) $(S_CORE_OBJS)

clean-generic:
	$(LIBTOOL) $(AM_V_lt) --mode=clean $(RB_RM) -f $(S_OBJS) $(S_CORE_OBJS)  $(ST_OBJS)

install_modules: $(S_OBJS)
	-@@$(MKDIR_P) $(extensionsdir); \
	for x in $(S_OBJS); do $(SHELL) ../install-mod.sh $$x $(extensionsdir);done

#install-mkdirs:
#	@echo "ircd: setting up modular directory structure"
#	-@if test ! -d $(DESTDIR)$(moduledir); then \
#		mkdir $(DESTDIR)$(moduledir); \
#	fi
#	-@if test -d $(DESTDIR)$(moduledir)-old; then \
#		rm -rf $(DESTDIR)$(moduledir)-old; \
#	fi
#	-@if test -d $(DESTDIR)$(moduledir); then \
#		echo "ircd: backing up modules"; \
#		mv $(DESTDIR)$(moduledir) $(DESTDIR)$(moduledir)-old; \
#	fi
#
#	@mkdir -p -m 755 $(DESTDIR)$(moduledir)

install: install_modules
