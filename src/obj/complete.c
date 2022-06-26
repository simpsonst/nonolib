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

static void prep(void *,
		 const struct nonogram_lim *, struct nonogram_req *);
static int init(void *, struct nonogram_ws *ws,
		const struct nonogram_initargs *);
static int step(void *, void *ws);

const struct nonogram_linesuite nonogram_completesuite = {
  &prep,
  &init,
  &step,
  0
};

static void prep(void *c,
		 const struct nonogram_lim *l, struct nonogram_req *r)
{
  UNUSED(c);
  r->byte = sizeof(nonogram_completework);
  r->ptrdiff = 0;
  r->size = 0;
  r->nonogram_size = l->maxrule * 2;
}

static int init(void *ct, struct nonogram_ws *ws,
		const struct nonogram_initargs *a)
{
  nonogram_completework *c = ws->byte;
  unsigned long i;

  UNUSED(ct);

  c->log = a->log;
  c->fits = a->fits;
  c->ln = a->line;
  c->lnlen = a->linelen;
  c->lni = a->linestep;

  c->ru = a->rule;
  c->rulen = a->rulelen;
  c->rui = a->rulestep;
  c->re = a->result;
  c->rei = a->resultstep;
  c->pos = ws->nonogram_size;
  c->limit = c->pos + c->rulen;
  c->remunk = 0;
  c->blockno = 0;
  c->move_back = false;
  c->pos[c->blockno] = 0;

  for (i = 0; i < c->lnlen; i++)
    c->remunk +=
      (c->re[i * c->rei] = c->ln[i * c->lni]) == nonogram_BLANK;

  if (c->rulen > 0) {
    c->limit[c->rulen - 1] = c->lnlen -
      c->ru[c->rui * (c->rulen - 1)] + 1;
    for (i = c->rulen - 1; i > 0; i--)
      c->limit[i - 1] = c->limit[i] - c->ru[c->rui * (i - 1)] - 1;
  }

  if (c->remunk == 0) {
    *c->fits =
      !nonogram_checkline(c->ru, c->rulen, c->rui, c->ln, c->lnlen, c->lni);
    return false;
  } else {
    *c->fits = 0;
    return true;
  }
}

static void merge(nonogram_cell *rec, size_t len, ptrdiff_t ri, int *blank,
		  const nonogram_sizetype *rule, size_t rulelen,
		  ptrdiff_t rstep, const nonogram_sizetype *pos)
{
  nonogram_sizetype i;
  size_t ruleno;
  nonogram_cell old;

  i = 0;
  for (ruleno = 0; ruleno < rulelen; ruleno++) {
    while (i < pos[ruleno]) {
      old = rec[i * ri];
      rec[i * ri] |= nonogram_DOT;
      if (rec[i * ri] == nonogram_BOTH && old != nonogram_BOTH)
        (*blank)--;
      i++;
    }
    while (i < pos[ruleno] + rule[ruleno * rstep]) {
      old = rec[i * ri];
      rec[i * ri] |= nonogram_SOLID;
      if (rec[i * ri] == nonogram_BOTH && old != nonogram_BOTH)
        (*blank)--;
      i++;
    }
  }
  while (i < len) {
    old = rec[i * ri];
    rec[i * ri] |= nonogram_DOT;
    if (rec[i * ri] == nonogram_BOTH && old != nonogram_BOTH)
      (*blank)--;
    i++;
  }
}

