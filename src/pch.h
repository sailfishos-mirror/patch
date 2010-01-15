/* reading patches */

/* Copyright (C) 1986, 1987, 1988 Larry Wall

   Copyright (C) 1990, 1991, 1992, 1993, 1997, 1998, 1999, 2000, 2001,
   2002, 2003, 2009, 2010 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

enum nametype { OLD, NEW, INDEX, NONE };

LINENUM pch_end (void);
LINENUM pch_first (void);
LINENUM pch_hunk_beg (void);
char const *pch_c_function (void);
char const * pch_timestr (bool which);
LINENUM pch_newfirst (void);
LINENUM pch_prefix_context (void);
LINENUM pch_ptrn_lines (void);
LINENUM pch_repl_lines (void);
LINENUM pch_suffix_context (void);
bool pch_swap (void);
bool pch_write_line (LINENUM, FILE *);
bool there_is_another_patch (bool);
char *pfetch (LINENUM);
char pch_char (LINENUM);
int another_hunk (enum diff, bool);
int pch_says_nonexistent (bool);
size_t pch_line_len (LINENUM);
const char *pch_name(enum nametype);
time_t pch_timestamp (bool);
void do_ed_script (FILE *);
void open_patch_file (char const *);
void re_patch (void);
void set_hunkmax (void);
void pch_normalize (enum diff);