/* reading patches */

/* $Id: pch.c,v 1.6 1997/04/07 01:07:00 eggert Exp $ */

/*
Copyright 1986, 1987, 1988 Larry Wall
Copyright 1990, 1991, 1992, 1993, 1997 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#define XTERN extern
#include <common.h>
#include <inp.h>
#include <util.h>
#undef XTERN
#define XTERN
#include <pch.h>

#define INITHUNKMAX 125			/* initial dynamic allocation size */

/* Patch (diff listing) abstract type. */

static FILE *pfp;			/* patch file pointer */
static off_t p_filesize;		/* size of the patch file */
static LINENUM p_first;			/* 1st line number */
static LINENUM p_newfirst;		/* 1st line number of replacement */
static LINENUM p_ptrn_lines;		/* # lines in pattern */
static LINENUM p_repl_lines;		/* # lines in replacement text */
static LINENUM p_end = -1;		/* last line in hunk */
static LINENUM p_max;			/* max allowed value of p_end */
static LINENUM p_prefix_context;	/* # of prefix context lines */
static LINENUM p_suffix_context;	/* # of suffix context lines */
static LINENUM p_input_line;		/* current line # from patch file */
static char **p_line;			/* the text of the hunk */
static size_t *p_len;			/* line length including \n if any */
static char *p_Char;			/* +, -, and ! */
static size_t hunkmax = INITHUNKMAX;	/* size of above arrays */
static int p_indent;			/* indent to patch */
static long p_base;			/* where to intuit this time */
static LINENUM p_bline;			/* line # of p_base */
static long p_start;			/* where intuit found a patch */
static LINENUM p_sline;			/* and the line number for it */
static LINENUM p_hunk_beg;		/* line number of current hunk */
static LINENUM p_efake = -1;		/* end of faked up lines--don't free */
static LINENUM p_bfake = -1;		/* beg of faked up lines */

static enum diff intuit_diff_type PARAMS ((void));
static size_t pget_line PARAMS ((int));
static size_t get_line PARAMS ((void));
static bool incomplete_line PARAMS ((void));
static bool grow_hunkmax PARAMS ((void));
static void next_intuit_at PARAMS ((long, LINENUM));
static void skip_to PARAMS ((long, LINENUM));

/* Prepare to look for the next patch in the patch file. */

void
re_patch()
{
    p_first = 0;
    p_newfirst = 0;
    p_ptrn_lines = 0;
    p_repl_lines = 0;
    p_end = -1;
    p_max = 0;
    p_indent = 0;
}

/* Open the patch file at the beginning of time. */

void
open_patch_file(filename)
     char const *filename;
{
    long file_pos = 0;
    struct stat st;
    if (!filename || !*filename || strEQ (filename, "-"))
      {
	if (fstat (STDIN_FILENO, &st) != 0)
	  pfatal ("fstat");
	if (S_ISREG (st.st_mode))
	  {
	    pfp = stdin;
	    file_pos = ftell (stdin);
	    filename = 0;
	  }
	else
	  {
	    size_t charsread;
	    pfp = fopen (TMPPATNAME, "w");
	    if (!pfp)
	      pfatal ("can't create %s", TMPPATNAME);
	    while ((charsread = fread (buf, 1, bufsize, stdin)) != 0)
	      if (fwrite (buf, 1, charsread, pfp) != charsread)
		write_fatal ();
	    if (ferror (stdin) || fclose (stdin) != 0)
	      read_fatal ();
	    if (fclose (pfp) != 0)
	      write_fatal ();
	    filename = TMPPATNAME;
	  }
      }
    if (filename) {
	pfp = fopen (filename, "r");
	if (!pfp)
	  pfatal ("patch file %s not found", filename);
	if (fstat (fileno (pfp), &st) != 0)
	  pfatal ("fstat");
    }
    p_filesize = st.st_size;
    next_intuit_at (file_pos, (LINENUM) 1);
    set_hunkmax();
}

/* Make sure our dynamically realloced tables are malloced to begin with. */

void
set_hunkmax()
{
    if (!p_line)
	p_line = (char **) malloc (hunkmax * sizeof *p_line);
    if (!p_len)
	p_len = (size_t *) malloc (hunkmax * sizeof *p_len);
    if (!p_Char)
	p_Char = malloc (hunkmax * sizeof *p_Char);
}

/* Enlarge the arrays containing the current hunk of patch. */

static bool
grow_hunkmax()
{
    hunkmax *= 2;
    assert (p_line && p_len && p_Char);
    if ((p_line = (char **) realloc (p_line, hunkmax * sizeof (*p_line)))
	&& (p_len = (size_t *) realloc (p_len, hunkmax * sizeof (*p_len)))
	&& (p_Char = realloc (p_Char, hunkmax * sizeof (*p_Char))))
      return TRUE;
    if (!using_plan_a)
      memory_fatal ();
    /* Don't free previous values of p_line etc.,
       since some broken implementations free them for us.
       Whatever is null will be allocated again from within plan_a (),
       of all places.  */
    return FALSE;
}

/* True if the remainder of the patch file contains a diff of some sort. */

