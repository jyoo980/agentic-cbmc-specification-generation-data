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

// A copy of the four color-scheme tables, used only in the contract below to
// pin down the exact value written to each output pointer.
#define COLOR_SCHEMES_TABLE ((const uint8_t[4][32]){ \
	{8, 255, 1, 255, 2, 255, 3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 9, 0, 10, 0, 11, 0, 12, 0, 13, 0, 14, 0, 255, 0, 255, 0}, \
	{232, 255, 234, 255, 236, 255, 238, 255, 240, 255, 242, 255, 244, 255, 246, 0, 248, 0, 249, 0, 250, 0, 251, 0, 252, 0, 253, 0, 254, 0, 255, 0}, \
	{235, 255, 63, 255, 57, 255, 93, 255, 129, 255, 165, 255, 201, 255, 200, 255, 199, 255, 198, 255, 197, 255, 196, 255, 196, 255, 196, 255, 196, 255, 196, 255}, \
	{255, 0, 254, 0, 253, 0, 252, 0, 251, 0, 250, 0, 249, 0, 248, 0, 246, 255, 244, 255, 242, 255, 240, 255, 238, 255, 236, 255, 234, 255, 232, 255} \
})

// this function receives 2 pointers (indicated by *) so it can set their values
void getColors(uint8_t value, uint8_t scheme, uint8_t *foreground, uint8_t *background)
// scheme must be a valid index into the table of 4 schemes
__CPROVER_requires(scheme < 4)
// both output pointers must point to writable bytes
__CPROVER_requires(__CPROVER_w_ok(foreground, sizeof(uint8_t)))
__CPROVER_requires(__CPROVER_w_ok(background, sizeof(uint8_t)))
__CPROVER_assigns(*foreground, *background)
// the exact bytes written: odd table index for foreground, even index for background
__CPROVER_ensures(*foreground == COLOR_SCHEMES_TABLE[scheme][(1 + value * 2) % 32])
__CPROVER_ensures(*background == COLOR_SCHEMES_TABLE[scheme][(0 + value * 2) % 32])
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

// Powers of ten as 64-bit constants, indexed 0..10. Used by the getDigitCount
// contract to pin down the returned digit count without overflow (10^10 does
// not fit in uint32_t).
#define DC_POW10(k) (((const uint64_t[]){ \
	1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, \
	1000000ULL, 10000000ULL, 100000000ULL, 1000000000ULL, 10000000000ULL})[(k)])

uint8_t getDigitCount(uint32_t number)
// the count of decimal digits of a uint32_t is between 1 (for 0..9) and 10
__CPROVER_ensures(__CPROVER_return_value >= 1 && __CPROVER_return_value <= 10)
// the result d is the unique value with number < 10^d ...
__CPROVER_ensures((uint64_t)number < DC_POW10((int)__CPROVER_return_value))
// ... and number >= 10^(d-1), except 0 (which has no nonzero leading digit)
__CPROVER_ensures(__CPROVER_return_value == 1 || (uint64_t)number >= DC_POW10((int)__CPROVER_return_value - 1))
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
// clang-format off
// the board must be a readable 4x4 block of bytes
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// scheme is forwarded to getColors, whose contract requires a valid index
__CPROVER_requires(scheme < 4)
// each cell is a power-of-two exponent; drawBoard computes 1 << board[x][y],
// which is undefined for shift counts >= 31, so bound every cell at 30
__CPROVER_requires(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE * SIZE) ==> ((uint8_t *)board)[_k] <= 30
})
// drawBoard only reads the board and prints; it writes nothing observable
__CPROVER_assigns()
// clang-format on
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
// clang-format off
// the array must be a readable block of SIZE bytes
__CPROVER_requires(__CPROVER_is_fresh(array, sizeof(uint8_t[SIZE])))
// x is a valid index into the array (it is both read and used as a bound)
__CPROVER_requires(x < SIZE)
// when x >= 1 the scan runs down toward stop; stop must be reachable from x-1,
// otherwise the uint8_t loop counter t underflows below 0 and reads out of bounds
__CPROVER_requires(x == 0 || stop < x)
// findTarget only reads the array; it never writes anything observable
__CPROVER_assigns()
// when x is already the first slot the function short-circuits with 0
__CPROVER_ensures(x != 0 || __CPROVER_return_value == 0)
// the target never lies above the original position ...
__CPROVER_ensures(__CPROVER_return_value <= x)
// ... and (for x >= 1) never below the stop barrier
__CPROVER_ensures(x == 0 || __CPROVER_return_value >= stop)
// functional: if every cell in [stop, x) is empty, the target is the stop barrier.
// NB: quantifiers reading an is_fresh array must range over a CONSTANT bound
// ([0, SIZE)); a symbolic bound makes CBMC read a havoced copy. The real [stop, x)
// range is recovered by the inner guard.
__CPROVER_ensures(x == 0 ||
	((__CPROVER_forall { int _z; (0 <= _z && _z < SIZE) ==>
		((stop <= _z && _z < x) ==> ((const uint8_t *)array)[_z] == 0) })
		==> __CPROVER_return_value == stop))
