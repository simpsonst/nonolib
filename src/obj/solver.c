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
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#include "nonogram.h"
#include "internal.h"

#ifndef nonogram_LOGLEVEL
#define nonogram_LOGLEVEL 1
#endif

static void makescore(nonogram_lineattr *attr,
                      const struct nonogram_rule *rule, int len);

static void setupstep(nonogram_solver *);
static void step(nonogram_solver *);

/* Returns true if a change was detected. */
static int redeemstep(nonogram_solver *c);

int nonogram_initsolver(nonogram_solver *c)
{
  /* these can get stuffed; nah, maybe not */
  c->reversed = false;
  c->first = nonogram_SOLID;
  c->cycles = 50;

  /* no puzzle loaded */
  c->puzzle = NULL;

  /* no display */
  c->display = NULL;

  /* no place to send solutions */
  c->client = NULL;

  /* no logging */
  c->log.file = NULL;
  c->log.indent = 0;
  c->log.level = 0;

  /* no internal workspace */
  c->work = NULL;
  c->rowattr = c->colattr = NULL;
  c->rowflag = c->colflag = NULL;
  c->stack = NULL;

  /* start with no linesolvers */
  c->levels = 0;
  c->linesolver = NULL;
  c->workspace.byte = NULL;
  c->workspace.ptrdiff = NULL;
  c->workspace.size = NULL;
  c->workspace.nonogram_size = NULL;
  c->workspace.cell = NULL;

  /* then add the default */
  nonogram_setlinesolvers(c, 1);
  nonogram_setlinesolver(c, 1, "fcomp", &nonogram_fcompsuite, 0);

  c->focus = false;

  return 0;
}

int nonogram_termsolver(nonogram_solver *c)
{
  /* ensure a current puzzle and linesolver are removed */
  nonogram_unload(c);

  /* release all workspace */
  /* free(NULL) should be safe */
  free(c->workspace.byte), c->workspace.byte = NULL;
  free(c->workspace.ptrdiff), c->workspace.ptrdiff = NULL;
  free(c->workspace.size), c->workspace.size = NULL;
  free(c->workspace.nonogram_size), c->workspace.nonogram_size = NULL;
  free(c->workspace.cell), c->workspace.cell = NULL;
  free(c->work), c->work = NULL;
  return 0;
}

int nonogram_unload(nonogram_solver *c)
{
  nonogram_stack *st = c->stack;

  /* cancel current linesolver */
  if (c->puzzle)
    switch (c->status) {
    case nonogram_DONE:
    case nonogram_WORKING:
      if (c->linesolver[c->level].suite->term)
        (*c->linesolver[c->level].suite->term)
          (c->linesolver[c->level].context);
      break;
    }

  /* free stack */
  while (st) {
    c->stack = st->next;
    free(st);
    st = c->stack;
  }
  c->focus = false;
  c->puzzle = NULL;
  return 0;
}

static void gathersolvers(nonogram_solver *c);

int nonogram_load(nonogram_solver *c, const nonogram_puzzle *puzzle,
                  nonogram_cell *grid, int remcells)
{
  /* local iterators */
  size_t lineno;

  /* is there already a puzzle loaded */
  if (c->puzzle)
    return -1;

  c->puzzle = puzzle;

  c->lim.maxline =
    (puzzle->width > puzzle->height) ? puzzle->width : puzzle->height;
  c->lim.maxrule = 0;

  /* initialise the grid */
  c->grid = grid;
  c->remcells = remcells;

  /* working data */
  free(c->work);
#if 0
  /* These can't work, since they are allocated in a single block,
     which c->work points to. */
  free(c->rowflag);
  free(c->colflag);
  free(c->rowattr);
  free(c->colattr);
#endif
  {
    size_t amount = sizeof(nonogram_cell) * c->lim.maxline;
    amount = align(amount, nonogram_level);
    size_t flag_offset = amount;
    amount += sizeof(nonogram_level) * (puzzle->height + puzzle->width);
    amount = align(amount, nonogram_lineattr);
    size_t attr_offset = amount;
    amount += sizeof(nonogram_lineattr) * (puzzle->height + puzzle->width);

    char *mem = malloc(amount);
    if (!mem)
      return -1;
    c->work = (void *) mem;
    c->rowflag = (void *) (mem + flag_offset);
    c->colflag = c->rowflag + puzzle->height;
    c->rowattr = (void *) (mem + attr_offset);
    c->colattr = c->rowattr + puzzle->height;
  }

  c->reminfo = 0;
  c->stack = NULL;

  c->unkarea.min.x = 0;
  c->unkarea.min.y = 0;
  c->unkarea.max.x = c->puzzle->width;
  c->unkarea.max.y = c->puzzle->height;

  /* determine heuristic scores for each column */
  for (lineno = 0; lineno < puzzle->width; lineno++) {
    struct nonogram_rule *rule = puzzle->col + lineno;

    c->reminfo += !!(c->colflag[lineno] = c->levels);

    if (rule->len > c->lim.maxrule)
      c->lim.maxrule = rule->len;

    makescore(c->colattr + lineno, rule, puzzle->height);
  }

  /* determine heuristic scores for each row */
  for (lineno = 0; lineno < puzzle->height; lineno++) {
    struct nonogram_rule *rule = puzzle->row + lineno;

    c->reminfo += !!(c->rowflag[lineno] = c->levels);

    if (rule->len > c->lim.maxrule)
      c->lim.maxrule = rule->len;

    makescore(c->rowattr + lineno, rule, puzzle->width);
  }

  gathersolvers(c);

  /* configure line solver */
  c->status = nonogram_EMPTY;

  return 0;
}

static void colfocus(nonogram_solver *c, int lineno, int v);
static void rowfocus(nonogram_solver *c, int lineno, int v);
static void mark1col(nonogram_solver *c, int lineno);
static void mark1row(nonogram_solver *c, int lineno);


static void makeguess(nonogram_solver *c,
                      const struct nonogram_point *pos,
                      nonogram_cell choice,
                      nonogram_cell *altp);
