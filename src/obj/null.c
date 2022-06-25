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
 *  Contact Steven Simpson <ss@comp.lancs.ac.uk>
 */

#include <stdio.h>
#include <stdlib.h>

#include "nonogram.h"
#include "internal.h"

#ifndef nonogram_LOGLEVEL
#define nonogram_LOGLEVEL 1
#endif

static void prep(void *, const struct nonogram_lim *, struct nonogram_req *);
static int init(void *, struct nonogram_ws *ws,
		const struct nonogram_initargs *a);

const struct nonogram_linesuite nonogram_nullsuite = {
  &prep, &init, 0, 0
};

static void prep(void *c, const struct nonogram_lim *l, struct nonogram_req *r)
{
  UNUSED(c);
  UNUSED(l);
  r->byte = 0;
  r->size = 0;
  r->ptrdiff = 0;
  r->nonogram_size = 0;
}

static int init(void *ct, struct nonogram_ws *c,
		const struct nonogram_initargs *a)
{
  size_t i;

  UNUSED(ct);
  UNUSED(c);
  /* reveal nothing */
  for (i = 0; i < a->linelen; i++)
    switch (a->line[i * a->linestep]) {
    case nonogram_DOT:
    case nonogram_SOLID:
      a->result[i * a->resultstep] = a->line[i * a->linestep];
      break;
    default:
      a->result[i * a->resultstep] = nonogram_BOTH;
      break;
    }

  switch (nonogram_checkline(a->rule, a->rulelen, a->rulestep,
			     a->line, a->linelen, a->linestep)) {
  case 0:
  case 1:
    *a->fits = 1;
    break;
  default:
    *a->fits = 0;
    break;
  }

  return false;
}