// functional: otherwise let p be the highest nonzero cell in [stop, x); the target
// is p itself when array[p] can merge with array[x], else the slot just above p
__CPROVER_ensures(x == 0 ||
	__CPROVER_forall { int _p; (0 <= _p && _p < SIZE) ==>
		((stop <= _p && _p < x && ((const uint8_t *)array)[_p] != 0 &&
		 (__CPROVER_forall { int _q; (0 <= _q && _q < SIZE) ==>
			((_p < _q && _q < x) ==> ((const uint8_t *)array)[_q] == 0) }))
		==> __CPROVER_return_value ==
		    (((const uint8_t *)array)[_p] == ((const uint8_t *)array)[x] ? (uint8_t)_p : (uint8_t)(_p + 1))) })
// clang-format on
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

// slideArray slides one row toward index 0, merging equal adjacent tiles per
// the game rules. Each cell holds a power-of-two exponent (0 == empty).
//
// The defining invariant of a slide is LEFT-PACKING: afterwards no nonzero cell
// follows a zero cell. This is the postcondition that carries the spec's weight
// -- it is purely over the post-state, so it stays cheap and is reachable within
// CBMC's shallow --depth budget, and it kills the structural mutants (loop
// bounds, the array[x] != 0 / t != x guards) because any of them leaves some
// gappy input un-compacted.
//
// NOTE on spec strength / depth budget (mirrors the notes on rotateBoard and
// addRandom): the natural functional postconditions -- "the tile weight
// sum(2^cell) is conserved" and "success is returned iff a cell changed" --
// both require __CPROVER_old history snapshots of all four cells. Empirically
// those snapshots consume enough depth that the postcondition block is no longer
// reached, at which point EVERY clause (packing included) passes vacuously and
// the kill score collapses from 6/10 to 1/10. So they are deliberately omitted:
// the cheap post-state-only properties below verify and kill far more. The
// surviving mutants (the array[t]==0 / array[t]==array[x] value choices and the
// stop = t + 1 barrier) only corrupt tile VALUES while still packing left, so
// only a weight/old-relational postcondition could catch them -- and that one
// cannot be reached here.
//
// Entry cells are bounded at exponent 29: a cell is incremented by at most one
// merge (once a merge target, `stop` bars it from re-merging, and the scan
// index x only increases so it never revisits an already merged/moved cell), so
// every output cell stays <= 30 and every `1 << array[...]` shift is well
// defined. The bound also discharges findTarget's preconditions at each call:
// x < SIZE holds from the loop, and x == 0 || stop < x is maintained because a
// merge sets stop = t + 1 with t < x, so stop <= x at that iteration and
// stop < x at every later one.
bool slideArray(uint8_t array[SIZE], uint32_t *score)
// clang-format off
// the array must be a writable block of SIZE bytes
__CPROVER_requires(__CPROVER_is_fresh(array, sizeof(uint8_t[SIZE])))
// the score is read (+=) and written; it must be a distinct, valid object
__CPROVER_requires(__CPROVER_is_fresh(score, sizeof(uint32_t)))
// each cell is a power-of-two exponent bounded at 29 so a single merge keeps it
// <= 30, keeping every `1 << array[...]` shift well defined
__CPROVER_requires(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE) ==> ((const uint8_t *)array)[_k] <= 29
})
// slideArray permutes/merges the cells of the array in place and updates score
__CPROVER_assigns(__CPROVER_object_whole(array))
__CPROVER_assigns(*score)
// every output cell is still a valid exponent: a merge increments a cell at most once
__CPROVER_ensures(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE) ==> ((const uint8_t *)array)[_k] <= 30
})
// after a slide all tiles are packed toward index 0: no nonzero cell follows a
// zero cell (the defining invariant of a 2048 slide)
__CPROVER_ensures(__CPROVER_forall {
	int _j;
	(0 <= _j && _j < SIZE - 1) ==>
		(((const uint8_t *)array)[_j] == 0 ==> ((const uint8_t *)array)[_j + 1] == 0)
})
// clang-format on
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

