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

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lib src tests
dist_man1_MANS = patch.man
EXTRA_DIST = \
	ChangeLog-2011 \
	cfg.mk \
	m4/gnulib-cache.m4 \
	m4/mkdir.m4 \
	m4/setmode.m4 \
	bootstrap \
	pc

if ALPHA_VERSION
  EXTRA_DIST += README-alpha
  GNU_SERVER = alpha.gnu.org
  RELEASE_TYPE = alpha
else
  GNU_SERVER = ftp.gnu.org
  RELEASE_TYPE = stable
endif

EXTRA_DIST += $(top_srcdir)/.version
BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook: gen-ChangeLog
	echo $(VERSION) > $(distdir)/.tarball-version

gen_start_date = 2011-02-22
.PHONY: gen-ChangeLog
gen-ChangeLog:
	$(AM_V_GEN)if test -d .git; then				\
	  $(top_srcdir)/build-aux/gitlog-to-changelog			\
	    --since=$(gen_start_date) > $(distdir)/cl-t;		\
	  rm -f $(distdir)/ChangeLog;					\
	  mv $(distdir)/cl-t $(distdir)/ChangeLog;			\
	fi
