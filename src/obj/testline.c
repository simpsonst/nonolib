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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>

#include "nonogram.h"

#define MAX_LINE (50)
#define MAX_RULE ((MAX_LINE+1)/2)

#define R 0
#define L 25


#ifdef __unix__
#include <sys/time.h>
typedef struct timeval TIMER;
static inline int GETTIME(TIMER *t) {
  return gettimeofday(t, NULL);
}
static inline double DIFFTIME(const TIMER *t1, const TIMER *t2) {
  return t1->tv_sec - t2->tv_sec + 1e-6 * (t1->tv_usec - t2->tv_usec);
}
#else
typedef clock_t TIMER;
#endif


static unsigned long unirand(unsigned long low, unsigned long high)
{
  unsigned long N = high - low + 1;
  return low + (unsigned long) ((double) rand() / ((double) RAND_MAX + 1) * N);
}

static int printline(const nonogram_cell *p, size_t n)
{
  static const char c[] = { ' ', '-', '#', '+' };
  unsigned long i;

  for (i = 0; i < n; i++)
    putchar(c[p[i] & 3]);
  return n;
}

static void makeline(nonogram_cell *line, size_t *linelenp,
                     nonogram_sizetype *rule, size_t *rulelenp)
{
  size_t x, i;
  nonogram_cell cand = nonogram_SOLID;
  *linelenp = unirand(3, *linelenp);
  x = unirand(2, *linelenp);
  x = unirand(1, x);
  x = unirand(0, x);
  i = 0;
  *rulelenp = 0;

  while (i < x)
    line[i++] = nonogram_DOT;

  while (x < *linelenp) {
    nonogram_sizetype step = unirand(1, unirand(1, *linelenp - x));
    if (cand == nonogram_SOLID) {
      rule[(*rulelenp)++] = step;
    }
    x += step;
    while (i < x)
      line[i++] = cand;
    cand ^= nonogram_BOTH;
  }
}

static void breakline(const nonogram_cell *line, nonogram_cell *broken,
                      size_t linelen)
{
  size_t x = 0, i = 0;
  nonogram_cell mask = unirand(0, 1) ? nonogram_BOTH : nonogram_BLANK;

  while (x < linelen) {
    nonogram_sizetype step = unirand(1, unirand(1, linelen - x));
    x += step;
    while (i < x)
      (broken[i] = line[i] & mask), i++;
    mask ^= nonogram_BOTH;
  }
}

static int printrule(const nonogram_sizetype *r, size_t len)
{
  if (len == 0) {
    return printf("0");
  } else {
    int c = 0;
    size_t i;

    c += printf("%lu", r[0]);
    for (i = 1; i < len; i++)
      c += printf(",%lu", r[i]);
    return c;
  }
}

static void setline(nonogram_cell *cp, size_t *lp, const char *txt)
{
  for (*lp = 0; *txt; ++*lp) {
    switch (*txt++) {
    case '-':
      *cp++ = nonogram_DOT;
      break;
    case '#':
      *cp++ = nonogram_SOLID;
      break;
    default:
      *cp++ = nonogram_BLANK;
      break;
    }
  }
}

struct supply_suite {
  int (*get)(void *, size_t maxline, size_t maxrule,
             nonogram_cell *complete, nonogram_cell *broken, size_t *linelen,
             nonogram_sizetype *rule, size_t *rulelen);
};

struct interest_suite {
  int (*interest)(void *, size_t linelen, const nonogram_cell *result1,
                  const nonogram_cell *result2);
};

static void argsetrule(nonogram_sizetype *rp, size_t *lp,
                       int argc, const char *const *argv)
{
  int i;
  *lp = argc;
  for (i = 0; i < argc; i++)
    rp[i] = atoi(argv[i]);
}

static void vsetrule(nonogram_sizetype *rp, size_t *lp, size_t n, ...)
{
  va_list ap;

  va_start(ap, n);
  for (*lp = 0; *lp < n; ++*lp)
    *rp++ = va_arg(ap, int);
  va_end(ap);
}

union workspace {
  nonogram_completework complete;
  nonogram_fastwork fast;
  nonogram_oddoneswork oddones;
};

int solveline(const struct nonogram_linesuite *fs, void *fw,
              const nonogram_cell *line, size_t linelen,
              const nonogram_sizetype *rule, size_t rulelen,
              nonogram_cell *result,
              ptrdiff_t *difw,
              nonogram_sizetype *pos,
              nonogram_cell *cell,
              union workspace *data,
              struct nonogram_log *logp)
{
  static struct nonogram_req zero;
  struct nonogram_lim lim;
  struct nonogram_req req = zero;
  struct nonogram_ws ws;
  struct nonogram_initargs args;
  int fits = 0, status;