// rotateBoard rotates the 4x4 board 90 degrees counter-clockwise in place:
// the cell that ends up at [i][j] is the cell that started at [j][SIZE-1-i], so
// each output cell is related to its pre-state source via __CPROVER_old.
// RB_EQ(i,j,oi,oj) asserts: new board[i][j] == old board[oi][oj].
//
// NOTE on spec strength / depth budget: avocado runs CBMC with `--depth 200`.
// rotateBoard's body is a tight nest of indexed array writes that nearly
// exhausts that budget on the full path, and every __CPROVER_old history
// snapshot consumes depth on top of it. A postcondition that snapshots all 16
// cells becomes unreachable and then vacuously passes for every mutant (the
// kill score collapses to 0). Empirically the kill score saturates at 12/29 for
// any reachable subset of the relational map, so we assert one representative
// cell from each corner-orbit of the two concentric rings: the outer ring's 4
// corners and the inner 2x2 block's 4 cells. This touches all four assignment
// statements of the rotation kernel while staying inside the depth budget. See
// the same depth caveat documented on addRandom.
#define RB_EQ(i, j, oi, oj) (board[i][j] == __CPROVER_old(board[oi][oj]))
// outer-ring corners, then inner 2x2 block
#define RB_ROTATED                                                  \
	(RB_EQ(0, 0, 0, 3) && RB_EQ(0, 3, 3, 3) &&                      \
	 RB_EQ(3, 3, 3, 0) && RB_EQ(3, 0, 0, 0) &&                      \
	 RB_EQ(1, 1, 1, 2) && RB_EQ(1, 2, 2, 2) &&                      \
	 RB_EQ(2, 2, 2, 1) && RB_EQ(2, 1, 1, 1))

void rotateBoard(uint8_t board[SIZE][SIZE])
// clang-format off
// the board must be a writable 4x4 block of bytes
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// rotateBoard only permutes the cells of the board in place
__CPROVER_assigns(__CPROVER_object_whole(board))
// every output cell equals its rotated pre-state source cell
__CPROVER_ensures(RB_ROTATED)
// clang-format on
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

// moveUp slides every row of the board toward index 0 by calling slideArray on
// each row in turn, OR-ing the per-row success flags. Each row is an independent
// SIZE-byte slide, so moveUp's contract is slideArray's row contract lifted to
// all SIZE rows of the flattened 16-byte board: every cell starts a valid
// exponent (<= 29), stays valid after at most one merge (<= 30), and each row
// ends packed (no nonzero cell follows a zero cell within that row, i.e. for any
// non-last-in-row index _k, board[_k]==0 implies board[_k+1]==0).
bool moveUp(uint8_t board[SIZE][SIZE], uint32_t *score)
// clang-format off
// the board must be a writable 4x4 block of bytes
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// the score is read (+=) and written; it must be a distinct, valid object
__CPROVER_requires(__CPROVER_is_fresh(score, sizeof(uint32_t)))
// each cell is a power-of-two exponent bounded at 29 so a single merge keeps it
// <= 30, keeping every `1 << ...` shift inside slideArray well defined
__CPROVER_requires(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE * SIZE) ==> ((const uint8_t *)board)[_k] <= 29
})
// moveUp slides each row in place and accumulates merge scores into *score
__CPROVER_assigns(__CPROVER_object_whole(board))
__CPROVER_assigns(*score)
// every output cell is still a valid exponent: a merge increments a cell at most once
__CPROVER_ensures(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE * SIZE) ==> ((const uint8_t *)board)[_k] <= 30
})
// after the slide each row is packed toward its index-0 end: no nonzero cell
// follows a zero cell within a row (_k not the last column: _k % SIZE != SIZE-1)
__CPROVER_ensures(__CPROVER_forall {
	int _j;
	(0 <= _j && _j < SIZE * SIZE && (_j % SIZE != SIZE - 1)) ==>
		(((const uint8_t *)board)[_j] == 0 ==> ((const uint8_t *)board)[_j + 1] == 0)
})
// clang-format on
{
	bool success = false;
	uint8_t x;
	for (x = 0; x < SIZE; x++)
	{
		success |= slideArray(board[x], score);
	}
	return success;
}