static void flipguess(nonogram_solver *restrict c,
                      const struct nonogram_point *restrict pos,
                      nonogram_cell old);
static void chooseguess(nonogram_solver *restrict c,
                        const struct nonogram_rect *restrict from,
                        struct nonogram_point *restrict pos,
                        nonogram_cell *restrict choice);
static void findminrect(nonogram_solver *c, struct nonogram_rect *b,
                        const struct nonogram_rect *from);
static void findeasiest(nonogram_solver *c);

int nonogram_testtries(void *vt)
{
  int *tries = vt;
  return (*tries)-- > 0;
}

int nonogram_testtime(void *vt)
{
  const clock_t *t = vt;
  return clock() < *t;
}

int nonogram_runsolver_n(nonogram_solver *c, int *tries)
{
  int cy = c->cycles;
  int r = nonogram_runlines_tries(c, tries, &cy);
  return r == nonogram_LINE ? nonogram_UNFINISHED : r;
}

int nonogram_runlines(nonogram_solver *, int *, int (*)(void *), void *);

int nonogram_runlines_tries(nonogram_solver *c, int *lines, int *cycles)
{
  return nonogram_runlines(c, lines, &nonogram_testtries, cycles);
}

int nonogram_runlines_until(nonogram_solver *c, int *lines, clock_t lim)
{
  return nonogram_runlines(c, lines, &nonogram_testtime, &lim);
}

int nonogram_runlines(nonogram_solver *c, int *lines,
                      int (*test)(void *), void *data)
{
  int r = c->puzzle ? nonogram_UNFINISHED : nonogram_UNLOADED;

  while (*lines > 0) {
    if ((r = nonogram_runcycles(c, test, data)) == nonogram_LINE)
      --*lines;
    else
      return r;
  }
  return r;
}

int nonogram_runcycles_tries(nonogram_solver *c, int *cycles)
{
  return nonogram_runcycles(c, &nonogram_testtries, cycles);
}

int nonogram_runcycles_until(nonogram_solver *c, clock_t lim)
{
  return nonogram_runcycles(c, &nonogram_testtime, &lim);
}

