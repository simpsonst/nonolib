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

/**
 * This algorithm works in two main states, INVALID (initially) and
 * SLIDING, oscillating between them.
 *
 * During INVALID, blocks prior to the current block are considered to
 * be tentatively valid, and a position is sought after the prior
 * block to place the current block, such that no dot is covered, and
 * no solid is touched, and the block is completely within the line.
 * (Additionally, the final block must have no trailing solids.)
 *
 * Once all blocks are tentatively valid, the state as a whole is
 * valid, and the positions are recorded in the results as a possible
 * solution.  SLIDING then begins.
 *
 * During SLIDING, the current block is slid rightwards from one valid
 * position to another, recording the effect on the results.  It is
 * not allowed to touch the trailing block or exceed the line, nor to
 * expose a solid.  It will not start to hide a solid, as such a solid
 * must be between two blocks, and therefore the current state would
 * not be valid.  The block is allowed to jump a dot if there is
 * space.
 *
 * Having slid everything, SLIDING chooses the right-most block with a
 * solid, sets it as the current and the 'target', and switches to a
 * short-lived state, DRAWING.  A 'mininv' block number is reset (by
 * making it exceed the number of blocks).
 *
 * DRAWING aims to find a block earlier than the current block, such
 * that it will cover a solid currently hidden by the 'target'.  In
 * searching leftwards, if it passes a block covering a solid, that
 * block becomes the new target, so any chosen block will not be
 * jumping over a solid to cover a later one.  The chosen block is
 * moved to cover the target solid, thus rendering itself and later
 * blocks invalid.  If mininv is unset or greater than the chosen
 * block, it is set to the chosen block.  INVALID is set, with the
 * chosen block as current.
 *
 * During INVALID, if trailing solids are found, or a block exceeds
 * the line, blocks are restored to their last valid positions (in a
 * brief RESTORING state), and DRAWING is attempted with the current
 * block set to the earliest block that was restored (as given by
 * mininv), and the target set to the left-most of the set restored
 * that covers a solid.  This means that an even earlier block will no
 * be chosen to cover a solid.
 **/

#include <stdbool.h>
#include <assert.h>

#include "nonogram.h"
#include "internal.h"

static void prep(void *, const struct nonogram_lim *, struct nonogram_req *);
static int init(void *, struct nonogram_ws *ws,
                const struct nonogram_initargs *a);
static int step(void *, void *ws);

const struct nonogram_linesuite nonogram_fcompsuite = {
  &prep, &init, &step, 0
};

/*
 * 'remunk' is initially the number of unknown cells.  Whenever a cell
 * in the result becomes 'BOTH', it is decremented.  If it ever
 * becomes zero, then we know straight away that no information can be
 * obtained from this line.
 *
 * 'block' is the current block number in focus.  We are normally
 * attempting to slide this block right one cell at a time, from one
 * valid position to another.  Or we are trying to find a valid
 * position for it, when all to its left are already tentatively
 * valid.
 *
 * 'base' identifies the left-most block which is not yet known to
 * have reached its right-most position.  Blocks from 'max' onwards
 * are known to have reached their right-most positions.  Hence, only
 * blocks in the range [base, max) actually need to be handled.
 *
 * Before DRAWING, 'target' must be set to indicate a block that
 * covers a solid, and 'block' must be no greater than it.  DRAWING
 * will seek a block earlier than 'block' which could be brought up to
 * cover the solid currently covered by 'target'.  Note that 'target'
 * is set to 'block' if that block is found to cover a solid.
 *
 * During DRAWING stage, 'mininv' holds the minimum block number that
 * might have changed, i.e. blocks to the left of this did not change
 * since the last valid state.  During INVALID, if an inconsistent
 * state is detected (such as a block going beyond the end of a line),
 * blocks ['mininv', 'block'] must be reset, and 'target' is chosen as
 * the left-most of these that covers a solid.
 */

#if 1
#define printf(...) ((void) 0)
#define putchar(X) ((void) 0)
#endif

typedef enum {
  INVALID,
  SLIDING,
  DRAWING,
  RESTORING,
} stepmode;

#ifdef VLA_IN_STRUCT