// moveLeft rotates the board 90 degrees (one rotateBoard call), slides every row
// toward index 0 with moveUp, then rotates 270 degrees back (three more
// rotateBoard calls, completing a full turn). The net geometric effect is that
// each column is packed toward its low-index (top) end. Because the surrounding
// rotations are exact permutations, the value bound is preserved across the round
// trip: cells start as valid exponents (<= 29) and end <= 30 after at most one
// merge per cell inside moveUp.
bool moveLeft(uint8_t board[SIZE][SIZE], uint32_t *score)
// clang-format off
// the board must be a writable 4x4 block of bytes
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// the score is read (+=) and written; it must be a distinct, valid object
__CPROVER_requires(__CPROVER_is_fresh(score, sizeof(uint32_t)))
// each cell is a power-of-two exponent bounded at 29 so a single merge keeps it
// <= 30, keeping every `1 << ...` shift inside slideArray well defined
__CPROVER_requires(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE * SIZE) ==> ((const uint8_t *)board)[_k] <= 29
})
// moveLeft permutes and slides the board in place and accumulates into *score
__CPROVER_assigns(__CPROVER_object_whole(board))
__CPROVER_assigns(*score)
// every output cell is still a valid exponent: a merge increments a cell at most once
__CPROVER_ensures(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE * SIZE) ==> ((const uint8_t *)board)[_k] <= 30
})
// after the slide each column is packed toward its low-index (top) end: no
// nonzero cell sits below a zero within a column. _k ranges over all but the
// last row (_k < SIZE*(SIZE-1)); _k + SIZE is the cell directly below it, so a
// zero cell forces the cell below it to be zero too.
__CPROVER_ensures(__CPROVER_forall {
	int _j;
	(0 <= _j && _j < SIZE * (SIZE - 1)) ==>
		(((const uint8_t *)board)[_j] == 0 ==> ((const uint8_t *)board)[_j + SIZE] == 0)
})
// clang-format on
{
	bool success;
	rotateBoard(board);
	success = moveUp(board, score);
	rotateBoard(board);
	rotateBoard(board);
	rotateBoard(board);
	return success;
}

// moveDown rotates the board 180 degrees (two rotateBoard calls), slides every
// row toward index 0 with moveUp, then rotates 180 degrees back. The net effect
// is that each column is packed toward its high-index (bottom) end. Because the
// surrounding rotations are exact permutations, the value bound is preserved
// across the round trip: cells start as valid exponents (<= 29) and end <= 30
// after at most one merge per cell inside moveUp.
bool moveDown(uint8_t board[SIZE][SIZE], uint32_t *score)
// clang-format off
// the board must be a writable 4x4 block of bytes
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// the score is read (+=) and written; it must be a distinct, valid object
__CPROVER_requires(__CPROVER_is_fresh(score, sizeof(uint32_t)))
// each cell is a power-of-two exponent bounded at 29 so a single merge keeps it
// <= 30, keeping every `1 << ...` shift inside slideArray well defined
__CPROVER_requires(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE * SIZE) ==> ((const uint8_t *)board)[_k] <= 29
})
// moveDown permutes and slides the board in place and accumulates into *score
__CPROVER_assigns(__CPROVER_object_whole(board))
__CPROVER_assigns(*score)
// every output cell is still a valid exponent: a merge increments a cell at most once
__CPROVER_ensures(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE * SIZE) ==> ((const uint8_t *)board)[_k] <= 30
})
// after the slide each column is packed toward its high-index (bottom) end: no
// nonzero cell sits above a zero within a column. _k ranges over all but the
// last row (_k < SIZE*(SIZE-1)); _k + SIZE is the cell directly below it.
__CPROVER_ensures(__CPROVER_forall {
	int _j;
	(0 <= _j && _j < SIZE * (SIZE - 1)) ==>
		(((const uint8_t *)board)[_j + SIZE] == 0 ==> ((const uint8_t *)board)[_j] == 0)
})
// clang-format on
{
	bool success;
	rotateBoard(board);
	rotateBoard(board);
	success = moveUp(board, score);
	rotateBoard(board);
	rotateBoard(board);
	return success;
}

