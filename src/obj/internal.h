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

#ifndef NONOGRAM_INTERNAL_HEADER
#define NONOGRAM_INTERNAL_HEADER

#if __STDC__ != 1
#error "You need an ISO C compiler."
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "nonogram.h"

  int nonogram_printrule(const struct nonogram_rule *rule, FILE *fp);



  /* Alignment technique seen here:
     http://www.monkeyspeak.com/alignment/ */

#define Tchar(T) struct { T t; char c; }

#define strideof(T) \
  ((sizeof(Tchar(T)) > sizeof(T)) ? \
  sizeof(Tchar(T))-sizeof(T) : sizeof(T))


#define align(S,T) (((S)+(strideof(T)-1u))/strideof(T)*strideof(T))



#define UNUSED(X) ((X) = (X))
#ifdef __cplusplus
}
#endif

#endif
