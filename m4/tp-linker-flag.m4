dnl A version of AS_COMPILER_FLAG that supports linker flags
dnl Based on:

dnl as-compiler-flag.m4 0.1.0
dnl autostars m4 macro for detection of compiler flags
dnl David Schleef <ds@schleef.org>
dnl $Id: as-compiler-flag.m4,v 1.1 2005/06/18 18:02:46 burgerman Exp $

dnl TP_LINKER_FLAG(LDFLAGS, ACTION-IF-ACCEPTED, [ACTION-IF-NOT-ACCEPTED])
dnl Tries to compile with the given LDFLAGS.
dnl
dnl Runs ACTION-IF-ACCEPTED if the compiler/linker for the currently selected
dnl AC_LANG can compile with the flags, and ACTION-IF-NOT-ACCEPTED otherwise.
dnl
dnl Note that LDFLAGS are passed to the linker via the compiler, so you
dnl should check for -Wl,--no-add-needed rather than --no-add-needed.

AC_DEFUN([TP_LINKER_FLAG],
[
  AC_MSG_CHECKING([to see if compiler/linker understand $1])

  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS $1"

  AC_COMPILE_IFELSE(AC_LANG_SOURCE([]), [flag_ok=yes], [flag_ok=no])

  LDFLAGS="$save_LDFLAGS"

  if test "X$flag_ok" = Xyes ; then
    $2
    true
  else
    $3
    true
  fi
  AC_MSG_RESULT([$flag_ok])
])

dnl TP_ADD_LINKER_FLAG(VARIABLE, LDFLAGS)
dnl Append LDFLAGS to VARIABLE if the linker supports them.
AC_DEFUN([TP_ADD_LINKER_FLAG],
[
  TP_LINKER_FLAG([$2], [$1="[$]$1 $2"])
])