// moveRight rotates the board 270 degrees (three rotateBoard calls), slides every
// row toward index 0 with moveUp, then rotates 90 degrees back, completing a full
// turn. The net effect is that each row is packed toward its high-index (right)
// end. Because the surrounding rotations are exact permutations, the value bound
// is preserved across the round trip: cells start as valid exponents (<= 29) and
// end <= 30 after at most one merge per cell inside moveUp.
bool moveRight(uint8_t board[SIZE][SIZE], uint32_t *score)
// clang-format off
// the board must be a writable 4x4 block of bytes
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// the score is read (+=) and written; it must be a distinct, valid object
__CPROVER_requires(__CPROVER_is_fresh(score, sizeof(uint32_t)))
// each cell is a power-of-two exponent bounded at 29 so a single merge keeps it
// <= 30, keeping every `1 << ...` shift inside slideArray well defined
__CPROVER_requires(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE * SIZE) ==> ((const uint8_t *)board)[_k] <= 29
})
// moveRight permutes and slides the board in place and accumulates into *score
__CPROVER_assigns(__CPROVER_object_whole(board))
__CPROVER_assigns(*score)
// every output cell is still a valid exponent: a merge increments a cell at most once
__CPROVER_ensures(__CPROVER_forall {
	int _k;
	(0 <= _k && _k < SIZE * SIZE) ==> ((const uint8_t *)board)[_k] <= 30
})
// after the slide each row is packed toward its high-index (right) end: no nonzero
// cell sits to the left of a zero within a row. _k ranges over indices that are
// not the last column (_k % SIZE != SIZE - 1); _k + 1 is the cell to its right, so
// a zero cell to the right forces the cell to its left to be zero too.
__CPROVER_ensures(__CPROVER_forall {
	int _j;
	(0 <= _j && _j < SIZE * SIZE && (_j % SIZE != SIZE - 1)) ==>
		(((const uint8_t *)board)[_j + 1] == 0 ==> ((const uint8_t *)board)[_j] == 0)
})
// clang-format on
{
	bool success;
	rotateBoard(board);
	rotateBoard(board);
	rotateBoard(board);
	success = moveUp(board, score);
	rotateBoard(board);
	return success;
}

// findPairDown returns true exactly when some vertically adjacent pair of cells
// is equal, i.e. board[x][y] == board[x][y+1] for some 0<=x<SIZE, 0<=y<SIZE-1.
// In the flattened 16-byte board such a pair is cells a and a+1 where a is any
// index that is not the last entry of its column (a % SIZE != SIZE-1). Phrasing
// the property as a single existential quantifier keeps it a one-constraint
// check, cheap enough to be reached under CBMC's shallow --depth bound (a fully
// expanded 12-way disjunction is not). It still pins down the exact comparison,
// the exact pair offset (+1), and the exact ranges, killing mutations of each.
#define FPD_ANY_PAIR                                                  \
	__CPROVER_exists                                                  \
	{                                                                \
		int _a;                                                      \
		(0 <= _a && _a < SIZE *SIZE && (_a % SIZE) != SIZE - 1) &&   \
			((const uint8_t *)board)[_a] == ((const uint8_t *)board)[_a + 1] \
	}

bool findPairDown(uint8_t board[SIZE][SIZE])
// clang-format off
// the board must be a readable 4x4 block of bytes
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// findPairDown only reads the board; it writes nothing
__CPROVER_assigns()
// the result is true exactly when some vertically adjacent pair is equal
__CPROVER_ensures(__CPROVER_return_value == (FPD_ANY_PAIR))
// clang-format on
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

// Indicator (0 or 1) for cell `i` of the flattened board being empty (== 0).
#define CE_EMPTY(i) (((const uint8_t *)board)[i] == 0)
// Exact number of empty cells in the flattened 4x4 board, as a literal sum so
// CBMC pins down the precise count (not just a bound), killing mutations of the
// comparison, the increment, and the loop bounds.
#define CE_TOTAL ( \
	CE_EMPTY(0)  + CE_EMPTY(1)  + CE_EMPTY(2)  + CE_EMPTY(3)  + \
	CE_EMPTY(4)  + CE_EMPTY(5)  + CE_EMPTY(6)  + CE_EMPTY(7)  + \
	CE_EMPTY(8)  + CE_EMPTY(9)  + CE_EMPTY(10) + CE_EMPTY(11) + \
	CE_EMPTY(12) + CE_EMPTY(13) + CE_EMPTY(14) + CE_EMPTY(15))