bool
there_is_another_patch()
{
    if (p_base != 0 && p_base >= p_filesize) {
	if (verbosity == VERBOSE)
	    say ("done\n");
	return FALSE;
    }
    if (verbosity == VERBOSE)
	say ("Hmm...");
    diff_type = intuit_diff_type();
    if (diff_type == NO_DIFF) {
	if (verbosity == VERBOSE)
	  say (p_base
	       ? "  Ignoring the trailing garbage.\ndone\n"
	       : "  I can't seem to find a patch in there anywhere.\n");
	return FALSE;
    }
    if (verbosity == VERBOSE)
	say ("  %sooks like %s to me...\n",
	    (p_base == 0 ? "L" : "The next patch l"),
	    diff_type == UNI_DIFF ? "a unified diff" :
	    diff_type == CONTEXT_DIFF ? "a context diff" :
	    diff_type == NEW_CONTEXT_DIFF ? "a new-style context diff" :
	    diff_type == NORMAL_DIFF ? "a normal diff" :
	    "an ed script" );
    if (p_indent && verbosity != SILENT)
	say ("(Patch is indented %d space%s.)\n", p_indent, p_indent==1?"":"s");
    skip_to(p_start,p_sline);
    while (!inname) {
	if (force || batch) {
	    say ("No file to patch.  Skipping...\n");
	    skip_rest_of_patch = TRUE;
	    return TRUE;
	}
	ask ("File to patch: ");
	inname = fetchname (buf, 0);
	if (!inname) {
	    ask ("Skip this patch? [y] ");
	    if (*buf == 'n') {
		continue;
	    }
	    if (verbosity != SILENT)
		say ("Skipping patch...\n");
	    skip_rest_of_patch = TRUE;
	    return TRUE;
	}
    }
    return TRUE;
}

/* Determine what kind of diff is in the remaining part of the patch file. */

static enum diff
intuit_diff_type()
{
    register char *s;
    register char *t;
    register int indent;
    register long this_line = 0;
    register long previous_line;
    register long first_command_line = -1;
    LINENUM fcl_line = 0; /* Pacify `gcc -W'.  */
    register bool last_line_was_command = FALSE;
    register bool this_is_a_command = FALSE;
    register bool stars_last_line = FALSE;
    register bool stars_this_line = FALSE;
    enum nametype { OLD, NEW, INDEX, NONE } i;
    char *name[3];
    struct stat st[3];
    int stat_errno[3];
    register enum diff retval;

    name[OLD] = name[NEW] = name[INDEX] = 0;
    ok_to_create_file = FALSE;
    Fseek (pfp, p_base, SEEK_SET);
    p_input_line = p_bline - 1;
    for (;;) {
	previous_line = this_line;
	last_line_was_command = this_is_a_command;
	stars_last_line = stars_this_line;
	this_line = ftell(pfp);
	indent = 0;
	if (! pget_line (0)) {
	    if (first_command_line >= 0) {
					/* nothing but deletes!? */
		p_start = first_command_line;
		p_sline = fcl_line;
		retval = ED_DIFF;
		goto scan_exit;
	    }
	    else {
		p_start = this_line;
		p_sline = p_input_line;
		retval = NO_DIFF;
		goto scan_exit;
	    }
	}
	for (s = buf; *s == ' ' || *s == '\t' || *s == 'X'; s++) {
	    if (*s == '\t')
		indent = (indent + 8) & ~7;
	    else
		indent++;
	}
	for (t = s;  ISDIGIT (*t) || *t == ',';  t++)
	  continue;
	this_is_a_command = (ISDIGIT (*s) &&
	  (*t == 'd' || *t == 'c' || *t == 'a') );
	if (first_command_line < 0 && this_is_a_command) {
	    first_command_line = this_line;
	    fcl_line = p_input_line;
	    p_indent = indent;		/* assume this for now */
	}
	if (!stars_last_line && strnEQ(s, "*** ", 4))
	    name[OLD] = fetchname (s+4, strippath);
	else if (strnEQ(s, "--- ", 4))
	    name[NEW] = fetchname (s+4, strippath);
	else if (strnEQ(s, "+++ ", 4))
	    name[OLD] = fetchname (s+4, strippath); /* Swap with NEW below.  */
	else if (strnEQ(s, "Index:", 6))
	    name[INDEX] = fetchname (s+6, strippath);
	else if (strnEQ(s, "Prereq:", 7)) {
	    for (t = s + 7;  ISSPACE ((unsigned char) *t);  t++)
	      continue;
	    revision = t;
	    for (t = revision;  *t && !ISSPACE ((unsigned char) *t);  t++)
	      continue;
	    if (t == revision)
		revision = 0;
	    else {
		char oldc = *t;
		*t = '\0';
		revision = savestr (revision);
		*t = oldc;
	    }
	}
	if ((diff_type == NO_DIFF || diff_type == ED_DIFF) &&
	  first_command_line >= 0 &&
	  strEQ(s, ".\n") ) {
	    p_indent = indent;
	    p_start = first_command_line;
	    p_sline = fcl_line;
	    retval = ED_DIFF;
	    goto scan_exit;
	}
	if ((diff_type == NO_DIFF || diff_type == UNI_DIFF)
	    && strnEQ(s, "@@ -", 4)) {
	    s += 3;
	    if (reverse)
	      {
		while (*s != ' ' && *s != '\n')
		  s++;
		while (*s == ' ')
		  s++;
	      }
	    if (!atol(s))
		ok_to_create_file = TRUE;
	    p_indent = indent;
	    p_start = this_line;
	    p_sline = p_input_line;
	    retval = UNI_DIFF;
	    t = name[OLD];
	    name[OLD] = name[NEW];
	    name[NEW] = t;
	    goto scan_exit;
	}
	stars_this_line = strnEQ(s, "********", 8);
	if ((diff_type == NO_DIFF
	     || diff_type == CONTEXT_DIFF
	     || diff_type == NEW_CONTEXT_DIFF)
	    && stars_last_line && strnEQ (s, "*** ", 4)) {
	    if (! reverse)
	      if (! atol (s + 4))
		ok_to_create_file = TRUE;
	    /* if this is a new context diff the character just before */
	    /* the newline is a '*'. */
	    while (*s != '\n')
		s++;
	    p_indent = indent;
	    p_start = previous_line;
	    p_sline = p_input_line - 1;
	    retval = (*(s-1) == '*' ? NEW_CONTEXT_DIFF : CONTEXT_DIFF);
	    if (reverse)
	      {
		/* If the patch is a reversed context diff,
		   scan the entire first hunk to see whether
		   it's OK to create the file.  */
		long saved_p_base = p_base;
		LINENUM saved_p_bline = p_bline;
		p_input_line = p_sline;
		Fseek (pfp, previous_line, SEEK_SET);
		if (another_hunk (retval) && ! p_ptrn_lines && p_first == 1)
		  ok_to_create_file = TRUE;
		next_intuit_at (saved_p_base, saved_p_bline);
	      }
	    goto scan_exit;
	}
	if ((diff_type == NO_DIFF || diff_type == NORMAL_DIFF) &&
	  last_line_was_command &&
	  (strnEQ(s, "< ", 2) || strnEQ(s, "> ", 2)) ) {
	    p_start = previous_line;
	    p_sline = p_input_line - 1;
	    p_indent = indent;
	    retval = NORMAL_DIFF;
	    goto scan_exit;
	}
    }

  scan_exit:

    /* Use algorithm specified by POSIX 1003.2b/D11 to deduce `inname',
       the name of the file to patch; except that if the patch syntactically
       can create a file, before asking the user for a file name
       redo the earlier steps in sequence, this time ignoring the
       nonexistence of a file.  */

    for (i = OLD;  i <= INDEX;  i++)
      if (!inname && name[i])
	{
	  if (stat (name[i], &st[i]) != 0)
	    stat_errno[i] = errno;
	  else
	    {
	      stat_errno[i] = 0;
	      break;
	    }
	}

    if (i == NONE && ok_to_create_file)
      for (i = OLD;  i <= INDEX;  i++)
	if (!inname && name[i])
	  break;

    if (i == NONE)
      inerrno = -1;
    else
      {
	inname = name[i];
	name[i] = 0;
	inerrno = stat_errno[i];
	instat = st[i];
      }

    for (i = OLD;  i <= INDEX;  i++)
      if (name[i])
	free (name[i]);

    return retval;
}

