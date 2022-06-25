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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <float.h>
#include <stdarg.h>
#include <assert.h>

#include "nonogram.h"
#include "internal.h"

#ifndef nonogram_MAXRULES
#define nonogram_MAXRULES 60
#endif

struct linectxt {
  size_t lineno;
  size_t rowno, colno, maxrule;
  unsigned onrows : 1, oncolumns : 1,
    nowidth : 1, noheight : 1, nomaxrule : 1;
};

static int scanline(nonogram_puzzle *p, struct linectxt *ctxt,
		    const char *line, const char *end,
		    nonogram_errorproc *ef, void *eh);

static int fpfw(void *x, const char *fmt, ...)
{
  FILE *fp = x;
  va_list ap;
  int rc;

  va_start(ap, fmt);
  rc = vfprintf(fp, fmt, ap);
  va_end(ap);

  return rc;
}

int nonogram_fscanpuzzle(nonogram_puzzle *p, FILE *fp)
{
  return nonogram_fscanpuzzle_ef(p, fp, &fpfw, stderr);
}

int nonogram_fscanpuzzle_ef(nonogram_puzzle *p, FILE *fp,
			    nonogram_errorproc *ef, void *eh)
{
  struct linectxt ctxt;
  static char line[400];
  int rc = 0;

  p->row = p->col = NULL;
  p->width = p->height = 0;
#if false
  p->title = NULL;
#endif
  p->notes = NULL;

  ctxt.nowidth = ctxt.noheight = ctxt.nomaxrule = true;
  ctxt.onrows = ctxt.oncolumns = false;
  ctxt.lineno = 1;

  while (((ctxt.noheight || ctxt.rowno < p->height) ||
	  (ctxt.nowidth || ctxt.colno < p->width)) &&
	 fgets(line, sizeof line, fp) &&
	 (rc = scanline(p, &ctxt, line, line + strlen(line), ef, eh)))
    ctxt.lineno++;

  if (ef) {
    if (ctxt.noheight)
      (*ef)(eh, "No height specified\n");
    else if (ctxt.rowno < p->height)
      (*ef)(eh, "Insufficient row data (%d still expected)\n",
	    (int) (p->height - ctxt.rowno));

    if (ctxt.nowidth)
      (*ef)(eh, "No width specified\n");
    else if (ctxt.colno < p->width)
      (*ef)(eh, "Insufficient column data (%d still expected)\n",
	    (int) (p->width - ctxt.colno));
  }

  if (rc == 0 || ctxt.noheight || ctxt.rowno < p->height ||
      ctxt.nowidth || ctxt.colno < p->width) {
    nonogram_freepuzzle(p);
    return -1;
  }

  return 0;
}

static const char *mygetline(const char **s, const char *e,
			     const char **st, const char **en)
{
  if (*s >= e || !**s)
    return NULL;

  *st = *s;
  while (*s < e && **s && **s != '\n')
    (*s)++;
  *en = *s;
  if (*s < e && **s == '\n')
    (*s)++;
  return *st;
}

int nonogram_spscanpuzzle(nonogram_puzzle *p,
			  const char **s, const char *e)
{
  return nonogram_spscanpuzzle_ef(p, s, e, &fpfw, stderr);
}

int nonogram_spscanpuzzle_ef(nonogram_puzzle *p,
			     const char **s, const char *e,
			     nonogram_errorproc *ef, void *eh)
{
  struct linectxt ctxt;
  const char *line, *end;
  int rc = 0;

  p->row = p->col = NULL;
  p->width = p->height = 0;
#if false
  p->title = NULL;
#endif
  p->notes = NULL;

  ctxt.nowidth = ctxt.noheight = ctxt.nomaxrule = true;
  ctxt.onrows = ctxt.oncolumns = false;
  ctxt.lineno = 1;

  while (((ctxt.noheight || ctxt.rowno < p->height) ||
	  (ctxt.nowidth || ctxt.colno < p->width)) &&
	 mygetline(s, e, &line, &end) &&
	 (rc = scanline(p, &ctxt, line, end, ef, eh)))
    ctxt.lineno++;

  if (ef) {
    if (ctxt.noheight)
      (*ef)(eh, "No height specified\n");
    else if (ctxt.rowno < p->height)
      (*ef)(eh, "Insufficient row data (%d still expected)\n",
	    (int) (p->height - ctxt.rowno));

    if (ctxt.nowidth)
      (*ef)(eh, "No width specified\n");
    else if (ctxt.colno < p->width)
      (*ef)(eh, "Insufficient column data (%d still expected)\n",
	    (int) (p->width - ctxt.colno));
  }

  if (rc == 0 || ctxt.noheight || ctxt.rowno < p->height ||
      ctxt.nowidth || ctxt.colno < p->width) {
    nonogram_freepuzzle(p);
    return -1;
  }

  return 0;
}