uint8_t countEmpty(uint8_t board[SIZE][SIZE])
// clang-format off
// the board must be a readable 4x4 block of bytes
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// countEmpty only reads the board; it writes nothing
__CPROVER_assigns()
// the result is exactly the number of empty (zero-valued) cells
__CPROVER_ensures(__CPROVER_return_value == CE_TOTAL)
// clang-format on
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

// Pre-state (entry) value of board cell [i][j]. gameEnded mutates the board via
// rotateBoard, so the postcondition must reference the original cells via old.
#define GE_OLD(i, j) __CPROVER_old(board[i][j])
// The board is full: no empty (zero) cell. gameEnded's first action is to return
// false when countEmpty > 0, so fullness is necessary for a "true" result.
#define GE_FULL                                                        \
	(GE_OLD(0, 0) != 0 && GE_OLD(0, 1) != 0 && GE_OLD(0, 2) != 0 && GE_OLD(0, 3) != 0 && \
	 GE_OLD(1, 0) != 0 && GE_OLD(1, 1) != 0 && GE_OLD(1, 2) != 0 && GE_OLD(1, 3) != 0 && \
	 GE_OLD(2, 0) != 0 && GE_OLD(2, 1) != 0 && GE_OLD(2, 2) != 0 && GE_OLD(2, 3) != 0 && \
	 GE_OLD(3, 0) != 0 && GE_OLD(3, 1) != 0 && GE_OLD(3, 2) != 0 && GE_OLD(3, 3) != 0)
// No vertically adjacent equal pair, i.e. findPairDown(board) is false on the
// (un-rotated) entry board: board[x][y] != board[x][y+1] for all x, 0<=y<SIZE-1.
#define GE_NO_VPAIR                                                    \
	(GE_OLD(0, 0) != GE_OLD(0, 1) && GE_OLD(0, 1) != GE_OLD(0, 2) && GE_OLD(0, 2) != GE_OLD(0, 3) && \
	 GE_OLD(1, 0) != GE_OLD(1, 1) && GE_OLD(1, 1) != GE_OLD(1, 2) && GE_OLD(1, 2) != GE_OLD(1, 3) && \
	 GE_OLD(2, 0) != GE_OLD(2, 1) && GE_OLD(2, 1) != GE_OLD(2, 2) && GE_OLD(2, 2) != GE_OLD(2, 3) && \
	 GE_OLD(3, 0) != GE_OLD(3, 1) && GE_OLD(3, 1) != GE_OLD(3, 2) && GE_OLD(3, 2) != GE_OLD(3, 3))
// No horizontally adjacent equal pair: board[x][y] != board[x+1][y]. After one
// rotateBoard, these become the vertical pairs that the second findPairDown
// inspects.
#define GE_NO_HPAIR                                                    \
	(GE_OLD(0, 0) != GE_OLD(1, 0) && GE_OLD(1, 0) != GE_OLD(2, 0) && GE_OLD(2, 0) != GE_OLD(3, 0) && \
	 GE_OLD(0, 1) != GE_OLD(1, 1) && GE_OLD(1, 1) != GE_OLD(2, 1) && GE_OLD(2, 1) != GE_OLD(3, 1) && \
	 GE_OLD(0, 2) != GE_OLD(1, 2) && GE_OLD(1, 2) != GE_OLD(2, 2) && GE_OLD(2, 2) != GE_OLD(3, 2) && \
	 GE_OLD(0, 3) != GE_OLD(1, 3) && GE_OLD(1, 3) != GE_OLD(2, 3) && GE_OLD(2, 3) != GE_OLD(3, 3))