int nonogram_runcycles(nonogram_solver *c, int (*test)(void *), void *data)
{
  if (!c->puzzle) {
    return nonogram_UNLOADED;
  } else if (c->status == nonogram_WORKING) {
    /* in the middle of solving a line */
    while ((*test)(data) && c->status == nonogram_WORKING)
      step(c);
    return nonogram_UNFINISHED;
  } else if (c->status == nonogram_DONE)  {
    /* a line is solved, but not acted upon */
    size_t linelen;

    /* indicate end of line-processing */
    if (c->on_row)
      rowfocus(c, c->lineno, false), linelen = c->puzzle->width;
    else
      colfocus(c, c->lineno, false), linelen = c->puzzle->height;

#if false
    if (c->logfile) {
      fprintf(c->logfile, "%*sFits: %d\n", c->indent, "", c->fits);
      fflush(c->logfile);
    }
#endif

    /* test for consistency */
    if (c->fits == 0) {
      /* nothing fitted; must be an error */
      c->remcells = -1;
#if nonogram_LOGLEVEL > 0
      if (c->log.file) {
        fprintf(c->log.file, "%*s         Inconsistency!\n",
                c->log.indent, "");
        fflush(c->log.file);
      }
#endif
    } else {
      int changed;

#if nonogram_LOGLEVEL > 0
      if (c->log.file) {
        size_t i;
        fprintf(c->log.file, "%*s   End: >", c->log.indent, "");
        for (i = 0; i < linelen; i++)
          switch (c->work[i]) {
          case nonogram_BLANK:
            fputc(' ', c->log.file);
            break;
          case nonogram_DOT:
            fputc('-', c->log.file);
            break;
          case nonogram_SOLID:
            fputc('#', c->log.file);
            break;
          case nonogram_BOTH:
            fputc('+', c->log.file);
            break;
          default:
            fputc('?', c->log.file);
            break;
          }
        fprintf(c->log.file, "<\n");
        fflush(c->log.file);
      } /* log file reported */
#endif

      /* update display and count number of changed cells and flags */
      changed = redeemstep(c);

      /* indicate choice to display */
      if (c->on_row) {
        if (c->rowattr[c->lineno].dot == 0 &&
            c->rowattr[c->lineno].solid == 0)
          c->rowflag[c->lineno] = 0;
        else if (c->fits < 0 && changed)
          c->rowflag[c->lineno] = c->levels;
        else
          --c->rowflag[c->lineno];
        if (!c->rowflag[c->lineno]) c->reminfo--;
        mark1row(c, c->lineno);
      } else {
        if (c->colattr[c->lineno].dot == 0 &&
            c->colattr[c->lineno].solid == 0)
          c->colflag[c->lineno] = 0;
        else if (c->fits < 0 && changed)
          c->colflag[c->lineno] = c->levels;
        else
          --c->colflag[c->lineno];
        if (!c->colflag[c->lineno]) c->reminfo--;
        mark1col(c, c->lineno);
      }
    }

#if nonogram_LOGLEVEL > 0
    c->log.indent -= 2;
    if (c->log.file) {
      fprintf(c->log.file, "%*s}%s\n", c->log.indent, "",
              c->reversed ? " reversed" : "");
      fflush(c->log.file);
    }
#endif

#if nonogram_LOGLEVEL > 0
    if (c->log.file) {
      fprintf(c->log.file, "%*sCells: %d; Lines: %d\n", c->log.indent, "",
              c->remcells, c->reminfo);
      fflush(c->log.file);
    }
#endif

    /* set state to indicate no line currently chosen */
    c->status = nonogram_EMPTY;
    return nonogram_LINE;
  } else if (c->remcells < 0) {
    /* back-track caused by error or completion of grid */
    nonogram_stack *st = c->stack;

    /* If there is nothing pushed, there's an error in the puzzle (I
       think). */
    if (!st) {
      return nonogram_FINISHED;
    } else {
      assert(st);

#if nonogram_LOGLEVEL > 0
      if (c->log.file) {
        fprintf(c->log.file, "%*sRestoring (%lu,%lu)-(%lu,%lu) from stack\n",
                c->log.indent, "",
                (unsigned long) st->unkarea.min.x,
                (unsigned long) st->unkarea.min.y,
                (unsigned long) (st->unkarea.max.x - 1),
                (unsigned long) (st->unkarea.max.y - 1));
        fflush(c->log.file);
      }
#endif

      /* copy from stack */
      c->remcells = st->remcells;
      c->reminfo = 2;

#if nonogram_LOGLEVEL > 0
      if (c->log.file) {
        fprintf(c->log.file, "%*sCells: %d; Lines: %d\n", c->log.indent, "",
                c->remcells, c->reminfo);
        fflush(c->log.file);
      }
#endif

      /* Restore the area of the grid that was saved. */

      /* How big is this area? */
      const size_t w = st->unkarea.max.x - st->unkarea.min.x;
      const size_t h = st->unkarea.max.y - st->unkarea.min.y;

      /* Re-expand the 'unknown cell' area. */
      c->unkarea = st->unkarea;

      c->reversed = false;

      /* Restore rows. */
      for (size_t y = 0; y < h; y++) {
        /* Translate the co-ordinate system. */
        const size_t ry = y + st->unkarea.min.y;

        /* Restore each row of cells. */
        memcpy(c->grid + st->unkarea.min.x + ry * c->puzzle->width,
               st->grid + y * w, w * sizeof(nonogram_cell));

        /* Indicate that the line has no solvers yet to be applied. */
        c->rowflag[ry] = 0;

        /* Also restore scores. */
        c->rowattr[ry] = st->rowattr[y];
      }

      /* Restore columns. */
      for (size_t x = 0; x < w; x++) {
        /* Translate the co-ordinate system. */
        const size_t rx = x + st->unkarea.min.x;

        /* Indicate that the line has no solvers yet to be applied. */
        c->colflag[rx] = 0;

        /* Also restore scores. */
        c->colattr[rx] = st->colattr[x];
      }

      /* Correct the flags for the row and column where the last guess
         was made. */
      c->colflag[st->guesspos.x] = c->levels;
      c->rowflag[st->guesspos.y] = c->levels;

      /* Update screen with restored data. */
      if (c->display && c->display->redrawarea)
        (*c->display->redrawarea)(c->display_data, &st->unkarea);
      if (c->display && c->display->colmark)
        (*c->display->colmark)(c->display_data,
                               st->unkarea.min.x, st->unkarea.max.x);
      if (c->display && c->display->rowmark)
        (*c->display->rowmark)(c->display_data,
                               st->unkarea.min.y, st->unkarea.max.y);

      /* Pop the entry from the stack. */
      c->log.indent -= 2;
      if (c->log.file)
        fprintf(c->log.file, "%*s}\n", c->log.indent, "");
      c->stack = st->next;
      free(st);
      return nonogram_LINE;
      /* back-tracking dealt with */
    }
  } else if (c->reminfo > 0) {
    /* no errors, but there are still lines to test */
    findeasiest(c);

    /* set up context for solving a row or column */
    if (c->on_row) {
#if nonogram_LOGLEVEL > 0
      if (c->log.file) {
        fprintf(c->log.file, "%*sRow %d [%d]: (%lu) ",
                c->log.indent, "", c->lineno, c->rowattr[c->lineno].score,
                (unsigned long) c->puzzle->row[c->lineno].len);
        nonogram_printrule(c->puzzle->row + c->lineno, c->log.file);
        fprintf(c->log.file, " {\n");
        fflush(c->log.file);
      }
#endif
      c->log.indent += 2;
      c->editarea.max.y = (c->editarea.min.y = c->lineno) + 1;
      rowfocus(c, c->lineno, true);
    } else {
#if nonogram_LOGLEVEL > 0
      if (c->log.file) {
        fprintf(c->log.file, "%*sColumn %d [%d]: (%lu) ",
                c->log.indent, "", c->lineno, c->colattr[c->lineno].score,
                (unsigned long) c->puzzle->col[c->lineno].len);
        nonogram_printrule(c->puzzle->col + c->lineno, c->log.file);
        fprintf(c->log.file, " {\n");
        fflush(c->log.file);
      }
#endif
      c->log.indent += 2;
      c->editarea.max.x = (c->editarea.min.x = c->lineno) + 1;
      colfocus(c, c->lineno, true);
    }
    setupstep(c);
    /* a line still to be tested has now been set up for solution */
    return nonogram_UNFINISHED;
  } else if (c->remcells == 0) {
    /* no remaining lines or cells; no error - must be solution */

#if nonogram_LOGLEVEL > 0
    if (c->log.file) {
      fprintf(c->log.file, "%*sCorrect grid.\n", c->log.indent, "");
      fflush(c->log.file);
    }
#endif
    if (c->client && c->client->present)
      (*c->client->present)(c->client_data);
    c->remcells = -1;
    return c->stack ? nonogram_FOUND : nonogram_FINISHED;
  } else {
    /* There is no more info to process, no errors, yet some cells
       left. */

    /* Make a complementary pair of guesses.  Push one onto the stack
       and leave the other one in our main working area.  When we
       later exhaust the current one, we'll simply discard it and
       start processing the other one. */

    /* Determine the area to be recorded by shrinking the 'unknown
       cell' area of the current grid. */
    struct nonogram_rect area;
    findminrect(c, &area, &c->unkarea);
    c->unkarea = area;

    /* How big is the unknown area? */
    const size_t w = area.max.x - area.min.x;
    const size_t h = area.max.y - area.min.y;

    /* Choose a position to make guess. */
    struct nonogram_point pos;
    nonogram_cell choice;
    chooseguess(c, &area, &pos, &choice);

    /* Make one guess before saving on the stack, and work out what
       the alternative is. */
    nonogram_cell alt_choice;
    makeguess(c, &pos, choice, &alt_choice);

    /* Allocate space for a new stack element, a copy of the affected
       grid, and associated line attributes. */
    nonogram_stack *st;
    {
      size_t amount = sizeof(nonogram_stack);
      size_t grid_offset = amount = align(amount, nonogram_cell);
      amount += w * h * sizeof(nonogram_cell);
      size_t attr_offset = amount = align(amount, nonogram_lineattr);
      amount += (w + h) * sizeof(nonogram_lineattr);

      char *mem = malloc(amount);
      if (!mem)
        return nonogram_ERROR;
      st = (void *) mem;
      st->grid = (void *) (mem + grid_offset);
      st->rowattr = (void *) (mem + attr_offset);
      st->colattr = st->rowattr + h;
    }
    st->next = c->stack;
    c->stack = st;

    /* Record the current state. */
    st->guesspos = pos;
    st->unkarea = area;
    st->remcells = c->remcells;

    /* Copy rows. */
    for (size_t y = 0; y < h; y++) {
      /* Translate the co-ordinate system. */
      const size_t ry = y + st->unkarea.min.y;

      /* Copy a row of cells. */
      memcpy(st->grid + y * w,
             c->grid + st->unkarea.min.x + ry * c->puzzle->width,
             w * sizeof(nonogram_cell));

      /* Copy row attributes. */
      st->rowattr[y] = c->rowattr[ry];
    }

    /* Copy columns. */
    for (size_t x = 0; x < w; x++) {
      const size_t rx = x + st->unkarea.min.x;
      st->colattr[x] = c->colattr[rx];
    }

#if nonogram_LOGLEVEL > 0
    if (c->log.file) {
      fprintf(c->log.file, "%*sPushing area (%lu,%lu)-(%lu,%lu) {\n",
              c->log.indent, "",
              (unsigned long) st->unkarea.min.x,
              (unsigned long) st->unkarea.min.y,
              (unsigned long) (st->unkarea.max.x - 1),
              (unsigned long) (st->unkarea.max.y - 1));
      fflush(c->log.file);
    }
#endif
    c->log.indent += 2;

    /* Flip the guess in the current state, adjusting the heuristics
       for the corresponding row and column. */
    flipguess(c, &pos, alt_choice);

    return nonogram_LINE;
  }

  return nonogram_UNFINISHED;
}

