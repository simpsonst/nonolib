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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "nonogram.h"

#ifndef nonogram_LOGLEVEL
#define nonogram_LOGLEVEL 1
#endif

int nonogram_checkline(const nonogram_sizetype *r,
		       size_t rlen, ptrdiff_t rstep,
                       const nonogram_cell *st, size_t len, ptrdiff_t step)
{
  unsigned rno = 0, index = 0, blen = 0;

  while (index < len) {
    int c = st[index * step];

    if (c != nonogram_DOT && c != nonogram_SOLID)
      return 1;

    switch (rno % 2) {
    case 0:
      if (c == nonogram_SOLID) {
        blen = 1;
        rno++;
        if (rno / 2 >= rlen)
          return -1;
      }
      index++;
      break;
    case 1:
      if (c == nonogram_DOT) {
        if (blen != r[(rno / 2) * rstep])
          return -1;
        rno++;
      } else {
        blen++;
        if (blen > r[(rno / 2) * rstep])
          return -1;
      }
      index++;
      break;
    }
  }
  if (rno % 2) {
    if (blen != r[(rno / 2) * rstep])
      return -1;
    rno++;
  }
  if (rno != 2 * rlen) return -1;

  return 0;
}

/* line, linelen and linestep, rule, rulelen and rulestep describe the
   line being solved.  The results will appear in pos[0],
   pos[posstep], ...  solid is workspace, an array of at least rulelen
   elements.  Return 0 if inconsistency detected; 1 if okay. */