static void freenote(struct nonogram_tn *n)
{
  if (n->c[0]) freenote(n->c[0]);
  if (n->c[1]) freenote(n->c[1]);
  free(n->n);
  free(n->v);
  free(n);
}

static struct nonogram_tn *copynotes(const struct nonogram_tn *n)
{
  struct nonogram_tn *c;

  c = malloc(sizeof *c);
  if (!c)
    return c;
  c->n = c->v = NULL;
  c->c[0] = c->c[1] = NULL;
  if (!(c->n = malloc(strlen(n->n) + 1)))
    goto cleanup;
  strcpy(c->n, n->n);
  if (!(c->v = malloc(strlen(n->v) + 1)))
    goto cleanup;
  strcpy(c->v, n->v);
  if (n->c[0] && !(c->c[0] = copynotes(n->c[0])))
    goto cleanup;
  if (n->c[1] && !(c->c[1] = copynotes(n->c[1])))
    goto cleanup;
  return c;

 cleanup:
  freenote(c);
  return NULL;
}

int nonogram_copypuzzle(nonogram_puzzle *to,
			const nonogram_puzzle *from)
{
  size_t line;

  to->width_cap = to->width = from->width;
  to->height_cap = to->height = from->height;
  to->row = to->col = NULL;
  to->notes = NULL;
  if (from->notes && !(to->notes = copynotes(from->notes)))
    goto cleanup;

  to->row = malloc(to->height_cap * sizeof *to->row);
  if (!to->row)
    goto cleanup;

  to->col = malloc(to->width_cap * sizeof *to->col);
  if (!to->col)
    goto cleanup;

  for (line = 0; line < to->width; line++) {
    struct nonogram_rule *xa = &to->col[line];
    const struct nonogram_rule *xb = &from->col[line];;
    xa->cap = xa->len = xb->len;
    xa->val = malloc(xa->cap * sizeof *xa->val);
    if (!xa->val)
      goto cleanup;
    memcpy(xa->val, xb->val, sizeof *xa->val * xa->len);
  }

  for (line = 0; line < to->height; line++) {
    struct nonogram_rule *xa = &to->row[line];
    const struct nonogram_rule *xb = &from->row[line];
    xa->cap = xa->len = xb->len;
    xa->val = malloc(xa->cap * sizeof *xa->val);
    if (!xa->val)
      goto cleanup;
    memcpy(xa->val, xb->val, sizeof *xa->val * xa->len);
  }
  return 0;

 cleanup:
  nonogram_freepuzzle(to);
  return -1;
}

void nonogram_freepuzzle(nonogram_puzzle *p)
{
  unsigned i;

  if (p->row) {
    for (i = 0; i < p->height; i++)
      free(p->row[i].val);
    free(p->row);
    p->row = NULL;
  }
  if (p->col) {
    for (i = 0; i < p->width; i++)
      free(p->col[i].val);
    free(p->col);
    p->col = NULL;
  }

#if false
  free(p->title);
#endif
  if (p->notes) {
    freenote(p->notes);
    p->notes = NULL;
  }
}

int nonogram_verifypuzzle(const nonogram_puzzle *p)
{
  int sum = 0;
  size_t lineno, relno;

  for (lineno = 0; lineno < p->height; lineno++)
    for (relno = 0; relno < p->row[lineno].len; relno++)
      sum += p->row[lineno].val[relno];

  for (lineno = 0; lineno < p->width; lineno++)
    for (relno = 0; relno < p->col[lineno].len; relno++)
      sum -= p->col[lineno].val[relno];

  return sum;
}

#if false
int nonogram_settitle(nonogram_puzzle *p, const char *s)
{
  char *tmp = malloc(strlen(s) + 1);

  if (tmp) {
    free(p->title);
    p->title = tmp;
    strcpy(tmp, s);
    return 0;
  }
  return -1;
}

int nonogram_unsettitle(nonogram_puzzle *p)
{
  free(p->title), p->title = NULL;
  return 0;
}
#endif