static void flipguess(nonogram_solver *restrict c,
                      const struct nonogram_point *restrict pos,
                      nonogram_cell newval)
{
  /* Change the cell to the alternative guess. */
  c->grid[pos->x + pos->y * c->puzzle->width] = newval;
#if nonogram_LOGLEVEL > 0
  if (c->log.file) {
    fprintf(c->log.file, "%*sFlipped guess %c at (%zu,%zu)\n",
            c->log.indent, "",
            newval == nonogram_DOT ? '-' : '#',
            pos->x, pos->y);
    fflush(c->log.file);
  }
#endif

  if (newval == nonogram_SOLID) {
    /* Update the row heuristics. */
    c->rowattr[pos->y].dot++;
    c->rowattr[pos->y].solid--;

    /* Update the column heuristics. */
    c->colattr[pos->x].dot++;
    c->colattr[pos->x].solid--;
  } else  {
    /* Update the row heuristics. */
    c->rowattr[pos->y].dot--;
    c->rowattr[pos->y].solid++;

    /* Update the column heuristics. */
    c->colattr[pos->x].dot--;
    c->colattr[pos->x].solid++;
  }

  /* Make the row and column selectable for processing. */
  c->rowflag[pos->y] = c->levels;
  c->colflag[pos->x] = c->levels;
  mark1row(c, pos->y);
  mark1col(c, pos->x);

  /* Update the display. */
  if (c->display && c->display->redrawarea) {
    struct nonogram_rect gp;
    gp.max.x = (gp.min.x = pos->x) + 1;
    gp.max.y = (gp.min.y = pos->y) + 1;
    (*c->display->redrawarea)(c->display_data, &gp);
  }
}

static void makeguess(nonogram_solver *restrict c,
                      const struct nonogram_point *restrict pos,
                      nonogram_cell guess,
                      nonogram_cell *restrict altp)
{
  assert(guess == nonogram_DOT || guess == nonogram_SOLID);

  /* Work out the alternative guess. */
  *altp = guess ^ nonogram_BOTH;

  /* Change the grid to reflect the guess. */
  c->grid[pos->x + pos->y * c->puzzle->width] = guess;
#if nonogram_LOGLEVEL > 0
  if (c->log.file) {
    fprintf(c->log.file, "%*sGuessing %c at (%zu,%zu)\n", c->log.indent, "",
            guess == nonogram_DOT ? '-' : '#',
            pos->x, pos->y);
    fflush(c->log.file);
  }
#endif

  /* Update heuristics for row. */
  if (!--*(guess == nonogram_DOT ?
           &c->rowattr[pos->y].dot : &c->rowattr[pos->y].solid))
    c->rowattr[pos->y].score = c->puzzle->height;
  else
    c->rowattr[pos->y].score++;

  /* Update heuristics for column. */
  if (!--*(guess == nonogram_DOT ?
           &c->colattr[pos->x].dot : &c->colattr[pos->x].solid))
    c->colattr[pos->x].score = c->puzzle->width;
  else
    c->colattr[pos->x].score++;

  /* Update other summary information for the grid. */
  c->remcells--;
  c->reminfo = 2;
#if nonogram_LOGLEVEL > 0
  if (c->log.file) {
    fprintf(c->log.file, "%*sCells: %d; Lines: %d\n", c->log.indent, "",
            c->remcells, c->reminfo);
    fflush(c->log.file);
  }
#endif
}

/* This sets the rectangle *b to the smallest inclusive rectangle that
 * covers all the unknown cells. */
