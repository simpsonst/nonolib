// -*- c-basic-offset: 2; indent-tabs-mode: nil -*-

/*
 *  Nonolib - Nonogram-solver library
 *  Copyright (C) 2001,2005-8,2012  Steven Simpson
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Contact Steven Simpson <https://github.com/simpsonst>
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "nonogram.h"

static void compprep(void *c,
                     const struct nonogram_lim *l, struct nonogram_req *r)
{
  c = c;

  /* Include workspace for the push function. */
  r->ptrdiff += l->maxrule;

  /* We need to remember the positions of the blocks when pushed left. */
  r->nonogram_size += l->maxrule;

  /* We need to remember the positions of the blocks when pushed right. */
  r->nonogram_size += l->maxrule;

  /* We also need some discardable space when we simply want to check
     for inconsistency using the push function. */
  r->nonogram_size += l->maxrule;

  /* We need to identify what to guess at each cell. */
  r->cell += l->maxline;

  /* We need to provide a modified version of the line to the push
     function. */
  r->cell += l->maxline;
}


#define INPUT(X) (a->line[a->linestep * (X)])
#define OUTPUT(X) (a->result[a->resultstep * (X)])
#define RULE(X) (a->rule[a->rulestep * (X)])

struct working {
  /* These are the arrays of block positions, one for when the blocks
     are pushed to their extreme left, and one to their extreme
     right. */
  nonogram_sizetype *left, *right, *waste;

  /* This array indicates which cells are testable - flags[n] will
     contain either a dot or a solid, but not both. */
  nonogram_cell *flags;

  /* This is workspace for the push function. */
  ptrdiff_t *pushspace;
};

static void check_cell(const struct nonogram_initargs *a,
                       struct nonogram_ws *c,
                       struct working *wp,
                       nonogram_sizetype pos,
                       int *skip)
{
  /* Ignore cells which we already know. */
  if (INPUT(pos) != nonogram_BLANK) {
    /* But if we go past a known cell, that resets skipping. */
    *skip = false;
    return;
  }

  /* Ignore if we've previously made a guess and failed to deduce
     anything from it, and have not encountered a known cell
     since. */
  if (*skip)
    return;

  /* Ignore cells which are different in the two extreme
     arrangements. */
  if (wp->flags[pos] == nonogram_BOTH)
    return;

  /* Make a contrary guess here. */
  OUTPUT(pos) = nonogram_BOTH ^ wp->flags[pos];

#if 0
  printf("%9d >%*sX\n", (int) pos, (int) pos, "");
#endif

  /* Now see if it still fits. */
  if (nonogram_push(a->result, a->linelen, a->resultstep,
                    a->rule, a->rulelen, a->rulestep,
                    wp->waste, 1,
                    wp->pushspace,
                    NULL, 0, 0)) {
    /* The pattern still matches, so we can't deduce that our
       contrary guess is false.  Put it back, and start skipping. */
    OUTPUT(pos) = nonogram_BLANK;
    *skip = true;

    /* Also prevent this cell from being guessed again (on the way
       back). */
    wp->flags[pos] = nonogram_BOTH;
  } else {
    /* We got a contradiction, so our guess must be wrong. */
    OUTPUT(pos) ^= nonogram_BOTH;
    assert(!*skip);
  }
}

/* Search a section of the line for positions that agree in both the
   extreme-left and extreme-right arrangements. */
static void search_section(const struct nonogram_initargs *a,
                           struct nonogram_ws *c,
                           struct working *wp,
                           nonogram_sizetype start,
                           nonogram_sizetype end)
{
  nonogram_sizetype pos;
  int skip = false;

  if (start > end) {
#if 0
    printf("%6d-%2d >%*s%.*s\n", (int) end, (int) start,
           (int) end, "", (int) (start - end),
           "***********************************************************");
#endif
    for (pos = start; pos > end; pos--)
      check_cell(a, c, wp, pos - 1, &skip);
  } else {
#if 0
    printf("%6d-%2d >%*s%.*s\n", (int) start, (int) end,
           (int) start, "", (int) (end - start),
           "***********************************************************");
#endif
    for (pos = start; pos < end; pos++)
      check_cell(a, c, wp, pos, &skip);
  }
}

