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

#include "nonogram.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#if false
void nonogram_cleargrid(nonogram_cell *grid, size_t width, size_t height)
{
  size_t row, col;

  for (row = 0; row < height; row++)
    for (col = 0; col < width; col++)
      grid[row * width + col] = nonogram_BLANK;
}
#endif

#if false
int nonogram_scangrid(nonogram_cell *grid, size_t width, size_t height,
                      FILE *fp, char solid, char dot)
{
  size_t row, col;
  int c;

  c = getc(fp);
  for (row = 0; row < height; row++) {
    for (col = 0; col < width; col++) {
      if (c == solid)
        grid[col + row * width] = nonogram_SOLID;
      else if (c == dot)
        grid[col + row * width] = nonogram_DOT;
      else
        grid[col + row * width] = nonogram_BLANK;
      if (c != EOF && c != '\n')
        c = getc(fp);
    }
    while (c != '\n' && c != EOF)
      c = getc(fp);
    if (c == '\n')
      c = getc(fp);
  }
  return 1;
}
#endif

int nonogram_checkgrid(const nonogram_puzzle *p, const nonogram_cell *g)
{
  size_t lineno;

  for (lineno = 0; lineno < p->width; lineno++)
    if (nonogram_checkline(p->col[lineno].val, p->col[lineno].len, 1,
                           g + lineno, p->height, p->width) < 0)
      return -1;

  for (lineno = 0; lineno < p->height; lineno++)
    if (nonogram_checkline(p->row[lineno].val, p->row[lineno].len, 1,
                           g + lineno * p->width, p->width, 1) < 0)
      return -1;

  return 0;
}

int nonogram_printgrid(const nonogram_cell *grid,
                       size_t width, size_t height,
                       FILE *fp, const char *solid, const char *dot,
                       const char *blank)
{
  int r = 0;

  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++)
      switch (grid[x + y * width]) {
      case nonogram_DOT:
        r += fprintf(fp, "%s", dot);
        break;

      case nonogram_SOLID:
        r += fprintf(fp, "%s", solid);
        break;

      case nonogram_BLANK:
        r += fprintf(fp, "%s", blank);
        break;

      default:
        fputc('?', fp);
        r++;
        break;
      }
    fputc('\n', fp);
    r++;
  }
  return r;
}