static void findminrect(nonogram_solver *restrict c,
                        struct nonogram_rect *restrict b,
                        const struct nonogram_rect *restrict orig)
{
  assert(orig);

  size_t pwidth = c->puzzle->width;

  const nonogram_cell *grid = c->grid;

#if nonogram_LOGLEVEL > 3
  if (c->log.level > 3 && c->log.file) {
    fprintf(c->log.file, "Total: [0,0]-(%zu,%zu)\n",
            c->puzzle->width, c->puzzle->height);
    fprintf(c->log.file, "orig: [%zu,%zu]-(%zu,%zu)\n",
            orig->min.x, orig->min.y, orig->max.x, orig->max.y);
    size_t pos = 0;
    for (size_t y = 0; y < c->puzzle->height; y++) {
      for (size_t x = 0; x < c->puzzle->width; x++, pos++) {
        if (x >= orig->min.x && x < orig->max.x &&
            y >= orig->min.y && y < orig->max.y) {
          switch (grid[pos]) {
          case nonogram_BLANK:
            fputc('x', c->log.file);
            break;
          case nonogram_DOT:
            fputc('\'', c->log.file);
            break;
          case nonogram_SOLID:
            fputc('O', c->log.file);
            break;
          default:
            fputc('"', c->log.file);
            break;
          }
        } else {
          switch (grid[pos]) {
          case nonogram_BLANK:
            fputc(' ', c->log.file);
            break;
          case nonogram_DOT:
            fputc('-', c->log.file);
            break;
          case nonogram_SOLID:
            fputc('#', c->log.file);
            break;
          default:
            fputc('?', c->log.file);
            break;
          }
        }
      }
      fputc('\n', c->log.file);
    }
    fflush(c->log.file);
  }
#endif

  assert(orig->max.x > orig->min.x);
  assert(orig->max.y > orig->min.y);

  /* Find the first blank cell on the first row with a blank cell. */
  for (b->min.y = orig->min.y; b->min.y < orig->max.y; b->min.y++) {
    const size_t lim = orig->max.x + b->min.y * pwidth;
    size_t ls = orig->min.x + b->min.y * pwidth;
    while (ls < lim) {
      if (grid[ls] == nonogram_BLANK) {
        b->max.x = (b->min.x = ls % pwidth) + 1;
        goto found_first;
      }
      ls++;
    }
  }
 found_first:
  b->max.y = b->min.y + 1;

#if nonogram_LOGLEVEL > 4
  if (c->log.level > 4 && c->log.file) {
    fprintf(c->log.file, "selected left of top: [%zu,%zu]-(%zu,%zu)\n",
            b->min.x, b->min.y, b->max.x, b->max.y);
  }
#endif

  /* Search for any blank cells below and to the left of that cell. */
  {
    size_t xlim = b->min.x + b->max.y * pwidth;
    for (size_t y = b->min.y + 1; y < orig->max.y; y++, xlim += pwidth) {
      size_t ls = orig->min.x + y * pwidth;
      assert(xlim >= ls);
      assert(xlim - ls <= orig->max.x - orig->min.x);
      while (ls < xlim) {
        if (grid[ls] == nonogram_BLANK) {
          b->max.y = y + 1;
          size_t nx = ls % pwidth;
          if (nx < b->min.x)
            b->min.x = nx;
          break;
        }
        ls++;
      }
    }
  }


#if nonogram_LOGLEVEL > 4
  if (c->log.level > 4 && c->log.file) {
    fprintf(c->log.file, "selected bottom left corner: [%zu,%zu]-(%zu,%zu)\n",
            b->min.x, b->min.y, b->max.x, b->max.y);
  }
#endif


  /* Searching row by row upwards, find the first blank in the bottom
     right area. */
  {
    size_t xlim = b->max.x + (orig->max.y - 1) * pwidth;
    size_t old_b_x = b->max.x;
    for (size_t y = orig->max.y; y > b->max.y; y--, xlim -= pwidth) {
      size_t ls = orig->max.x + (y - 1) * pwidth;
      assert(ls >= xlim);
      assert(ls - xlim == orig->max.x - old_b_x);
      while (ls > xlim) {
        if (grid[ls - 1] == nonogram_BLANK) {
          b->max.x = (ls + pwidth - 1) % pwidth + 1;
          b->max.y = y;
          break;
        }
        ls--;
      }
    }
  }

#if nonogram_LOGLEVEL > 4
  if (c->log.level > 4 && c->log.file) {
    fprintf(c->log.file, "selected right of bottom: [%zu,%zu]-(%zu,%zu)\n",
            b->min.x, b->min.y, b->max.x, b->max.y);
  }
#endif

  /* Now search for anything prodding out to the right. */
  {
    size_t xlim = b->max.x + (b->max.y - 1) * pwidth;
    for (size_t y = b->max.y;
         b->max.x < orig->max.x && y > b->min.y;
         y--, xlim -= pwidth) {
      size_t ls = orig->max.x + (y - 1) * pwidth;
      assert(ls >= xlim);
      while (ls > xlim) {
        if (grid[ls - 1] == nonogram_BLANK) {
          b->max.x = (ls + pwidth - 1) % pwidth + 1;
#if nonogram_LOGLEVEL > 4
          if (c->log.level > 4 && c->log.file) {
            fprintf(c->log.file, "row %zu detected blank at %zu\n",
                    y, b->max.x);
          }
#endif
          xlim = ls;
          break;
        }
        ls--;
      }
    }
  }

  assert(b->min.x >= orig->min.x);
  assert(b->min.y >= orig->min.y);
  assert(b->max.x <= orig->max.x);
  assert(b->max.y <= orig->max.y);

#ifndef NDEBUG
  {
    /* There absolutely must be no blanks outside the chosen
       rectangle. */
    for (size_t y = 0; y < b->min.y; y++)
      for (size_t x = 0; x < c->puzzle->width; x++)
        assert(grid[x + y * pwidth] != nonogram_BLANK);

    for (size_t y = b->min.y; y < b->max.y; y++) {
      for (size_t x = 0; x < b->min.x; x++)
        assert(grid[x + y * pwidth] != nonogram_BLANK);
      for (size_t x = b->max.x; x < c->puzzle->width; x++)
        assert(grid[x + y * pwidth] != nonogram_BLANK);
    }
    for (size_t y = b->max.y; y < c->puzzle->height; y++)
      for (size_t x = 0; x < c->puzzle->width; x++)
        assert(grid[x + y * pwidth] != nonogram_BLANK);
  }
#endif

#if nonogram_LOGLEVEL > 3
  if (c->log.level > 3 && c->log.file) {
    fprintf(c->log.file, "selected: [%zu,%zu]-(%zu,%zu)\n",
            b->min.x, b->min.y, b->max.x, b->max.y);
    size_t pos = 0;
    for (size_t y = 0; y < c->puzzle->height; y++) {
      for (size_t x = 0; x < c->puzzle->width; x++, pos++) {
        if (x >= b->min.x && x < b->max.x &&
            y >= b->min.y && y < b->max.y) {
          switch (grid[pos]) {
          case nonogram_BLANK:
            fputc('=', c->log.file);
            break;
          case nonogram_DOT:
            fputc('.', c->log.file);
            break;
          case nonogram_SOLID:
            fputc('*', c->log.file);
            break;
          default:
            fputc('!', c->log.file);
            break;
          }
          continue;
        }

        if (x >= orig->min.x && x < orig->max.x &&
            y >= orig->min.y && y < orig->max.y) {
          switch (grid[pos]) {
          case nonogram_BLANK:
            fputc('x', c->log.file);
            break;
          case nonogram_DOT:
            fputc('\'', c->log.file);
            break;
          case nonogram_SOLID:
            fputc('O', c->log.file);
            break;
          default:
            fputc('"', c->log.file);
            break;
          }
        } else {
          switch (grid[pos]) {
          case nonogram_BLANK:
            fputc(' ', c->log.file);
            break;
          case nonogram_DOT:
            fputc('-', c->log.file);
            break;
          case nonogram_SOLID:
            fputc('#', c->log.file);
            break;
          default:
            fputc('?', c->log.file);
            break;
          }
        }
      }
      fputc('\n', c->log.file);
    }
    fflush(c->log.file);
  }
#endif
}