/* This was is not standard C, not even C99.  Certain versions of GCC
   allowed it, but newer versions forbid it in C99 mode, and one
   version even crashed when I used gnu99 instead. */

#define CONTEXT(MR) \
struct { \
  struct nonogram_initargs a; \
 \
  size_t remunk, block, base, max, mininv, target; \
  stepmode mode; \
 \
  nonogram_sizetype pos[MR], oldpos[MR], solid[MR], oldsolid[MR], maxpos; \
}

#define RULE(I) (a->rule[(I) * a->rulestep])
#define POS(I) (ctxt->pos[I])
#define OLDPOS(I) (ctxt->oldpos[I])
#define SOLID(I) (ctxt->solid[I])
#define OLDSOLID(I) (ctxt->oldsolid[I])
#define B (ctxt->block)
#define LEN (a->linelen)
#define CELL(X) (a->line[(X) * a->linestep])
#define RESULT(X) (a->result[(X) * a->resultstep])
#define BASE (ctxt->base)
#define MAX (ctxt->max)
#define MININV (ctxt->mininv)
#define RULES (a->rulelen)


#else

/* This might be a more portable version.  We use some macros in
   "internal.h" to compute alignment requirements. */

struct hdr {
  struct nonogram_initargs a;

  size_t remunk, block, base, max, mininv, target;
  stepmode mode;

  nonogram_sizetype maxpos;
};

#define CONTEXTSIZE(MR) \
  (align(sizeof(struct hdr), nonogram_sizetype) \
   + 4 * sizeof(nonogram_sizetype) * (MR))
#define POSOFFSET(HDRP) \
  ((nonogram_sizetype *) \
   ((char *) (HDRP) + align(sizeof(struct hdr), nonogram_sizetype)))
#define OLDPOSOFFSET(HDRP, MR) (POSOFFSET(HDRP)+(MR))
#define SOLIDOFFSET(HDRP, MR) (OLDPOSOFFSET((HDRP),(MR))+(MR))
#define OLDSOLIDOFFSET(HDRP, MR) (SOLIDOFFSET((HDRP),(MR))+(MR))

#define RULE(I) (a->rule[(I) * a->rulestep])
#define B (ctxt->block)
#define LEN (a->linelen)
#define CELL(X) (a->line[(X) * a->linestep])
#define RESULT(X) (a->result[(X) * a->resultstep])
#define BASE (ctxt->base)
#define MAX (ctxt->max)
#define MININV (ctxt->mininv)
#define RULES (a->rulelen)

#define POS(I) (POSOFFSET(ctxt)[I])
#define OLDPOS(I) (OLDPOSOFFSET(ctxt,RULES)[I])
#define SOLID(I) (SOLIDOFFSET(ctxt,RULES)[I])
#define OLDSOLID(I) (OLDSOLIDOFFSET(ctxt,RULES)[I])



#endif


/* Return true if there are no cells remaining from which information
   could be obtained. */
static int record_section(const struct nonogram_initargs *a,
			  nonogram_sizetype from,
			  nonogram_sizetype to,
			  nonogram_cell v,
			  size_t *remunk)
{
  ++*a->fits;
  for (nonogram_sizetype i = from; i < to; i++) {
    assert(i < LEN);
    if (CELL(i) != nonogram_BLANK)
      continue;
    nonogram_cell *cp = &RESULT(i);
    if (*cp & v)
      continue;
    *cp |= v;
    assert(*cp < 4);
    if (*cp == nonogram_BOTH)
      if (--*remunk == 0)
	return true;
  }

  printf("Accumulate>");
  for (nonogram_sizetype i = 0; i < LEN; i++) {
    assert(i < LEN);
    if (i < from || i >= to) {
      putchar('.');
      continue;
    }
    switch (RESULT(i)) {
    case nonogram_BLANK:
      putchar(' ');
      break;
    case nonogram_DOT:
      putchar('-');
      break;
    case nonogram_SOLID:
      putchar('#');
      break;
    case nonogram_BOTH:
      putchar('+');
      break;
    default:
      putchar('?');
      break;
    }
  }
  printf("<\n");
  return false;
}

