/* common definitions for 'patch' */

/* Copyright 1990-2024 Free Software Foundation, Inc.
   Copyright 1986, 1988 Larry Wall

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

#ifndef DEBUGGING
#define DEBUGGING 1
#endif

#include <config.h>

#include <attribute.h>
#include <c-ctype.h>
#include <intprops.h>
#include <progname.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* handy definitions */

#define strEQ(s1,s2) (!strcmp(s1, s2))
#define strnEQ(s1,s2,l) (!strncmp(s1, s2, l))
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))

/* typedefs */

typedef off_t lin;			/* must be signed */

#define LINENUM_MIN TYPE_MINIMUM (lin)
#define LINENUM_MAX TYPE_MAXIMUM (lin)

/* A description of an output file.  It may be temporary.  */
struct outfile
{
  /* Name of the file.  */
  char *name;

  /* Whether the file is intended to be temporary, and therefore
     should be cleaned up before exit, if it exists.  */
  bool temporary;

  /* Whether the file exists.  This is volatile so that a signal
     handler can use this struct reasonably reliably.  */
  bool volatile exists;
};

/* globals */

extern char *patchbuf;			/* general purpose buffer */
extern size_t patchbufsize;		/* allocated size of buf */

extern bool using_plan_a;		/* try to keep everything in memory */

extern char *inname;
extern char *outfile;
extern int inerrno;
extern int invc;
extern struct stat instat;
extern bool dry_run;
extern bool posixly_correct;

extern char const *origprae;
extern char const *origbase;
extern char const *origsuff;

extern struct outfile tmped;
extern struct outfile tmpin;
extern struct outfile tmppat;

#if DEBUGGING
extern int debug;
#else
# define debug 0
#endif
extern bool force;
extern bool batch;
extern bool noreverse_flag;
extern bool reverse_flag;
extern enum verbosity { DEFAULT_VERBOSITY, SILENT, VERBOSE } verbosity;
extern bool skip_rest_of_patch;
extern int strippath;
extern bool canonicalize_ws;
extern int patch_get;
extern bool set_time;
extern bool set_utc;
extern bool follow_symlinks;

enum diff
  {
    NO_DIFF,
    CONTEXT_DIFF,
    NORMAL_DIFF,
    ED_DIFF,
    NEW_CONTEXT_DIFF,
    UNI_DIFF,
    GIT_BINARY_DIFF
  };

extern enum diff diff_type;

extern char *revision;			/* prerequisite revision, if any */

_Noreturn void fatal_exit (int);

#if HAVE_FSEEKO
  typedef off_t file_offset;
# define file_seek fseeko
# define file_tell ftello
#else
  typedef long file_offset;
# define file_seek fseek
# define file_tell ftell
#endif

#if ! (HAVE_GETEUID || defined geteuid)
# if ! (HAVE_GETUID || defined getuid)
#  define geteuid() (-1)
# else
#  define geteuid() getuid ()
# endif
#endif

#ifdef HAVE_SETMODE_DOS
  extern int binary_transput;	/* O_BINARY if binary i/o is desired */
#else
# define binary_transput 0
#endif

/* Disable the CR stripping heuristic?  */
extern bool no_strip_trailing_cr;

#ifndef NULL_DEVICE
#define NULL_DEVICE "/dev/null"
#endif

#ifndef TTY_DEVICE
#define TTY_DEVICE "/dev/tty"
#endif

/* Output stream state.  */
struct outstate
{
  FILE *ofp;
  bool after_newline;
  bool zero_output;
};

/* offset in the input and output at which the previous hunk matched */
extern lin in_offset;
extern lin out_offset;

/* how many input lines have been irretractably output */
extern lin last_frozen_line;

bool copy_till (struct outstate *, lin);
bool similar (char const *, size_t, char const *, size_t) ATTRIBUTE_PURE;

#ifdef ENABLE_MERGE
enum conflict_style { MERGE_MERGE, MERGE_DIFF3 };
extern enum conflict_style conflict_style;

bool merge_hunk (int hunk, struct outstate *, lin where, bool *);
#else
# define merge_hunk(hunk, outstate, where, somefailed) false
#endif