static struct nonogram_tn **findnote(struct nonogram_tn **t, const char *n)
{
  int c;
  while (*t && (c = strcmp(n, (*t)->n)))
    t = &(*t)->c[c > 0];
  return t;
}

int nonogram_unsetnote(nonogram_puzzle *p, const char *n)
{
  struct nonogram_tn **npp, *np, *npc;

  if (!p) return -1;
  npp = findnote(&p->notes, n);

  if (!*npp) return 0;

  np = *npp;
  if (np->c[0])
    *npp = np->c[0], npc = np->c[1];
  else
    *npp = np->c[1], npc = np->c[0];

  if (npc) {
    npp = findnote(npp, npc->n);
    *npp = npc;
  }

  free(np->n);
  free(np->v);
  free(np);
  return 0;
}

int nonogram_setnote(nonogram_puzzle *p, const char *n, const char *v)
{
  struct nonogram_tn **npp;
  int len;

  if (!p) return -1;
  npp = findnote(&p->notes, n);

  if (!*npp) {
    *npp = malloc(sizeof(struct nonogram_tn));
    if (!*npp) return -1;
    (*npp)->c[0] = (*npp)->c[1] = NULL;
    (*npp)->n = (*npp)->v = NULL;
    len = strlen(n) + 1;
    (*npp)->n = malloc(len);
    if (!(*npp)->n) {
      free(*npp);
      return -1;
    }
    memcpy((*npp)->n, n, len);
    (*npp)->v = NULL;
  }

  if ((*npp)->v)
    free((*npp)->v);
  len = strlen(v) + 1;
  (*npp)->v = malloc(len);
  if (!(*npp)->v) return -1;
  memcpy((*npp)->v, v, len);

  return 0;
}

const char *nonogram_getnote(nonogram_puzzle *p, const char *n)
{
  struct nonogram_tn *np;

  if (!p) return NULL;
  np = *findnote(&p->notes, n);

  return np ? np->v : NULL;
}