static int merge1(const struct nonogram_initargs *a,
		  nonogram_sizetype *pos,
		  nonogram_sizetype *oldpos,
		  nonogram_sizetype *solid,
		  nonogram_sizetype *oldsolid,
		  size_t *remunk,
		  size_t b)
{
  if (record_section(a, oldpos[b], pos[b], nonogram_DOT, remunk) ||
      record_section(a, oldpos[b] + RULE(b), pos[b] + RULE(b),
		     nonogram_SOLID, remunk))
    return true;

  oldpos[b] = pos[b];
  oldsolid[b] = solid[b];
  return false;
}

static int record_sections(const struct nonogram_initargs *a,
			   size_t base, size_t max,
			   nonogram_sizetype *pos,
			   nonogram_sizetype *oldpos,
			   nonogram_sizetype *solid,
			   nonogram_sizetype *oldsolid,
			   size_t *remunk)
{
  // Where does the section of dots before the first block start?
  nonogram_sizetype left = base > 0 ? pos[base - 1] + RULE(base - 1) : 0;

  for (size_t b = base; b < max; b++) {
    // Mark the dots leading up to the block.
    if (record_section(a, left, pos[b], nonogram_DOT, remunk))
      return true;

    // Where does this block end, and the next set of dots start?
    left = pos[b] + RULE(b);

    // Mark the solids of this block.
    if (record_section(a, pos[b], left, nonogram_SOLID, remunk))
      return true;

    oldpos[b] = pos[b];
    oldsolid[b] = solid[b];
  }

  // Mark the trailing dots.
  return record_section(a, left,
			max == RULES ? LEN : pos[max],
			nonogram_DOT, remunk);
}

void prep(void *vp, const struct nonogram_lim *lim, struct nonogram_req *req)
{
#ifdef VLA_IN_STRUCT
  CONTEXT(lim->maxrule) ctxt;
  req->byte = sizeof ctxt;
#else
  req->byte = CONTEXTSIZE(lim->maxrule);
#endif
}

int init(void *vp, struct nonogram_ws *ws, const struct nonogram_initargs *a)
{
  printf("\n\n\nNEW PUZZLE!\n");

  *a->fits = 0;

  // Handle the special case of an empty rule.
  if (RULES == 0) {
    for (nonogram_sizetype i = 0; i < LEN; i++) {
      if (CELL(i) == nonogram_DOT)
	continue;
      if (CELL(i) != nonogram_BLANK)
	return false;
      RESULT(i) = nonogram_DOT;
    }
    *a->fits = 1;
    return false;
  }

#ifdef VLA_IN_STRUCT
  CONTEXT(RULES) *ctxt = ws->byte;
#else
  struct hdr *ctxt = ws->byte;
#endif
  ctxt->a = *a;


  // The left-most block that hasn't been fixed yet is 0.
  BASE = 0;

  // No blocks on the right have fixed either.
  MAX = RULES;
  ctxt->maxpos = LEN;

  // Record how many cells are left that have not yet been determined
  // to be 'BOTH'.
  ctxt->remunk = 0;
  printf("X:%8zu>", LEN);
  for (nonogram_sizetype i = 0; i < LEN; i++) {
    if (CELL(i) == nonogram_BLANK) {
      ctxt->remunk++;
      RESULT(i) = nonogram_BLANK;
    } else
      RESULT(i) = CELL(i);

    switch (CELL(i)) {
    case nonogram_BLANK:
      putchar(' ');
      break;
    case nonogram_DOT:
      putchar('-');
      break;
    case nonogram_SOLID:
      putchar('#');
      break;
    default:
      putchar('?');
      break;
    }
  }
  printf("<\n");
  printf("          >");
  for (nonogram_sizetype i = 0; i < LEN; i++)
    putchar('0' + i % 10);
  printf("<\n");

  // No blocks are valid to start with, and their minimum positions
  // are all zero.
  ctxt->mode = INVALID;
  B = 0;
  MININV = 0;
  printf("rule: ");
  for (size_t b = 0; b < RULES; b++) {
    printf("%s%" nonogram_PRIuSIZE, b ? "," : "", RULE(b));
    POS(b) = OLDPOS(b) = 0;
    SOLID(b) = OLDSOLID(b) = LEN + 1;
  }
  printf("\n");

  // We assume that there is more work to do.
  return true;
}