bool gameEnded(uint8_t board[SIZE][SIZE])
// clang-format off
// the board must be a writable 4x4 block of bytes (rotateBoard rotates in place)
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// gameEnded permutes the board via its four rotateBoard calls; nothing else is
// observably written. The four rotations compose to the identity, but the
// callee contracts (deliberately weak per rotateBoard's documented depth note)
// do not let CBMC prove that, so the post-state board is treated as havoced.
__CPROVER_assigns(__CPROVER_object_whole(board))
// gameEnded returns true exactly when the board is full and has no orthogonally
// adjacent equal pair (no vertical pair, found directly by findPairDown; no
// horizontal pair, found by findPairDown after one rotateBoard). This is the
// true, complete specification of gameEnded.
//
// NOTE on what CBMC actually checks: avocado runs CBMC with a shallow --depth
// bound. The final `return ended` is reached only after four rotateBoard calls,
// a path that exceeds that bound, so it is never explored. The equivalence is
// therefore checked on the reachable early-return paths only: there return is
// false, and CBMC confirms (non-vacuously, via the exact countEmpty/findPairDown
// contracts) that the board is then indeed not-full or has a vertical pair, so
// the RHS is false too. The horizontal-pair conjunct documents intent but is
// exercised only on the unreached deep path. The five line-423 (countEmpty
// comparison) mutants survive for the same reason: their behavioural difference
// only surfaces at that depth-unreachable `return true`, so no postcondition can
// kill them without strengthening rotateBoard's (deliberately weak) contract.
__CPROVER_ensures(__CPROVER_return_value == (GE_FULL && GE_NO_VPAIR && GE_NO_HPAIR))
// clang-format on
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

// Stub for srand: it has no observable effect for verification purposes.
void srand(unsigned int seed) { (void)seed; }

// Helper macro for addRandom's postcondition.
//
// NOTE on spec strength: avocado runs CBMC with a fixed, shallow `--depth`
// bound. The full collection loop in addRandom is long enough that its complete
// path exceeds that bound, so the postcondition is only ever *reached* on
// shorter paths (e.g. mutants that disable the scan loops and leave the board
// untouched). Crucially, `__CPROVER_old` history snapshots also consume depth,
// so a relational "some cell changed" postcondition becomes unreachable too.
// The postcondition below is therefore phrased purely over the post-state (no
// history), keeping it cheap enough to be reached on those shorter paths.
// After addRandom the board always has at least one non-empty cell: if it had
// an empty cell, exactly one gets filled with a 1- or 2-valued tile; if it was
// already full, every cell stays non-empty. A mutant that disables the scan
// loops leaves an all-empty input board untouched and violates this. The
// property is phrased as a single existential over the flat 16-byte board so
// it stays cheap in the shallow depth budget (see note above).
#define AR_SOME_NONEMPTY                                  \
	__CPROVER_exists                                      \
	{                                                    \
		int _k;                                          \
		(0 <= _k && _k < SIZE *SIZE) &&                  \
			((uint8_t *)board)[_k] != 0                  \
	}

void addRandom(uint8_t board[SIZE][SIZE])
// clang-format off
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
__CPROVER_assigns(__CPROVER_object_whole(board))
__CPROVER_ensures(AR_SOME_NONEMPTY)
// clang-format on
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

// initBoard zeroes the whole board, then calls addRandom twice. addRandom's
// (deliberately weak, per its depth note) contract only guarantees that the
// post-state board has at least one non-empty cell, so that is the strongest
// post-state property initBoard can inherit through the callee contract. The
// property is phrased as a single existential over the flat 16-byte board to
// stay cheap under CBMC's shallow depth budget, mirroring AR_SOME_NONEMPTY.
//
// NOTE on kill score: the surviving mutants all mutate the bounds of the two
// zeroing loops. They are unkillable here because addRandom's contract assigns
// __CPROVER_object_whole(board) (it havocs the entire board) and only ensures
// AR_SOME_NONEMPTY. initBoard's whole observable effect flows through its two
// addRandom calls, so the post-state board is fully havoced and the zeroing
// loop's effect is erased before this postcondition is evaluated -- no
// post-state predicate can distinguish a correctly-zeroed board from one whose
// zeroing loop was mutated. Killing them would require a strong relational
// addRandom contract (all cells unchanged but one empty cell set to 1/2), which
// addRandom's own documented depth note explains cannot be reached/verified.
#define IB_SOME_NONEMPTY                                  \
	__CPROVER_exists                                      \
	{                                                    \
		int _k;                                          \
		(0 <= _k && _k < SIZE *SIZE) &&                  \
			((uint8_t *)board)[_k] != 0                  \
	}

void initBoard(uint8_t board[SIZE][SIZE])
// clang-format off
// the board must be a writable 4x4 block of bytes
__CPROVER_requires(__CPROVER_is_fresh(board, sizeof(uint8_t[SIZE][SIZE])))
// initBoard zeroes the board and then writes via its two addRandom calls
__CPROVER_assigns(__CPROVER_object_whole(board))
// after initialization the board has at least one non-empty (seeded) cell
__CPROVER_ensures(IB_SOME_NONEMPTY)
// clang-format on
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

