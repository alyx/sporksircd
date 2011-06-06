# XXX fix this for static modules

prefix          = @prefix@
libcoredir      = @libdir@
modulesdir      = @moduledir@/autoload
coredir         = @moduledir@
servicesdir     = @moduledir@/autoload
libmodulesdir   = @moduledir@
extensionsdir	= @moduledir@/extensions

INCLUDES = -I../include -I../libratbox/include $(INCLTDL)
AM_CFLAGS=$(WARNFLAGS)

SRCS = \
chm_adminonly.c		\
chm_blockcaps.c		\
chm_censor.c		\
chm_censor_operonly.c	\
chm_nocolour.c		\
chm_noctcp.c		\
chm_nonotices.c		\
chm_norepeat.c		\
chm_operonly.c		\
chm_operonly_compat.c	\
chm_quietunreg_compat.c	\
chm_sslonly.c		\
chm_sslonly_compat.c	\
createauthonly.c	\
createoperonly.c	\
example_module.c	\
extb_account.c		\
extb_canjoin.c		\
extb_channel.c		\
extb_extgecos.c		\
extb_oper.c		\
extb_realname.c		\
extb_server.c		\
extb_ssl.c		\
force_user_invis.c	\
hurt.c			\
m_adminwall.c		\
m_chgflags.c		\
m_cycle.c		\
m_findforwards.c	\
m_forcejoin.c		\
m_forcenick.c		\
m_forcepart.c		\
m_identify.c		\
m_mkpasswd.c		\
m_oaccept.c		\
m_ojoin.c		\
m_okick.c		\
m_olist.c		\
m_omode.c		\
m_opme.c		\
m_remove.c		\
m_roleplay.c		\
m_sendbans.c		\
m_webirc.c		\
no_locops.c		\
no_oper_invis.c		\
sno_channeljoin.c	\
sno_farconnect.c	\
sno_globalkline.c	\
sno_globaloper.c	\
sno_whois.c		\
spy_admin_notice.c	\
spy_info_notice.c	\
spy_links_notice.c	\
spy_motd_notice.c	\
spy_stats_notice.c	\
spy_stats_p_notice.c	\
spy_trace_notice.c	\
usm_botmode.c		\
usm_ip_cloaking.c	\
usm_override.c


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