// Look for a gap of length 'req', starting at '*at', going no further
// than 'lim'.  Return true, and write the position in '*at'.
static int can_jump(const struct nonogram_initargs *a,
		    nonogram_sizetype req,
		    nonogram_sizetype lim,
		    nonogram_sizetype *at)
{
  nonogram_sizetype got = 0;
  for (nonogram_sizetype i = *at; i < lim && got < req; i++) {
    if (CELL(i) == nonogram_DOT) {
      got = 0;
      *at = i + 1;
    } else {
      // We can be sure that there are no solids between us and the
      // next block/end of line.
      assert(CELL(i) == nonogram_BLANK);
      got++;
    }
  }

  return got >= req;
}

static int step_invalid(void *vp, void *ws);
static int step_drawing(void *vp, void *ws);
static int step_sliding(void *vp, void *ws);
static int step_restoring(void *vp, void *ws);

static int step(void *vp, void *ws)
{
  const struct nonogram_initargs *a = ws;
#ifdef VLA_IN_STRUCT
  CONTEXT(RULES) *ctxt = ws;
#else
  struct hdr *ctxt = ws;
#endif
  putchar('\n');

  switch (ctxt->mode) {
  case RESTORING:
    printf("RESTORING\n");
    break;
  case SLIDING:
    printf("SLIDING\n");
    break;
  case INVALID:
    printf("INVALID\n");
    break;
  case DRAWING:
    printf("DRAWING\n");
    break;
  default:
    printf("unknown mode!!! %u", (unsigned) ctxt->mode);
    break;
  }

  if (B < RULES)
    printf("Block %zu of %" nonogram_PRIuSIZE " at %" nonogram_PRIuSIZE "\n",
	   B, RULE(B), POS(B));

  printf("X:%8zu>", LEN);
  for (nonogram_sizetype i = 0; i < LEN; i++) {
    switch (CELL(i)) {
    case nonogram_BLANK:
      putchar(' ');
      break;
    case nonogram_DOT:
      putchar('-');
      break;
    case nonogram_SOLID:
      putchar('#');
      break;
    default:
      putchar('?');
      break;
    }
  }
  printf("<\n");
  printf("          >");
  for (nonogram_sizetype i = 0; i < LEN; i++)
    putchar('0' + i % 10);
  printf("<\n");

  printf("B%-9zu>", B);
  {
    nonogram_sizetype i = 0;
    for (size_t b = 0; b < RULES; b++) {
      if (b == 0 || POS(b) > POS(b - 1) + RULE(b - 1))
	printf("%*s", (int) (POS(b) - i), "");
      else
	printf("\n%*s", (int) (POS(b) + 11), "");
      for (i = POS(b); i < POS(b) + RULE(b); i++)
	putchar(((ctxt->mode == SLIDING || b < B) && POS(b) + SOLID(b) == i) ?
		(b == B ? '!' : 'X') :
		(b == B ? '#' : '+'));
    }
    printf("%*s", (int) (LEN - i), "");
    printf("< [%zu-%zu) m%zu\n",
	   BASE, MAX, MININV);
  }

  switch (ctxt->mode) {
  case DRAWING:
    return step_drawing(vp, ws);
  case SLIDING:
    return step_sliding(vp, ws);
  case INVALID:
    return step_invalid(vp, ws);
  case RESTORING:
    return step_restoring(vp, ws);
  default:
    assert(!"Invalid state");
  }

  return false;
}

