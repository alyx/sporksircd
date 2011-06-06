# $Id: acinclude.m4 23020 2006-09-01 18:20:19Z androsyn $ - aclocal.m4 - Autoconf fun...
AC_DEFUN([AC_DEFINE_DIR], [
  test "x$prefix" = xNONE && prefix="$ac_default_prefix"
  test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
  ac_define_dir=`eval echo [$]$2`
  ac_define_dir=`eval echo [$]ac_define_dir`
  $1="$ac_define_dir"
  AC_SUBST($1)
  ifelse($3, ,
    AC_DEFINE_UNQUOTED($1, "$ac_define_dir"),
    AC_DEFINE_UNQUOTED($1, "$ac_define_dir", $3))
])

AC_DEFUN([AC_SUBST_DIR], [
        ifelse($2,,,$1="[$]$2")
        $1=`(
            test "x$prefix" = xNONE && prefix="$ac_default_prefix"
            test "x$exec_prefix" = xNONE && exec_prefix="${prefix}"
            eval echo \""[$]$1"\"
        )`
        AC_SUBST($1)
])


dnl IPv6 support macros..pretty much swiped from wget

dnl RB_PROTO_INET6
 
AC_DEFUN([RB_PROTO_INET6],[
  AC_CACHE_CHECK([for INET6 protocol support], [rb_cv_proto_inet6],[
    AC_TRY_CPP([
#include <sys/types.h>
#include <sys/socket.h>

#ifndef PF_INET6
#error Missing PF_INET6
#endif
#ifndef AF_INET6
#error Mlssing AF_INET6
#endif
    ],[
      rb_cv_proto_inet6=yes
    ],[
      rb_cv_proto_inet6=no
    ])
  ])  

  if test "X$rb_cv_proto_inet6" = "Xyes"; then :
    $1
  else :
    $2  
  fi    
])      


AC_DEFUN([RB_TYPE_STRUCT_SOCKADDR_IN6],[
  rb_have_sockaddr_in6=
  AC_CHECK_TYPES([struct sockaddr_in6],[
    rb_have_sockaddr_in6=yes
  ],[
    rb_have_sockaddr_in6=no
  ],[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
  ])

  if test "X$rb_have_sockaddr_in6" = "Xyes"; then :
    $1
  else :
    $2
  fi
])


AC_DEFUN([RB_CHECK_TIMER_CREATE],
  [AC_CACHE_CHECK([for a working timer_create(CLOCK_REALTIME)], 
    [rb__cv_timer_create_works],
    [AC_TRY_RUN([
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
int main(int argc, char *argv[])
{
#if HAVE_TIMER_CREATE
    struct sigevent ev;
    timer_t timer;
    ev.sigev_notify = SIGEV_SIGNAL;
    ev.sigev_signo  = SIGVTALRM;
    if (timer_create(CLOCK_REALTIME, &ev, &timer) != 0) {
       return 1;
    }
#else
    return 1;
#endif
    return 0;
}
     ],
     [rb__cv_timer_create_works=yes],
     [rb__cv_timer_create_works=no])
  ])
case $rb__cv_timer_create_works in
    yes) AC_DEFINE([USE_TIMER_CREATE], 1, 
                   [Define to 1 if we can use timer_create(CLOCK_REALTIME,...)]);;
esac
])



AC_DEFUN([RB_CHECK_TIMERFD_CREATE],
  [AC_CACHE_CHECK([for a working timerfd_create(CLOCK_REALTIME)], 
    [rb__cv_timerfd_create_works],
    [AC_TRY_RUN([
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIMERFD_H
#include <sys/timerfd.h>
#endif
int main(int argc, char *argv[])
{
#if defined(HAVE_TIMERFD_CREATE) && defined(HAVE_SYS_TIMERFD_H)
    if (timerfd_create(CLOCK_REALTIME, 0) < 0) {
       return 1;
    }
#else
    return 1;
#endif
    return 0;
}
     ],
     [rb__cv_timerfd_create_works=yes],
     [rb__cv_timerfd_create_works=no])
  ])
case $rb__cv_timerfd_create_works in
    yes) AC_DEFINE([USE_TIMERFD_CREATE], 1, 
                   [Define to 1 if we can use timerfd_create(CLOCK_REALTIME,...)]);;
esac
])

dnl
dnl Useful macros for autoconf to check for ssp-patched gcc
dnl 1.0 - September 2003 - Tiago Sousa <mirage@kaotik.org>
dnl 1.1 - August 2006 - Ted Percival <ted@midg3t.net>
dnl     * Stricter language checking (C or C++)
dnl     * Adds GCC_STACK_PROTECT_LIB to add -lssp to LDFLAGS as necessary
dnl     * Caches all results
dnl     * Uses macros to ensure correct ouput in quiet/silent mode
dnl 1.2 - April 2007 - Ted Percival <ted@midg3t.net>
dnl     * Added GCC_STACK_PROTECTOR macro for simpler (one-line) invocation
dnl     * GCC_STACK_PROTECT_LIB now adds -lssp to LIBS rather than LDFLAGS
dnl
dnl About ssp:
dnl GCC extension for protecting applications from stack-smashing attacks
dnl http://www.research.ibm.com/trl/projects/security/ssp/
dnl
dnl Usage:
dnl Most people will simply call GCC_STACK_PROTECTOR.
dnl If you only use one of C or C++, you can save time by only calling the
dnl macro appropriate for that language. In that case you should also call
dnl GCC_STACK_PROTECT_LIB first.
dnl
dnl GCC_STACK_PROTECTOR
dnl Tries to turn on stack protection for C and C++ by calling the following
dnl three macros with the right languages.
dnl
dnl GCC_STACK_PROTECT_CC
dnl checks -fstack-protector with the C compiler, if it exists then updates
dnl CFLAGS and defines ENABLE_SSP_CC
dnl
dnl GCC_STACK_PROTECT_CXX
dnl checks -fstack-protector with the C++ compiler, if it exists then updates
dnl CXXFLAGS and defines ENABLE_SSP_CXX
dnl
dnl GCC_STACK_PROTECT_LIB
dnl adds -lssp to LIBS if it is available
dnl ssp is usually provided as part of libc, but was previously a separate lib
dnl It does not hurt to add -lssp even if libc provides SSP - in that case
dnl libssp will simply be ignored.
dnl

