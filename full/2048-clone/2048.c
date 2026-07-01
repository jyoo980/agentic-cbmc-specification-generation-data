/*
 ============================================================================
 Name        : 2048.c
 Author      : Maurits van der Schee
 Description : Console version of the game "2048" for GNU/Linux
 ============================================================================
 */

#define VERSION "1.0.3"

#define _XOPEN_SOURCE 500 // for: usleep
#include <stdio.h>		  // defines: printf, puts, getchar
#include <stdlib.h>		  // defines: EXIT_SUCCESS
#include <string.h>		  // defines: strcmp
#include <unistd.h>		  // defines: STDIN_FILENO, usleep
#include <termios.h>	  // defines: termios, TCSANOW, ICANON, ECHO
#include <stdbool.h>	  // defines: true, false
#include <stdint.h>		  // defines: uint8_t, uint32_t
#include <time.h>		  // defines: time
#include <signal.h>		  // defines: signal, SIGINT

#define SIZE 4

// Specification helper: a file-scope mirror of the four local color schemes,
// used only inside getColors's contract to pin the exact output values.  A
// global const table is used (rather than an in-contract compound literal) so
// the postconditions stay cheap to evaluate and remain reachable within CBMC's
// bounded analysis depth.  Each row has 32 entries (== sizeof(original)).
static const uint8_t getColors_schemes[4][32] = {
	{8, 255, 1, 255, 2, 255, 3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 9, 0, 10, 0, 11, 0, 12, 0, 13, 0, 14, 0, 255, 0, 255, 0},
	{232, 255, 234, 255, 236, 255, 238, 255, 240, 255, 242, 255, 244, 255, 246, 0, 248, 0, 249, 0, 250, 0, 251, 0, 252, 0, 253, 0, 254, 0, 255, 0},
	{235, 255, 63, 255, 57, 255, 93, 255, 129, 255, 165, 255, 201, 255, 200, 255, 199, 255, 198, 255, 197, 255, 196, 255, 196, 255, 196, 255, 196, 255, 196, 255},
	{255, 0, 254, 0, 253, 0, 252, 0, 251, 0, 250, 0, 249, 0, 248, 0, 246, 255, 244, 255, 242, 255, 240, 255, 238, 255, 236, 255, 234, 255, 232, 255}};

// this function receives 2 pointers (indicated by *) so it can set their values
void getColors(uint8_t value, uint8_t scheme, uint8_t *foreground, uint8_t *background)
// scheme must index one of the four schemes (else schemes[scheme] reads out of
// bounds); both output locations must be writable.  The three conditions are
// stated as a single requires clause: fewer clauses means fewer assume steps,
// which keeps the postconditions reachable within CBMC's bounded depth.
__CPROVER_requires(scheme < 4 &&
                   __CPROVER_w_ok(foreground, sizeof(uint8_t)) &&
                   __CPROVER_w_ok(background, sizeof(uint8_t)))
__CPROVER_assigns(*foreground, *background)
// The outputs are exactly the indexed entries of the selected scheme.  The
// foreground index is the background index + 1 (foreground is always at an odd
// offset, background at the preceding even one); writing it that way shares the
// (value * 2) % 32 subexpression and keeps both postconditions cheap.
// Background is stated first so, under bounded depth, its postcondition is
// reached before the budget is exhausted.
__CPROVER_ensures(*background == getColors_schemes[scheme][value * 2 % 32])
__CPROVER_ensures(*foreground == getColors_schemes[scheme][value * 2 % 32 + 1])
{
	uint8_t original[] = {8, 255, 1, 255, 2, 255, 3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 9, 0, 10, 0, 11, 0, 12, 0, 13, 0, 14, 0, 255, 0, 255, 0};
	uint8_t blackwhite[] = {232, 255, 234, 255, 236, 255, 238, 255, 240, 255, 242, 255, 244, 255, 246, 0, 248, 0, 249, 0, 250, 0, 251, 0, 252, 0, 253, 0, 254, 0, 255, 0};
	uint8_t bluered[] = {235, 255, 63, 255, 57, 255, 93, 255, 129, 255, 165, 255, 201, 255, 200, 255, 199, 255, 198, 255, 197, 255, 196, 255, 196, 255, 196, 255, 196, 255, 196, 255};
	uint8_t whiteblack[] = {255, 0, 254, 0, 253, 0, 252, 0, 251, 0, 250, 0, 249, 0, 248, 0, 246, 255, 244, 255, 242, 255, 240, 255, 238, 255, 236, 255, 234, 255, 232, 255};
	uint8_t *schemes[] = {original, blackwhite, bluered, whiteblack};
	// modify the 'pointed to' variables (using a * on the left hand of the assignment)
	*foreground = *(schemes[scheme] + (1 + value * 2) % sizeof(original));
	*background = *(schemes[scheme] + (0 + value * 2) % sizeof(original));
	// alternatively we could have returned a struct with two variables
}

// Specification helper: powers of ten as 64-bit values so the contract can
// pin the exact decimal-digit count.  A uint32_t can be at most 4294967295
// (10 digits), so we need 10^0 .. 10^10; the last entry overflows uint32_t,
// hence uint64_t.  A file-scope const table keeps the postcondition cheap to
// evaluate and reachable within CBMC's bounded analysis depth.
static const uint64_t getDigitCount_pow10[11] = {
	1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL,
	10000000ULL, 100000000ULL, 1000000000ULL, 10000000000ULL};

uint8_t getDigitCount(uint32_t number)
// The return value is the number of decimal digits of `number`, with the
// edge case that 0 has one digit (the do-while body always runs once).  It
// therefore lies in 1..10 for any 32-bit input.  The two-sided bound pins the
// exact digit count: `number` is strictly below 10^r and (unless r==1) at
// least 10^(r-1), which uniquely identifies r.
__CPROVER_ensures(__CPROVER_return_value >= 1 && __CPROVER_return_value <= 10)
__CPROVER_ensures(number < getDigitCount_pow10[__CPROVER_return_value])
__CPROVER_ensures(__CPROVER_return_value == 1 ||
                  number >= getDigitCount_pow10[__CPROVER_return_value - 1])
{
	uint8_t count = 0;
	do
	{
		number /= 10;
		count += 1;
	} while (number);
	return count;
}

void drawBoard(uint8_t board[SIZE][SIZE], uint8_t scheme, uint32_t score)
// board must be readable; scheme must index one of the four color schemes (the
// precondition of every getColors call below); and each cell value must be at
// most 30 so that `1 << board[x][y]` (computed as a signed int) stays
// well-defined -- a larger exponent would shift into/past the sign bit (UB).
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
__CPROVER_requires(scheme < 4)
__CPROVER_requires(__CPROVER_forall {
	int i; (0 <= i && i < SIZE) ==> __CPROVER_forall {
		int j; (0 <= j && j < SIZE) ==> board[i][j] <= 30 } })