static int step_invalid(void *vp, void *ws)
{
  const struct nonogram_initargs *a = ws;
#ifdef VLA_IN_STRUCT
  CONTEXT(RULES) *ctxt = ws;
#else
  struct hdr *ctxt = ws;
#endif

  if (B >= MAX) {
    // All blocks are in place.

    if (B <= BASE) {
      // That must be the lot.
      printf("All blocks fixed\n");
      return false;
    }

    // Go back to the previous block for some checks.
    B--;

    // Check for trailing solids before the next block or the end of
    // the line.
    for (nonogram_sizetype i = POS(B) + RULE(B); i < ctxt->maxpos; i++)
      if (CELL(i) == nonogram_SOLID) {
	// A trailing solid has been found.
	printf("Trailing solid at %" nonogram_PRIuSIZE "\n", i);

	// Can we jump to it without uncovering a solid?
	if (POS(B) + SOLID(B) + RULE(B) > i) {
	  // Yes, so do so, and revalidate.
	  POS(B) = i + 1 - RULE(B);
	  ctxt->mode = INVALID;
	  return true;
	}
	// No, we'd uncover a solid.

	// Try to bring up an earlier block.
	assert(SOLID(B) < RULE(B));
	ctxt->target = B;
	ctxt->mode = DRAWING;
	return true;
      }
    // All blocks in position, and no trailing solids found - all valid.
    printf("New valid state found\n");

    // Record the current state in the results.
    if (record_sections(a, MININV, MAX,
			&POS(0), &OLDPOS(0),
			&SOLID(0), &OLDSOLID(0),
			&ctxt->remunk))
      return false;
    MININV = MAX;

    // Start sliding the right-most unfixed block right.
    ctxt->mode = SLIDING;
    return true;
  }

  if (POS(B) + RULE(B) > ctxt->maxpos) {
    // This block has spilled over the end of the line or onto a
    // right-most fixed block.
    printf("Spilled over\n");

    // Restore everything back to where we diverged from a valid
    // state.
    ctxt->mode = RESTORING;
    return true;
  }

  // Determine whether this block covers any solids or dots.
  nonogram_sizetype i = POS(B), end = i + RULE(B);
  SOLID(B) = LEN + 1;
  while (i < end && CELL(i) != nonogram_DOT) {
    if (SOLID(B) >= RULE(B) && CELL(i) == nonogram_SOLID)
      SOLID(B) = i - POS(B);
    i++;
  }

  if (i < end) {
    // There is a dot under this block.
    printf("Dot at offset %" nonogram_PRIuSIZE "\n", i - POS(B));

    if (SOLID(B) < RULE(B)) {
      // But there's a solid before it, so we can't jump.
      printf("Earlier solid at offset %" nonogram_PRIuSIZE "\n", SOLID(B));

      // Bring up another block.
      ctxt->target = B;
      ctxt->mode = DRAWING;
      return true;
    }

    // Otherwise, skip the dot, and recompute the solid offset.
    POS(B) = i + 1;
    printf("Skipped to %" nonogram_PRIuSIZE "\n", POS(B));
    ctxt->mode = INVALID;
    return true;
  }

  // If the block is not covering a solid, but just touching one on
  // its right, pretend that the block overlaps it.
  if (SOLID(B) >= RULE(B) && end < LEN && CELL(end) == nonogram_SOLID)
    SOLID(B) = RULE(B);

  // Keep moving the block one cell at a time, in order to cover any
  // adjacent solid on its right.
  while (POS(B) + RULE(B) < ctxt->maxpos &&
	 CELL(POS(B) + RULE(B)) == nonogram_SOLID) {
    if (SOLID(B) == 0) {
      // We can't span both the solid we cover and the adjacent solid
      // at the end.
      printf("Can't overlap solids at %" nonogram_PRIuSIZE
	     " and %" nonogram_PRIuSIZE "\n", POS(B), POS(B) + RULE(B));

      // Bring up another block.
      ctxt->target = B;
      ctxt->mode = DRAWING;
      return true;
    }
    POS(B)++;
    SOLID(B)--;
  }
  // This block is in a valid position.
  printf("Valid at %" nonogram_PRIuSIZE "\n", POS(B));
  if (SOLID(B) < RULE(B))
    printf("  Solid at offset %" nonogram_PRIuSIZE "\n", SOLID(B));

  // Position the next block and try to validate it.
  if (B + 1 < MAX && POS(B + 1) < POS(B) + RULE(B) + 1)
    POS(B + 1) = POS(B) + RULE(B) + 1;
  B++;
  ctxt->mode = INVALID;
  return true;
}