/* Remember where this patch ends so we know where to start up again. */

static void
next_intuit_at(file_pos,file_line)
long file_pos;
LINENUM file_line;
{
    p_base = file_pos;
    p_bline = file_line;
}

/* Basically a verbose fseek() to the actual diff listing. */

static void
skip_to(file_pos,file_line)
long file_pos;
LINENUM file_line;
{
    register FILE *i = pfp;
    register FILE *o = stdout;
    register int c;

    assert(p_base <= file_pos);
    if ((verbosity == VERBOSE || !inname) && p_base < file_pos) {
	Fseek (i, p_base, SEEK_SET);
	say ("The text leading up to this was:\n--------------------------\n");

	while (ftell (i) < file_pos)
	  {
	    putc ('|', o);
	    do
	      {
		if ((c = getc (i)) == EOF)
		  read_fatal ();
		putc (c, o);
	      }
	    while (c != '\n');
	  }

	say ("--------------------------\n");
    }
    else
	Fseek (i, file_pos, SEEK_SET);
    p_input_line = file_line - 1;
}

/* Make this a function for better debugging.  */
static void
malformed ()
{
    fatal ("malformed patch at line %ld: %s", p_input_line, buf);
		/* about as informative as "Syntax error" in C */
}

/* 1 if there is more of the current diff listing to process;
   0 if not; -1 if ran out of memory. */

int
another_hunk (difftype)
     enum diff difftype;
{
    register char *s;
    register LINENUM context = 0;
    register size_t chars_read;

    while (p_end >= 0) {
	if (p_end == p_efake)
	    p_end = p_bfake;		/* don't free twice */
	else
	    free(p_line[p_end]);
	p_end--;
    }
    assert(p_end == -1);
    p_efake = -1;