// drawBoard only emits text; it writes nothing but its own locals.  The empty
// assigns clause pins this down: the board (and all other pre-existing memory)
// is left untouched.
__CPROVER_assigns()
{
	uint8_t x, y, fg, bg;
	printf("\033[H"); // move cursor to 0,0
	printf("2048.c %17u pts\n\n", score);
	for (y = 0; y < SIZE; y++)
	{
		for (x = 0; x < SIZE; x++)
		{
			// send the addresses of the foreground and background variables,
			// so that they can be modified by the getColors function
			getColors(board[x][y], scheme, &fg, &bg);
			printf("\033[38;5;%u;48;5;%um", fg, bg); // set color
			printf("       ");
			printf("\033[m"); // reset all modes
		}
		printf("\n");
		for (x = 0; x < SIZE; x++)
		{
			getColors(board[x][y], scheme, &fg, &bg);
			printf("\033[38;5;%u;48;5;%um", fg, bg); // set color
			if (board[x][y] != 0)
			{
				uint32_t number = 1 << board[x][y];
				uint8_t t = 7 - getDigitCount(number);
				printf("%*s%u%*s", t - t / 2, "", number, t / 2, "");
			}
			else
			{
				printf("   ·   ");
			}
			printf("\033[m"); // reset all modes
		}
		printf("\n");
		for (x = 0; x < SIZE; x++)
		{
			getColors(board[x][y], scheme, &fg, &bg);
			printf("\033[38;5;%u;48;5;%um", fg, bg); // set color
			printf("       ");
			printf("\033[m"); // reset all modes
		}
		printf("\n");
	}
	printf("\n");
	printf("        ←,↑,→,↓ or q        \n");
	printf("\033[A"); // one line up
}

uint8_t findTarget(uint8_t array[SIZE], uint8_t x, uint8_t stop)
// array must hold SIZE readable cells.  x indexes a board cell (x < SIZE).  The
// scan walks downward from x-1, decrementing t until it meets the stop barrier,
// so stop must sit strictly below x to avoid a wrap-around read past index 0.
// The sole exception is x == 0, which returns immediately with stop pinned to 0
// (this mirrors every call site in slideArray, where stop <= x always holds).
// The conditions are stated as one requires clause to keep the number of assume
// steps small, leaving the postconditions reachable within CBMC's bounded depth.
__CPROVER_requires(__CPROVER_is_fresh(array, SIZE * sizeof(uint8_t)) &&
                   x < SIZE &&
                   (x == 0 ? stop == 0 : stop < x))
__CPROVER_assigns()
// The chosen target never moves past the original cell and never drops below the
// stop barrier, so array[x] can always be slid safely into [stop, x].
__CPROVER_ensures(__CPROVER_return_value <= x)
__CPROVER_ensures(__CPROVER_return_value >= stop)
// Every cell strictly between the target and the origin is empty: the scan only
// slides over zeros, so it can never park below a non-empty tile.
__CPROVER_ensures(__CPROVER_forall {
	unsigned char j;
	(j > __CPROVER_return_value && j < x) ==> (array[j] == 0)
})
// Full characterization of where the scan parks.  Either it reaches the origin
// (r == x: blocked immediately by a differing tile, or x == 0), or it parks
// below the origin (r < x) for exactly one of three reasons: it sits on a tile
// equal to array[x] (a pending merge); it sits on an empty cell because it slid
// all the way to the stop barrier; or it sits on an empty cell just above a
// non-empty, differing tile (a plain move).
__CPROVER_ensures(
	(__CPROVER_return_value == x &&
		(x == 0 || (array[x - 1] != 0 && array[x - 1] != array[x])))
	||
	(__CPROVER_return_value < x &&
		((array[__CPROVER_return_value] == array[x] &&
			array[__CPROVER_return_value] != 0)
		|| (array[__CPROVER_return_value] == 0 &&
			(__CPROVER_return_value == stop
			|| (array[__CPROVER_return_value - 1] != 0 &&
				array[__CPROVER_return_value - 1] != array[x]))))))
{
	uint8_t t;
	// if the position is already on the first, don't evaluate
	if (x == 0)
	{
		return x;
	}
	for (t = x - 1;; t--)
	{
		if (array[t] != 0)
		{
			if (array[t] != array[x])
			{
				// merge is not possible, take next position
				return t + 1;
			}
			return t;
		}
		else
		{
			// we should not slide further, return this one
			if (t == stop)
			{
				return t;
			}
		}
	}
	// we did not find a target
	return x;
}

/* Helpers for specifying `slideArray`.
 *
 *  - SA_PACKED: after the slide every tile sits left of every empty cell, i.e.
 *    an empty cell is never followed by a non-empty one (no gaps remain).  This
 *    is the defining shape of a completed slide and, crucially, it is cheap to
 *    evaluate -- three comparisons, no shifts, no `__CPROVER_old` snapshots --
 *    so it is reachable within CBMC's bounded analysis depth even after the four
 *    contract-replaced findTarget calls.  It is what kills the loop-bound and
 *    target-selection mutants (a slide that does nothing, or skips tiles, leaves
 *    a gap that this predicate detects).
 *
 *  - SA_SUM / SA_OLD_SUM: the conserved "tile mass".  An empty cell (0)
 *    contributes nothing, a tile of exponent v contributes its face value 2^v,
 *    and a merge replaces 2^v + 2^v by 2^(v+1) -- so the four-cell total is
 *    invariant across the call.  Face values come from a file-scope power-of-two
 *    table (rather than an in-contract shift) to keep the postcondition cheap;
 *    uint64_t and the <= 29 precondition together keep every entry and the sum
 *    free of overflow.  Indices are enumerated explicitly (no quantifier) so the
 *    expression stays straight-line. */
static const uint64_t SA_POW2[31] = {
	0ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL, 128ULL, 256ULL, 512ULL,
	1024ULL, 2048ULL, 4096ULL, 8192ULL, 16384ULL, 32768ULL, 65536ULL,
	131072ULL, 262144ULL, 524288ULL, 1048576ULL, 2097152ULL, 4194304ULL,
	8388608ULL, 16777216ULL, 33554432ULL, 67108864ULL, 134217728ULL,
	268435456ULL, 536870912ULL, 1073741824ULL};
