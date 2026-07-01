// CBMC proof harnesses for 2048.c contract enforcement.
// Each harness sets up nondeterministic arguments and calls one function.
// The function's __CPROVER_requires clause (assumed during enforcement)
// constrains those arguments; __CPROVER_ensures/__CPROVER_assigns are checked.
#include <stdint.h>
#include <stdbool.h>

#define SIZE 4

void getColors(uint8_t value, uint8_t scheme, uint8_t *foreground, uint8_t *background);
uint8_t getDigitCount(uint32_t number);
void drawBoard(uint8_t board[SIZE][SIZE], uint8_t scheme, uint32_t score);
uint8_t findTarget(uint8_t array[SIZE], uint8_t x, uint8_t stop);
bool slideArray(uint8_t array[SIZE], uint32_t *score);
void rotateBoard(uint8_t board[SIZE][SIZE]);
bool moveUp(uint8_t board[SIZE][SIZE], uint32_t *score);
bool moveLeft(uint8_t board[SIZE][SIZE], uint32_t *score);
bool moveDown(uint8_t board[SIZE][SIZE], uint32_t *score);
bool moveRight(uint8_t board[SIZE][SIZE], uint32_t *score);
bool findPairDown(uint8_t board[SIZE][SIZE]);
uint8_t countEmpty(uint8_t board[SIZE][SIZE]);
bool gameEnded(uint8_t board[SIZE][SIZE]);
void addRandom(uint8_t board[SIZE][SIZE]);
void initBoard(uint8_t board[SIZE][SIZE]);
bool testSucceed(void);

void harness_getColors(void)
{
	uint8_t value, scheme, *fg, *bg;
	getColors(value, scheme, fg, bg);
}

void harness_getDigitCount(void)
{
	uint32_t n;
	uint8_t r = getDigitCount(n);
}

void harness_drawBoard(void)
{
	uint8_t (*board)[SIZE];
	uint8_t scheme;
	uint32_t score;
	drawBoard(board, scheme, score);
}

void harness_findTarget(void)
{
	uint8_t (*array);
	uint8_t x, stop;
	uint8_t r = findTarget(array, x, stop);
}

void harness_slideArray(void)
{
	uint8_t (*array);
	uint32_t *score;
	bool r = slideArray(array, score);
}

void harness_rotateBoard(void)
{
	uint8_t (*board)[SIZE];
	rotateBoard(board);
}

void harness_moveUp(void)
{
	uint8_t (*board)[SIZE];
	uint32_t *score;
	bool r = moveUp(board, score);
}

void harness_moveLeft(void)
{
	uint8_t (*board)[SIZE];
	uint32_t *score;
	bool r = moveLeft(board, score);
}

void harness_moveDown(void)
{
	uint8_t (*board)[SIZE];
	uint32_t *score;
	bool r = moveDown(board, score);
}

void harness_moveRight(void)
{
	uint8_t (*board)[SIZE];
	uint32_t *score;
	bool r = moveRight(board, score);
}

void harness_findPairDown(void)
{
	uint8_t (*board)[SIZE];
	bool r = findPairDown(board);
}

void harness_countEmpty(void)
{
	uint8_t (*board)[SIZE];
	uint8_t r = countEmpty(board);
}

void harness_gameEnded(void)
{
	uint8_t (*board)[SIZE];
	bool r = gameEnded(board);
}

void harness_addRandom(void)
{
	uint8_t (*board)[SIZE];
	addRandom(board);
}

void harness_initBoard(void)
{
	uint8_t (*board)[SIZE];
	initBoard(board);
}

void harness_testSucceed(void)
{
	bool r = testSucceed();
}