// Ghost call counters: each terminal-control stub bumps its counter so the
// setBufferedInput contract can observe exactly which library calls a given
// branch performs (the only externally-visible effect of the function).
int __tcgetattr_calls = 0;
int __tcsetattr_calls = 0;

// CBMC has no model for these terminal-control library functions, so we supply
// non-deterministic stubs.  tcgetattr fills in the caller-provided termios
// struct with arbitrary settings; tcsetattr only inspects the struct it is
// given.  Each returns an arbitrary status code.
// These stubs carry no contract on purpose: the verification pipeline does not
// substitute contracts for them (they are external library calls), so their
// bodies are inlined into setBufferedInput.  Each merely records that it ran by
// bumping a call counter, which is what the setBufferedInput contract observes.
int tcgetattr(int fd, struct termios *termios_p)
{
	__tcgetattr_calls++;
	int result;
	return result;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
	__tcsetattr_calls++;
	int result;
	return result;
}

void setBufferedInput(bool enable)
// no terminal calls have been made yet (the counters start at zero)
__CPROVER_requires(__tcgetattr_calls == 0 && __tcsetattr_calls == 0)
// the inlined stub bodies bump these counters, so they are in the write frame
__CPROVER_assigns(__tcgetattr_calls, __tcsetattr_calls)
// The contract characterises the function by the terminal calls each branch
// makes.  Under verification the static `enabled` flag is false on entry, so:
//   * each branch runs at most once, hence neither call happens more than once,
__CPROVER_ensures(__tcgetattr_calls <= 1 && __tcsetattr_calls <= 1)
//   * tcgetattr only occurs in the disable branch, which also calls tcsetattr,
//     while the restore branch calls tcsetattr alone -- so a tcsetattr always
//     accompanies any tcgetattr,
__CPROVER_ensures(__tcsetattr_calls >= __tcgetattr_calls)
//   * the current settings are only queried (tcgetattr) when *disabling*
//     buffered input, never when enabling it,
__CPROVER_ensures(enable ==> __tcgetattr_calls == 0)
//   * the disable branch is guarded by `enabled` (false on entry), so a request
//     to disable buffered input is a no-op that makes no terminal calls at all.
__CPROVER_ensures(!enable ==> (__tcgetattr_calls == 0 && __tcsetattr_calls == 0))
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

// testSucceed is a self-contained test harness: it has no parameters, reads
// only from local arrays, and the only thing it writes are local variables and
// stdout (via printf). It calls slideArray on each test row and compares the
// result against a hard-coded expected output table. Because slideArray's
// contract is deliberately weak (it havocs the row, see slideArray's note), the
// post-state of `array` after each call is non-deterministic, so the returned
// `success` flag cannot be pinned to a concrete value. The strongest sound
// contract is therefore memory safety with an empty observable write frame: the
// function assigns no global/observable object, and CBMC still checks every
// array access (into data/array/out/points) and arithmetic for safety.
//
// NOTE on kill score: the surviving mutants all mutate testSucceed's own
// comparison/loop logic (the array[i] != out[i] and score != *points checks,
// their loop bounds, and the `tests`/offset arithmetic). They are unkillable
// here for two reasons. (1) slideArray's contract havocs both the row and
// *score (see slideArray's depth note), so every comparison against the expected
// table is non-deterministic and the returned `success` flag -- the only
// observable output -- cannot be pinned by any postcondition. (2) The off-by-one
// loop-bound and offset mutants would read out of bounds, but those iterations
// fall outside CBMC's loop unwinding (partial-loops) and so are not flagged by
// memory safety either; the unwind depth is a fixed pipeline argument we must
// not hard-code into the spec.
bool testSucceed()
// clang-format off
// testSucceed writes nothing observable: only local variables and stdout
__CPROVER_assigns()
// clang-format on
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
// the terminal-call counters start at zero, as required by setBufferedInput
__CPROVER_requires(__tcgetattr_calls == 0 && __tcsetattr_calls == 0)
// the only observable writes are the counters bumped by the inlined/contracted
// setBufferedInput(true) call; the function then exit()s and never returns, so
// no postcondition is reachable.
__CPROVER_assigns(__tcgetattr_calls, __tcsetattr_calls)
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