static int compinit(void *ct, struct nonogram_ws *c,
                    const struct nonogram_initargs *a)
{
  size_t b;
  struct working w;
  nonogram_sizetype last_end, pos;

  w.left = c->nonogram_size;
  w.right = w.left + a->rulelen;
  w.waste = w.right + a->rulelen;
  w.pushspace = c->ptrdiff;
  w.flags = c->cell;

  /* Deal with the special case of a completely empty line. */
  if ((a->rulelen == 1 && a->rule[0] == 0) || a->rulelen == 0) {
    /* Try to fill the line with dots. */
    *a->fits = 1;
    for (pos = 0; pos < a->linelen; pos++) {
      /* Detect an inconsistency at this point. */
      if (INPUT(pos) == nonogram_SOLID) {
        *a->fits = 0;
        return false;
      }
      OUTPUT(pos) = nonogram_DOT;
    }
    return false;
  }

  *a->fits = 0;

  /* Find the left-most limits of the blocks.  This gives our starting
     position. */
  if (!nonogram_push(a->line, a->linelen, a->linestep,
                     a->rule, a->rulelen, a->rulestep,
                     w.left, 1,
                     w.pushspace,
                     a->log->file, a->log->level, a->log->indent)) {
    assert(*a->fits == 0);
    return false;
  }

  /* Find the right-most limits of the blocks. */
  assert(a->rulelen > 0);
  if (!nonogram_push(a->line + (a->linelen - 1) * a->linestep, a->linelen,
                     -a->linestep,
                     a->rule + (a->rulelen - 1) * a->rulestep, a->rulelen,
                     -a->rulestep,
                     w.right + (a->rulelen - 1) , -1,
                     w.pushspace,
                     a->log->file, a->log->level, a->log->indent)) {
    assert(*a->fits == 0);
    return false;
  }

  *a->fits = 1;

  /* Change the right-most limits to be expressed as exclusive
     positions from the left, instead of inclusive from the right. */
  for (b = 0; b < a->rulelen; b++)
    w.right[b] = a->linelen - w.right[b] - RULE(b);

  /* What do we get (in the flags array) when we merge the pushed-left
     and pushed-right solutions? */
  for (pos = 0, b = 0; b < a->rulelen; b++) {
    for ( ; pos < w.left[b]; pos++)
      w.flags[pos] = nonogram_DOT;
    for ( ; pos < w.left[b] + RULE(b); pos++)
      w.flags[pos] = nonogram_SOLID;
  }
  for ( ; pos < a->linelen; pos++)
    w.flags[pos] = nonogram_DOT;
  for (pos = 0, b = 0; b < a->rulelen; b++) {
    for ( ; pos < w.right[b]; pos++)
      w.flags[pos] |= nonogram_DOT;
    for ( ; pos < w.right[b] + RULE(b); pos++)
      w.flags[pos] |= nonogram_SOLID;
  }
  for ( ; pos < a->linelen; pos++)
    w.flags[pos] |= nonogram_DOT;

  /* We also need a copy of the original line to do tests on, and
     we'll modify it as we go. */
  for (pos = 0; pos < a->linelen; pos++)
    OUTPUT(pos) = INPUT(pos);

  /* Now we go through each of the blocks pushed right and the gaps to
     their left (gap first, then the block). */
#if 0
  printf("Going left to right...\n");
#endif
  for (last_end = 0, b = 0; b < a->rulelen; b++) {
    /* Deal with the gap first. */
    search_section(a, c, &w, last_end, w.right[b]);

    /* Then deal with the block itself. */
    last_end = w.right[b] + RULE(b);
    assert(last_end <= a->linelen);
    search_section(a, c, &w, w.right[b], last_end); 
  }
  search_section(a, c, &w, last_end, a->linelen);

  /* Now we go through each of the blocks pushed left and the gaps to
     their right (gap first, then the block). */
#if 0
  printf("Going right to left...\n");
#endif
  for (last_end = a->linelen, b = a->rulelen; b > 0; b--) {
    nonogram_sizetype block_end = w.right[b - 1] + RULE(b - 1);
    assert(block_end <= a->linelen);

    /* Deal with the gap first. */
    search_section(a, c, &w, last_end, block_end);

    /* Then deal with the block itself. */
    last_end = w.right[b - 1];
    search_section(a, c, &w, block_end, last_end); 
  }
  search_section(a, c, &w, last_end, 0); 


  /* Finally convert BLANKs into BOTHs. */
  for (pos = 0; pos < a->linelen; pos++)
    if (OUTPUT(pos) == nonogram_BLANK)
      OUTPUT(pos) = nonogram_BOTH;

  /* There is no more work to do. */
  return false;
}

const struct nonogram_linesuite nonogram_olsaksuite = {
  &compprep, &compinit, 0, 0
};



















#if 0
static void log_result(const struct nonogram_initargs *a)
{
  if (a->log && a->log->file && a->log->level > 0) {
    nonogram_sizetype i;
    fprintf(a->log->file, "          >");
    for (i = 0; i < a->linelen; i++)
      switch (OUTPUT(i)) {
      case nonogram_BLANK:
        putc(' ', a->log->file);
        break;
      case nonogram_DOT:
        putc('-', a->log->file);
        break;
      case nonogram_SOLID:
        putc('#', a->log->file);
        break;
      case nonogram_BOTH:
        putc('+', a->log->file);
        break;
      default:
        putc('?', a->log->file);
        break;
      }
    fprintf(a->log->file, "<\n");
  }
}

static void merge(const nonogram_sizetype *pos,
                  const struct nonogram_initargs *a)
{
  nonogram_sizetype i;
  size_t b;

  for (i = 0; i < pos[0]; i++)
    OUTPUT(i) = nonogram_DOT;
  for (b = 0; b < a->rulelen; b++) {
    nonogram_sizetype solidend = pos[b] + RULE(b);
    nonogram_sizetype dotend =
      b + 1 < a->rulelen ? pos[b + 1] : a->linelen;
    for (i = pos[b]; i < solidend; i++) {
      assert(i < a->linelen);
      OUTPUT(i) |= nonogram_SOLID;
    }
    for ( ; i < dotend; i++) {
      assert(i < a->linelen);
      OUTPUT(i) |= nonogram_DOT;
    }
  }
}
#endif