static void findeasiest(nonogram_solver *c)
{
  int score;
  size_t i;

  c->level = c->rowflag[0];
  score = c->rowattr[0].score;
  c->on_row = true;
  c->lineno = 0;

  for (i = 0; i < c->puzzle->height; i++)
    if (c->rowflag[i] > c->level ||
        (c->level > 0 && c->rowflag[i] == c->level &&
         c->rowattr[i].score > score)) {
      c->level = c->rowflag[i];
      score = c->rowattr[i].score;
      c->lineno = i;
    }

  for (i = 0; i < c->puzzle->width; i++)
    if (c->colflag[i] > c->level ||
        (c->level > 0 && c->colflag[i] == c->level &&
         c->colattr[i].score > score)) {
      c->level = c->colflag[i];
      score = c->colattr[i].score;
      c->lineno = i;
      c->on_row = false;
    }
}

static void makescore(nonogram_lineattr *attr,
                      const struct nonogram_rule *rule, int len)
{
  size_t relem;

  attr->score = 0;
  for (relem = 0; relem < rule->len; relem++)
    attr->score += rule->val[relem];
  attr->dot = len - (attr->solid = attr->score);
  if (!attr->solid)
    attr->score = len;
  else {
    attr->score *= rule->len + 1;
    attr->score += rule->len * (rule->len - len - 1);
  }
}

static void colfocus(nonogram_solver *c, int lineno, int v)
{
  c->focus = v;
  if (c->display && c->display->colfocus)
    (*c->display->colfocus)(c->display_data, lineno, v);
}

static void rowfocus(nonogram_solver *c, int lineno, int v)
{
  c->focus = v;
  if (c->display && c->display->rowfocus)
    (*c->display->rowfocus)(c->display_data, lineno, v);
}

static void mark1col(nonogram_solver *c, int lineno)
{
  if (c->display && c->display->colmark)
    (*c->display->colmark)(c->display_data, lineno, lineno + 1);
}

static void mark1row(nonogram_solver *c, int lineno)
{
  if (c->display && c->display->rowmark)
    (*c->display->rowmark)(c->display_data, lineno, lineno + 1);
}

static void gathersolvers(nonogram_solver *c)
{
  static struct nonogram_req zero;
  struct nonogram_req most = zero, req;
  nonogram_level n;

  for (n = 0; n < c->levels; n++)
    if (c->linesolver[n].suite && c->linesolver[n].suite->prep) {
      req = zero;
      (*c->linesolver[n].suite->prep)(c->linesolver[n].context, &c->lim, &req);
      if (req.byte > most.byte)
        most.byte = req.byte;
      if (req.ptrdiff > most.ptrdiff)
        most.ptrdiff = req.ptrdiff;
      if (req.size > most.size)
        most.size = req.size;
      if (req.nonogram_size > most.nonogram_size)
        most.nonogram_size = req.nonogram_size;
      if (req.cell > most.cell)
        most.cell = req.cell;
    }

  free(c->workspace.byte);
  free(c->workspace.ptrdiff);
  free(c->workspace.size);
  free(c->workspace.nonogram_size);
  free(c->workspace.cell);
  c->workspace.byte = malloc(most.byte);
  c->workspace.ptrdiff = malloc(most.ptrdiff * sizeof(ptrdiff_t));
  c->workspace.size = malloc(most.size * sizeof(size_t));
  c->workspace.nonogram_size =
    malloc(most.nonogram_size * sizeof(nonogram_sizetype));
  c->workspace.cell = malloc(most.cell * sizeof(nonogram_cell));
}


static void redrawrange(nonogram_solver *c, int from, int to);
static void mark(nonogram_solver *c, int from, int to);

