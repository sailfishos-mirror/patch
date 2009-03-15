# xalloc.m4 serial 2
dnl Copyright (C) 2002-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

AC_DEFUN([gl_XALLOC],
[
  gl_PREREQ_XMALLOC
  gl_PREREQ_XSTRDUP
])

# Prerequisites of lib/xmalloc.c.
AC_DEFUN([gl_PREREQ_XMALLOC], [
  AC_REQUIRE([AC_HEADER_STDC])
  AC_REQUIRE([jm_FUNC_MALLOC])
  AC_REQUIRE([jm_FUNC_REALLOC])
])

# Prerequisites of lib/xstrdup.c.
AC_DEFUN([gl_PREREQ_XSTRDUP], [
  :
])