    p_max = hunkmax;			/* gets reduced when --- found */
    if (difftype == CONTEXT_DIFF || difftype == NEW_CONTEXT_DIFF) {
	long line_beginning = ftell(pfp);
					/* file pos of the current line */
	LINENUM repl_beginning = 0;	/* index of --- line */
	register LINENUM fillcnt = 0;	/* #lines of missing ptrn or repl */
	register LINENUM fillsrc;	/* index of first line to copy */
	register LINENUM filldst;	/* index of first missing line */
	bool ptrn_spaces_eaten = FALSE;	/* ptrn was slightly misformed */
	bool some_context = FALSE;	/* (perhaps internal) context seen */
	register bool repl_could_be_missing = TRUE;
	bool repl_missing = FALSE;	/* we are now backtracking */
	long repl_backtrack_position = 0;
					/* file pos of first repl line */
	LINENUM repl_patch_line;	/* input line number for same */
	LINENUM repl_context;		/* context for same */
	register LINENUM ptrn_copiable = 0;
					/* # of copiable lines in ptrn */

	/* Pacify `gcc -Wall'.  */
	fillsrc = filldst = repl_patch_line = repl_context = 0;

	chars_read = get_line ();
	if (chars_read == (size_t) -1
	    || chars_read <= 8
	    || strncmp (buf, "********", 8) != 0) {
	    next_intuit_at(line_beginning,p_input_line);
	    return chars_read == (size_t) -1 ? -1 : 0;
	}
	p_prefix_context = -1;
	p_hunk_beg = p_input_line + 1;
	while (p_end < p_max) {
	    chars_read = get_line ();
	    if (chars_read == (size_t) -1)
	      return -1;
	    if (!chars_read) {
		if (repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		if (p_max - p_end < 4) {
		    strcpy (buf, "  \n");  /* assume blank lines got chopped */
		    chars_read = 3;
		} else {
		    fatal ("unexpected end of file in patch");
		}
	    }
	    p_end++;
	    if (p_end == hunkmax)
		fatal ("unterminated hunk starting at line %ld; giving up at line %ld: %s",
		       pch_hunk_beg (), p_input_line, buf);
	    assert(p_end < hunkmax);
	    p_Char[p_end] = *buf;
	    p_len[p_end] = 0;
	    p_line[p_end] = 0;
	    switch (*buf) {
	    case '*':
		if (strnEQ(buf, "********", 8)) {
		    if (repl_beginning && repl_could_be_missing) {
			repl_missing = TRUE;
			goto hunk_done;
		    }
		    else
			fatal ("unexpected end of hunk at line %ld",
			    p_input_line);
		}
		if (p_end != 0) {
		    if (repl_beginning && repl_could_be_missing) {
			repl_missing = TRUE;
			goto hunk_done;
		    }
		    fatal ("unexpected `***' at line %ld: %s",
			   p_input_line, buf);
		}
		context = 0;
		p_len[p_end] = strlen (buf);
		if (! (p_line[p_end] = savestr (buf))) {
		    p_end--;
		    return -1;
		}
		for (s = buf;  *s && !ISDIGIT (*s);  s++)
		  continue;
		if (!*s)
		    malformed ();
		if (strnEQ(s,"0,0",3))
		    remove_prefix (s, 2);
		p_first = (LINENUM) atol(s);
		while (ISDIGIT (*s))
		  s++;
		if (*s == ',') {
		    while (*s && !ISDIGIT (*s))
		      s++;
		    if (!*s)
			malformed ();
		    p_ptrn_lines = ((LINENUM)atol(s)) - p_first + 1;
		}
		else if (p_first)
		    p_ptrn_lines = 1;
		else {
		    p_ptrn_lines = 0;
		    p_first = 1;
		}
		p_max = p_ptrn_lines + 6;	/* we need this much at least */
		while (p_max >= hunkmax)
		    if (! grow_hunkmax ())
			return -1;
		p_max = hunkmax;
		break;
	    case '-':
		if (buf[1] == '-') {
		    if (p_prefix_context == -1)
		      p_prefix_context = context;
		    if (repl_beginning ||
			(p_end != p_ptrn_lines + 1 + (p_Char[p_end-1] == '\n')))
		    {
			if (p_end == 1) {
			    /* `old' lines were omitted - set up to fill */
			    /* them in from 'new' context lines. */
			    p_end = p_ptrn_lines + 1;
			    fillsrc = p_end + 1;
			    filldst = 1;
			    fillcnt = p_ptrn_lines;
			}
			else {
			    if (repl_beginning) {
				if (repl_could_be_missing){
				    repl_missing = TRUE;
				    goto hunk_done;
				}
				fatal (
"duplicate \"---\" at line %ld--check line numbers at line %ld",
				    p_input_line, p_hunk_beg + repl_beginning);
			    }
			    else {
				fatal (
"%s \"---\" at line %ld--check line numbers at line %ld",
				    (p_end <= p_ptrn_lines
					? "Premature"
					: "Overdue" ),
				    p_input_line, p_hunk_beg);
			    }
			}
		    }
		    repl_beginning = p_end;
		    repl_backtrack_position = ftell(pfp);
		    repl_patch_line = p_input_line;
		    repl_context = context;
		    p_len[p_end] = strlen (buf);
		    if (! (p_line[p_end] = savestr (buf))) {
			p_end--;
			return -1;
		    }
		    p_Char[p_end] = '=';
		    for (s = buf;  *s && !ISDIGIT (*s);  s++)
		      continue;
		    if (!*s)
			malformed ();
		    p_newfirst = (LINENUM) atol(s);
		    while (ISDIGIT (*s))
		      s++;
		    if (*s == ',') {
			do
			  {
			    if (!*++s)
			      malformed ();
			  }
			while (!ISDIGIT (*s));
			p_repl_lines = ((LINENUM)atol(s)) - p_newfirst + 1;
		    }
		    else if (p_newfirst)
			p_repl_lines = 1;
		    else {
			p_repl_lines = 0;
			p_newfirst = 1;
		    }
		    p_max = p_repl_lines + p_end;
		    while (p_max >= hunkmax)
			if (! grow_hunkmax ())
			    return -1;
		    if (p_repl_lines != ptrn_copiable
			&& (p_prefix_context != 0
			    || context != 0
			    || p_repl_lines != 1))
			repl_could_be_missing = FALSE;
		    context = 0;
		    break;
		}
		goto change_line;
	    case '+':  case '!':
		repl_could_be_missing = FALSE;
	      change_line:
		s = buf + 1;
		chars_read--;
		if (*s == '\n' && canonicalize) {
		    strcpy (s, " \n");
		    chars_read = 2;
		}
		if (*s == ' ' || *s == '\t') {
		    s++;
		    chars_read--;
		} else if (repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		if (p_prefix_context == -1)
		  p_prefix_context = context;
		chars_read -=
		  (1 < chars_read
		   && p_end == (repl_beginning ? p_max : p_ptrn_lines)
		   && incomplete_line ());
		p_len[p_end] = chars_read;
		if (! (p_line[p_end] = savebuf (s, chars_read))) {
		    p_end--;
		    return -1;
		}
		context = 0;
		break;
	    case '\t': case '\n':	/* assume spaces got eaten */
		s = buf;
		if (*buf == '\t') {
		    s++;
		    chars_read--;
		}
		if (repl_beginning && repl_could_be_missing &&
		    (!ptrn_spaces_eaten || difftype == NEW_CONTEXT_DIFF) ) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		chars_read -=
		  (1 < chars_read
		   && p_end == (repl_beginning ? p_max : p_ptrn_lines)
		   && incomplete_line ());
		p_len[p_end] = chars_read;
		if (! (p_line[p_end] = savebuf (buf, chars_read))) {
		    p_end--;
		    return -1;
		}
		if (p_end != p_ptrn_lines + 1) {
		    ptrn_spaces_eaten |= (repl_beginning != 0);
		    some_context = TRUE;
		    context++;
		    if (!repl_beginning)
			ptrn_copiable++;
		    p_Char[p_end] = ' ';
		}
		break;
	    case ' ':
		s = buf + 1;
		chars_read--;
		if (*s == '\n' && canonicalize) {
		    strcpy (s, "\n");
		    chars_read = 2;
		}
		if (*s == ' ' || *s == '\t') {
		    s++;
		    chars_read--;
		} else if (repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		some_context = TRUE;
		context++;
		if (!repl_beginning)
		    ptrn_copiable++;
		chars_read -=
		  (1 < chars_read
		   && p_end == (repl_beginning ? p_max : p_ptrn_lines)
		   && incomplete_line ());
		p_len[p_end] = chars_read;
		if (! (p_line[p_end] = savebuf (buf + 2, chars_read))) {
		    p_end--;
		    return -1;
		}
		break;
	    default:
		if (repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		malformed ();
	    }
	}

    hunk_done:
	if (p_end >=0 && !repl_beginning)
	    fatal ("no --- found in patch at line %ld", pch_hunk_beg ());

	if (repl_missing) {

	    /* reset state back to just after --- */
	    p_input_line = repl_patch_line;
	    context = repl_context;
	    for (p_end--; p_end > repl_beginning; p_end--)
		free(p_line[p_end]);
	    Fseek (pfp, repl_backtrack_position, SEEK_SET);

	    /* redundant 'new' context lines were omitted - set */
	    /* up to fill them in from the old file context */
	    fillsrc = 1;
	    filldst = repl_beginning+1;
	    fillcnt = p_repl_lines;
	    p_end = p_max;
	}
	else if (!some_context && fillcnt == 1) {
	    /* the first hunk was a null hunk with no context */
	    /* and we were expecting one line -- fix it up. */
	    while (filldst < p_end) {
		p_line[filldst] = p_line[filldst+1];
		p_Char[filldst] = p_Char[filldst+1];
		p_len[filldst] = p_len[filldst+1];
		filldst++;
	    }
#if 0
	    repl_beginning--;		/* this doesn't need to be fixed */
#endif
	    p_end--;
	    p_first++;			/* do append rather than insert */
	    fillcnt = 0;
	    p_ptrn_lines = 0;
	}

	p_suffix_context = context;

	if (difftype == CONTEXT_DIFF
	    && (fillcnt
		|| (p_first > 1
		    && p_prefix_context + p_suffix_context < ptrn_copiable))) {
	    if (verbosity == VERBOSE)
		say ("%s\n%s\n%s\n",
"(Fascinating--this is really a new-style context diff but without",
"the telltale extra asterisks on the *** line that usually indicate",
"the new style...)");
	    diff_type = difftype = NEW_CONTEXT_DIFF;
	}

	/* if there were omitted context lines, fill them in now */
	if (fillcnt) {
	    p_bfake = filldst;		/* remember where not to free() */
	    p_efake = filldst + fillcnt - 1;
	    while (fillcnt-- > 0) {
		while (fillsrc <= p_end && fillsrc != repl_beginning
		       && p_Char[fillsrc] != ' ')
		    fillsrc++;
		if (p_end < fillsrc || fillsrc == repl_beginning)
		    fatal ("replacement text or line numbers mangled in hunk at line %ld",
			p_hunk_beg);
		p_line[filldst] = p_line[fillsrc];
		p_Char[filldst] = p_Char[fillsrc];
		p_len[filldst] = p_len[fillsrc];
		fillsrc++; filldst++;
	    }
	    while (fillsrc <= p_end && fillsrc != repl_beginning)
	      {
		if (p_Char[fillsrc] == ' ')
		  fatal ("replacement text or line numbers mangled in hunk at line %ld",
			 p_hunk_beg);
		fillsrc++;
	      }
	    if (debug & 64)
		printf("fillsrc %ld, filldst %ld, rb %ld, e+1 %ld\n",
		    fillsrc,filldst,repl_beginning,p_end+1);
	    assert(fillsrc==p_end+1 || fillsrc==repl_beginning);
	    assert(filldst==p_end+1 || filldst==repl_beginning);
	}
    }
    else if (difftype == UNI_DIFF) {
	long line_beginning = ftell(pfp);
					/* file pos of the current line */
	register LINENUM fillsrc;	/* index of old lines */
	register LINENUM filldst;	/* index of new lines */
	char ch = '\0';

	chars_read = get_line ();
	if (chars_read == (size_t) -1
	    || chars_read <= 4
	    || strncmp (buf, "@@ -", 4) != 0) {
	    next_intuit_at(line_beginning,p_input_line);
	    return chars_read == (size_t) -1 ? -1 : 0;
	}
	s = buf+4;
	if (!*s)
	    malformed ();
	p_first = (LINENUM) atol(s);
	while (ISDIGIT (*s))
	  s++;
	if (*s == ',') {
	    p_ptrn_lines = (LINENUM) atol(++s);
	    while (ISDIGIT (*s))
	      s++;
	} else
	    p_ptrn_lines = 1;
	if (*s == ' ') s++;
	if (*s != '+' || !*++s)
	    malformed ();
	p_newfirst = (LINENUM) atol(s);
	while (ISDIGIT (*s))
	  s++;
	if (*s == ',') {
	    p_repl_lines = (LINENUM) atol(++s);
	    while (ISDIGIT (*s))
	      s++;
	} else
	    p_repl_lines = 1;
	if (*s == ' ') s++;
	if (*s != '@')
	    malformed ();
	if (!p_ptrn_lines)
	    p_first++;			/* do append rather than insert */
	if (!p_repl_lines)
	    p_newfirst++;
	p_max = p_ptrn_lines + p_repl_lines + 1;
	while (p_max >= hunkmax)
	    if (! grow_hunkmax ())
		return -1;
	fillsrc = 1;
	filldst = fillsrc + p_ptrn_lines;
	p_end = filldst + p_repl_lines;
	sprintf (buf,"*** %ld,%ld ****\n",p_first,p_first + p_ptrn_lines - 1);
	p_len[0] = strlen (buf);
	if (! (p_line[0] = savestr (buf))) {
	    p_end = -1;
	    return -1;
	}
	p_Char[0] = '*';
	sprintf (buf,"--- %ld,%ld ----\n",p_newfirst,p_newfirst+p_repl_lines-1);
	p_len[filldst] = strlen (buf);
	if (! (p_line[filldst] = savestr (buf))) {
	    p_end = 0;
	    return -1;
	}
	p_Char[filldst++] = '=';
	p_prefix_context = -1;
	p_hunk_beg = p_input_line + 1;
	while (fillsrc <= p_ptrn_lines || filldst <= p_end) {
	    chars_read = get_line ();
	    if (!chars_read) {
		if (p_max - filldst < 3) {
		    strcpy (buf, " \n");  /* assume blank lines got chopped */
		    chars_read = 2;
		} else {
		    fatal ("unexpected end of file in patch");
		}
	    }
	    if (chars_read == (size_t) -1)
		s = 0;
	    else if (*buf == '\t' || *buf == '\n') {
		ch = ' ';		/* assume the space got eaten */
		s = savebuf (buf, chars_read);
	    }
	    else {
		ch = *buf;
		s = savebuf (buf+1, --chars_read);
	    }
	    if (!s) {
		while (--filldst > p_ptrn_lines)
		    free(p_line[filldst]);
		p_end = fillsrc-1;
		return -1;
	    }
	    switch (ch) {
	    case '-':
		if (fillsrc > p_ptrn_lines) {
		    free(s);
		    p_end = filldst-1;
		    malformed ();
		}
		chars_read -= fillsrc == p_ptrn_lines && incomplete_line ();
		p_Char[fillsrc] = ch;
		p_line[fillsrc] = s;
		p_len[fillsrc++] = chars_read;
		break;
	    case '=':
		ch = ' ';
		/* FALL THROUGH */
	    case ' ':
		if (fillsrc > p_ptrn_lines) {
		    free(s);
		    while (--filldst > p_ptrn_lines)
			free(p_line[filldst]);
		    p_end = fillsrc-1;
		    malformed ();
		}
		context++;
		chars_read -= fillsrc == p_ptrn_lines && incomplete_line ();
		p_Char[fillsrc] = ch;
		p_line[fillsrc] = s;
		p_len[fillsrc++] = chars_read;
		s = savebuf (s, chars_read);
		if (!s) {
		    while (--filldst > p_ptrn_lines)
			free(p_line[filldst]);
		    p_end = fillsrc-1;
		    return -1;
		}
		/* FALL THROUGH */
	    case '+':
		if (filldst > p_end) {
		    free(s);
		    while (--filldst > p_ptrn_lines)
			free(p_line[filldst]);
		    p_end = fillsrc-1;
		    malformed ();
		}
		chars_read -= filldst == p_end && incomplete_line ();
		p_Char[filldst] = ch;
		p_line[filldst] = s;
		p_len[filldst++] = chars_read;
		break;
	    default:
		p_end = filldst;
		malformed ();
	    }
	    if (ch != ' ') {
		if (p_prefix_context == -1)
		    p_prefix_context = context;
		context = 0;
	    }
	}/* while */
	p_suffix_context = context;
    }
    else {				/* normal diff--fake it up */
	char hunk_type;
	register int i;
	LINENUM min, max;
	long line_beginning = ftell(pfp);

	p_prefix_context = p_suffix_context = 0;
	chars_read = get_line ();
	if (chars_read == (size_t) -1 || !chars_read || !ISDIGIT (*buf)) {
	    next_intuit_at(line_beginning,p_input_line);
	    return chars_read == (size_t) -1 ? -1 : 0;
	}
	p_first = (LINENUM)atol(buf);
	for (s = buf;  ISDIGIT (*s);  s++)
	  continue;
	if (*s == ',') {
	    p_ptrn_lines = (LINENUM)atol(++s) - p_first + 1;
	    while (ISDIGIT (*s))
	      s++;
	}
	else
	    p_ptrn_lines = (*s != 'a');
	hunk_type = *s;
	if (hunk_type == 'a')
	    p_first++;			/* do append rather than insert */
	min = (LINENUM)atol(++s);
	while (ISDIGIT (*s))
	  s++;
	if (*s == ',')
	    max = (LINENUM)atol(++s);
	else
	    max = min;
	if (hunk_type == 'd')
	    min++;
	p_end = p_ptrn_lines + 1 + max - min + 1;
	while (p_end >= hunkmax)
	  if (! grow_hunkmax ())
	    {
	      p_end = -1;
	      return -1;
	    }
	p_newfirst = min;
	p_repl_lines = max - min + 1;
	sprintf (buf, "*** %ld,%ld\n", p_first, p_first + p_ptrn_lines - 1);
	p_len[0] = strlen (buf);
	if (! (p_line[0] = savestr (buf))) {
	    p_end = -1;
	    return -1;
	}
	p_Char[0] = '*';
	for (i=1; i<=p_ptrn_lines; i++) {
	    chars_read = get_line ();
	    if (chars_read == (size_t) -1)
	      {
		p_end = i - 1;
		return -1;
	      }
	    if (!chars_read)
		fatal ("unexpected end of file in patch at line %ld",
		  p_input_line);
	    if (buf[0] != '<' || (buf[1] != ' ' && buf[1] != '\t'))
		fatal ("`<' expected at line %ld of patch", p_input_line);
	    chars_read -= 2 + (i == p_ptrn_lines && incomplete_line ());
	    p_len[i] = chars_read;
	    if (! (p_line[i] = savebuf (buf + 2, chars_read))) {
		p_end = i-1;
		return -1;
	    }
	    p_Char[i] = '-';
	}
	if (hunk_type == 'c') {
	    chars_read = get_line ();
	    if (chars_read == (size_t) -1)
	      {
		p_end = i - 1;
		return -1;
	      }
	    if (! chars_read)
		fatal ("unexpected end of file in patch at line %ld",
		    p_input_line);
	    if (*buf != '-')
		fatal ("`---' expected at line %ld of patch", p_input_line);
	}
	sprintf (buf, "--- %ld,%ld\n", min, max);
	p_len[i] = strlen (buf);
	if (! (p_line[i] = savestr (buf))) {
	    p_end = i-1;
	    return -1;
	}
	p_Char[i] = '=';
	for (i++; i<=p_end; i++) {
	    chars_read = get_line ();
	    if (chars_read == (size_t) -1)
	      {
		p_end = i - 1;
		return -1;
	      }
	    if (!chars_read)
		fatal ("unexpected end of file in patch at line %ld",
		    p_input_line);
	    if (buf[0] != '>' || (buf[1] != ' ' && buf[1] != '\t'))
		fatal ("`>' expected at line %ld of patch", p_input_line);
	    chars_read -= 2 + (i == p_end && incomplete_line ());
	    p_len[i] = chars_read;
	    if (! (p_line[i] = savebuf (buf + 2, chars_read))) {
		p_end = i-1;
		return -1;
	    }
	    p_Char[i] = '+';
	}
    }
    if (reverse)			/* backwards patch? */
	if (!pch_swap())
	    say ("Not enough memory to swap next hunk!\n");
    if (debug & 2) {
	LINENUM i;
	char special;

	for (i=0; i <= p_end; i++) {
	    if (i == p_ptrn_lines)
		special = '^';
	    else
		special = ' ';
	    fprintf (stderr, "%3ld %c %c ", i, p_Char[i], special);
	    pch_write_line (i, stderr);
	    fflush (stderr);
	}
    }
    if (p_end+1 < hunkmax)	/* paranoia reigns supreme... */
	p_Char[p_end+1] = '^';  /* add a stopper for apply_hunk */
    return 1;
}

static size_t
get_line ()
{
   return pget_line (p_indent);
}

/* Input a line from the patch file, worrying about indentation.
   Strip up to INDENT characters' worth of leading indentation.
   Ignore any partial lines at end of input, but warn about them.
   Succeed if a line was read; it is terminated by "\n\0" for convenience.
   Return the number of characters read, including '\n' but not '\0'.
   Return -1 if we ran out of memory.  */

static size_t
pget_line (indent)
     int indent;
{
  register FILE *fp = pfp;
  register int c;
  register int i = 0;
  register char *b;
  register size_t s;

  for (;;)
    {
      c = getc (fp);
      if (c == EOF)
	{
	  if (ferror (fp))
	    read_fatal ();
	  return 0;
	}
      if (indent <= i)
	break;
      if (c == ' ' || c == 'X')
	i++;
      else if (c == '\t')
	i = (i + 8) & ~7;
      else
	break;
    }

  i = 0;
  b = buf;
  s = bufsize;

  for (;;)
    {
      if (i == s - 1)
	{
	  s *= 2;
	  b = realloc (b, s);
	  if (!b)
	    {
	      if (!using_plan_a)
		memory_fatal ();
	      return (size_t) -1;
	    }
	  buf = b;
	  bufsize = s;
	}
      b[i++] = c;
      if (c == '\n')
	break;
      c = getc (fp);
      if (c == EOF)
	{
	  if (ferror (fp))
	    read_fatal ();
	  say ("patch unexpectedly ends in middle of line\n");
	  return 0;
	}
    }

  b[i] = '\0';
  p_input_line++;
  return i;
}

static bool
incomplete_line ()
{
  register FILE *fp = pfp;
  register int c;
  register long line_beginning = ftell (fp);

  if (getc (fp) == '\\')
    {
      while ((c = getc (fp)) != '\n'  &&  c != EOF)
	continue;
      return TRUE;
    }
  else
    {
      /* We don't trust ungetc.  */
      Fseek (pfp, line_beginning, SEEK_SET);
      return FALSE;
    }
}

/* Reverse the old and new portions of the current hunk. */

bool
pch_swap()
{
    char **tp_line;		/* the text of the hunk */
    size_t *tp_len;		/* length of each line */
    char *tp_char;		/* +, -, and ! */
    register LINENUM i;
    register LINENUM n;
    bool blankline = FALSE;
    register char *s;

    i = p_first;
    p_first = p_newfirst;
    p_newfirst = i;

    /* make a scratch copy */

    tp_line = p_line;
    tp_len = p_len;
    tp_char = p_Char;
    p_line = 0;	/* force set_hunkmax to allocate again */
    p_len = 0;
    p_Char = 0;
    set_hunkmax();
    if (!p_line || !p_len || !p_Char) {
	if (p_line)
	  free (p_line);
	p_line = tp_line;
	if (p_len)
	  free (p_len);
	p_len = tp_len;
	if (p_Char)
	  free (p_Char);
	p_Char = tp_char;
	return FALSE;		/* not enough memory to swap hunk! */
    }

    /* now turn the new into the old */

    i = p_ptrn_lines + 1;
    if (tp_char[i] == '\n') {		/* account for possible blank line */
	blankline = TRUE;
	i++;
    }
    if (p_efake >= 0) {			/* fix non-freeable ptr range */
	if (p_efake <= i)
	    n = p_end - i + 1;
	else
	    n = -i;
	p_efake += n;
	p_bfake += n;
    }
    for (n=0; i <= p_end; i++,n++) {
	p_line[n] = tp_line[i];
	p_Char[n] = tp_char[i];
	if (p_Char[n] == '+')
	    p_Char[n] = '-';
	p_len[n] = tp_len[i];
    }
    if (blankline) {
	i = p_ptrn_lines + 1;
	p_line[n] = tp_line[i];
	p_Char[n] = tp_char[i];
	p_len[n] = tp_len[i];
	n++;
    }
    assert(p_Char[0] == '=');
    p_Char[0] = '*';
    for (s=p_line[0]; *s; s++)
	if (*s == '-')
	    *s = '*';

    /* now turn the old into the new */

    assert(tp_char[0] == '*');
    tp_char[0] = '=';
    for (s=tp_line[0]; *s; s++)
	if (*s == '*')
	    *s = '-';
    for (i=0; n <= p_end; i++,n++) {
	p_line[n] = tp_line[i];
	p_Char[n] = tp_char[i];
	if (p_Char[n] == '-')
	    p_Char[n] = '+';
	p_len[n] = tp_len[i];
    }
    assert(i == p_ptrn_lines + 1);
    i = p_ptrn_lines;
    p_ptrn_lines = p_repl_lines;
    p_repl_lines = i;
    if (tp_line)
      free (tp_line);
    if (tp_len)
      free (tp_len);
    if (tp_char)
      free (tp_char);
    return TRUE;
}

/* Return the specified line position in the old file of the old context. */

LINENUM
pch_first()
{
    return p_first;
}

/* Return the number of lines of old context. */

LINENUM
pch_ptrn_lines()
{
    return p_ptrn_lines;
}

/* Return the probable line position in the new file of the first line. */

LINENUM
pch_newfirst()
{
    return p_newfirst;
}

/* Return the number of lines in the replacement text including context. */

LINENUM
pch_repl_lines()
{
    return p_repl_lines;
}

/* Return the number of lines in the whole hunk. */

LINENUM
pch_end()
{
    return p_end;
}

/* Return the number of context lines before the first changed line. */

LINENUM
pch_prefix_context ()
{
    return p_prefix_context;
}

/* Return the number of context lines after the last changed line. */

LINENUM
pch_suffix_context ()
{
    return p_suffix_context;
}

/* Return the length of a particular patch line. */

size_t
pch_line_len(line)
LINENUM line;
{
    return p_len[line];
}

/* Return the control character (+, -, *, !, etc) for a patch line. */

char
pch_char(line)
LINENUM line;
{
    return p_Char[line];
}

/* Return a pointer to a particular patch line. */

char *
pfetch(line)
LINENUM line;
{
    return p_line[line];
}

/* Output a patch line.  */

bool
pch_write_line (line, file)
     LINENUM line;
     FILE *file;
{
  bool after_newline = p_line[line][p_len[line] - 1] == '\n';
  if (! fwrite (p_line[line], sizeof (*p_line[line]), p_len[line], file))
    write_fatal ();
  return after_newline;
}

/* Return where in the patch file this hunk began, for error messages. */

LINENUM
pch_hunk_beg()
{
    return p_hunk_beg;
}

/* Apply an ed script by feeding ed itself. */

void
do_ed_script (ofp)
     FILE *ofp;
{
    static char const ed_program[] = ED_PROGRAM;

    register char *t;
    register long beginning_of_this_line;
    register bool this_line_is_command = FALSE;
    register FILE *pipefp = 0;
    register size_t chars_read;

    if (!skip_rest_of_patch) {
	copy_file (inname, TMPOUTNAME);
	sprintf (buf, "%s %s%s", ed_program, verbosity == VERBOSE ? "" : "- ",
		 TMPOUTNAME);
	pipefp = popen(buf, "w");
	if (!pipefp)
	  pfatal (buf);
    }
    for (;;) {
	beginning_of_this_line = ftell(pfp);
	chars_read = get_line ();
	if (! chars_read) {
	    next_intuit_at(beginning_of_this_line,p_input_line);
	    break;
	}
	for (t = buf;  ISDIGIT (*t) || *t == ',';  t++)
	  continue;
	this_line_is_command = (ISDIGIT (*buf) &&
	  (*t == 'd' || *t == 'c' || *t == 'a' || *t == 'i' || *t == 's') );
	if (this_line_is_command) {
	    if (pipefp)
		if (! fwrite (buf, sizeof *buf, chars_read, pipefp))
		    write_fatal ();
	    if (*t != 'd' && *t != 's') {
		while ((chars_read = get_line ()) != 0) {
		    if (pipefp)
			if (! fwrite (buf, sizeof *buf, chars_read, pipefp))
			    write_fatal ();
		    if (chars_read == 2  &&  strEQ (buf, ".\n"))
			break;
		}
	    }
	}
	else {
	    next_intuit_at(beginning_of_this_line,p_input_line);
	    break;
	}
    }
    if (!pipefp)
      return;
    if (fwrite ("w\nq\n", sizeof (char), (size_t) 4, pipefp) == 0
	|| fflush (pipefp) != 0)
      write_fatal ();
    if (pclose (pipefp) != 0)
      fatal ("%s FAILED", ed_program);

    if (ofp)
      {
	FILE *ifp = fopen (TMPOUTNAME, "r");
	int c;
	if (!ifp)
	  pfatal (TMPOUTNAME);
	while ((c = getc (ifp)) != EOF)
	  if (putc (c, ofp) == EOF)
	    write_fatal ();
	if (ferror (ifp) || fclose (ifp) != 0)
	  read_fatal ();
      }
}
