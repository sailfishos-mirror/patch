# Copyright 1989-2025 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

## Process this file with automake to produce Makefile.in.

AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)

bin_PROGRAMS = patch
patch_SOURCES = \
	bestmatch.h \
	common.h \
	inp.c \
	inp.h \
	patch.c \
	pch.c \
	pch.h \
	safe.c \
	safe.h \
	util.c \
	util.h \
	version.c \
	version.h \
	list.h

AM_CPPFLAGS = -I$(top_builddir)/lib -I$(top_srcdir)/lib
patch_LDADD = $(LDADD) $(top_builddir)/lib/libpatch.a \
 $(CLOCK_TIME_LIB) $(EUIDACCESS_LIBGEN) $(GETRANDOM_LIB) \
 $(HARD_LOCALE_LIB) $(LIBINTL) $(MBRTOWC_LIB) $(SETLOCALE_NULL_LIB) \
 $(LIB_XATTR)

if ENABLE_MERGE
  patch_SOURCES += merge.c
  AM_CPPFLAGS += -DENABLE_MERGE
endif