static int step(void *ws, void *vc)
{
  nonogram_completework *c = vc;
  nonogram_sizetype last, dot;
  int solid;

  UNUSED(ws);

  if (c->move_back || c->remunk <= 0) {
    c->move_back = false;
    if (c->blockno > 0) {
      c->blockno--;
      /* unplaced(c->ru[c->rui * c->blockno], c->pos[c->blockno]); */
#if false
      if (c->log->file && c->log->level > 0) {
        fprintf(c->log->file, "%*s%6d: >", c->log->indent, "", -c->blockno);
        fprintf(c->log->file, "%*s", c->pos[c->blockno], "");
        fprintf(c->log->file, "%0*d", c->ru[c->blockno * c->rui], 0);
        fprintf(c->log->file, "%*s", c->lnlen - c->pos[c->blockno] -
		c->ru[c->blockno * c->rui], "");
        fprintf(c->log->file, "<\n");
      }
#endif
      if (c->remunk <= 0 ||
          c->ln[c->lni * c->pos[c->blockno]] == nonogram_SOLID) {
        c->move_back = true;
        return true;
      }
      c->pos[c->blockno]++;
    } else {
      return false;
    }
  }

  if (c->blockno == c->rulen) {
    /* all blocks have fitted in */
    nonogram_sizetype bp;
#if nonogram_LOGLEVEL > 1
    size_t i, j, k;
#endif

    /* check there are no remaining solids */
    bp = (c->blockno > 0 ?
	 c->pos[c->blockno - 1] + c->ru[c->rui * (c->blockno - 1)] + 1 : 0);
    while (bp < c->lnlen) {
      if (c->ln[c->lni * bp] == nonogram_SOLID) {
        c->move_back = true;
        return true;
      }
      bp++;
    }

#if nonogram_LOGLEVEL > 1
    /* log the fit */
    if (c->log->file && c->log->level > 0) {
      fprintf(c->log->file, "%*s   Fit: >", c->log->indent, "");
      for (i = j = 0; i < c->rulen; i++) {
	k = c->pos[i];
	while (j < k)
	  j++, fputc('-', c->log->file);
	k += c->ru[c->rui * i];
	while (j < k)
	  j++, fputc('#', c->log->file);
      }
      while (j < c->lnlen)
	j++, fputc('-', c->log->file);
      fprintf(c->log->file, "<\n");
    }
#endif

    /* merge with work */
    /* merge();  */
    merge(c->re, c->lnlen, c->rei, &c->remunk,
	  c->ru, c->rulen, c->rui, c->pos);
    (*c->fits)++;
    c->move_back = true;
    return true;
  }

  /* find first non-dot, upto c->blockno's limit */
  while (c->pos[c->blockno] < c->limit[c->blockno] &&
         c->ln[c->lni * c->pos[c->blockno]] == nonogram_DOT)
    c->pos[c->blockno]++;
  /* if limit exceeded, move back to previous block */
  if (c->pos[c->blockno] >= c->limit[c->blockno]) {
    c->move_back = true;
    if (0 && c->log->file) {
      fprintf(c->log->file, "%*sBack-up: no room\n",
              c->log->indent, "");
      fflush(c->log->file);
    }
    return true;
  }

  /* find first covered dot, while looking for solids before it */
  dot = c->pos[c->blockno];
  solid = false;
  last = c->pos[c->blockno] + c->ru[c->rui * c->blockno];
  while (dot < last && c->ln[c->lni * dot] != nonogram_DOT) {
    solid = solid || c->ln[c->lni * dot] == nonogram_SOLID;
    dot++;
  }

  /* dot covered? */
  if (dot < last) {
    /* solid covered before dot? */
    if (solid) {
      /* failure */
      c->move_back = true;
#if false
      if (0 && c->log->file) {
        fprintf(c->log->file, "%*sBack-up: covered solid before dot\n",
                c->log->indent, "");
        fflush(c->log->file);
      }
#endif
      return true;
    } else {
      /* move to beyond dot */
      c->pos[c->blockno] = dot + 1;
      return true;
    }
  }

  /* move block on until there is no solid immediately after it,
     or a solid appears before it */
  while (c->pos[c->blockno] < c->limit[c->blockno] - 1 &&
         c->ln[c->lni * last] == nonogram_SOLID &&
         c->ln[c->lni * c->pos[c->blockno]] != nonogram_SOLID)
    last++, c->pos[c->blockno]++;

  /* if there is still a solid after it, it's wrong */
  if (last < c->lnlen &&
      c->ln[c->lni * last] == nonogram_SOLID) {
    /* failure */
#if false
    if (0 && c->log->file) {
      fprintf(c->log->file, "%*sBack-up: closing dot covers solid\n",
              c->log->indent, "");
      fflush(c->log->file);
    }
#endif
    c->pos[c->blockno]++;
    c->move_back = true;
    return true;
  }

  /* we're ready to try next block */
  /* placed(c->ru[c->rui * c->blockno], c->pos[c->blockno]); */
#if false
  if (0 && c->log->file) {
    fprintf(c->log->file, "%*s%6d: >", c->log->indent, "", c->blockno);
    fprintf(c->log->file, "%*s", (int) c->pos[c->blockno], "");
    fprintf(c->log->file, "%0*d", (int) c->ru[c->blockno * c->rui], 0);
    fprintf(c->log->file, "%*s", (int) (c->lnlen - c->pos[c->blockno] -
					c->ru[c->blockno * c->rui]), "");
    fprintf(c->log->file, "<\n");
  }
#endif
  if (++c->blockno < c->rulen)
    c->pos[c->blockno] = last + 1;
  return true;
}