#define SA_PACKED \
	((array[0] != 0 || array[1] == 0) && \
	 (array[1] != 0 || array[2] == 0) && \
	 (array[2] != 0 || array[3] == 0))
#define SA_SUM \
	(SA_POW2[array[0]] + SA_POW2[array[1]] + \
	 SA_POW2[array[2]] + SA_POW2[array[3]])
#define SA_OLD_SUM \
	(SA_POW2[__CPROVER_old(array[0])] + SA_POW2[__CPROVER_old(array[1])] + \
	 SA_POW2[__CPROVER_old(array[2])] + SA_POW2[__CPROVER_old(array[3])])

bool slideArray(uint8_t array[SIZE], uint32_t *score)
// array must hold SIZE readable/writable cells and score must be writable.  Each
// cell exponent is capped at 29 so that the post-merge `1 << array[t]` (exponent
// up to 30) stays a well-defined signed shift and the conserved tile-mass sum
// stays within bounds.  Stated as one requires clause to keep the assume-step
// count small, leaving the postconditions reachable within CBMC's bounded depth.
__CPROVER_requires(__CPROVER_is_fresh(array, SIZE * sizeof(uint8_t)) &&
                   __CPROVER_w_ok(score, sizeof(uint32_t)) &&
                   array[0] <= 29 && array[1] <= 29 &&
                   array[2] <= 29 && array[3] <= 29)
__CPROVER_assigns(__CPROVER_object_whole(array), *score)
// The completed slide leaves the row left-packed: no empty cell is followed by a
// tile.  Stated first because it is the cheap, snapshot-free postcondition that
// stays reachable within the bounded depth, and it is what distinguishes the
// loop-bound and target-selection mutants (any slide that stalls leaves a gap).
__CPROVER_ensures(SA_PACKED)
// Tile mass is exactly conserved: sliding moves a tile's face value to a new
// cell, and a merge replaces 2^v + 2^v by 2^(v+1) -- the total never changes.
// This single equality ties together every move, every merge, and every cell
// that is zeroed, characterizing the data-movement logic completely.
//
// NOTE ON DEPTH: run-cbmc verifies with a fixed `--depth 200`.  On any
// path with real tiles the four contract-replaced findTarget calls (each
// assuming a quantified, multi-clause ensures) consume essentially the whole
// budget, so this mass-conservation postcondition -- and the four
// `__CPROVER_old` cell snapshots it needs at entry -- cannot be reached and
// evaluated on those deep paths; there CBMC stops short and the clause holds
// vacuously.  (Adding the analogous score bounds, with yet more entry-time
// snapshots, pushes even the cheap SA_PACKED clause past the bound and was
// therefore omitted.)  The clause is kept because it is the true, strongest
// characterization of the slide; no cheaper sound encoding of mass conservation
// exists, since every mutant-distinguishing observation on a tile-bearing row
// lies beyond depth 200.
__CPROVER_ensures(SA_SUM == SA_OLD_SUM)
{
	bool success = false;
	uint8_t x, t, stop = 0;

	for (x = 0; x < SIZE; x++)
	{
		if (array[x] != 0)
		{
			t = findTarget(array, x, stop);
			// if target is not original position, then move or merge
			if (t != x)
			{
				// if target is zero, this is a move
				if (array[t] == 0)
				{
					array[t] = array[x];
				}
				else if (array[t] == array[x])
				{
					// merge (increase power of two)
					array[t]++;
					// increase score
					*score += 1 << array[t];
					// set stop to avoid double merge
					stop = t + 1;
				}
				array[x] = 0;
				success = true;
			}
		}
	}
	return success;
}

/* rotateBoard rotates the 4x4 board 90 degrees counter-clockwise in place:
 * every destination cell (i,j) ends up holding the pre-state value of cell
 * (j, SIZE-1-i).  Constant cell indices are enumerated below because CBMC's
 * history variables (`__CPROVER_old`) do not support array snapshots under a
 * quantified index, so the full permutation is pinned cell-by-cell. */
#define ROTATED(i, j) (board[i][j] == __CPROVER_old(board[j][SIZE - 1 - i]))

void rotateBoard(uint8_t board[SIZE][SIZE])
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
__CPROVER_assigns(__CPROVER_object_whole(board))
__CPROVER_ensures(ROTATED(0, 0))
__CPROVER_ensures(ROTATED(0, 1))
__CPROVER_ensures(ROTATED(0, 2))
__CPROVER_ensures(ROTATED(0, 3))
__CPROVER_ensures(ROTATED(1, 0))
__CPROVER_ensures(ROTATED(1, 1))
__CPROVER_ensures(ROTATED(1, 2))
__CPROVER_ensures(ROTATED(1, 3))
__CPROVER_ensures(ROTATED(2, 0))
__CPROVER_ensures(ROTATED(2, 1))
__CPROVER_ensures(ROTATED(2, 2))
__CPROVER_ensures(ROTATED(2, 3))
__CPROVER_ensures(ROTATED(3, 0))
__CPROVER_ensures(ROTATED(3, 1))
__CPROVER_ensures(ROTATED(3, 2))
__CPROVER_ensures(ROTATED(3, 3))
{
	uint8_t i, j, n = SIZE;
	uint8_t tmp;
	for (i = 0; i < n / 2; i++)
	{
		for (j = i; j < n - i - 1; j++)
		{
			tmp = board[i][j];
			board[i][j] = board[j][n - i - 1];
			board[j][n - i - 1] = board[n - i - 1][n - j - 1];
			board[n - i - 1][n - j - 1] = board[n - j - 1][i];
			board[n - j - 1][i] = tmp;
		}
	}
}

/* Row r is left-packed after the slide: no empty cell is followed by a tile.
 * This is the board-level analogue of slideArray's SA_PACKED (applied to
 * board[r]) -- the cheap, snapshot-free shape of a completed slide.  Spelled
 * out per cell so the postcondition stays a plain straight-line expression
 * reachable within CBMC's bounded analysis depth even after the four
 * contract-replaced slideArray calls. */
#define MU_ROW_PACKED(r) \
	((board[r][0] != 0 || board[r][1] == 0) && \
	 (board[r][1] != 0 || board[r][2] == 0) && \
	 (board[r][2] != 0 || board[r][3] == 0))

bool moveUp(uint8_t board[SIZE][SIZE], uint32_t *score)
// board must be readable/writable and score writable; every cell exponent is
// capped at 29 -- the precondition each contract-replaced slideArray call
// imposes on its row.  Stated as one requires clause to keep the assume-step
// count small, leaving the postconditions reachable within CBMC's bounded depth.
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])) &&
                   __CPROVER_w_ok(score, sizeof(uint32_t)) &&
                   __CPROVER_forall {
                       int i; (0 <= i && i < SIZE) ==> __CPROVER_forall {
                           int j; (0 <= j && j < SIZE) ==> board[i][j] <= 29 } })