AC_DEFUN([GCC_STACK_PROTECT_LIB],[
  AC_CACHE_CHECK([whether libssp exists], ssp_cv_lib,
    [ssp_old_libs="$LIBS"
     LIBS="$LIBS -lssp"
     AC_TRY_LINK(,, ssp_cv_lib=yes, ssp_cv_lib=no)
     LIBS="$ssp_old_libs"
    ])
  if test $ssp_cv_lib = yes; then
    LIBS="$LIBS -lssp"
  fi
])

AC_DEFUN([GCC_STACK_PROTECT_CC],[
  AC_LANG_ASSERT(C)
  if test "X$CC" != "X"; then
    AC_CACHE_CHECK([whether ${CC} accepts -fstack-protector],
      ssp_cv_cc,
      [ssp_old_cflags="$CFLAGS"
       CFLAGS="$CFLAGS -fstack-protector"
       AC_TRY_COMPILE(,, ssp_cv_cc=yes, ssp_cv_cc=no)
       CFLAGS="$ssp_old_cflags"
      ])
    if test $ssp_cv_cc = yes; then
      CFLAGS="$CFLAGS -fstack-protector"
      AC_DEFINE([ENABLE_SSP_CC], 1, [Define if SSP C support is enabled.])
    fi
  fi
])

AC_DEFUN([GCC_STACK_PROTECT_CXX],[
  AC_LANG_ASSERT(C++)
  if test "X$CXX" != "X"; then
    AC_CACHE_CHECK([whether ${CXX} accepts -fstack-protector],
      ssp_cv_cxx,
      [ssp_old_cxxflags="$CXXFLAGS"
       CXXFLAGS="$CXXFLAGS -fstack-protector"
       AC_TRY_COMPILE(,, ssp_cv_cxx=yes, ssp_cv_cxx=no)
       CXXFLAGS="$ssp_old_cxxflags"
      ])
    if test $ssp_cv_cxx = yes; then
      CXXFLAGS="$CXXFLAGS -fstack-protector"
      AC_DEFINE([ENABLE_SSP_CXX], 1, [Define if SSP C++ support is enabled.])
    fi
  fi
])

AC_DEFUN([GCC_STACK_PROTECTOR],[
  GCC_STACK_PROTECT_LIB

  AC_LANG_PUSH([C])
  GCC_STACK_PROTECT_CC
  AC_LANG_POP([C])

  AC_LANG_PUSH([C++])
  GCC_STACK_PROTECT_CXX
  AC_LANG_POP([C++])
])

dnl SYNOPSIS
dnl
dnl   AX_LIB_SOCKET_NSL
dnl
dnl DESCRIPTION
dnl
dnl   This macro figures out what libraries are required on this platform to
dnl   link sockets programs.
dnl
dnl   The common cases are not to need any extra libraries, or to need
dnl   -lsocket and -lnsl. We need to avoid linking with libnsl unless we need
dnl   it, though, since on some OSes where it isn't necessary it will totally
dnl   break networking. Unisys also includes gethostbyname() in libsocket but
dnl   needs libnsl for socket().
dnl
dnl LICENSE
dnl
dnl   Copyright (c) 2008 Russ Allbery <rra@stanford.edu>
dnl   Copyright (c) 2008 Stepan Kasal <kasal@ucw.cz>
dnl   Copyright (c) 2008 Warren Young <warren@etr-usa.com>
dnl
dnl   Copying and distribution of this file, with or without modification, are
dnl   permitted in any medium without royalty provided the copyright notice
dnl   and this notice are preserved.

AU_ALIAS([LIB_SOCKET_NSL], [AX_LIB_SOCKET_NSL])
AC_DEFUN([AX_LIB_SOCKET_NSL],
[
  AC_SEARCH_LIBS([gethostbyname], [nsl])
  AC_SEARCH_LIBS([socket], [socket], [], [
    AC_CHECK_LIB([socket], [socket], [LIBS="-lsocket -lnsl $LIBS"],
    [], [-lnsl])])
])

dnl found in a pastebin somewhere... http://paste.lisp.org/display/48866
AC_DEFUN([AX_CHECK_SUNCC],
[
	AC_MSG_CHECKING([if $CC is Sun CC])

	if test ! -z $($CC -V 2>&1 | grep ': Sun C') ; then
		HAS_SUNCC="yes"
	else
		HAS_SUNCC="no"
	fi

	AC_MSG_RESULT([$HAS_SUNCC])
]
)