int nonogram_push(const nonogram_cell *line,
		  size_t linelen, ptrdiff_t linestep,
		  const nonogram_sizetype *rule,
		  size_t rulelen, ptrdiff_t rulestep,
		  nonogram_sizetype *pos, ptrdiff_t posstep,
		  ptrdiff_t *solid, FILE *log, int level, int indent)
{
  /* working variables */
  size_t i;
  const nonogram_cell *cp, *cp2;
  nonogram_sizetype posv, rulev;

  /* initial state */
  size_t block = 0;

  if (rulelen > 0) pos[block * posstep] = 0;

#if nonogram_LOGLEVEL > 1
  if (log && level > 1) {
    size_t rn;
#if false
    fprintf(log, "%*sLine range: %8p to %8p\n", indent, "",
            line, line + (linelen - 1) * linestep);
#endif
    fprintf(log, "%*sPushing rule:", indent, "");
    for (rn = 0; rn < rulelen; rn++)
      fprintf(log, " %" nonogram_PRIuSIZE, rule[rn * rulestep]);
    fprintf(log, "\n%*sPushing line: >", indent, "");
    for (rn = 0; rn < linelen; rn++)
      switch (line[rn * linestep]) {
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
#else
  log = log;
  level = level;
  indent = indent;
#endif

  while (block < rulelen) {
    /* find first/next non-dot:
       stop if block won't fit into remainder of line */
    posv = pos[block * posstep];
    rulev = rule[block * rulestep];
#if nonogram_LOGLEVEL > 2
    if (log && level > 1)
      fprintf(log, "%*s     start %2d >%*s%0*d\n",
              indent, "", (int) block, (int) posv, "", (int) rulev, 0);
#endif
    cp = line + (posv * linestep);
    while (posv + rulev < linelen && *cp == nonogram_DOT)
      posv++, cp += linestep;
    pos[block * posstep] = posv;

#if nonogram_LOGLEVEL > 2
    if (log && level > 1)
      fprintf(log, "%*s       end %2d >%*s%0*d\n",
              indent, "", (int) block, (int) posv, "", (int) rulev, 0);
#endif
    /* no room left */
    if (posv + rulev > linelen || *cp == nonogram_DOT) {
#if nonogram_LOGLEVEL > 2
      if (log && level > 1)
        fprintf(log, "%*s    no fit %2d\n", indent, "", (int) block);
#endif
      return 0;
    }

    /* assume current position doesn't cover a solid */
    solid[block] = -1;

    /* check if the block fits in before the next dot;
       monitor for passing over a solid */
    i = 0;
    while (i < rulev && *cp != nonogram_DOT) {
      if (solid[block] < 0 && *cp == nonogram_SOLID) {
#if nonogram_LOGLEVEL > 2
        if (log && level > 1)
          fprintf(log, "%*s     solid %2d >%*s#\n",
                  indent, "", (int) block, (int) (posv + i), "");
#endif
        solid[block] = i;
      }
      i++, cp += linestep;
    }

    /* if a dot was encountered... */
    if (i < rulev) {
#if nonogram_LOGLEVEL > 2
      if (log && level > 1)
        fprintf(log, "%*s       dot %2d >%*s-\n",
                indent, "", (int) block, (int) (posv + i), "");
#endif
      /* if a solid is covered, get an earlier block to fit */
      if (solid[block] >= 0) {
        /* if all previous blocks cover solids, then fail */

        /* find an earlier block that can be moved to cover the solid
           covered by the next block without uncovering its own */
	do {
	  if (block == 0)
	    return 0;
	  block--;
	} while (solid[block] >= 0 &&
		 pos[(block + 1) * posstep] + solid[block + 1]
		 - rule[block * rulestep] + 1 >
		 pos[block * posstep] + solid[block]);

        /* set the block near to the position of the next block,
           so that it just overlaps the solid, and try again */
        pos[block * posstep] = pos[(block + 1) * posstep] +
          solid[block + 1] - rule[block * rulestep] + 1;
        continue;
      }

      /* otherwise, simply move to the dot, and try again */
      pos[block * posstep] += i;
      continue;
    }

    /* now check to see if the end of the current block touches an
       existing solid - if so, the block must be shuffled further to
       ensure it overlaps the solid, but no solid may emerge from the
       other end */
    posv = pos[block * posstep];
    cp = line + (posv * linestep);
    cp2 = cp + rulev * linestep;
    if (posv + rulev < linelen && *cp2 == nonogram_SOLID && solid[block] < 0)
      solid[block] = rulev;
    while (posv + rulev < linelen &&
           *cp2 == nonogram_SOLID &&
           *cp != nonogram_SOLID)
      posv++, cp += linestep, cp2 += linestep, solid[block]--;
    pos[block * posstep] = posv;
#if nonogram_LOGLEVEL > 2
    if (log && level > 1)
      fprintf(log, "%*s   shuffle %2d >%*s%0*d\n",
              indent, "", (int) block, (int) posv, "", (int) rulev, 0);
#endif

    /* if there's still a solid immediately after the block, there's
       an error, so find an earlier block to move */
    if (posv + rulev < linelen && *cp2 == nonogram_SOLID) {
#if nonogram_LOGLEVEL > 2
      if (log && level > 1)
        fprintf(log, "%*s stretched %2d >%*s#\n",
                indent, "", (int) block, (int) (posv + i), "");
#endif
      /* if all previous blocks cover solids, then fail */

      /* find an earlier block that isn't covering a solid */
      do {
	if (block == 0)
	  return 0;
	block--;
      } while (solid[block] >= 0 &&
	       pos[(block + 1) * posstep] + solid[block + 1]
	       - rule[block * rulestep] + 1 >
	       pos[block * posstep] + solid[block]);

      /* set the block near to the position of the next block,
         so that it just overlaps the solid, and try again */
      pos[block * posstep] = pos[(block + 1) * posstep] +
        solid[block + 1] - rule[block * rulestep] + 1;
      continue;
    }

    /* the block is in place, so try the next */
    posv = pos[block * posstep] + 1 + rule[block * rulestep];
    if (block + 1 < rulelen) {
      pos[++block * posstep] = posv;
    } else {
      /* no more blocks, so just check for any remaining solids */
      cp = line + (posv * linestep);
      while (posv < linelen && *cp != nonogram_SOLID)
        posv++, cp += linestep;
      /* if a solid was found... */
      if (posv < linelen) {
#if nonogram_LOGLEVEL > 2
        if (log && level > 1)
          fprintf(log, "%*s     trailing >%*s#\n",
                  indent, "", (int) posv, "");
#endif

        /* move the block so it covers it, but check if solid
           becomes uncovered */
        if (solid[block] >= 0 &&
            posv - rulev + 1 > pos[block * posstep] + solid[block]) {
          /* a solid was uncovered, so look for an earlier block */

          /* if all previous blocks cover solids, then fail */

          /* find an earlier block that isn't covering a solid */
	  do {
	    if (block == 0)
	      return 0;
	    block--;
	  } while (solid[block] >= 0 &&
		   pos[(block + 1) * posstep] + solid[block + 1]
		   - rule[block * rulestep] + 1 >
		   pos[block * posstep] + solid[block]);

          /* set the block near to the position of the next block,
             so that it just overlaps the solid, and try again */
          pos[block * posstep] = pos[(block + 1) * posstep] +
            solid[block + 1] - rule[block * rulestep] + 1;
          continue;
        }
        /* solid[block] -= posv - rulev + 1 - pos[block * posstep]; */
        pos[block * posstep] = posv - rulev + 1;
        continue;
      }
      /* no solid found */
      block++;
    }
  }
  /* all okay */
  return 1;
}