static int step_drawing(void *vp, void *ws)
{
  const struct nonogram_initargs *a = ws;
#ifdef VLA_IN_STRUCT
  CONTEXT(RULES) *ctxt = ws;
#else
  struct hdr *ctxt = ws;
#endif

  assert(SOLID(ctxt->target) < RULE(ctxt->target));

  printf("Target is %zu\n", ctxt->target);
  do {
    if (B <= BASE)
      // There are no more solids.  We must have exhausted all
      // possibilities, so stop here.
      return false;

    if (MININV < MAX) {
      // mininv has been set.

      assert(B >= MININV);
      if (B == MININV) {
	printf("Can't draw any more without restoring\n");
	assert(MAX > 0);
	B = MAX - 1;
	ctxt->mode = RESTORING;
	return true;
      }
    }

    if (SOLID(B) < RULE(B))
      ctxt->target = B;
    B--;
  } while (SOLID(B) < RULE(B) &&
	   POS(ctxt->target) + SOLID(ctxt->target) - RULE(B) + 1 >
	   POS(B) + SOLID(B));
  printf("New target is %zu\n", ctxt->target);

  // We must record which left-most block we are about to disturb from
  // its last valid position.
  if (B < MININV)
    MININV = B;

  // Set the block near to the position of the next block, so that
  // it just overlaps the solid, and try again.
  POS(B) = POS(ctxt->target) + SOLID(ctxt->target) - RULE(B) + 1;
  printf("Block %zu brought to %" nonogram_PRIuSIZE "\n", B, POS(B));

  // This block should still have a position completely on the line,
  // since the solid we aimed to cover must be on the line.
  assert(POS(B) + RULE(B) <= LEN);

  // Start checking whether this and later blocks are valid.
  ctxt->mode = INVALID;

  // Try again.
  return true;
}

static int step_sliding(void *vp, void *ws)
{
  const struct nonogram_initargs *a = ws;
#ifdef VLA_IN_STRUCT
  CONTEXT(RULES) *ctxt = ws;
#else
  struct hdr *ctxt = ws;
#endif

  // Attempt to slide this block to the right.

  // How far to the right can we shift it before it hits the next
  // block or the end of the line?
  nonogram_sizetype lim = B + 1 < RULES ? POS(B + 1) - 1 : LEN;
  printf("Limit is %" nonogram_PRIuSIZE "\n", lim);

  assert(POS(B) == OLDPOS(B));
  assert(SOLID(B) == OLDSOLID(B));

  // Slide right until we reach the next block, the end of the line,
  // or a dot.  Also stop if we are about to uncover a solid.
  while (POS(B) + RULE(B) < lim &&
	 CELL(POS(B) + RULE(B)) != nonogram_DOT &&
	 SOLID(B) != 0) {
    // We shouldn't be about to cover a solid by moving.  Otherwise,
    // how could we be in a valid state?
    assert(CELL(POS(B) + RULE(B)) != nonogram_SOLID);

    // TODO: Simplify the following 'if', as only one branch should
    // ever be taken.

    // Keep track of the left-most solid that we are covering, as we
    // move right one cell.
    if (SOLID(B) > 0) {
      // This always happens.
      SOLID(B)--;
    } else {
      assert(false); // This should never happen, due to the loop
		     // condition.
      SOLID(B) = LEN + 1;
    }
    POS(B)++;
  }

  assert(POS(B) >= OLDPOS(B));

  if (POS(B) != OLDPOS(B)) {
    // We have managed to slide this block some distance, so record
    // all those possibilities, and try later.  However, if this rules
    // out further information from this line, stop.
    printf("Merging block %zu from %" nonogram_PRIuSIZE
	   " to %" nonogram_PRIuSIZE "\n", B, OLDPOS(B), POS(B));
    if (merge1(a, &POS(0), &OLDPOS(0),
	       &SOLID(0), &OLDSOLID(0), &ctxt->remunk, B))
      return false;
  }

  // We failed to slide the block beyond this point.  Why?

  if (POS(B) + RULE(B) == lim && B + 1 == MAX) {
    // This block is at its right-most position.
    printf("Right block is right-most\n");
    if (MAX == BASE) {
      printf("Fixing final block\n");
      return false;
    }
    MAX--;
    ctxt->maxpos = POS(B) - 1;
  } else if (POS(B) + RULE(B) < lim &&
      CELL(POS(B) + RULE(B)) == nonogram_DOT) {
    // There's a dot in the way.
    printf("Dot obstructs at %" nonogram_PRIuSIZE "\n", POS(B) + RULE(B));

    // Can we jump it?
    nonogram_sizetype at = POS(B) + RULE(B) + 1;
    if (POS(B) + RULE(B) * 2 < lim && can_jump(a, RULE(B), lim, &at)) {
      // There is space to jump over the dot.

      assert(OLDPOS(B) == POS(B));
      printf("Jumpable\n");

      if (SOLID(B) >= RULE(B)) {
	// And we're not covering a solid, so we can do it now, and
	// keep sliding.

	// Jump to the space.
	POS(B) = at;

	// Record the previous section as dots.
	if (record_section(a, OLDPOS(B), OLDPOS(B) + RULE(B),
			   nonogram_DOT, &ctxt->remunk))
	  return false;

	// Record the new section as solids.
	if (record_section(a, POS(B), POS(B) + RULE(B),
			   nonogram_SOLID, &ctxt->remunk))
	  return false;

	// Note that this move has been recorded.
	OLDPOS(B) = POS(B);
	OLDSOLID(B) = SOLID(B) = LEN + 1;

	// Keep sliding this block.
	ctxt->mode = SLIDING;
	return true;
      }
      // There's space to jump the dot, but we'd uncover a solid if we
      // did that.
      printf("But covering solid\n");
    } else {
      // There's no space to jump over the dot.
      printf("No space past dot\n");

      if (B + 1 == MAX) {
	// This block is at its right-most position.
	printf("Right block is right-most for dot\n");
	if (MAX == BASE) {
	  printf("Fixing final block\n");
	  return false;
	}
	MAX--;
	ctxt->maxpos = POS(B) - 1;
      }
    }
    // Finished handling an obstructive dot.
  }
  // Either there was no dot, or we failed to jump over it.

  printf("Limit reached; %zu blocks left\n", B - BASE);

  // We can't go any further for the moment.  Try sliding a previous
  // block.
  if (B > BASE) {
    B--;
    return true;
  }
  // This is the left-most block that hasn't been fixed.

  // Now go to the right-most block that hasn't been fixed, and try to
  // get an earlier block to cover it.
  if (MAX <= BASE) {
    // All blocks have been fixed from the right.  Stop here.
    printf("All blocks fixed\n");
    return false;
  }

  assert(MAX > 0);
  B = MAX - 1;

  // Skip blocks which don't cover solids.
  while (B > BASE && SOLID(B) >= RULE(B))
    B--;

  if (SOLID(B) >= RULE(B)) {
    // We ran out of blocks, so it must be the end.
    return false;
  }

  // Pull a block up to cover this one's solid.
  ctxt->target = B;
  ctxt->mode = DRAWING;
  return true;
}