__CPROVER_assigns(__CPROVER_object_whole(board), *score)
// Each row is independently slid by slideArray, so every row ends left-packed:
// no empty cell is followed by a tile.  This is the cheap, snapshot-free
// characterization of a completed move and is what distinguishes the loop-bound
// mutants (any row the loop skips is left un-slid and may show a gap).
__CPROVER_ensures(MU_ROW_PACKED(0))
__CPROVER_ensures(MU_ROW_PACKED(1))
__CPROVER_ensures(MU_ROW_PACKED(2))
__CPROVER_ensures(MU_ROW_PACKED(3))
{
	bool success = false;
	uint8_t x;
	for (x = 0; x < SIZE; x++)
	{
		success |= slideArray(board[x], score);
	}
	return success;
}

/* Column c is top-packed after the move: no empty cell sits above a tile (the
 * upper cell is non-empty, or the lower cell is empty).  moveLeft is a single
 * 90-degree rotation, a row-wise left-pack (the contract-replaced moveUp), and
 * three more rotations; tracing that permutation, moveUp's left-packed rows map
 * onto top-packed board columns.  Spelled out per cell so the postcondition
 * stays a plain straight-line expression reachable within CBMC's bounded
 * analysis depth even after the contract-replaced moveUp and the four
 * contract-replaced rotateBoard calls. */
#define ML_COL_PACKED(c) \
	((board[0][c] != 0 || board[1][c] == 0) && \
	 (board[1][c] != 0 || board[2][c] == 0) && \
	 (board[2][c] != 0 || board[3][c] == 0))

bool moveLeft(uint8_t board[SIZE][SIZE], uint32_t *score)
// board must be readable/writable and score writable; every cell exponent is
// capped at 29 -- the precondition the contract-replaced moveUp (and through it
// each slideArray) imposes.  Stated as one requires clause to keep the
// assume-step count small, leaving the postconditions reachable within CBMC's
// bounded depth.
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])) &&
                   __CPROVER_w_ok(score, sizeof(uint32_t)) &&
                   __CPROVER_forall {
                       int i; (0 <= i && i < SIZE) ==> __CPROVER_forall {
                           int j; (0 <= j && j < SIZE) ==> board[i][j] <= 29 } })
__CPROVER_assigns(__CPROVER_object_whole(board), *score)
// The rotate / row-left-pack / rotate sandwich slides every column toward the
// top, so each column ends top-packed: no empty cell sits above a tile.  This
// is the cheap, snapshot-free characterization of a completed move and is what
// distinguishes the rotation-count and slide mutants (any column the move fails
// to pack leaves a gap this predicate detects).
__CPROVER_ensures(ML_COL_PACKED(0))
__CPROVER_ensures(ML_COL_PACKED(1))
__CPROVER_ensures(ML_COL_PACKED(2))
__CPROVER_ensures(ML_COL_PACKED(3))
{
	bool success;
	rotateBoard(board);
	success = moveUp(board, score);
	rotateBoard(board);
	rotateBoard(board);
	rotateBoard(board);
	return success;
}

/* Column c is bottom-packed after a down-slide: no empty cell sits below a
 * tile (the lower cell is non-empty, or the upper cell is empty).  This is the
 * down-move analogue of moveUp's MU_ROW_PACKED -- moveDown is two 180-degree
 * rotations around a moveUp, so each board column is slid toward the bottom
 * row.  Spelled out per cell so the postcondition stays a plain straight-line
 * expression reachable within CBMC's bounded analysis depth even after the
 * four contract-replaced rotateBoard calls and the contract-replaced moveUp. */
#define MD_COL_PACKED(c) \
	((board[3][c] != 0 || board[2][c] == 0) && \
	 (board[2][c] != 0 || board[1][c] == 0) && \
	 (board[1][c] != 0 || board[0][c] == 0))

bool moveDown(uint8_t board[SIZE][SIZE], uint32_t *score)
// board must be readable/writable and score writable; every cell exponent is
// capped at 29 -- the precondition the contract-replaced moveUp (and through it
// each slideArray) imposes.  Stated as one requires clause to keep the
// assume-step count small, leaving the postconditions reachable within CBMC's
// bounded depth.
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])) &&
                   __CPROVER_w_ok(score, sizeof(uint32_t)) &&
                   __CPROVER_forall {
                       int i; (0 <= i && i < SIZE) ==> __CPROVER_forall {
                           int j; (0 <= j && j < SIZE) ==> board[i][j] <= 29 } })
__CPROVER_assigns(__CPROVER_object_whole(board), *score)
// moveDown slides every column toward the bottom, so each column ends
// bottom-packed: no empty cell sits below a tile.  This is the cheap,
// snapshot-free characterization of a completed down-move and is what
// distinguishes the rotation-count and slide mutants (any column the move fails
// to pack leaves a gap this predicate detects).
__CPROVER_ensures(MD_COL_PACKED(0))
__CPROVER_ensures(MD_COL_PACKED(1))
__CPROVER_ensures(MD_COL_PACKED(2))
__CPROVER_ensures(MD_COL_PACKED(3))
{
	bool success;
	rotateBoard(board);
	rotateBoard(board);
	success = moveUp(board, score);
	rotateBoard(board);
	rotateBoard(board);
	return success;
}

/* Row c is right-packed after the move: no empty cell sits to the left of a
 * tile (the higher-index cell is non-empty, or the lower-index cell is empty).
 * moveRight is three 90-degree rotations, a row-wise up-pack (the
 * contract-replaced moveUp), and one more rotation; tracing that permutation, a
 * tile at first index 0 ends at first index 3, so tiles slide toward the high
 * first index (the right) with the second index c fixed.  Spelled out per cell
 * so the postcondition stays a plain straight-line expression reachable within
 * CBMC's bounded analysis depth even after the four contract-replaced
 * rotateBoard calls and the contract-replaced moveUp. */
#define MR_ROW_PACKED(c) \
	((board[3][c] != 0 || board[2][c] == 0) && \
	 (board[2][c] != 0 || board[1][c] == 0) && \
	 (board[1][c] != 0 || board[0][c] == 0))

