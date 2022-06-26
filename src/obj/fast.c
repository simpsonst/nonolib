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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "nonogram.h"
#include "internal.h"

#ifndef nonogram_LOGLEVEL
#define nonogram_LOGLEVEL 1
#endif

static void prep(void *c,
                 const struct nonogram_lim *l, struct nonogram_req *r)
{
  UNUSED(c);
  r->byte = 0;
  r->size = 0;
  r->ptrdiff = l->maxrule;
  r->nonogram_size = l->maxrule * 2;
}

static int solve(const nonogram_cell *line,
                 size_t linelen, ptrdiff_t linestep,
                 const nonogram_sizetype *rule,
                 size_t rulelen, ptrdiff_t rulestep,
                 nonogram_cell *work, ptrdiff_t workstep,
                 nonogram_sizetype *lpos, nonogram_sizetype *rpos,
                 ptrdiff_t *solid, FILE *log, int level, int indent)
{
  /* nonogram_size */
  size_t i, j, k;
  nonogram_cell *cp;

  /* bug fix for Easy C */
  const nonogram_cell *rline = line + (linelen - 1) * linestep;

#if false
  if (log) {
    fprintf(log, "%*sLine range = %p to %p\n", indent, "",
            line, line + (linelen - 1) * linestep);
  }
#endif

  if (rulelen == 1 && *rule == 0) rulelen = 0;

  if (!nonogram_push(line, linelen, linestep, rule, rulelen, rulestep,
                     lpos, 1, solid, log, level, indent))
    return 0;

#if nonogram_LOGLEVEL > 1
  if (log && level > 0) {
    nonogram_sizetype x = 0;
    size_t bl;
    fprintf(log, "%*s  Left:", indent, "");
    for (bl = 0; bl < rulelen; bl++)
      fprintf(log, " (%2" nonogram_PRIuSIZE " + %2" nonogram_PRIuSIZE ")",
              lpos[bl], rule[bl * rulestep]);
    fprintf(log, "\n%*s  Left: >", indent, "");
    for (bl = 0; bl < rulelen; bl++) {
      while (x < lpos[bl])
        putc('-', log), x++;
      while (x < lpos[bl] + rule[bl * rulestep])
        putc('#', log), x++;
    }
    while (x < linelen)
      putc('-', log), x++;
    fprintf(log, "<\n");
  }
#endif

#if false
  if (log) {
    nonogram_sizetype x;
    fprintf(log, "%*sMiddle: >", indent, "");
    for (x = 0; x < linelen; x++)
      switch (line[x * linestep]) {
      case nonogram_BLANK:
        putc(' ', log);
        break;
      case nonogram_DOT:
        putc('-', log);
        break;
      case nonogram_SOLID:
        putc('#', log);
        break;
      default:
        putc('?', log);
        break;
      }
    fprintf(log, "<\n");
  }
#endif

#if false
  if (log) {
    fprintf(log, "%*sLine range = %p to %p\n", indent, "",
            line, line + (linelen - 1) * linestep);
  }
#endif

  if (!nonogram_push(rline, linelen, -linestep,
                     rule + (rulelen - 1) * rulestep,
                     rulelen, -rulestep, rpos + (rulelen - 1), -1, solid,
                     log, level, indent))
    return 0;
#if nonogram_LOGLEVEL > 1
  if (log && level > 0) {
    nonogram_sizetype x = 0;
    size_t bl;
    fprintf(log, "%*s Right:", indent, "");
    for (bl = 0; bl < rulelen; bl++)
      fprintf(log, " (%2" nonogram_PRIuSIZE " + %2" nonogram_PRIuSIZE ")",
              rpos[bl], rule[bl * rulestep]);
    fprintf(log, "\n%*s Right: >", indent, "");
    for (bl = 0; bl < rulelen; bl++) {
      while (x < linelen - rpos[bl] - rule[bl * rulestep])
        putc('-', log), x++;
      while (x < linelen - rpos[bl])
        putc('#', log), x++;
    }
    while (x < linelen)
      putc('-', log), x++;
    fprintf(log, "<\n");
  }
#endif

  for (i = j = 0, cp = work; i < rulelen; i++) {
    rpos[i] = linelen - rpos[i] - rule[i * rulestep];
    k = lpos[i];
    while (j < k)
      *cp = nonogram_DOT, j++, cp += workstep;
    k = rpos[i];
    while (j < k)
      *cp = nonogram_BOTH, j++, cp += workstep;
    k = lpos[i] + rule[i * rulestep];
    while (j < k)
      *cp = nonogram_SOLID, j++, cp += workstep;
    k = rpos[i] + rule[i * rulestep];
    while (j < k)
      *cp = nonogram_BOTH, j++, cp += workstep;
  }
  while (j < linelen)
    *cp = nonogram_DOT, j++, cp += workstep;
  for (j = 0; j < linelen; j++)
    if (line[j * linestep] != nonogram_BLANK)
      work[j * workstep] = line[j * linestep];

  if (log && level > 0) {
    fprintf(log, "%*sLeft:     >", indent, "");
    for (i = j = 0; i < rulelen; i++) {
      k = lpos[i];
      while (j < k)
        j++, fputc('-', log);
      k += rule[i * rulestep];
      while (j < k)
        j++, fputc('#', log);
    }
    while (j < linelen)
      j++, fputc('-', log);
    fprintf(log, "<\n%*sRight:    >", indent, "");
    for (i = j = 0; i < rulelen; i++) {
      k = rpos[i];
      while (j < k)
        j++, fputc('-', log);
      k += rule[i * rulestep];
      while (j < k)
        j++, fputc('#', log);
    }
    while (j < linelen)
      j++, fputc('-', log);
    fprintf(log, "<\n");
  }

  return 1;  
}

static int init(void *ct, struct nonogram_ws *c,
                const struct nonogram_initargs *a)
{
  UNUSED(ct);
  *a->fits = solve(a->line, a->linelen, a->linestep,
                   a->rule, a->rulelen, a->rulestep,
                   a->result, a->resultstep,
                   c->nonogram_size, c->nonogram_size + a->rulelen,
                   c->ptrdiff, a->log->file, a->log->level, a->log->indent);
  return false;
}

const struct nonogram_linesuite nonogram_fastsuite = {
  &prep, &init, 0, 0
};