  lim.maxline = linelen;
  lim.maxrule = rulelen;

  fs->prep(fw, &lim, &req);

  if (req.byte > sizeof(union workspace) ||
      req.ptrdiff > MAX_RULE || req.size > 0 ||
      req.nonogram_size > 2 * MAX_RULE ||
      req.cell > MAX_LINE * 2) {
    fprintf(stderr, "Memory requirements!\n"
            "%lu bytes\n"
            "%lu ptrdiff_t\n"
            "%lu size_t\n"
            "%lu nonogram_sizetype\n", 
            (unsigned long) req.byte, 
            (unsigned long) req.ptrdiff, 
            (unsigned long) req.size, 
            (unsigned long) req.nonogram_size);
    return -1;
  }

  ws.nonogram_size = pos;
  ws.size = NULL;
  ws.ptrdiff = difw;
  ws.byte = data;
  ws.cell = cell;

  args.fits = &fits;
  args.log = logp;
  args.rule = rule;
  args.line = line;
  args.result = result;
  args.linelen = linelen;
  args.rulelen = rulelen;
  args.linestep = args.rulestep = args.resultstep = 1;

  status = fs->init(fw, &ws, &args);

  while (status && fs->step)
    status = fs->step(fw, data);
  if (fs->term) fs->term(fw);

  return fits;
}

static void compare_solvers(const nonogram_cell line[MAX_LINE],
                            const nonogram_cell broken[MAX_LINE],
                            size_t linelen,
                            const nonogram_sizetype rule[MAX_RULE],
                            size_t rulelen,
                            struct nonogram_log *log,
                            struct nonogram_log *nolog)
{
  int fast_fits, complete_fits, oddones_fits;

  nonogram_cell fast[MAX_LINE], complete[MAX_LINE],
    oddones_input[MAX_LINE], oddones[MAX_LINE],
    cellspace[MAX_LINE * 2];
  ptrdiff_t difw[MAX_RULE];
  nonogram_sizetype pos[MAX_RULE * 2];
  union workspace wksp;

  complete_fits = solveline(&nonogram_completesuite, NULL,
                            broken, linelen, rule, rulelen,
                            complete, difw, pos, cellspace,
                            &wksp, nolog);

  fast_fits = solveline(&nonogram_fastsuite, NULL,
                        broken, linelen, rule, rulelen,
                        fast, difw, pos, cellspace,
                        &wksp, nolog);

  memcpy(oddones_input, fast, sizeof(nonogram_cell) * linelen);
  {
    size_t i;
    for (i = 0; i < linelen; i++)
      if (oddones_input[i] == nonogram_BOTH)
        oddones_input[i] = nonogram_BLANK;
  }
  oddones_fits = solveline(&nonogram_oddonessuite, NULL,
                           oddones_input, linelen, rule, rulelen,
                           oddones, difw, pos, cellspace,
                           &wksp, nolog);