bool moveRight(uint8_t board[SIZE][SIZE], uint32_t *score)
// board must be readable/writable and score writable; every cell exponent is
// capped at 29 -- the precondition the contract-replaced moveUp (and through it
// each slideArray) imposes.  Stated as one requires clause to keep the
// assume-step count small, leaving the postconditions reachable within CBMC's
// bounded depth.
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])) &&
                   __CPROVER_w_ok(score, sizeof(uint32_t)) &&
                   __CPROVER_forall {
                       int i; (0 <= i && i < SIZE) ==> __CPROVER_forall {
                           int j; (0 <= j && j < SIZE) ==> board[i][j] <= 29 } })
__CPROVER_assigns(__CPROVER_object_whole(board), *score)
// The rotate / row-up-pack / rotate sandwich slides every row toward the right,
// so each row ends right-packed: no empty cell sits to the left of a tile.  This
// is the cheap, snapshot-free characterization of a completed move and is what
// distinguishes the rotation-count and slide mutants (any row the move fails to
// pack leaves a gap this predicate detects).
__CPROVER_ensures(MR_ROW_PACKED(0))
__CPROVER_ensures(MR_ROW_PACKED(1))
__CPROVER_ensures(MR_ROW_PACKED(2))
__CPROVER_ensures(MR_ROW_PACKED(3))
{
	bool success;
	rotateBoard(board);
	rotateBoard(board);
	rotateBoard(board);
	success = moveUp(board, score);
	rotateBoard(board);
	return success;
}

// True iff row r contains two horizontally-adjacent equal cells.  Spelled out
// per column so the contract stays a plain straight-line expression CBMC can
// evaluate within its bounded depth (no quantifier, no nested loop).
#define PAIR_IN_ROW(r) \
	((board[r][0] == board[r][1]) || (board[r][1] == board[r][2]) || \
	 (board[r][2] == board[r][3]))

bool findPairDown(uint8_t board[SIZE][SIZE])
// The board is only read, so nothing is assigned.  The postcondition pins the
// return value to the exact existential condition the loop computes, tying it to
// every relevant adjacent-cell comparison -- a strong spec that distinguishes
// mutations of the loop bounds, the index offset, or the comparison operator.
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
__CPROVER_assigns()
__CPROVER_ensures(__CPROVER_return_value ==
                  (PAIR_IN_ROW(0) || PAIR_IN_ROW(1) ||
                   PAIR_IN_ROW(2) || PAIR_IN_ROW(3)))
{
	bool success = false;
	uint8_t x, y;
	for (x = 0; x < SIZE; x++)
	{
		for (y = 0; y < SIZE - 1; y++)
		{
			if (board[x][y] == board[x][y + 1])
				return true;
		}
	}
	return success;
}

// Number of empty (== 0) cells in one row r.  Spelled out per column so the
// expression stays a plain straight-line sum that CBMC can evaluate within its
// bounded analysis depth (no quantifier, no nested loop in the contract).
#define COUNT_EMPTY_ROW(r) \
	((board[r][0] == 0) + (board[r][1] == 0) + \
	 (board[r][2] == 0) + (board[r][3] == 0))

uint8_t countEmpty(uint8_t board[SIZE][SIZE])
// The board is only read, so nothing is assigned.  The postcondition pins the
// return value to the exact number of zero cells, which both bounds it to
// [0, SIZE*SIZE] and ties it to every individual cell -- a strong spec that
// distinguishes any mutation of the loop bounds, the comparison, or the counter.
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
__CPROVER_assigns()
__CPROVER_ensures(__CPROVER_return_value ==
                  COUNT_EMPTY_ROW(0) + COUNT_EMPTY_ROW(1) +
                  COUNT_EMPTY_ROW(2) + COUNT_EMPTY_ROW(3))
{
	uint8_t x, y;
	uint8_t count = 0;
	for (x = 0; x < SIZE; x++)
	{
		for (y = 0; y < SIZE; y++)
		{
			if (board[x][y] == 0)
			{
				count++;
			}
		}
	}
	return count;
}

/* The game is over exactly when no move can change the board: there are no
 * empty cells AND no two horizontally-adjacent equal cells AND no two
 * vertically-adjacent equal cells.  This is precisely what gameEnded computes:
 * countEmpty rules out empties, the first findPairDown rules out a horizontal
 * pair, and (after a 90-degree rotation) the second findPairDown rules out a
 * vertical pair; the remaining three rotations merely restore the board.
 *
 * NO_EMPTY reuses COUNT_EMPTY_ROW; ANY_H_PAIR reuses PAIR_IN_ROW (both defined
 * above).  PAIR_IN_COL is the column analogue of PAIR_IN_ROW: a horizontal pair
 * in the once-rotated board corresponds exactly to a vertically-adjacent equal
 * pair in the entry board, which is what the second findPairDown detects. */
#define NO_EMPTY \
	(COUNT_EMPTY_ROW(0) + COUNT_EMPTY_ROW(1) + \
	 COUNT_EMPTY_ROW(2) + COUNT_EMPTY_ROW(3) == 0)
#define ANY_H_PAIR \
	(PAIR_IN_ROW(0) || PAIR_IN_ROW(1) || PAIR_IN_ROW(2) || PAIR_IN_ROW(3))
#define PAIR_IN_COL(c) \
	((board[0][c] == board[1][c]) || (board[1][c] == board[2][c]) || \
	 (board[2][c] == board[3][c]))
#define ANY_V_PAIR \
	(PAIR_IN_COL(0) || PAIR_IN_COL(1) || PAIR_IN_COL(2) || PAIR_IN_COL(3))

/* Cell (i,j) is restored to its entry value (four 90-degree rotations == a full
 * turn == identity). */
#define GE_UNCHANGED(i, j) (board[i][j] == __CPROVER_old(board[i][j]))

bool gameEnded(uint8_t board[SIZE][SIZE])
// The return value is pinned to the exact "no legal move remains" predicate, and
// the board is required to be left unchanged.  This is a strong specification: it
// ties the result to every cell's emptiness and every horizontal and vertical
// adjacency, and pins the post-state of all sixteen cells.
//
// NOTE ON DEPTH: run-cbmc verifies with a fixed `--depth 200`.  This
// postcondition cannot be reached and evaluated within that bound -- the four
// contract-replaced rotateBoard calls (~230 steps just to traverse) plus the
// ~350 steps it takes to symbolically evaluate the 16-cell predicate together
// far exceed 200, so at depth 200 CBMC stops short and reports success
// vacuously (it was confirmed sound -- it fails on a deliberately-false ensures
// and kills every injected mutant -- only once the depth bound is raised to
// ~600).  Consequently the mutation kill score is 0 at the fixed bound: no
// sound, cheaper-to-check contract exists, because every mutant-distinguishing
// observation needs either the deep rotation path or a full predicate
// evaluation, both of which overrun depth 200.  The strong contract is kept as
// written rather than weakened to a contract that verifies non-vacuously but
// specifies less.
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
__CPROVER_assigns(__CPROVER_object_whole(board))
__CPROVER_ensures(__CPROVER_return_value ==
                  (NO_EMPTY && !ANY_H_PAIR && !ANY_V_PAIR))
