# MindSweeper

Welcome to the home page for MindSweeper, yet another incarnation of
minesweeper --- everybody's favourite waste of CPU cycles!  My
implementation's claim to fame is that it includes a game solver so that
at each move it can tell you whether or not you need to guess and/or
what the mine probabilities are.  Or just let the computer play for you!

MindSweeper's old SourceForge [project
page](http://sourceforge.net/projects/mindsweeper).  Requires a
GNU/Linux-like system with GTK2, POSIX threads, etc., etc..

## Status

Current features:

- Unlimited board size (limited by screen size).
- Adjustable cell size.
- Four preset difficulty levels.
- Option to analyze the game board in real-time to see if a move can be
  made without guessing.
- Option to display the probability of finding a mine in each cell based
  on the information available.
- Option to autoplay the game:  to automatically make any moves that
  can be made without guessing.
- Option to autoplay a specified number of games and record the
  percentage that could be completed without guessing, thereby measuring
  the playability of a given board size and mine density.

A recent copy of the TODO list:

- Improve the search status graphics and odometer numerals.
- Split the solver into a front-end and back-end so the algorithm can be
  more easily re-used.
- Option to filter games on solvability.
- A random number generator with a longer period would be nice.
- Make cell graphics scale with cell size.
- Compute the number of flags expected to be left over after each move
  and use this to assign a mine probability to the non-border cells.
- Internationalization.

## Screenshots

Everybody loves screenshots.

<img src="html/screenshot.png" data-border="0" width="475" height="379" />  
  
A difficult board to analyze, showing the result for one cell.

## Solving Minesweeper

The solver in this game uses a brute force approach.  It begins by
identifying all of the unmarked cells for which information is available
and then systematically tests each possible arrangement of flags in
those cells searching for flag arrangements that are consistent with all
available information.  As the search proceeds, for each cell a count is
kept of the number of valid arrangements in which it was flagged.  When
the search terminates, the ratio of this number to the total number of
valid arrangements gives the probability of finding a mine in that
cell.  If this ratio is 0 then the cell can be cleared with certainty,
if the ratio is 1.0 then it can be flagged with certainty.

Some tricks are used in order to speed up the analysis process.  First
of all, it is often the case that a cell can be cleared or flagged based
solely on the information available in its immediate neighbours;  for
example a corner square with a 1 diagonally adjacent to it.

<img src="html/a_corner.png" width="83" height="71" />

A special algorithm is included in the solver for handling these cases
and it is only when this algorithm fails to identify a move that the
full brute-force search is begun.  Another trick that is used is the
separation of the border cells into independent groups:  groups of cells
whose flag placements do not influence each other.  A full solution to
the problem of identifying logically independent cell groups is as
complex as actually solving the game.  The test that is used here is
simply to check if two cells share the same information and if they
don't then they are said to be independent.  In this sense, it is more
accurate to say the cells are divided into geographically distinct
sets.  This separation greatly reduces the number of flag permutations
that need to be searched.

The final trick that is used, although it is not much of trick, is to
search the permutations for valid flag arrangements using an in-order
tree traversal algorithm.  This allows entire branches of the tree to be
ruled out early on without having to actually test each possible
permutation one-by-one.  Consider, for example, that the border region
in the screenshot above contains 78 unmarked cells.  Given that there
are 50 flags remaining to be placed on the board there are, in
principle, 3\*10<sup>23</sup> permutations available.  On a 1.3 GHz
Pentium 3, the solver takes approximately 87 seconds to decide that the
information available does not identify any moves with certainty --- an
effective analysis rate of 3.4\*10<sup>21</sup> permutations per second
or 2.7\*10<sup>12</sup> permutations per CPU clock cycle.  This is
clearly way beyond the processing capabilities of the computer and so
illustrates the efficiency with which permutations are analyzed.

At the same time, the solver also takes into account the fact that it is
playing on a finite field with a known total number of mines.  This
information is used to rule out flag permutations that would exceed the
number of mines and this additional restriction sometimes allows boards
to be played to completion that could not be completed otherwise.  The
limit on the number of mines can also sometimes allow cells outside of
the border region to be played.  By knowing, for example, that the
border cells must use all remaining flags then all other cells can be
cleared.  Alternatively, knowing that the border cells must not use more
than some number of flags it may be necessary to flag all other cells.

Overall, on modern personal computers the solver is capable of playing
the game much faster than a human which allows it to play along with the
user in real time.  In practice, Advanced level boards rarely take
more than 2 seconds to play to completion on the system described above
and are most often completed with 0 still showing on the clock.

For some information on the minesweeper problem, check out [Richard
Kaye's Minesweeper
Pages](http://www.mat.bham.ac.uk/R.W.Kaye/minesw/minesw.htm).  For
another solver-equipped minefield game, check out [Truffle-Swine
Keeper](http://people.freenet.de/hskopp/swinekeeper.html).