  if (memcmp(complete, fast, sizeof(nonogram_cell) * linelen)) {
    size_t i;

    printf("Length:   %lu\n", (unsigned long) linelen);
    printf("Rule:     ");
    printrule(rule, rulelen);
    if (line) {
      printf("\nOriginal: >");
      printline(line, linelen);
    }
    printf("<\nBroken:   >");
    printline(broken, linelen);
    printf("<\nComplete: >");
    printline(complete, linelen);
    printf("<\n");
    complete_fits = solveline(&nonogram_completesuite, NULL,
                              broken, linelen, rule, rulelen,
                              complete, difw, pos, cellspace,
                              &wksp, log);
    printf("Fast:     >");
    printline(fast, linelen);
    printf("<\n");
    fast_fits = solveline(&nonogram_fastsuite, NULL,
                          broken, linelen, rule, rulelen,
                          fast, difw, pos, cellspace,
                          &wksp, log);

    printf("Odd ones: >");
    printline(oddones, linelen);
    printf("<%s\n",
           memcmp(complete, oddones,
                  sizeof(nonogram_cell) * linelen) ?
           " wrong" : " correct");
    oddones_fits = solveline(&nonogram_oddonessuite, NULL,
                             oddones_input, linelen, rule, rulelen,
                             oddones, difw, pos, cellspace,
                             &wksp, log);

    printf("Diff:     >");
    for (i = 0; i < linelen; i++) {
      if (complete[i] == fast[i]) {
        putchar(' ');
        continue;
      }
      switch (complete[i]) {
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
    printf("\n");
  }
}

struct solver {
  const char *name;
  const struct nonogram_linesuite *ops;
  void *conf;
};

static struct solver solvers[] = {
  { .name = "complete", .ops = &nonogram_completesuite },
  { .name = "fast", .ops = &nonogram_fastsuite },
  { .name = "fcomp", .ops = &nonogram_fcompsuite },
};

#define SOLVERS (sizeof solvers / sizeof solvers[0])

struct file_supply {
  const char *name;
  FILE *fp;
};

static const struct nonogram_req null_req;

static const struct supply_suite random_supply, file_supply;

static const struct interest_suite always_interested, diff_is_interesting;

int main(int argc, const char *const *argv)
{
  srand(time(NULL));

  int puzzle_counter = -1;
  struct file_supply file_ctxt;

  const struct supply_suite *supply = &random_supply;
  void *supply_conf = &puzzle_counter;

  const struct interest_suite *interest = &diff_is_interesting;
  //&always_interested;
  void *interest_conf = NULL;

  struct nonogram_log stdoutlog = {
    .file = stdout,
    .indent = 0,
    .level = 2,
  }, nolog = {
    .file = NULL,
    .indent = 0,
    .level = 0,
  };


  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-f")) {
      file_ctxt.name = argv[++i];
      file_ctxt.fp = NULL;
      supply = &file_supply;
      supply_conf = &file_ctxt;
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "%s: unknown switch %s\n", argv[0], argv[i]);
      return EXIT_FAILURE;
    }
  }


  // Specify the maximum dimensions that must be dealt with.
  const struct nonogram_lim lim = {
    .maxline = MAX_LINE,
    .maxrule = MAX_RULE,
  };

  // Find out how much workspace each line solver needs.
  struct nonogram_req req = null_req;
  for (size_t i = 0; i < SOLVERS; i++) {
    struct nonogram_req tmp = null_req;
    if (solvers[i].ops->prep) {
      solvers[i].ops->prep(solvers[i].conf, &lim, &tmp);
      if (tmp.byte > req.byte)
        req.byte = tmp.byte;
      if (tmp.ptrdiff > req.ptrdiff)
        req.ptrdiff = tmp.ptrdiff;
      if (tmp.nonogram_size > req.nonogram_size)
        req.nonogram_size = tmp.nonogram_size;
      if (tmp.cell > req.cell)
        req.cell = tmp.cell;
    }
  }

  struct nonogram_ws ws;
  ws.byte = malloc(req.byte);
  if (req.byte && !ws.byte) {
    fprintf(stderr, "Unable to allocate workspace: byte %zu\n",
            req.byte);
    return EXIT_FAILURE;
  }
  ws.ptrdiff = malloc(req.ptrdiff * sizeof *ws.ptrdiff);
  if (req.ptrdiff && !ws.ptrdiff) {
    fprintf(stderr, "Unable to allocate workspace: ptrdiff %zu\n",
            req.ptrdiff);
    return EXIT_FAILURE;
  }
  ws.nonogram_size = malloc(req.nonogram_size * sizeof *ws.nonogram_size);
  if (req.nonogram_size && !ws.nonogram_size) {
    fprintf(stderr, "Unable to allocate workspace: nonogram_size %zu\n",
            req.nonogram_size);
    return EXIT_FAILURE;
  }
  ws.cell = malloc(req.cell * sizeof *ws.cell);
  if (req.cell && !ws.cell) {
    fprintf(stderr, "Unable to allocate workspace: cell %zu\n",
            req.cell);
    return EXIT_FAILURE;
  }

  // details of the current line
  nonogram_cell line[MAX_LINE], broken[MAX_LINE];
  nonogram_sizetype rule[MAX_RULE];
  size_t rulelen, linelen;

  while (supply->get(supply_conf, MAX_LINE, MAX_RULE,
                     line, broken, &linelen,
                     rule, &rulelen)) {
    //printf("New puzzle!\n");

    // results for each solver
    int fits[SOLVERS];
    nonogram_cell solutions[SOLVERS][MAX_LINE];
    double delay[SOLVERS];

    struct nonogram_initargs args = {
      .rule = rule,
      .rulelen = rulelen,
      .rulestep = 1,
      .line = broken,
      .linelen = linelen,
      .linestep = 1,
      .log = &nolog,
      .resultstep = 1,
    };

    for (size_t i = 0; i < SOLVERS; i++) {
      //printf("Trying %s\n", solvers[i].name);
      args.fits = &fits[i];
      args.result = solutions[i];

      TIMER start, end;

      GETTIME(&start);
      bool more = solvers[i].ops->init(solvers[i].conf, &ws, &args);
      while (more)
        more = solvers[i].ops->step(solvers[i].conf, ws.byte);
      GETTIME(&end);
      delay[i] = DIFFTIME(&end, &start);
    }

    // Determine whether the provided solutions are sufficiently
    // interesting.
    bool ignore = true;
    for (size_t i = 0; ignore && i + 1 < SOLVERS; i++)
      for (size_t j = i + 1; ignore && j < SOLVERS; j++)
        ignore = !interest->interest(interest_conf, linelen,
                                     solutions[i], solutions[j]);

    if (ignore)
      continue;

    // Describe the puzzle.
    printf("Length:   %zu\n", linelen);
    printf("Rule:     ");
    printrule(rule, rulelen);
    printf("\nOriginal: >");
    printline(line, linelen);
    printf("<\nBroken:   >");
    printline(broken, linelen);
    printf("<\n");
    
    // Make a report on each solver.
    for (size_t i = 0; i < SOLVERS; i++) {
      printf("%10.10s>", solvers[i].name);
      printline(solutions[i], linelen);
      printf("< (%3d) ", fits[i]);
      if (delay[i] >= 0.1)
        printf("%5.3f s", delay[i]);
      else
        printf("%5.3fms", delay[1] * 1000.0);
      printf("\n");
    }
  }

  for (size_t i = 0; i < SOLVERS; i++)
    if (solvers[i].ops->term)
      solvers[i].ops->term(solvers[i].conf);

  free(ws.cell);
  free(ws.nonogram_size);
  free(ws.ptrdiff);
  free(ws.byte);

  return EXIT_SUCCESS;
}