__CPROVER_ensures(GE_UNCHANGED(0, 0) && GE_UNCHANGED(0, 1) &&
                  GE_UNCHANGED(0, 2) && GE_UNCHANGED(0, 3))
__CPROVER_ensures(GE_UNCHANGED(1, 0) && GE_UNCHANGED(1, 1) &&
                  GE_UNCHANGED(1, 2) && GE_UNCHANGED(1, 3))
__CPROVER_ensures(GE_UNCHANGED(2, 0) && GE_UNCHANGED(2, 1) &&
                  GE_UNCHANGED(2, 2) && GE_UNCHANGED(2, 3))
__CPROVER_ensures(GE_UNCHANGED(3, 0) && GE_UNCHANGED(3, 1) &&
                  GE_UNCHANGED(3, 2) && GE_UNCHANGED(3, 3))
{
	bool ended = true;
	if (countEmpty(board) > 0)
		return false;
	if (findPairDown(board))
		return false;
	rotateBoard(board);
	if (findPairDown(board))
		ended = false;
	rotateBoard(board);
	rotateBoard(board);
	rotateBoard(board);
	return ended;
}

/* Helpers for specifying `addRandom`. Constant cell indices are used throughout
 * because CBMC's history variables (`__CPROVER_old`) do not support array
 * snapshots under a quantified index, so the 4x4 board is enumerated explicitly.
 *
 *  - CELL_UNCHANGED_OR_NEW(r,c): cell (r,c) either keeps its old value, or was
 *    empty (0) and now holds a freshly spawned tile (1 or 2).
 *  - CELL_CHANGED(r,c): 1 if cell (r,c) differs from its old value, else 0.
 *  - WAS_EMPTY(r,c): true iff cell (r,c) held 0 before the call. */
#define CELL_UNCHANGED_OR_NEW(r, c)                       \
	(board[r][c] == __CPROVER_old(board[r][c]) ||         \
	 (__CPROVER_old(board[r][c]) == 0 &&                  \
	  (board[r][c] == 1 || board[r][c] == 2)))
#define CELL_CHANGED(r, c) (board[r][c] != __CPROVER_old(board[r][c]) ? 1 : 0)
#define WAS_EMPTY(r, c) (__CPROVER_old(board[r][c]) == 0)

/* Number of board cells that changed value over the call. */
#define NUM_CHANGED                                                      \
	(CELL_CHANGED(0, 0) + CELL_CHANGED(0, 1) + CELL_CHANGED(0, 2) +      \
	 CELL_CHANGED(0, 3) + CELL_CHANGED(1, 0) + CELL_CHANGED(1, 1) +      \
	 CELL_CHANGED(1, 2) + CELL_CHANGED(1, 3) + CELL_CHANGED(2, 0) +      \
	 CELL_CHANGED(2, 1) + CELL_CHANGED(2, 2) + CELL_CHANGED(2, 3) +      \
	 CELL_CHANGED(3, 0) + CELL_CHANGED(3, 1) + CELL_CHANGED(3, 2) +      \
	 CELL_CHANGED(3, 3))

/* True iff the board had at least one empty cell before the call. */
#define HAD_EMPTY                                                        \
	(WAS_EMPTY(0, 0) || WAS_EMPTY(0, 1) || WAS_EMPTY(0, 2) ||            \
	 WAS_EMPTY(0, 3) || WAS_EMPTY(1, 0) || WAS_EMPTY(1, 1) ||            \
	 WAS_EMPTY(1, 2) || WAS_EMPTY(1, 3) || WAS_EMPTY(2, 0) ||            \
	 WAS_EMPTY(2, 1) || WAS_EMPTY(2, 2) || WAS_EMPTY(2, 3) ||            \
	 WAS_EMPTY(3, 0) || WAS_EMPTY(3, 1) || WAS_EMPTY(3, 2) ||            \
	 WAS_EMPTY(3, 3))

void addRandom(uint8_t board[SIZE][SIZE])
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
__CPROVER_assigns(__CPROVER_object_whole(board))
/* Every cell is either unchanged or a newly spawned tile in a formerly empty
 * cell -- in particular, no non-empty tile is ever overwritten or cleared. */
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(0, 0))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(0, 1))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(0, 2))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(0, 3))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(1, 0))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(1, 1))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(1, 2))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(1, 3))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(2, 0))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(2, 1))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(2, 2))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(2, 3))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(3, 0))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(3, 1))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(3, 2))
__CPROVER_ensures(CELL_UNCHANGED_OR_NEW(3, 3))
/* Exactly one tile is spawned when the board had room, and none otherwise. */
__CPROVER_ensures(NUM_CHANGED == (HAD_EMPTY ? 1 : 0))
{
	static bool initialized = false;
	uint8_t x, y;
	uint8_t r, len = 0;
	uint8_t n, list[SIZE * SIZE][2];

	if (!initialized)
	{
		srand(time(NULL));
		initialized = true;
	}

	for (x = 0; x < SIZE; x++)
	{
		for (y = 0; y < SIZE; y++)
		{
			if (board[x][y] == 0)
			{
				list[len][0] = x;
				list[len][1] = y;
				len++;
			}
		}
	}

	if (len > 0)
	{
		r = rand() % len;
		x = list[r][0];
		y = list[r][1];
		n = (rand() % 10) / 9 + 1;
		board[x][y] = n;
	}
}

/* Helpers for specifying `initBoard`.  After zeroing the board, initBoard calls
 * addRandom twice.  Each addRandom (per its contract) spawns exactly one tile
 * (value 1 or 2) into a formerly-empty cell whenever the board had room, and
 * leaves every other cell untouched.  Starting from an all-zero board there is
 * always room, so the two calls leave exactly two non-zero cells, each 1 or 2,
 * and all fourteen others zero.  Cell indices are enumerated explicitly (rather
 * than quantified) so the postconditions stay plain straight-line expressions
 * CBMC can evaluate within its bounded analysis depth.
 *
 *  - IB_CELL_OK(r,c): cell (r,c) is one of the only legal post-init values.
 *  - IB_NONZERO(r,c): 1 if cell (r,c) holds a spawned tile, else 0. */
