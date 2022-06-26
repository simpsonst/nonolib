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

#ifndef nonogram_HEADER
#define nonogram_HEADER

#if __STDC__ != 1
#error "You need an ISO C compiler."
#endif

#ifdef __GNUC__
#define nonogram_deprecated(D) __attribute__ ((deprecated)) D
#else
#define nonogram_deprecated(D) D
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <string.h>

#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif


  /******* cell representation *******/

  typedef unsigned char nonogram_cell;
  nonogram_deprecated(typedef nonogram_cell nonogram_cell_t);

#undef nonogram_BLANK
#undef nonogram_DOT
#undef nonogram_SOLID
#undef nonogram_BOTH
#define nonogram_BLANK  '\00'
#define nonogram_DOT    '\01'
#define nonogram_SOLID  '\02'
#define nonogram_BOTH   '\03'

#define nonogram_makegrid(w,h) \
  ((nonogram_cell *) malloc((w)*(h)*sizeof(nonogram_cell)))
#define nonogram_freegrid(g) (free(g))
#define nonogram_setgrid(G,W,H,V) memset((G),(V),(W)*(H))
#define nonogram_cleargrid(G,W,H) nonogram_setgrid((G),(W),(H),nonogram_BLANK)
#define nonogram_xfergrid(T,F,W,H) memcpy(T,F,(W)*(H)*sizeof(nonogram_cell))


  /******* puzzle representation *******/

  typedef unsigned long nonogram_sizetype;
  nonogram_deprecated(typedef nonogram_sizetype nonogram_size_t);
#define nonogram_PRIuSIZE "lu"
#define nonogram_PRIoSIZE "lo"
#define nonogram_PRIxSIZE "lx"
#define nonogram_PRIXSIZE "lX"

  typedef struct nonogram_puzzle nonogram_puzzle;
  nonogram_deprecated(typedef nonogram_puzzle nonogram_puzzle_t);
  typedef int nonogram_errorproc(void *, const char *, ...);
  nonogram_deprecated(typedef nonogram_errorproc nonogram_error_f);

  int nonogram_spscanpuzzle_ef(nonogram_puzzle *p,
			       const char **s, const char *e,
			       nonogram_errorproc *, void *);
  int nonogram_spscanpuzzle(nonogram_puzzle *p,
			    const char **s, const char *e);

  int nonogram_fscanpuzzle_ef(nonogram_puzzle *p, FILE *fp,
			      nonogram_errorproc *, void *);
  int nonogram_fscanpuzzle(nonogram_puzzle *p, FILE *fp);

  int nonogram_makepuzzle(nonogram_puzzle *p, const nonogram_cell *g,
			  size_t w, size_t h);
  void nonogram_freepuzzle(nonogram_puzzle *p);
  int nonogram_copypuzzle(nonogram_puzzle *to,
			  const nonogram_puzzle *from);

  int nonogram_verifypuzzle(const nonogram_puzzle *p);
  float nonogram_judgepuzzle(const nonogram_puzzle *p);
  int nonogram_comparepuzzles(const nonogram_puzzle *p1,
			      const nonogram_puzzle *p2);

  int nonogram_fprintpuzzle(const nonogram_puzzle *p, FILE *fp);
#define nonogram_puzzlewidth(P) ((size_t) (P)->width)
#define nonogram_puzzleheight(P) ((size_t) (P)->height)

  int nonogram_setpuzzleheight(nonogram_puzzle *p, size_t);
  int nonogram_setpuzzlewidth(nonogram_puzzle *p, size_t);
  int nonogram_setrowlen(nonogram_puzzle *p, size_t line, size_t len);
  int nonogram_setcollen(nonogram_puzzle *p, size_t line, size_t len);
  int nonogram_setrowblock(nonogram_puzzle *p, size_t line,
			   size_t pos, nonogram_sizetype val);
  int nonogram_setcolblock(nonogram_puzzle *p, size_t line, size_t pos,
			   nonogram_sizetype val);
  int nonogram_appendrowblock(nonogram_puzzle *p,
			      size_t line, nonogram_sizetype val);
  int nonogram_appendcolblock(nonogram_puzzle *p,
			      size_t line, nonogram_sizetype val);

  int nonogram_unsetnote(nonogram_puzzle *p, const char *n);
  int nonogram_setnote(nonogram_puzzle *p, const char *n, const char *v);
  const char *nonogram_getnote(nonogram_puzzle *p, const char *n);
#define nonogram_settitle(P,S) nonogram_setnote((P),"title",(S))
#define nonogram_unsettitle(P) nonogram_unsetnote((P),"title")
#define nonogram_puzzletitle(P) nonogram_getnote((P),"title")

#define nonogram_getrowdata(P,N) ((const nonogram_sizetype *) (P)->row[N].val)
#define nonogram_getrowlen(P,N) ((const size_t) (P)->row[N].len)

