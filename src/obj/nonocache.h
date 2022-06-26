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

#ifndef nonocache_HEADER
#define nonocache_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "nonogram.h"

  int nonocache_encodepuzzle(char **out, size_t *len, const nonogram_puzzle *);
  int nonocache_decodepuzzle(const char **in, size_t *len, nonogram_puzzle *);
  int nonocache_encodecells(char **out, size_t *len,
			    size_t w, size_t h, const nonogram_cell *);
  int nonocache_decodecells(const char **in, size_t *len,
			    size_t w, size_t h, nonogram_cell *);

#ifdef __cplusplus
}
#endif

#endif
