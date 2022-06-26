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

int nonogram_printrule(const struct nonogram_rule *rule, FILE *fp)
{
  size_t ruleno, count = 0;
  if (rule->len == 0)
    return fprintf(fp, "0");
  for (ruleno = 0; ruleno < rule->len; ruleno++)
    count += fprintf(fp, "%lu%s", (unsigned long) rule->val[ruleno],
                     (ruleno == rule->len - 1) ? "" : ",");
  return count;
}