#define nonogram_getcoldata(P,N) ((const nonogram_sizetype *) (P)->col[N].val)
#define nonogram_getcollen(P,N) ((const size_t) (P)->col[N].len)

  /******* puzzle/grid comparison *******/

  int nonogram_checkgrid(const nonogram_puzzle *p, const nonogram_cell *g);
  int nonogram_checkline(const nonogram_sizetype *r, size_t, ptrdiff_t rstep,
			 const nonogram_cell *st, size_t, ptrdiff_t step);


  /******* solver state ******/

  typedef struct nonogram_solver nonogram_solver;
  nonogram_deprecated(typedef nonogram_solver nonogram_solver_t);

  int nonogram_initsolver(nonogram_solver *);
  int nonogram_termsolver(nonogram_solver *);

  int nonogram_load(nonogram_solver *c,
		    const nonogram_puzzle *puzzle,
		    nonogram_cell *grid, int remcells);
  int nonogram_unload(nonogram_solver *c);

  int nonogram_setlog(nonogram_solver *c,
		      FILE *logfile, int indent, int level);


  /******* solver activity *******/

#define nonogram_setlinelim(C,N) ((C)->cycles = (N))
  int nonogram_runsolver_n(nonogram_solver *c, int *tries);
  int nonogram_runlines_tries(nonogram_solver *c, int *lines, int *cycles);
  int nonogram_runlines_until(nonogram_solver *c, int *lines, clock_t lim);
  int nonogram_runcycles_tries(nonogram_solver *c, int *cycles);
  int nonogram_runcycles_until(nonogram_solver *c, clock_t lim);
  enum { /* return codes for above calls */
    nonogram_UNLOADED = 0,
    nonogram_FINISHED = 1,
    nonogram_UNFINISHED = 2,
    nonogram_FOUND = 4,
    nonogram_LINE = 8,
    nonogram_ERROR = 16
  };


  /******* user of complete solutions *******/

  typedef void nonogram_presentproc(void *ctxt);
  nonogram_deprecated(typedef nonogram_presentproc nonogram_present_f);
  struct nonogram_client {
    nonogram_presentproc *present;
  };

  int nonogram_setclient(nonogram_solver *c,
			 const struct nonogram_client *client,
			 void *client_data);


  /******* verbose solution *******/

  struct nonogram_point { size_t x, y; };
  struct nonogram_rect { struct nonogram_point min, max; };

  typedef void nonogram_redrawareaproc(void *ctxt,
				       const struct nonogram_rect *area);
  nonogram_deprecated(typedef nonogram_redrawareaproc nonogram_redrawarea_f);
  typedef void nonogram_focusproc(void *ctxt, size_t, int);
  nonogram_deprecated(typedef nonogram_focusproc nonogram_focus_f);
  typedef void nonogram_markproc(void *ctxt, size_t from, size_t to);
  nonogram_deprecated(typedef nonogram_markproc nonogram_mark_f);

  struct nonogram_display {
    nonogram_redrawareaproc *redrawarea;
    nonogram_focusproc *rowfocus, *colfocus;
    nonogram_markproc *rowmark, *colmark;
  };

  int nonogram_setdisplay(nonogram_solver *c,
			  const struct nonogram_display *display,
			  void *display_data);

#define nonogram_limitrow(S,R,X,F) \
  ((R) < (S)->puzzle->height ? (X) : (F))
#define nonogram_limitcol(S,C,X,F) \
  ((C) < (S)->puzzle->width ? (X) : (F))

  /* check what work needs to be done for a particular line */
#define nonogram_getrowmark(S,R) \
  nonogram_limitrow((S),(R),(S)->rowflag?(S)->rowflag[R]:0,0)
#define nonogram_getcolmark(S,C) \
  nonogram_limitcol((S),(C),(S)->colflag?(S)->colflag[C]:0,0)

  /* check if a line is being worked on */
#define nonogram_getrowfocus(S,R) \
  nonogram_limitrow((S),(R),(S)->on_row &&				\
		    (S)->focus && (unsigned) (S)->lineno == (R),false)
