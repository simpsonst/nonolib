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

#include <stdlib.h>
#include <stdio.h>

#include "nonogram.h"

int main(int argc, char **argv)
{
  nonogram_puzzle puz;
  FILE *fp;

  if (argc < 2) {
    fprintf(stderr, "usage: %s filename\n", argv[0]);
    return EXIT_FAILURE;
  }

  fp = fopen(argv[1], "r");
  if (!fp) {
    perror(argv[1]);
    return EXIT_FAILURE;
  }

  if (nonogram_fscanpuzzle(&puz, fp) < 0) {
    fprintf(stderr, "%s: error on input\n", argv[0]);
    fclose(fp);
    return EXIT_FAILURE;
  }

  fclose(fp);

  nonogram_fprintpuzzle(&puz, stdout);

  return EXIT_SUCCESS;
}
