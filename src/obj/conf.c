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

#include <stdlib.h>

#include "nonogram.h"

int nonogram_setlinesolver(nonogram_solver *c,
			   nonogram_level lvl, const char *n,
			   const struct nonogram_linesuite *s, void *conf)
{
  if (c->puzzle) return -1;
  if (lvl < 1 || lvl > c->levels) return -1;
  c->linesolver[lvl - 1].suite = s;
  c->linesolver[lvl - 1].context = conf;
  c->linesolver[lvl - 1].name = n;
  return 0;
}

int nonogram_setlinesolvers(nonogram_solver *c, nonogram_level levels)
{
  struct nonogram_lsnt *tmp;

  if (c->puzzle) return -1;
  tmp = realloc(c->linesolver, sizeof(struct nonogram_lsnt) * levels);
  if (!tmp) return -1;
  c->linesolver = tmp;
  while (c->levels < levels)
    c->linesolver[c->levels++].suite = 0;
  c->levels = levels;
  return 0;
}

nonogram_level nonogram_getlinesolvers(nonogram_solver *c)
{
  return c ? c->levels : 0;
}

int nonogram_setclient(nonogram_solver *c,
		       const struct nonogram_client *client,
		       void *client_data)
{
  if (c->puzzle) return -1;
  c->client_data = client_data;
  c->client = client;
  return 0;
}

int nonogram_setdisplay(nonogram_solver *c,
			const struct nonogram_display *display,
			void *display_data)
{
  if (c->puzzle) return -1;
  c->display_data = display_data;
  c->display = display;
  return 0;
}

int nonogram_setlog(nonogram_solver *c, FILE *logfile, int indent, int lvl)
{
  if (c->puzzle) return -1;
  c->log.file = logfile;
  c->log.indent = indent;
  c->log.level = lvl;
  return 0;
}

int nonogram_setalgo(nonogram_solver *c, int i)
{
  if (c->puzzle) return -1;

  switch (i) {
  case nonogram_AFAST:
    nonogram_setlinesolvers(c, 1);
    nonogram_setlinesolver(c, 1, "fast", &nonogram_fastsuite, 0);
    break;

  case nonogram_ACOMPLETE:
    nonogram_setlinesolvers(c, 1);
    nonogram_setlinesolver(c, 1, "complete", &nonogram_completesuite, 0);
    break;

  case nonogram_ANULL:
    nonogram_setlinesolvers(c, 1);
    nonogram_setlinesolver(c, 1, "null", &nonogram_nullsuite, 0);
    break;

  case nonogram_AHYBRID:
  default:
    nonogram_setlinesolvers(c, 2);
    nonogram_setlinesolver(c, 1, "complete", &nonogram_completesuite, 0);
    nonogram_setlinesolver(c, 2, "fast", &nonogram_fastsuite, 0);
    break;

  case nonogram_AOLSAK:
    nonogram_setlinesolvers(c, 1);
    nonogram_setlinesolver(c, 1, "olsak", &nonogram_olsaksuite, 0);
    break;

  case nonogram_AFASTOLSAK:
    nonogram_setlinesolvers(c, 2);
    nonogram_setlinesolver(c, 1, "olsak", &nonogram_olsaksuite, 0);
    nonogram_setlinesolver(c, 2, "fast", &nonogram_fastsuite, 0);
    break;

  case nonogram_AFASTODDONES:
    nonogram_setlinesolvers(c, 2);
    nonogram_setlinesolver(c, 1, "odd-ones", &nonogram_oddonessuite, 0);
    nonogram_setlinesolver(c, 2, "fast", &nonogram_fastsuite, 0);
    break;

  case nonogram_AFASTOLSAKCOMPLETE:
    nonogram_setlinesolvers(c, 3);
    nonogram_setlinesolver(c, 1, "complete", &nonogram_completesuite, 0);
    nonogram_setlinesolver(c, 2, "olsak", &nonogram_olsaksuite, 0);
    nonogram_setlinesolver(c, 3, "fast", &nonogram_fastsuite, 0);
    break;

  case nonogram_AFASTODDONESCOMPLETE:
    nonogram_setlinesolvers(c, 3);
    nonogram_setlinesolver(c, 1, "complete", &nonogram_completesuite, 0);
    nonogram_setlinesolver(c, 2, "odd-ones", &nonogram_oddonessuite, 0);
    nonogram_setlinesolver(c, 3, "fast", &nonogram_fastsuite, 0);
    break;

  case nonogram_AFCOMP:
    nonogram_setlinesolvers(c, 1);
    nonogram_setlinesolver(c, 1, "fcomp", &nonogram_fcompsuite, 0);
    break;

  case nonogram_AFFCOMP:
    nonogram_setlinesolvers(c, 2);
    nonogram_setlinesolver(c, 1, "fcomp", &nonogram_fcompsuite, 0);
    nonogram_setlinesolver(c, 2, "fast", &nonogram_fastsuite, 0);
    break;
  }
  return 0;
}