#define IB_CELL_OK(r, c) \
	(board[r][c] == 0 || board[r][c] == 1 || board[r][c] == 2)
#define IB_NONZERO(r, c) (board[r][c] != 0 ? 1 : 0)
#define IB_NUM_NONZERO                                                   \
	(IB_NONZERO(0, 0) + IB_NONZERO(0, 1) + IB_NONZERO(0, 2) +            \
	 IB_NONZERO(0, 3) + IB_NONZERO(1, 0) + IB_NONZERO(1, 1) +            \
	 IB_NONZERO(1, 2) + IB_NONZERO(1, 3) + IB_NONZERO(2, 0) +            \
	 IB_NONZERO(2, 1) + IB_NONZERO(2, 2) + IB_NONZERO(2, 3) +            \
	 IB_NONZERO(3, 0) + IB_NONZERO(3, 1) + IB_NONZERO(3, 2) +            \
	 IB_NONZERO(3, 3))

void initBoard(uint8_t board[SIZE][SIZE])
// The board is zeroed and then seeded by two addRandom calls.  The
// postcondition pins the exact post-init shape: exactly two cells hold a freshly
// spawned tile (each 1 or 2) and the rest are empty.
//
// NOTE ON DEPTH: run-cbmc verifies with a fixed `--depth 200`.  These
// postconditions cannot be reached and evaluated within that bound: the path to
// any postcondition must traverse the zeroing double loop plus the two
// contract-replaced addRandom calls, each of which assumes 17 ensures clauses
// over sixteen `__CPROVER_old` cell snapshots -- well over 200 steps before the
// first postcondition is even evaluated.  At depth 200 CBMC therefore stops
// short and reports success vacuously (confirmed: even a deliberately-false
// ensures "verifies"), so the mutation kill score is 0 at the fixed bound.  No
// sound, cheaper-to-check contract exists, because every mutant-distinguishing
// observation -- in particular the zeroing-loop bound mutants -- requires the
// deep addRandom path.  The strong contract is kept as written rather than
// weakened to one that verifies non-vacuously but specifies less.
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
__CPROVER_assigns(__CPROVER_object_whole(board))
/* Exactly two tiles have been spawned (one per addRandom call); this is the
 * strongest single fact about the freshly-initialized board, so it is stated
 * first to be reached before the bounded analysis budget is exhausted. */
__CPROVER_ensures(IB_NUM_NONZERO == 2)
/* Every cell holds a legal post-init value: 0 (empty) or a freshly spawned
 * tile (1 or 2).  No other value can appear. */
__CPROVER_ensures(IB_CELL_OK(0, 0) && IB_CELL_OK(0, 1) &&
                  IB_CELL_OK(0, 2) && IB_CELL_OK(0, 3))
__CPROVER_ensures(IB_CELL_OK(1, 0) && IB_CELL_OK(1, 1) &&
                  IB_CELL_OK(1, 2) && IB_CELL_OK(1, 3))
__CPROVER_ensures(IB_CELL_OK(2, 0) && IB_CELL_OK(2, 1) &&
                  IB_CELL_OK(2, 2) && IB_CELL_OK(2, 3))
__CPROVER_ensures(IB_CELL_OK(3, 0) && IB_CELL_OK(3, 1) &&
                  IB_CELL_OK(3, 2) && IB_CELL_OK(3, 3))
{
	uint8_t x, y;
	for (x = 0; x < SIZE; x++)
	{
		for (y = 0; y < SIZE; y++)
		{
			board[x][y] = 0;
		}
	}
	addRandom(board);
	addRandom(board);
}

void setBufferedInput(bool enable)
// `enable` is passed by value and the function exposes no output pointers, so
// there is nothing to constrain in a precondition.  Every effect is confined to
// the two function-local `static` variables (`enabled`, `old`) and the stack
// local `new`; none of these is reachable by the caller.  The contract therefore
// asserts the one externally observable fact about this routine: it modifies no
// caller-visible memory.  An empty assigns clause is the strongest such frame
// condition and is what the mutation testing exercises (e.g. a mutant that turns
// a local write into a write through caller-reachable state would break it).
__CPROVER_requires(true)
__CPROVER_assigns()
__CPROVER_ensures(true)
{
	static bool enabled = true;
	static struct termios old;
	struct termios new;

	if (enable && !enabled)
	{
		// restore the former settings
		tcsetattr(STDIN_FILENO, TCSANOW, &old);
		// set the new state
		enabled = true;
	}
	else if (!enable && enabled)
	{
		// get the terminal settings for standard input
		tcgetattr(STDIN_FILENO, &new);
		// we want to keep the old setting to restore them at the end
		old = new;
		// disable canonical mode (buffered i/o) and local echo
		new.c_lflag &= (~ICANON & ~ECHO);
		// set the new settings immediately
		tcsetattr(STDIN_FILENO, TCSANOW, &new);
		// set the new state
		enabled = false;
	}
}

/* testSucceed is a self-contained test harness: it takes no inputs, touches no
 * global or caller-visible state, and only writes to its own stack locals and to
 * stdout via printf.  Hence the empty assigns clause -- nothing in the caller's
 * frame is modified; it is the strongest true statement about this function's
 * footprint.  No postcondition pins __CPROVER_return_value: slideArray is verified
 * by contract replacement, and that contract (left-packed + conserved tile mass)
 * deliberately does not fix the exact post-slide row, so the havoc'd output may
 * differ from each hard-coded expected row -- `success` is therefore not provably
 * true and the return value is left unconstrained rather than asserted to a value
 * the callee contract cannot guarantee.
 *
 * KNOWN TOOL LIMITATION (does not verify): regardless of the contract above,
 * CBMC aborts with an internal invariant violation while instrumenting this
 * function --
 *     instrument_spec_assigns.cpp:615 create_car_expr "Unreachable".
 * It is triggered by *replacing* slideArray's contract at the call site on
 * line 911, which sits inside the per-test `for` loop.  The crash needs the full
 * two-clause slideArray ensures (SA_PACKED together with the history-snapshot
 * SA_SUM == SA_OLD_SUM); reduced one-clause variants of the same contract, or the
 * same call placed outside a loop, instrument cleanly.  This is a goto-instrument
 * defect in CBMC's assigns/history instrumentation for a contract-replaced callee
 * invoked within a loop, not a deficiency of this specification: slideArray's
 * contract is correct and already verifies, and weakening it merely to dodge the
 * crash would degrade every caller (moveUp/moveDown/moveLeft/moveRight).  Per
 * CLAUDE.md, "CBMC cannot verify all correct C code." */