static int step_restoring(void *vp, void *ws)
{
  const struct nonogram_initargs *a = ws;
#ifdef VLA_IN_STRUCT
  CONTEXT(RULES) *ctxt = ws;
#else
  struct hdr *ctxt = ws;
#endif

  assert(B < RULES);
  assert(B < MAX);

  // Restore all blocks to their last valid positions, and find the
  // left-most of them that covers a solid - this will be the target
  // for drawing the next block up.
  ctxt->target = RULES;
  for (size_t j = MININV; j <= B; j++) {
    size_t i = B + MININV - j;
    printf("Restoring %zu from %" nonogram_PRIuSIZE
	   " to %" nonogram_PRIuSIZE "\n", i,
	   POS(i), OLDPOS(i));
    POS(i) = OLDPOS(i);
    SOLID(i) = OLDSOLID(i);
    if (SOLID(i) < RULE(i))
      ctxt->target = i;
  }

#if 0
  if (B + 1 == MAX) {
    printf("Blocks %zu to %zu fixed\n", MININV, B);
    MAX = MININV;
    ctxt->maxpos = POS(MAX) - 1;
  }
#endif

  B = MININV;
  MININV = RULES;
  if (B > MAX)
    B = MAX;
  printf("Returned to block %zu\n", B);

  if (ctxt->target >= RULES) {
    // Find a block from this position that covers a solid.
    while (B > BASE && SOLID(B) >= RULE(B))
      B--;

    if (SOLID(B) >= RULE(B)) {
      // There's no such block.
      printf("No block on left covers solid\n");
      return false;
    }
    ctxt->target = B;
  }

  // Now try drawing up an earlier block.
  ctxt->mode = DRAWING;
  return true;
}
