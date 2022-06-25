# Purpose

This is a library for solving rectangular monochrome Nonograms.
A Nonogram is a logic puzzle which reveals an image when solved.
You start with a grid of empty cells, and clues for each line.
Each clue lists the sizes of each block of adjacent cells of the same non-background colour, and you deduce the cells' colours from these.
The clue for a single line will probably not tell you the colours of all cells in the line, but what it does tell you intersects with perpendicular lines, and in combination with their clues leads to more deductions, and eventually a complete solution.

# Installation

Installation depends on [Binodeps](https://github.com/simpsonst/binodeps).
To install in `/usr/local`:

```
make
sudo make install
```

Override the default location by setting `PREFIX`.
This can be set on the command line:

```
make PREFIX=$HOME/.local install
```

&hellip;or in a local file `config.mk`:

```
PREFIX=$(HOME)/.local
```

&hellip;or in `nonolib-env.mk`, which is sought on GNU Make's search path, set by `-I`:

```
make -I$HOME/.config/local install
```

You probably want to set some more optimal compilation options, e.g., by placing them in `config.mk`:

```
CFLAGS += -O2 -g
PREFIX=$(HOME)/.local
```

Installation places installs the following files under `$(PREFIX)`:

```
include/nonogram.h
include/nonogram_version.h
include/nonocache.h
lib/libnonogram.a
```

# Puzzle format

Here's an image from which a Nonogram puzzle can be derived:

```
####
#--#
##--
--#-
```

The image is the solution to the puzzle.
`#` is the sole foreground colour; `-` is the background colour.
(The library only handles puzzles with two colours, including the background.)

The puzzle can be represented by the following:

```
width 4
height 4
title Nonogram Solver Logo
by "Steven Simpson"

rows
4
1,1
2
1

columns
3
1,1
1,1
2
```

`width` and `height` are required.
`title` and `by` are arbitrary meta-data.
Each clue appears on its own line, and gives the length of each block of `#` cells.
Blank lines are ignored, so `0` is used to signal a line with no `#`.

# Library

Most functionality is in a single header:

```
#include <nonogram.h>
```

## Puzzles

A puzzle is held in a `nonogram_puzzle` object.
To load one in from a stream:

```
nonogram_puzzle puz;
int rc = nonogram_fscanpuzzle(&puz, stdin);
```

`rc` is negative on error, and `0` on success.

The sums of the numbers in row clues and column clues must match, or there is an error in transcribing the puzzle.
Check that they match with:

```
int diff = nonogram_verifypuzzle(&puz);
```

The result is the sum of row clues minus the sum of column clues, so an unbalanced puzzle will give `0`, and cannot be solved.
(Some balanced puzzles also have no solution, so `0` is not a guarantee of solvability, but this usually picks up a lot of common mistakes.)

`nonogram_puzzlewidth(&puz)` will yield the number of columns in the puzzle, and `nonogram_puzzleheight(&puz)` will yield the number of rows.

Dynamic allocations are made within the object, which you should release with:

```
nonogram_freepuzzle(&puz);
```


## Cells and grids

A solver requires a grid to work on as state.

The type `nonogram_cell` represents the state of a cell in a grid, which itself is represented as an array of cells arranged row-by-row.
This declares a grid of 10 columns and 15 rows:

```
const int wid = 10;
const int hei = 15;
nonogram_cell grid[wid * hei];
```

Given `x` and `y` as the column and row numbers (starting from 0), the corresponding cell is accessed through `grid[x + y * wid]`.

A grid should normally be initialized to `nonogram_BLANK` in each cell:

```
nonogram_cleargrid(grid, wid, hei);
```

You can print a grid with this:

```
nonogram_printgrid(grid, wid, hei, stdout, "#", "-", " ");
```

You can load one in with this:

```
nonogram_scangrid(grid, wid, hei, stdin, '#', '-');
```

`#` is translated to `nonogram_SOLID` (the foreground colour).
`-` is translated to `nonogram_DOT` (the background colour).
Anything else is translated to `nonogram_BLANK`.

## Solver

A solver is declared as follows:

```
nonogram_solver solv;
nonogram_initsolver(&solv);
```

`nonogram_initsolver` returns zero on success.


### Setting the puzzle

Provide the solver with the puzzle and the grid to work on:

```
nonogram_load(&solv, &puz, grid, wid, hei);
```

### Handling solutions

```
static void my_present(void *ctxtp);
struct nonogram_client my_client = {
  .present = &my_present,
};
struct my_ctxt { ... };
nonogram_setclient(&solv, &my_client, &my_ctxt);
```

This specifies that `(*my_client.present)(&my_ctxt)` will be invoked each time a solution is found.
It returns zero on success, or negative on error (e.g., if the puzzle has not yet been loaded into the solver).



### Setting the algorithm

The solver's stategy is to select a line, and apply a 'line solver' to it, to see what can be deduced.
It then selects another line, and applies a line solver to it, and so on.
It will re-apply a line solver to a line it has already processed _only_ if something is deduced on the line since the line solver was last applied.
The puzzle solver is configured with a stack of line solvers (numbered 1, 2, 3, &hellip;), and it tries number 1 first, and only tries 2 if 1 fails to deduce anything.
Finally, if the solver has exhausted application of all line solvers to all lines, and still the puzzle is incomplete, it chooses an arbirary cell of unknown colour, and bifurcates.
In one branch, the cell is set to `nonogram_DOT`, but set to `nonogram_SOLID` in the other branch.
In each branch, this makes line solving on the intersecting row and column applicable again.

You can set the line-solving stack to one of several built-in configurations like this:

```
nonogram_setalgo(&solv, nonogram_AFCOMP);
```

This specifies the comprehensive (or fast-complete) algorithm, which is the default.
The full list of built-in line algorithms:

- `nonogram_AFAST` &ndash; the 'fast' algorithm, which is very fast, but doesn't always squeeze all information out of a line

- `nonogram_ACOMPLETE` &ndash; the 'complete' algorithm, which does squeeze all information out of a line, but not terribly efficiently

- `nonogram_AFCOMP` &ndash; the 'fast-complete' or 'comprehensive' algorithm, which fairly efficiently squeezes all information

- `nonogram_AOLSAK` &ndash; an attempt at replicating the [Olšáks' algorithm](http://www.olsak.net/grid.html), which (IIRC) is both efficient and almost complete

- `nonogram_AHYBRID` &ndash; use `AFAST` first, then `ACOMPLETE` if required

- `nonogram_AFASTOLSAK` &ndash; use `AFAST` first, then `AOLSAK` if required

- `nonogram_AFASTOLSAKCOMPLETE` &ndash; use `AFAST` first, then `AOLSAK`, then `ACOMPLETE`

- `nonogram_AFASTODDONES` &ndash; use `AFAST` first, then something else which I can't remember!

- `nonogram_AFASTODDONESCOMPLETE` &ndash; as `AFASTODDONES`, but with `ACOMPLETE` as the last option

- `nonogram_AFFCOMP` &ndash; `AFAST` then `AFCOMP`

- `nonogram_ANULL` (snigger) &ndash; No deductions are made, although inconsistent lines are detected.  This puts all the work on bifurcation.


### Setting the display

The user can be informed of changes to the internal state of the solver, primarily for display purposes:

```
static void my_redrawarea(void *ctxtp,
                          const struct nonogram_rect *area);
static void my_rowfocus(void *ctxtp, size_t lno, int st);
static void my_cowfocus(void *ctxtp, size_t lno, int st);
static void my_rowmark(void *ctxtp, size_t from, size_t to);
static void my_colmark(void *ctxtp, size_t from, size_t to);
struct nonogram_display my_display = {
  .redrawarea = &my_redrawarea,
  .rowfocus = &my_rowfocus,
  .colfocus = &my_colfocus,
  .rowmark = &my_rowmark,
  .colmark = &my_colmark,
};
nonogram_setdisplay(&solv, &my_display, &ctxt);
```

`(*my_display.redrawarea)(&ctxt, &area)` is invoked when a rectangular area of cells has changed.
The area is the intersection of columns `area.min.x` (inclusive) to `area.max.x` (exclusive) and rows `area.min.y` (inclusive) to `area.max.y` (exclusive).

`(*my_display.rowfocus)(&ctxt, lno, val)` or `(*my_display.colfocus)(&ctxt, lno, val)` is invoked when processing starts (`val` is non-zero) or ends (`val` is zero) on row or column `lno`, respectively.
`nonogram_getrowfocus(&solv, lno)` and `nonogram_getcolfocus(&solv, lno)` will also obtain the current `val` for a row or column `lno`, respectively.

`(*my_display.rowmark)(&ctxt, f, t)` or `(*my_display.colmark)(&ctxt, from, to)` indicates that the suitability for processing has changed for a range of rows or columns, respectively, from `f` (inclusive) to `t` (exclusive).
`nonogram_getrowmark(&solv, lno)` or `nonogram_getcolmark(&solv, lno)` obtain the current suitability for row or column `lno`, respectively.
A suitability of `0` means that the line is suitable for all line solving algorithms.
A higher value `val` indicates that algorithms at level `val` or lower in the stack have already been tried.
`nonogram_getlinesolvers(&solv)` yields the maximum value that `val` can take, indicating that all line-solving algorithms have been applied since the last determination of a cell.


### Logging

This sets logging to level 3, and written to `stderr`, while bifurcation indents the logging by two spaces:

```
nonogram_setlog(&solv, stderr, 2, 3);
```

This turns logging off:

```
nonogram_setlog(&solv, NULL, 0, 0);
```

### Processing

Keep solving until a test fails:

```
static int mytest(void *datap);

nonogram_runcyles(&solv, &mytest, &data);
```

Each time the solver has a moment (e.g., having exhausted a line, or when a line solver completes a step), `(*mytest)(&data)` is invoked, and processing continues if the result is non-zero, and there is still work to do.

`nonogram_runcyles` returns one of the following:

- `nonogram_UNLOADED` &ndash; No puzzle has been loaded into the solver.

- `nonogram_UNFINISHED` &ndash; There is more work to do on the puzzle.  The test returned zero.

- `nonogram_LINE` &ndash; There is more work to do on the puzzle, but the solver is not currently working on a specific line.  I think.

- `nonogram_FOUND` &ndash; The grid currently contains a solution.  One should nevertheless continue processing, as more solutions might be found.

- `nonogram_FINISHED` &ndash; There is no more work to do.  There are no more solutions to be found.  The grid might not contain a solution, which is the case if there are several solutions, no solutions, or bifurcation is required to find a single solution.

`nonogram_UNLOADED` should be considered a configuration error.
`nonogram_FINISHED` is an absolute terminating condition.
Other codes indicate further processing, so `nonogran_runcyles` should be called again.

Other functions wrap around `nonogram_runcyles`.
To terminate only after a time `when` is reached:

```
clock_t when;
nonogram_runcyles_until(&solv, when);
```

To terminate when a counter (decremented per cycle) reaches zero:

```
int cycles = 10;
nonogram_runcycles_tries(&solv, &cycles);
```

To run until a test fails, or a counter (decremented per processed line) reaches zero:

```
int lines = 10;
nonogram_runlines(&solv, &lines, &mytest, &data);
```

To run until either of two counters (`lines` decremented per processed line; `cycles` per cycle) reaches zero:

```
int lines = 10;
int cycles = 10;
nonogram_runlines_tries(&solv, &lines, &cycles);
```

To run until a number of lines are processed, or a time is reached:

```
int lines = 10;
clock_t when;
nonogram_runlines_until(&solv, &lines, when);
```

To run until a number of lines are processed, or after 50 cycles:

```
int lines = 10;
nonogram_runsolver_n(&solv, &lines);
```

### Deallocation

A solver's internal resources should be released after use:

```
nonogram_termsolver(&solv);
```

### Custom line solvers

If you want to use your own line algorithm, you need to define it with a `struct nonogram_linesuite`:

```
static void my_prepproc(void *ctxtp,
                        const struct nonogram_lim *,
						struct nonogram_req *);
static int my_initproc(void *ctxtp,
                       struct nonogram_ws *ws,
				       const struct nonogram_initargs *);
static int my_stepproc(void *ctxtp, void *ws);
static my_termproc(void *ctxtp);
struct nonogram_linesuite mysuite = {
  .prep = &my_prepproc,
  .init = &my_initproc,
  .step = &my_stepproc,
  .term = &my_termproc,
};
struct myctxt { ... } ctxt;

nonogram_setlinesolvers(&solv, 1);
nonogram_setlinesolver(&solv, 1, "mine", &mysuite, &ctxt);
```

The first call sets the stack size.
The second call sets the first and only algorithm to the one defined by the functions in `mysuite`.
Each function will be invoked with `ctxtp` set to `&ctxt`.

`(*mysuite.prep)(&ctxt, &lim, &req)` is invoked once before solving a puzzle, with `lim.maxline` giving the size of largest dimension of the puzzle, and with `lim.maxrule` giving the largest number of blocks in any single clue.
The function should set fields in `req` to indicate its minimum memory requirements to solve any line of such a puzzle:

```
struct nonogram_req {
  size_t byte, ptrdiff, size, nonogram_size, cell;
};
```

All fields are zero-initialized.
Each corresponds to a field in the following structure:

```
struct nonogram_ws {
  void *byte;
  ptrdiff_t *ptrdiff;
  size_t *size;
  nonogram_sizetype *nonogram_size;
  nonogram_cell *cell;
};
```

Except for `byte`, each will later be initialized as an array of the corresponding type, with at least as many elements as specified by the corresponding field in `req`.
`byte` only differs in that it will be an _untyped_ array of bytes.
If you have access to type alignment, you can probably get away with just using `req.byte` of a sufficient size for your workspace and all necessary alignment padding.

`(*mysuite.init)(&ctxt, &ws, &args)` is called when preparing to solve a line.
`ws` is of type `struct nonogram_ws`, with fields initialized to arrays at least as long as specified by the `mysuite.prep` call.
`args.line` points to the first cell in the line to be solved.
`args.line[args.linestep * n]` accesses the `n`th cell (starting from `0`), and there are `args.lineline` cells.
The clue consists of `args.rulelen` blocks, with `args.rule[args.rulestep * n]` giving the length of the `n`th block (starting from `0`).
`args.log.file` is a `FILE *` for logging.
`args.log.indent` indicates how many spaces log lines should be indented by.
`args.log.level` indicates the level of detail expected.

The algorithm should make itself ready to store new information in `args.result[args.resultstep * n]` for each of the `n` cells.
By the end of processing of this line, each cell should be `nonogram_DOT` if it is determined to be of the background colour, `nonogram_SOLID` if foreground, or `nonogram_BOTH` if unknown.

`(*mysuite.init)(&ctxt, &ws, &args)` must return non-zero if further processing is required with `mysuite.step`.
By the end of processing, `*args.fits` must be `0` if the current state of the cells is incompatible with the clues, or positive if at least one matching solution was found (even if it is not yet certain that it is the solution).

If `mysuite.init` returns non-zero, `(*mysuite.step)(&ctxt, ws.byte)` will be called repeatedly until it returns zero.
(Yes, that's a naff way to pass `ws.byte`; may as well have provided `&ws`.
Oh, well.)