static void setupstep(nonogram_solver *c)
{
  struct nonogram_initargs a;
  const char *name;

  if (c->on_row) {
    a.line = c->grid + c->puzzle->width * c->lineno;
    a.linelen = c->puzzle->width;
    a.linestep = 1;
    a.rule = c->puzzle->row[c->lineno].val;
    a.rulelen = c->puzzle->row[c->lineno].len;
  } else {
    a.line = c->grid + c->lineno;
    a.linelen = c->puzzle->height;
    a.linestep = c->puzzle->width;
    a.rule = c->puzzle->col[c->lineno].val;
    a.rulelen = c->puzzle->col[c->lineno].len;
  }
  a.rulestep = 1;
  a.fits = &c->fits;
  c->tmplog = c->log;
  a.log = &c->tmplog;
  a.result = c->work;
  a.resultstep = 1;

  c->reversed = false;

  if (c->level > c->levels || c->level < 1 ||
      !c->linesolver[c->level - 1].suite ||
      !c->linesolver[c->level - 1].suite->init)
    name = "backup";
  else
    name = c->linesolver[c->level - 1].name ?
      c->linesolver[c->level - 1].name : "unknown";

#if nonogram_LOGLEVEL > 0
  if (c->log.file) {
    size_t i;

    fprintf(c->log.file, "%*s  Algo: %s", c->log.indent, "", name);
    fprintf(c->log.file, "\n%*s Start: >", c->log.indent, "");
    for (i = 0; i < a.linelen; i++)
      switch (a.line[i * a.linestep]) {
      case nonogram_BLANK:
        fputc(' ', c->log.file);
        break;
      case nonogram_DOT:
        fputc('-', c->log.file);
        break;
      case nonogram_SOLID:
        fputc('#', c->log.file);
        break;
      case nonogram_BOTH:
        fputc('+', c->log.file);
        break;
      default:
        fputc('?', c->log.file);
        break;
      }
    fprintf(c->log.file, "<\n");
    fflush(c->log.file);
  }
#endif

  /* implement a backup solver */
  if (c->level > c->levels || c->level < 1 ||
      !c->linesolver[c->level - 1].suite ||
      !c->linesolver[c->level - 1].suite->init) {
    size_t i;

    /* reveal nothing */
    for (i = 0; i < a.linelen; i++)
      switch (a.line[i * a.linestep]) {
      case nonogram_DOT:
      case nonogram_SOLID:
        c->work[i] = a.line[i * a.linestep];
        break;
      default:
        c->work[i] = nonogram_BOTH;
        break;
      }

    c->status = nonogram_DONE;
    return;
  }

  c->status =
    (*c->linesolver[c->level - 1].suite->init)
    (c->linesolver[c->level - 1].context, &c->workspace, &a) ?
    nonogram_WORKING : nonogram_DONE;
}

static void step(nonogram_solver *c)
{
  if (c->level > c->levels || c->level < 1 ||
      !c->linesolver[c->level - 1].suite ||
      !c->linesolver[c->level - 1].suite->step) {
    size_t i, linelen = c->on_row ? c->puzzle->width : c->puzzle->height;
    ptrdiff_t linestep = c->on_row ? 1 : c->puzzle->width;
    const nonogram_cell *line =
      c->on_row ? c->grid + c->puzzle->width * c->lineno : c->grid + c->lineno;

    /* reveal nothing */
    for (i = 0; i < linelen; i++)
      switch (line[i * linestep]) {
      case nonogram_DOT:
      case nonogram_SOLID:
        c->work[i] = line[i * linestep];
        break;
      default:
        c->work[i] = nonogram_BOTH;
        break;
      }

    c->status = nonogram_DONE;
    return;
  }

  c->status = (*c->linesolver[c->level - 1].suite->step)
    (c->linesolver[c->level - 1].context, c->workspace.byte) ?
    nonogram_WORKING : nonogram_DONE;
}

static int redeemstep(nonogram_solver *c)
{
  int changed = 0;
  nonogram_cell *line;
#if 0
  nonogram_sizetype *rule;
  size_t rulelen;
  ptrdiff_t rulestep;
#endif
  size_t linelen, perplen;
  ptrdiff_t linestep, flagstep;
  nonogram_lineattr *attr, *rattr, *cattr;
  nonogram_level *flag;

  size_t i;

  struct {
    int from;
    unsigned inrange : 1;
  } cells = { 0, false }, flags = { 0, false };

  if (c->on_row) {
    line = c->grid + c->puzzle->width * c->lineno;
    linelen = c->puzzle->width;
    linestep = 1;
#if 0
    rule = c->puzzle->row[c->lineno].val;
    rulelen = c->puzzle->row[c->lineno].len;
    rulestep = 1;
#endif
    perplen = c->puzzle->height;
    rattr = c->colattr;
    cattr = c->rowattr + c->lineno;
    flag = c->colflag;
    flagstep = 1;
  } else {
    line = c->grid + c->lineno;
    linelen = c->puzzle->height;
    linestep = c->puzzle->width;
#if 0
    rule = c->puzzle->col[c->lineno].val;
    rulelen = c->puzzle->col[c->lineno].len;
    rulestep = 1;
#endif
    perplen = c->puzzle->width;
    cattr = c->colattr + c->lineno;
    rattr = c->rowattr;
    flag = c->rowflag;
    flagstep = 1;
  }

  for (i = 0; i < linelen; i++)
    switch (line[i * linestep]) {
    case nonogram_BLANK:
      switch (c->work[i]) {
      case nonogram_DOT:
      case nonogram_SOLID:
        changed = 1;
        if (!cells.inrange) {
          cells.from = i;
          cells.inrange = true;
        }
        line[i * linestep] = c->work[i];
        c->remcells--;

        /* update score for perpendicular line */
        attr = &rattr[i * flagstep];
        if (!--*(c->work[i] == nonogram_DOT ?
                 &attr->dot : &attr->solid))
          attr->score = perplen;
        else
          attr->score++;

        /* update score for solved line */
        if (!--*(c->work[i] == nonogram_DOT ? &cattr->dot : &cattr->solid))
          cattr->score = linelen;
        else
          cattr->score++;

        if (flag[i * flagstep] < c->levels) {
          if (flag[i * flagstep] == 0) c->reminfo++;
          flag[i * flagstep] = c->levels;

          if (!flags.inrange) {
            flags.from = i;
            flags.inrange = true;
          }
        } else if (flags.inrange) {
          mark(c, flags.from, i);
          flags.inrange = false;
        }
        break;
      }
      break;
    default:
      if (cells.inrange) {
        redrawrange(c, cells.from, i);
        cells.inrange = false;
      }
      if (flags.inrange) {
        mark(c, flags.from, i);
        flags.inrange = false;
      }
      break;
    }
  if (cells.inrange) {
    redrawrange(c, cells.from, i);
    cells.inrange = false;
  }
  if (flags.inrange) {
    mark(c, flags.from, i);
    flags.inrange = false;
  }

  if (c->level <= c->levels && c->level > 0 &&
      c->linesolver[c->level - 1].suite &&
      c->linesolver[c->level - 1].suite->term)
    (*c->linesolver[c->level - 1].suite->term)
      (c->linesolver[c->level - 1].context);

  return changed;
}