static int random_get(void *vp, size_t maxline, size_t maxrule,
                      nonogram_cell *complete, nonogram_cell *broken,
                      size_t *linelen,
                      nonogram_sizetype *rule, size_t *rulelen)
{
  int *rem = vp;

  if (*rem == 0)
    return false;
  if (*rem > 0)
    --*rem;

  *linelen = maxline;
  *rulelen = maxrule;

  makeline(complete, linelen, rule, rulelen);
  breakline(complete, broken, *linelen);

  return true;
}

static const struct supply_suite random_supply = {
  .get = &random_get,
};

static int true_interest(void *vp, size_t linelen,
                         const nonogram_cell *result1,
                         const nonogram_cell *result2)
{
  return true;
}

static const struct interest_suite always_interested = {
  .interest = &true_interest,
};

static int diff_interest(void *vp, size_t linelen,
                         const nonogram_cell *result1,
                         const nonogram_cell *result2)
{
  for (size_t i = 0; i < linelen; i++)
    if (result1[i] != result2[i])
      return true;
  return false;
}

static const struct interest_suite diff_is_interesting = {
  .interest = &diff_interest,
};

static int file_get(void *vp, size_t maxline, size_t maxrule,
                    nonogram_cell *complete, nonogram_cell *broken,
                    size_t *linelen,
                    nonogram_sizetype *rule, size_t *rulelen)
{
  struct file_supply *c = vp;

  if (!c->fp) {
    c->fp = fopen(c->name, "r");
    if (!c->fp)
      return false;
  }

  char line[100];
  while (fgets(line, sizeof line, c->fp) &&
         sscanf(line, "Length: %zu", linelen) < 1)
    ;

  if (feof(c->fp))
    return false;

  *rulelen = 0;
  if (!fgets(line, sizeof line, c->fp))
    return false;

  const char *ptr = line + strcspn(line, ":") + 1;
  while (*ptr != '\0') {
    char *next;
    rule[*rulelen] = strtoul(ptr, &next, 0);
    ++*rulelen;
    if (next == ptr)
      break;
    if (*next == ',')
      ptr = next + 1;
    else
      break;
  }

  

  if (!fgets(line, sizeof line, c->fp))
    return false;

  ptr = line + strcspn(line, ">") + 1;
  for (nonogram_sizetype i = 0; i < *linelen && *ptr && *ptr != '<';
       i++, ptr++) {
    switch (*ptr) {
    default:
      complete[i] = nonogram_DOT;
      break;
    case '#':
      complete[i] = nonogram_SOLID;
      break;
    }
  }

  if (!fgets(line, sizeof line, c->fp))
    return false;

  ptr = line + strcspn(line, ">") + 1;
  for (nonogram_sizetype i = 0; i < *linelen && *ptr && *ptr != '<';
       i++, ptr++) {
    switch (*ptr) {
    default:
      broken[i] = nonogram_BLANK;
      break;
    case '-':
      broken[i] = nonogram_DOT;
      assert(broken[i] == complete[i]);
      break;
    case '#':
      broken[i] = nonogram_SOLID;
      assert(broken[i] == complete[i]);
      break;
    }
  }

  return true;
}

static const struct supply_suite file_supply = {
  .get = &file_get,
};