#define nonogram_getcolfocus(S,C) \
  nonogram_limitcol((S),(C),!(S)->on_row &&			\
		    (S)->focus && (unsigned) (S)->lineno == (C),false)


  /******* line-solver characteristics *******/

  typedef unsigned int nonogram_level;
  nonogram_deprecated(typedef nonogram_level nonogram_level_t);

  struct nonogram_log {
    FILE *file;
    int indent, level;
  };

  /* greatest dimensions of the puzzle */
  struct nonogram_lim {
    size_t maxline, maxrule;
  };

  /* size of each allocated block of shared workspace */
  struct nonogram_req {
    size_t byte, ptrdiff, size, nonogram_size, cell;
  };

  /* pointers to each block of shared workspace */
  struct nonogram_ws {
    void *byte;
    ptrdiff_t *ptrdiff;
    size_t *size;
    nonogram_sizetype *nonogram_size;
    nonogram_cell *cell;
  };

  struct nonogram_initargs {
    int *fits;
    struct nonogram_log *log;
    const nonogram_sizetype *rule;
    const nonogram_cell *line;
    nonogram_cell *result;
    size_t linelen, rulelen;
    ptrdiff_t linestep, rulestep, resultstep;
  };

  typedef void nonogram_prepproc(void *, const struct nonogram_lim *,
				 struct nonogram_req *);
  nonogram_deprecated(typedef nonogram_prepproc nonogram_prep_f);
  typedef int nonogram_initproc(void *, struct nonogram_ws *ws,
				const struct nonogram_initargs *);
  nonogram_deprecated(typedef nonogram_initproc nonogram_init_f);
  typedef int nonogram_stepproc(void *, void *ws);
  nonogram_deprecated(typedef nonogram_stepproc nonogram_step_f);
  typedef void nonogram_termproc(void *);
  nonogram_deprecated(typedef nonogram_termproc nonogram_term_f);

  /* init and step return true if step should be called (again) */
  struct nonogram_linesuite {
    nonogram_prepproc *prep; /* indicate workspace requirements */
    nonogram_initproc *init; /* initialise for a particular line */
    nonogram_stepproc *step; /* perform a single step */
    nonogram_termproc *term; /* terminate line-processing */
  };

  int nonogram_setlinesolver(nonogram_solver *c, nonogram_level,
			     const char *n,
			     const struct nonogram_linesuite *, void *conf);
  int nonogram_setlinesolvers(nonogram_solver *c, nonogram_level levels);
  nonogram_level nonogram_getlinesolvers(nonogram_solver *c);

  enum {
    /* Compare pushed-left with pushed-right - partial solution. */
    nonogram_AFAST,

    /* Slowly exhaust all possibilities - complete solution. */
    nonogram_ACOMPLETE,

    /* FAST then COMPLETE if fast already tried. */
    nonogram_AHYBRID,

    /* Do nothing - rely on bifurcation. */
    nonogram_ANULL,

    /* The Olsaks' algorithm - I think. */
    nonogram_AOLSAK,

    /* FAST then OLSAK. */
    nonogram_AFASTOLSAK,

    /* FAST then OLSAK then COMPLETE. */
    nonogram_AFASTOLSAKCOMPLETE,

    /* FAST then ODDONES. */
    nonogram_AFASTODDONES,

    /* FAST then ODDONES then COMPLETE. */
    nonogram_AFASTODDONESCOMPLETE,

    /* Fast-complete */
    nonogram_AFCOMP,

    /* Fast, then fast-complete */
    nonogram_AFFCOMP
  };
  int nonogram_setalgo(nonogram_solver *, int);

  /* Push blocks as far towards the start of the array as they will
     go.  Return 0 if they won't go, or 1 if they will.

     line[0]..line[(linelen-1)*linestep] provides the current state of
     the line.

     rule[0]..rule[(rulelen-1)*rulestep] defines the rule for the
     line.

     The resultant positions will be written to
     pos[0]..pos[(rulelen-1)*posstep].

     solid[0]..solid[rulelen-1] are used as workspace.  Each contains
     the index of the left-most solid covered by the corresponding
     block, or -1 if no solid is covered.

     Internal workings will be written to log (if not NULL), and level
     is sufficiently high, indented by the specified amount.  */
  int nonogram_push(const nonogram_cell *line,
		    size_t linelen, ptrdiff_t linestep,
		    const nonogram_sizetype *rule,
		    size_t rulelen, ptrdiff_t rulestep,
		    nonogram_sizetype *pos, ptrdiff_t posstep,
		    ptrdiff_t *solid, FILE *log, int level, int indent);



  /******* 'complete' line solver *******/

  extern const struct nonogram_linesuite nonogram_completesuite;
  typedef struct nonogram_completework nonogram_completework;
  typedef struct nonogram_completeconf nonogram_completeconf;


  /******* 'fast' line solver *******/

  extern const struct nonogram_linesuite nonogram_fastsuite;
  typedef struct nonogram_fastwork nonogram_fastwork;
  typedef struct nonogram_fastconf nonogram_fastconf;


  /******* 'odd-ones' line solver *******/

  extern const struct nonogram_linesuite nonogram_oddonessuite;
  typedef struct nonogram_fastwork nonogram_oddoneswork;
  typedef struct nonogram_fastconf nonogram_oddonesconf;


  /******* 'olsak' line solver *******/

  extern const struct nonogram_linesuite nonogram_olsaksuite;
  typedef struct nonogram_fastwork nonogram_olsakwork;
  typedef struct nonogram_fastconf nonogram_olsakconf;


  /******* 'fcomp (fast-complete)' line solver *******/

  extern const struct nonogram_linesuite nonogram_fcompsuite;
  typedef struct nonogram_fastwork nonogram_fcompwork;
  typedef struct nonogram_fastconf nonogram_fcompconf;


  /******* 'null' line solver *******/

  extern const struct nonogram_linesuite nonogram_nullsuite;
  typedef struct nonogram_fastwork nonogram_nullwork;
  typedef struct nonogram_fastconf nonogram_nullconf;


  /******* Miscellaneous *******/

  extern const char *const nonogram_date;
  extern unsigned long const nonogram_loglevel;
  extern unsigned long const nonogram_maxrule;


  /******* private types and functions *******/

  typedef unsigned char nonogram_bool;

  typedef void nonogram_redrawrangeproc(void *ctxt, int from, int to);
  nonogram_deprecated(typedef nonogram_redrawrangeproc nonogram_redrawrange_f);

  struct nonogram_completework {
    struct nonogram_log *log;
    int *fits;
    const nonogram_cell *ln;
    const nonogram_sizetype *ru;
    nonogram_sizetype *limit, *pos;
    size_t lnlen, rulen;
    ptrdiff_t lni, rui, rei;
    nonogram_cell *re;
    size_t blockno;
    int remunk;
    unsigned move_back : 1;
  };

  struct nonogram_completeconf {
    /* no configuration for complete algorithm as yet */
    int x;
  };

  struct nonogram_fastwork {
    /* fast algorithm needs no workspace between calls to step ('cos
       it doesn't have a step) */
    int x;
  };

  struct nonogram_fastconf {
    /* no configuration for fast algorithm as yet */
    int x;
  };

  enum {
    nonogram_EMPTY,   /* processing, but not on line */
    nonogram_WORKING, /* currently on a line */
    nonogram_DONE     /* line processing completed */
  };

  typedef struct {
    int score, dot, solid;
  } nonogram_lineattr;

  typedef struct nonogram_stack {
    struct nonogram_stack *next;
    nonogram_cell *grid;
    struct nonogram_rect unkarea;
    struct nonogram_point guesspos;
    nonogram_lineattr *rowattr, *colattr;
    int remcells;
  } nonogram_stack;

  struct nonogram_lsnt {
    void *context;
    const char *name;
    const struct nonogram_linesuite *suite;
  };

  struct nonogram_solver {
    void *client_data;
    const struct nonogram_client *client;

    void *display_data;
    const struct nonogram_display *display;
    struct nonogram_rect editarea; /* temporary workspace */

    struct nonogram_ws workspace;
    struct nonogram_lsnt *linesolver; /* an array of length 'levels' */
    nonogram_level levels;

    nonogram_cell first; /* configured first guess; should be
                              replaced with choice based on remaining
                              unaccounted cells */
    int cycles; /* could be part of complete context */

    const nonogram_puzzle *puzzle;
    struct nonogram_lim lim;
    nonogram_cell *work;
    nonogram_lineattr *rowattr, *colattr;
    nonogram_level *rowflag, *colflag;

    nonogram_bool *rowdir, *coldir; /* to be removed */

    nonogram_stack *stack; /* pushed guesses */
    nonogram_cell *grid;
    int remcells, reminfo;
    struct nonogram_rect unkarea;

    /* (on_row,lineno) == line being solved */
    /* status == EMPTY => no line */
    /* status == WORKING => line being solved */
    /* status == DONE => line solved */
    /* focus => used by display */
    int fits, lineno;
    nonogram_level level;
    unsigned on_row : 1, focus : 1, status : 2, reversed : 1, alloc : 1;

    /* logfile */
    struct nonogram_log log, tmplog;
  };

  struct nonogram_rule {
    size_t len, cap;
    nonogram_sizetype *val;
  };

  struct nonogram_tn {
    struct nonogram_tn *c[2];
    char *n, *v;
  };

  struct nonogram_puzzle {
    struct nonogram_rule *row, *col;
    size_t width, height;
    size_t width_cap, height_cap;
    struct nonogram_tn *notes;
  };

#define nonogram_NULLPUZZLE { 0, 0, 0, 0, 0, 0, 0 }
  extern const nonogram_puzzle nonogram_nullpuzzle;

  int nonogram_printgrid(const nonogram_cell *grid,
			 size_t width, size_t height,
			 FILE *fp, const char *solid, const char *dot,
			 const char *blank);
  int nonogram_printhtmlgrid(const nonogram_cell *grid,
			     size_t width, size_t height,
			     FILE *fp, const char *solid, const char *dot,
			     const char *blank, int table);
  int nonogram_scangrid(nonogram_cell *grid,
			size_t width, size_t height, FILE *fp,
			char solid, char dot);

#ifdef __cplusplus
}
#endif

#endif