static void mark(nonogram_solver *c, int from, int to)
{
  if (c->display) {
    if (c->on_row) {
      if (c->display->colmark) {
        if (c->reversed) {
          int temp = c->puzzle->height - from;
          from = c->puzzle->height - to;
          to = temp;
        }
        (*c->display->colmark)(c->display_data, from, to);
      }
    } else {
      if (c->display->rowmark) {
        if (c->reversed) {
          int temp = c->puzzle->width - from;
          from = c->puzzle->width - to;
          to = temp;
        }
        (*c->display->rowmark)(c->display_data, from, to);
      }
    }
  }
}

static void redrawrange(nonogram_solver *c, int from, int to)
{
  if (!c->display || !c->display->redrawarea) return;

  if (c->on_row) {
    if (c->reversed) {
      c->editarea.max.x = c->puzzle->width - from;
      c->editarea.min.x = c->puzzle->width - to;
    } else {
      c->editarea.min.x = from;
      c->editarea.max.x = to;
    }
  } else {
    if (c->reversed) {
      c->editarea.max.y = c->puzzle->height - from;
      c->editarea.min.y = c->puzzle->height - to;
    } else {
      c->editarea.min.y = from;
      c->editarea.max.y = to;
    }
  }
  (*c->display->redrawarea)(c->display_data, &c->editarea);
}

/* Find the best cell to make a guess at, and specify what that guess
   should be. */
static void chooseguess(nonogram_solver *restrict c,
                        const struct nonogram_rect *restrict from,
                        struct nonogram_point *restrict pos,
                        nonogram_cell *restrict choice)
{
  size_t x, y;
#if 1
  for (x = from->min.x; x < from->max.x; x++)
    for (y = from->min.y; y < from->max.y; y++)
      if (c->grid[x + y * c->puzzle->width] == nonogram_BLANK)
        goto found;
  assert(false);
 found:
  pos->x = x;
  pos->y = y;
#else
  {
    int bestscore = INT_MIN;
    assert(bestscore <= (int) -(c->puzzle->width + c->puzzle->height));

    for (x = from->min.x; x < from->max.x; x++) {
      int unknown_in_col = c->colattr[x].dot + c->colattr[x].solid;
      for (y = from->min.y; y < from->max.y; y++) {
        if (c->grid[x + y * c->puzzle->width] != nonogram_BLANK)
          continue;

        int score = 0;

#if 1
        /* Use the heuristic line scores. */
        score += c->rowattr[y].score + c->colattr[x].score;
#endif

#if 0
        /* Add a constant to prevent negative numbers.  It's
           probably not worth using this, as it is just more work
           with no extra descrimination.  */
        score += c->puzzle->width + c->puzzle->height;
#endif

#if 0
        /* Favour lines with a high imbalance of unknown dots and
           solids. */

        score += 100 * c->rowattr[y].dot / c->rowattr[y].solid;
        score += 100 * c->rowattr[y].solid / c->rowattr[y].dot;

        score += 100 * c->colattr[x].dot / c->colattr[x].solid;
        score += 100 * c->colattr[x].solid / c->colattr[x].dot;
#endif

#if 0
        /* Favour cells with a high imbalance of unknown dots and
           solids. */

        score += 100 * (c->colattr[x].dot + c->rowattr[y].dot)
          / (c->rowattr[y].solid + c->colattr[x].solid);
        score += 100 * (c->rowattr[y].solid + c->colattr[x].solid)
          / (c->rowattr[y].dot + c->colattr[x].dot);
#endif

#if 1
        /* Ignore a line's unknown count if its perpendicular has
           a smaller unknown count. */
        int unknown_in_row = c->rowattr[y].dot + c->rowattr[y].solid;
        if (unknown_in_row < unknown_in_col)
          score -= unknown_in_row;
        else
          score -= unknown_in_col;
#endif

#if 0
        /* Favour cells that are in a 'corner'. */
        if (x == 0 ||
            c->grid[(x - 1) + y * c->puzzle->width]
            != nonogram_BLANK)
          score += c->puzzle->width;
        if (x + 1 == c->puzzle->width ||
            c->grid[(x + 1) + y * c->puzzle->width]
            != nonogram_BLANK)
          score += c->puzzle->width;
        if (y == 0 ||
            c->grid[x + (y - 1) * c->puzzle->width]
            != nonogram_BLANK)
          score += c->puzzle->height;
        if (y + 1 == c->puzzle->height ||
            c->grid[x + (y + 1) * c->puzzle->width]
            != nonogram_BLANK)
          score += c->puzzle->height;
#endif

#if 0
        /* Favour lines with fewer unknowns. */
        score -= c->colattr[x].dot;
        score -= c->rowattr[y].dot;
        score -= c->colattr[x].solid;
        score -= c->rowattr[y].solid;
#endif



        if (score <= bestscore)
          continue;
        bestscore = score;
        pos->x = x;
        pos->y = y;
      }
    }
  }
#endif

  /* Choose the colour based on what's left in the row and column. */
  if (c->colattr[pos->x].dot + c->rowattr[pos->y].dot >
      c->colattr[pos->x].solid + c->rowattr[pos->y].solid)
    *choice = nonogram_DOT;
  else
    *choice = nonogram_SOLID;
}

const char *const nonogram_date = __DATE__;