static int nonogram_parseline(const nonogram_cell *st,
			      size_t len, ptrdiff_t step,
			      nonogram_sizetype *v, ptrdiff_t vs)
{
  size_t i;
  int count = 0;
  int inblock = false;

#if 0
  printf(" >");
  for (i = 0; i < len; i++)
    switch (st[i * step]) {
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
  putchar('\n');
#endif

  for (i = 0; i < len; i++)
    switch (st[i * step]) {
    case nonogram_DOT:
      if (inblock) {
	inblock = false;
	count++;
      }
      break;
    case nonogram_SOLID:
      if (!inblock) {
	inblock = true;
	if (v)
	  v[count * vs] = 0;
      }
      if (v)
	v[count * vs]++;
      break;
    default:
      return -1;
    }
  return count + !!inblock;
}

int nonogram_makepuzzle(nonogram_puzzle *p, const nonogram_cell *g,
			size_t w, size_t h)
{
  size_t n;

  if (!p || !g) return -1;

  p->row = p->col = NULL;
  p->notes = NULL;

  p->width = p->width_cap = w;
  p->height = p->height_cap = h;
#if false
  p->title = NULL;
#endif
  p->notes = NULL;
  p->row = malloc(sizeof *p->row * p->height_cap);
  p->col = malloc(sizeof *p->col * p->width_cap);
  if (!p->row || !p->col)
    goto failure;

  for (n = 0; n < h; n++) {
    int rc = nonogram_parseline(g + w * n, w, 1, NULL, 0);
    if (rc < 0)
      goto failure;
    p->row[n].len = p->row[n].cap = rc;
    p->row[n].val = NULL;
  }
  for (n = 0; n < w; n++) {
    int rc = nonogram_parseline(g + n, h, w, NULL, 0);
    if (rc < 0)
      goto failure;
    p->col[n].len = p->col[n].cap = rc;
    p->col[n].val = NULL;
  }
  for (n = 0; n < h; n++) {
    struct nonogram_rule *rule = &p->row[n];
    rule->val = malloc(sizeof *rule->val * rule->cap);
    if (!rule->val)
      goto worse;
    int rc = nonogram_parseline(g + w * n, w, 1, rule->val, 1);
    assert(rc >= 0);
  }
  for (n = 0; n < w; n++) {
    struct nonogram_rule *rule = &p->col[n];
    rule->val = malloc(sizeof *rule->val * rule->cap);
    if (!rule->val)
      goto worse;
    int rc = nonogram_parseline(g + n, h, w, rule->val, 1);
    assert(rc >= 0);
  }
  return 0;

worse:
  for (n = 0; n < h; n++)
    free(p->row[n].val);
  for (n = 0; n < w; n++)
    free(p->col[n].val);
failure:
  free(p->row), p->row = NULL;
  free(p->col), p->col = NULL;
  return -1;
}

static int nonogram_getmaxrule(const nonogram_puzzle *p)
{
  size_t max = 1, n;

  for (n = 0; n < p->width; n++)
    if (p->col[n].len > max)
      max = p->col[n].len;

  for (n = 0; n < p->height; n++)
    if (p->row[n].len > max)
      max = p->row[n].len;

  return max;
}

static int printword(const char *cp, FILE *fp)
{
  int count = 0;
  for (; *cp; cp++)
    if (*cp == '\"')
      count += fprintf(fp, "\\\"");
    else if (*cp == '\\')
      count += fprintf(fp, "\\\\");
    else if (*cp < 32 || *cp > 126)
      count += fprintf(fp, "?");
    else
      count++, fputc(*cp, fp);
  return count;
}

static int printnotes(struct nonogram_tn *np, FILE *fp)
{
  int r = 0;
  if (np->c[0]) r += printnotes(np->c[0], fp);
  if (np->n && np->v) {
    r += printword(np->n, fp);
    r += fprintf(fp, " \"");
    r += printword(np->v, fp);
    r += fprintf(fp, "\"\n");
  }
  if (np->c[1]) r += printnotes(np->c[1], fp);
  return r;
}

int nonogram_fprintpuzzle(const nonogram_puzzle *p, FILE *fp)
{
  int count = 0;
  size_t n;

  count += fprintf(fp, "maxrule %d\n", nonogram_getmaxrule(p));
  count += fprintf(fp, "width %lu\nheight %lu\n",
		   (unsigned long) p->width, (unsigned long) p->height);
  if (p->notes) count += printnotes(p->notes, fp);
#if false
  if (p->title) {
    char *cp;
    count += fprintf(fp, "title \"");
    for (cp = p->title; *cp; cp++)
      if (*cp == '\"')
	count += fprintf(fp, "\\\"");
      else if (*cp == '\\')
	count += fprintf(fp, "\\\\");
      else if (*cp < 32 || *cp > 126)
	count += fprintf(fp, "?");
      else
	count++, fputc(*cp, fp);
    count += fprintf(fp, "\"\n");
  }
#endif

  count += fprintf(fp, "\nrows\n");
  for (n = 0; n < p->height; n++) {
    count += nonogram_printrule(&p->row[n], fp);
    count += fprintf(fp, "\n");
  }

  count += fprintf(fp, "\ncolumns\n");
  for (n = 0; n < p->width; n++) {
    count += nonogram_printrule(&p->col[n], fp);
    count += fprintf(fp, "\n");
  }
  return count;
}

void nonogram_judgeline(size_t linelen, const nonogram_sizetype *rule,
			size_t rulelen, ptrdiff_t rulestep,
			int *needed, int *known)
{
  size_t ruleno;
  int score;
  int solids = 0;

  for (ruleno = 0; ruleno < rulelen; ruleno++)
    solids += rule[ruleno * rulestep];

  score = solids * (rulelen + 1);
  score += rulelen * (rulelen - linelen - 1);
  if (needed)
    *needed += solids;
  if (known)
    *known += score;
}

float nonogram_judgepuzzle(const nonogram_puzzle *p)
{
  struct {
    int needed, known;
  } col = { 0, 0 }, row = { 0, 0 };

  float scale;
  size_t lineno;

  for (lineno = 0; lineno < p->width; lineno++)
    nonogram_judgeline(p->height, p->col[lineno].val, p->col[lineno].len,
		       1, &col.needed, &col.known);

  for (lineno = 0; lineno < p->height; lineno++)
    nonogram_judgeline(p->width, p->row[lineno].val, p->row[lineno].len,
		       1, &row.needed, &row.known);

  if (row.needed != col.needed)
    scale = FLT_MAX;
  else if (row.needed == 0)
    scale = 0.0;
  else
    scale = 1.0 - (float) (col.known + row.known) / 2.0 / row.needed;

  return scale * p->width * p->height;
}

unsigned long const nonogram_maxrule = nonogram_MAXRULES;


static int getword(const char *s, const char *e,
		   const char **sp, const char **ep)
{
  while (s < e && *s && isspace((int) *s))
    s++;
  *sp = s;

  while (s < e && *s && !isspace((int) *s))
    s++;
  *ep = s;
  return *sp < *ep && **sp;
}

static char *gettitle(const char *s, const char *e)
{
  char *res, *rp;
  const char *st = s, *en;
  size_t len = 0;
  int quotes = 0, spacelast = 0, escape = 0;

  while (st < e && *st && isspace((int) *st))
    st++;

  en = st;
  while (en < e && *en) {
    if (*en == '\"') {
      if (escape)
	len += 1 + spacelast, spacelast = 0, escape = 0;
      else
	quotes = !quotes;
    } else if (*en == '\\') {
      escape = !escape;
    } else if (isspace((int) *en)) {
      if (!escape && !quotes)
	spacelast = 1;
      else
	len += 1 + spacelast, spacelast = 0;
      escape = 0;
    } else {
      len += 1 + spacelast, spacelast = escape = 0;
    }
    en++;
  }

  res = malloc(len + 1);
  if (!res) return NULL;

  rp = res;
  quotes = spacelast = escape = 0;
  while (st < e && *st) {
    if (*st == '\"') {
      if (escape) {
	if (spacelast)
	  *(rp++) = ' ', spacelast = escape = 0;
	*(rp++) = *(st++);
      } else
	quotes = !quotes, st++;
    } else if (*st == '\\') {
      st++;
      escape = !escape;
    } else if (isspace((int) *st)) {
      if (!escape && !quotes)
	spacelast = 1, st++;
      else {
	if (spacelast)
	  *(rp++) = ' ', spacelast = 0;
	*(rp++) = *(st++);
      }
      escape = 0;
    } else {
      if (spacelast)
	*(rp++) = ' ', spacelast = 0;
      *(rp++) = *(st++);
      escape = 0;
    }
  }
  *rp = '\0';
  return res;
}

static int skipndigit(const char **pp, const char *e)
{
  while (*pp < e && **pp && !isdigit((int) **pp))
    (*pp)++;
  return **pp;
}

static int matchword(const char *s, const char *e, const char *t)
{
  while (s < e && *s == *t)
    s++, t++;

  return !*t;
}

static int getint(const char *s, const char **rem, const char *e, size_t *res)
{
  const char *sp = s;

  while (sp < e && *sp && isspace((int) *sp))
    sp++;

  if (!*sp || !isdigit((int) *sp))
    return 0;

  *res = 0;
  while (sp < e && *sp && isdigit((int) *sp)) {
    *res = *res * 10 + *sp - '0';
    sp++;
  }

  if (rem)
    *rem = sp;
  return 1;
}

static int loadrule(const char *line, const char *e,
		    struct nonogram_rule *rule)
{
  const char *p = line;
  size_t i = 0;
  size_t tmp;

  rule->len = 0;

  while (skipndigit(&p, e)) {
    const char *q;
    if (!getint(p, &q, e, &tmp) || tmp == 0)
      break;
    p = q;
    rule->len++;
  }
  if (rule->len == 0 && !isdigit((int) *p))
    return 0;

  rule->cap = rule->len;
  rule->val = malloc(rule->cap * sizeof *rule->val);
  p = line;
  while (i < rule->len && skipndigit(&p, e)) {
    getint(p, &p, e, &tmp);
    rule->val[i++] = tmp;
  }

  return 1;
}

static int scanline(nonogram_puzzle *p, struct linectxt *ctxt,
		    const char *line, const char *end,
		    nonogram_errorproc *ef, void *eh)
{
  const char *cmd;
  const char *cmdend;

  /* ignore blank lines */
  if (!getword(line, end, &cmd, &cmdend))
    return 1;

  /* ignore comments */
  if (*cmd == '#')
    return 1;

  if (matchword(cmd, cmdend, "width")) {
  reading_width:
    if (!ctxt->nowidth) {
      if (ef) (*ef)(eh, "%3d: width already specified\n", (int) ctxt->lineno);
      return 0;
    }
    ctxt->nowidth = false;
    if (!getint(cmdend, NULL, end, &p->width) || p->width  < 1) {
      if (ef) (*ef)(eh, "%3d: %-.*s needs positive integer\n",
		    ctxt->lineno, cmdend - cmd, cmd);
      return 0;
    }
    p->width_cap = p->width;
    p->col = malloc(p->width * sizeof *p->col);
    for (ctxt->colno = 0; ctxt->colno < p->width; ctxt->colno++) {
      p->col[ctxt->colno].len = 0;
      p->col[ctxt->colno].val = NULL;
    }
    ctxt->colno = 0;
  } else if (matchword(cmd, cmdend, "height")) {
  reading_height:
    if (!ctxt->noheight) {
      if (ef) (*ef)(eh, "%3d: height already specified\n", (int) ctxt->lineno);
      return 0;
    }
    ctxt->noheight = false;
    if (!getint(cmdend, NULL, end, &p->height) || p->height < 1) {
      if (ef) (*ef)(eh, "%3d: %-.*s needs non-negative integer\n",
		    ctxt->lineno, cmdend - cmd, cmd);
      return 0;
    }
    p->height_cap = p->height;
    p->row = malloc(p->height * sizeof *p->row);
    for (ctxt->rowno = 0; ctxt->rowno < p->height; ctxt->rowno++) {
      p->row[ctxt->rowno].len = 0;
      p->row[ctxt->rowno].val = NULL;
    }
    ctxt->rowno = 0;
  } else if (matchword(cmd, cmdend, "rows")) {
    size_t tmp;

    if (getint(cmdend, NULL, end, &tmp) && tmp > 0)
      goto reading_height;
    if (ctxt->noheight) {
      if (ef) (*ef)(eh, "%3d: specify height before rows\n",
		    (int) ctxt->lineno);
      return 0;
    }
    ctxt->onrows = true;
    ctxt->oncolumns = false;
  } else if (matchword(cmd, cmdend, "columns")) {
    size_t tmp;

    if (getint(cmdend, NULL, end, &tmp) && tmp > 0)
      goto reading_width;
    if (ctxt->nowidth) {
      if (ef) (*ef)(eh, "%3d: specify width before columns\n",
		    (int) ctxt->lineno);
      return 0;
    }
    ctxt->oncolumns = true;
    ctxt->onrows = false;
  } else if (matchword(cmd, cmdend, "maxrule")) {
    if (!ctxt->nomaxrule) {
      if (ef) (*ef)(eh, "%3d: maxrule already specified\n",
		    (int) ctxt->lineno);
      return 0;
    }
    ctxt->nomaxrule = false;
    if (getint(cmdend, NULL, end, &ctxt->maxrule) && ctxt->maxrule < 1) {
      if (ef)
	(*ef)(eh, "%3d: maxrule needs non-negative integer\n",
	      (int) ctxt->lineno);
      return 0;
    }
  } else if (isalpha((int) *cmd)) {
    char *value = gettitle(cmdend, end);
    int len = cmdend - cmd;
    char *name = malloc(len + 1);
    memcpy(name, cmd, len);
    name[len] = '\0';
    nonogram_setnote(p, name, value);
    free(value);
    free(name);
  } else if (ctxt->onrows) {
    if (ctxt->rowno >= p->height) {
      if (ef) (*ef)(eh, "%3d: too many rows\n", (int) ctxt->lineno);
      return 0;
    }
    if (loadrule(line, end, &p->row[ctxt->rowno]))
      ctxt->rowno++;
  } else if (ctxt->oncolumns) {
    if (ctxt->colno >= p->width) {
      if (ef) (*ef)(eh, "%3d: too many columns\n", (int) ctxt->lineno);
      return 0;
    }
    if (loadrule(line, end, &p->col[ctxt->colno]))
      ctxt->colno++;
  }
  return 1;
}

int nonogram_comparepuzzles(const nonogram_puzzle *p1,
			    const nonogram_puzzle *p2)
{
  size_t i, j;

  if (!p1)
    return p2 ? 1 : 0;
  if (!p2)
    return -1;

  if (p1->width != p2->width) return p2->width - p1->width;
  if (p1->height != p2->height) return p2->height - p1->height;

  for (i = 0; i < p1->width; i++)
    if (p1->col[i].len != p2->col[i].len)
      return p2->col[i].len - p1->col[i].len;
    else
      for (j = 0; j < p1->col[i].len; j++)
	if (p1->col[i].val[j] != p2->col[i].val[j])
	  return p2->col[i].val[j] - p1->col[i].val[j];

  for (i = 0; i < p1->height; i++)
    if (p1->row[i].len != p2->row[i].len)
      return p2->row[i].len - p1->row[i].len;
    else
      for (j = 0; j < p1->row[i].len; j++)
	if (p1->row[i].val[j] != p2->row[i].val[j])
	  return p2->row[i].val[j] - p1->row[i].val[j];

  return 0;
}

static int changedim(size_t *lenp, size_t *capp,
		     struct nonogram_rule **linesp,
		     size_t newlen)
{
  // If the length is already set to this value, do nothing and report
  // no error.
  if ((*lenp) == newlen)
    return 0;

  // Excess lines need to be released.
  for (size_t n = newlen; n < (*lenp); n++)
    free((*linesp)[n].val);

  // We must reallocate if increasing the number of lines, and we
  // should reallocate if reducing significantly.
  int change = 0;
  size_t nc = 0;
  if (newlen > (*capp))
    nc = newlen, change = 1;
  else if (newlen * 5 < (*capp))
    nc = (*capp) / 2, change = 1;

  // Do the reallocation if necessary.
  if (change) {
    void *np = realloc((*linesp), nc * sizeof *(*linesp));
    if (!np)
      return -1;
    (*linesp) = np;
    (*capp) = nc;
  }

  // Additional lines need to be nulled.
  for (size_t n = (*lenp); n < newlen; n++) {
    (*linesp)[n].len = (*linesp)[n].cap = 0;
    (*linesp)[n].val = NULL;
  }

  // Now the length can be recorded correctly.
  (*lenp) = newlen;
  return 0;
}

int nonogram_setpuzzleheight(nonogram_puzzle *p, size_t len)
{
  return changedim(&p->height, &p->height_cap, &p->row, len);
}

int nonogram_setpuzzlewidth(nonogram_puzzle *p, size_t len)
{
  return changedim(&p->width, &p->width_cap, &p->col, len);
}

static int changelinelen(struct nonogram_rule *rule, size_t newlen)
{
  // Do nothing if the line already has the right length.
  if (rule->len == newlen) return 0;

  int change = 0;
  size_t nc = 0;
  if (newlen > rule->cap)
    nc = newlen, change = 1;
  else if (newlen * 5 < rule->cap)
    nc = rule->cap / 2, change = 1;

  if (change) {
    void *np = realloc(rule->val, nc * sizeof *rule->val);
    if (!np) return -1;
    rule->val = np;
    rule->cap = nc;
  }

  for (size_t n = rule->len; n < newlen; n++)
    rule->val[n] = 0;

  rule->len = newlen;
  return 0;
}

int nonogram_setrowlen(nonogram_puzzle *p, size_t line, size_t len)
{
  // (Almost) silently ignore rows that exceed puzzle height.
  if (line >= p->height) return -2;

  return changelinelen(&p->row[line], len);
}

int nonogram_setcollen(nonogram_puzzle *p, size_t line, size_t len)
{
  // (Almost) silently ignore rows that exceed puzzle width.
  if (line >= p->width) return -2;

  return changelinelen(&p->col[line], len);
}

int nonogram_setrowblock(nonogram_puzzle *p, size_t line,
			 size_t pos, nonogram_sizetype val)
{
  if (line >= p->height) return -2;
  if (pos >= p->row[line].len) return -3;

  p->row[line].val[pos] = val;
  return 0;
}

int nonogram_setcolblock(nonogram_puzzle *p, size_t line, size_t pos,
			 nonogram_sizetype val)
{
  if (line >= p->width) return -2;
  if (pos >= p->col[line].len) return -3;

  p->col[line].val[pos] = val;
  return 0;
}

int nonogram_appendrowblock(nonogram_puzzle *p,
			    size_t line, nonogram_sizetype val)
{
  int rc;
  size_t pos = p->row[line].len;
  if ((rc = nonogram_setrowlen(p, line, pos + 1)) < 0)
    return rc;
  p->row[line].val[pos] = val;
  return 0;
}

int nonogram_appendcolblock(nonogram_puzzle *p,
			    size_t line, nonogram_sizetype val)
{
  int rc;
  size_t pos = p->col[line].len;
  if ((rc = nonogram_setcollen(p, line, pos + 1)) < 0)
    return rc;
  p->col[line].val[pos] = val;
  return 0;
}

const nonogram_puzzle nonogram_nullpuzzle = nonogram_NULLPUZZLE;