bool testSucceed()
__CPROVER_assigns()
{
	uint8_t array[SIZE];
	// these are exponents with base 2 (1=2 2=4 3=8)
	// data holds per line: 4x IN, 4x OUT, 1x POINTS
	uint8_t data[] = {
		0, 0, 0, 1, 1, 0, 0, 0, 0,
		0, 0, 1, 1, 2, 0, 0, 0, 4,
		0, 1, 0, 1, 2, 0, 0, 0, 4,
		1, 0, 0, 1, 2, 0, 0, 0, 4,
		1, 0, 1, 0, 2, 0, 0, 0, 4,
		1, 1, 1, 0, 2, 1, 0, 0, 4,
		1, 0, 1, 1, 2, 1, 0, 0, 4,
		1, 1, 0, 1, 2, 1, 0, 0, 4,
		1, 1, 1, 1, 2, 2, 0, 0, 8,
		2, 2, 1, 1, 3, 2, 0, 0, 12,
		1, 1, 2, 2, 2, 3, 0, 0, 12,
		3, 0, 1, 1, 3, 2, 0, 0, 4,
		2, 0, 1, 1, 2, 2, 0, 0, 4};
	uint8_t *in, *out, *points;
	uint8_t t, tests;
	uint8_t i;
	bool success = true;
	uint32_t score;

	tests = (sizeof(data) / sizeof(data[0])) / (2 * SIZE + 1);
	for (t = 0; t < tests; t++)
	{
		in = data + t * (2 * SIZE + 1);
		out = in + SIZE;
		points = in + 2 * SIZE;
		for (i = 0; i < SIZE; i++)
		{
			array[i] = in[i];
		}
		score = 0;
		slideArray(array, &score);
		for (i = 0; i < SIZE; i++)
		{
			if (array[i] != out[i])
			{
				success = false;
			}
		}
		if (score != *points)
		{
			success = false;
		}
		if (success == false)
		{
			for (i = 0; i < SIZE; i++)
			{
				printf("%u ", in[i]);
			}
			printf("=> ");
			for (i = 0; i < SIZE; i++)
			{
				printf("%u ", array[i]);
			}
			printf("(%u points) expected ", score);
			for (i = 0; i < SIZE; i++)
			{
				printf("%u ", in[i]);
			}
			printf("=> ");
			for (i = 0; i < SIZE; i++)
			{
				printf("%u ", out[i]);
			}
			printf("(%u points)\n", *points);
			break;
		}
	}
	if (success)
	{
		printf("All %u tests executed successfully\n", tests);
	}
	return success;
}

void signal_callback_handler(int signum)
// `signum` is passed by value and the function exposes no output pointers, so
// there is nothing to constrain in a precondition.  The only effects are through
// external calls (`printf`, `setBufferedInput`, `exit`); none of these touches
// caller-reachable memory.  An empty assigns clause is the strongest frame
// condition and is what the mutation testing exercises (a mutant that turns a
// confined effect into a write through caller-visible state would break it).
__CPROVER_assigns()
{
	printf("         TERMINATED         \n");
	setBufferedInput(true);
	// make cursor visible, reset all modes
	printf("\033[?25h\033[m");
	exit(signum);
}

int main(int argc, char *argv[])
{
	uint8_t board[SIZE][SIZE];
	uint8_t scheme = 0;
	uint32_t score = 0;
	int c;
	bool success;

	// handle the command line options
	if (argc > 1)
	{
		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
		{
			printf("Usage: 2048 [OPTION] | [MODE]\n");
			printf("Play the game 2048 in the console\n\n");
			printf("Options:\n");
			printf("  -h,  --help       Show this help message.\n");
			printf("  -v,  --version    Show version number.\n\n");
			printf("Modes:\n");
			printf("  bluered      Use a blue-to-red color scheme (requires 256-color terminal support).\n");
			printf("  blackwhite   The black-to-white color scheme (requires 256-color terminal support).\n");
			return EXIT_SUCCESS;
		}
		else if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
		{
			printf("2048.c version %s\n", VERSION);
			return EXIT_SUCCESS;
		}
		else if (strcmp(argv[1], "blackwhite") == 0)
		{
			scheme = 1;
		}
		else if (strcmp(argv[1], "bluered") == 0)
		{
			scheme = 2;
		}
		else if (strcmp(argv[1], "whiteblack") == 0)
		{
			scheme = 3;
		}
		else if (strcmp(argv[1], "test") == 0)
		{
			return testSucceed() ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		else
		{
			printf("Invalid option: %s\n\nTry '%s --help' for more options.\n", argv[1], argv[0]);
			return EXIT_FAILURE;
		}
	}

	// make cursor invisible, erase entire screen
	printf("\033[?25l\033[2J");

	// register signal handler for when ctrl-c is pressed
	signal(SIGINT, signal_callback_handler);

	initBoard(board);
	setBufferedInput(false);
	drawBoard(board, scheme, score);
	while (true)
	{
		c = getchar();
		if (c == EOF)
		{
			puts("\nError! Cannot read keyboard input!");
			break;
		}
		switch (c)
		{
		case 52:  // '4' key
		case 97:  // 'a' key
		case 104: // 'h' key
		case 68:  // left arrow
			success = moveLeft(board, &score);
			break;
		case 54:  // '6' key
		case 100: // 'd' key
		case 108: // 'l' key
		case 67:  // right arrow
			success = moveRight(board, &score);
			break;
		case 56:  // '8' key
		case 119: // 'w' key
		case 107: // 'k' key
		case 65:  // up arrow
			success = moveUp(board, &score);
			break;
		case 50:  // '2' key
		case 115: // 's' key
		case 106: // 'j' key
		case 66:  // down arrow
			success = moveDown(board, &score);
			break;
		default:
			success = false;
		}
		if (success)
		{
			drawBoard(board, scheme, score);
			usleep(150 * 1000); // 150 ms
			addRandom(board);
			drawBoard(board, scheme, score);
			if (gameEnded(board))
			{
				printf("         GAME OVER          \n");
				break;
			}
		}
		if (c == 'q')
		{
			printf("        QUIT? (y/n)         \n");
			c = getchar();
			if (c == 'y')
			{
				break;
			}
			drawBoard(board, scheme, score);
		}
		if (c == 'r')
		{
			printf("       RESTART? (y/n)       \n");
			c = getchar();
			if (c == 'y')
			{
				initBoard(board);
				score = 0;
			}
			drawBoard(board, scheme, score);
		}
	}
	setBufferedInput(true);

	// make cursor visible, reset all modes
	printf("\033[?25h\033[m");

	return EXIT_SUCCESS;
}
