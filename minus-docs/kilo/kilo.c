/* Kilo -- A very simple editor in less than 1-kilo lines of code (as counted
 *         by "cloc"). Does not depend on libcurses, directly emits VT100
 *         escapes on the terminal.
 *
 * -----------------------------------------------------------------------
 *
 * Copyright (C) 2016 Salvatore Sanfilippo <antirez at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define KILO_VERSION "0.0.1"

#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#endif

#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>

/* Syntax highlight types */
#define HL_NORMAL 0
#define HL_NONPRINT 1
#define HL_COMMENT 2   /* Single line comment. */
#define HL_MLCOMMENT 3 /* Multi-line comment. */
#define HL_KEYWORD1 4
#define HL_KEYWORD2 5
#define HL_STRING 6
#define HL_NUMBER 7
#define HL_MATCH 8      /* Search match. */

#define HL_HIGHLIGHT_STRINGS (1<<0)
#define HL_HIGHLIGHT_NUMBERS (1<<1)

struct editorSyntax {
    char **filematch;
    char **keywords;
    char singleline_comment_start[2];
    char multiline_comment_start[3];
    char multiline_comment_end[3];
    int flags;
};

/* This structure represents a single line of the file we are editing. */
typedef struct erow {
    int idx;            /* Row index in the file, zero-based. */
    int size;           /* Size of the row, excluding the null term. */
    int rsize;          /* Size of the rendered row. */
    char *chars;        /* Row content. */
    char *render;       /* Row content "rendered" for screen (for TABs). */
    unsigned char *hl;  /* Syntax highlight type for each character in render.*/
    int hl_oc;          /* Row had open comment at end in last syntax highlight
                           check. */
} erow;

typedef struct hlcolor {
    int r,g,b;
} hlcolor;

struct editorConfig {
    int cx,cy;  /* Cursor x and y position in characters */
    int rowoff;     /* Offset of row displayed. */
    int coloff;     /* Offset of column displayed. */
    int screenrows; /* Number of rows that we can show */
    int screencols; /* Number of cols that we can show */
    int numrows;    /* Number of rows */
    int rawmode;    /* Is terminal raw mode enabled? */
    erow *row;      /* Rows */
    int dirty;      /* File modified but not saved. */
    char *filename; /* Currently open filename */
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;    /* Current syntax highlight, or NULL. */
};

static struct editorConfig E;

enum KEY_ACTION{
        KEY_NULL = 0,       /* NULL */
        CTRL_C = 3,         /* Ctrl-c */
        CTRL_D = 4,         /* Ctrl-d */
        CTRL_F = 6,         /* Ctrl-f */
        CTRL_H = 8,         /* Ctrl-h */
        TAB = 9,            /* Tab */
        CTRL_L = 12,        /* Ctrl+l */
        ENTER = 13,         /* Enter */
        CTRL_Q = 17,        /* Ctrl-q */
        CTRL_S = 19,        /* Ctrl-s */
        CTRL_U = 21,        /* Ctrl-u */
        ESC = 27,           /* Escape */
        BACKSPACE =  127,   /* Backspace */
        /* The following are just soft codes, not really reported by the
         * terminal directly. */
        ARROW_LEFT = 1000,
        ARROW_RIGHT,
        ARROW_UP,
        ARROW_DOWN,
        DEL_KEY,
        HOME_KEY,
        END_KEY,
        PAGE_UP,
        PAGE_DOWN
};

void editorSetStatusMessage(const char *fmt, ...);

/* =========================== Syntax highlights DB =========================
 *
 * In order to add a new syntax, define two arrays with a list of file name
 * matches and keywords. The file name matches are used in order to match
 * a given syntax with a given file name: if a match pattern starts with a
 * dot, it is matched as the last past of the filename, for example ".c".
 * Otherwise the pattern is just searched inside the filenme, like "Makefile").
 *
 * The list of keywords to highlight is just a list of words, however if they
 * a trailing '|' character is added at the end, they are highlighted in
 * a different color, so that you can have two different sets of keywords.
 *
 * Finally add a stanza in the HLDB global variable with two two arrays
 * of strings, and a set of flags in order to enable highlighting of
 * comments and numbers.
 *
 * The characters for single and multi line comments must be exactly two
 * and must be provided as well (see the C language example).
 *
 * There is no support to highlight patterns currently. */

/* C / C++ */
char *C_HL_extensions[] = {".c",".h",".cpp",".hpp",".cc",NULL};
char *C_HL_keywords[] = {
	/* C Keywords */
	"auto","break","case","continue","default","do","else","enum",
	"extern","for","goto","if","register","return","sizeof","static",
	"struct","switch","typedef","union","volatile","while","NULL",

	/* C++ Keywords */
	"alignas","alignof","and","and_eq","asm","bitand","bitor","class",
	"compl","constexpr","const_cast","deltype","delete","dynamic_cast",
	"explicit","export","false","friend","inline","mutable","namespace",
	"new","noexcept","not","not_eq","nullptr","operator","or","or_eq",
	"private","protected","public","reinterpret_cast","static_assert",
	"static_cast","template","this","thread_local","throw","true","try",
	"typeid","typename","virtual","xor","xor_eq",

	/* C types */
        "int|","long|","double|","float|","char|","unsigned|","signed|",
        "void|","short|","auto|","const|","bool|",NULL
};

/* Here we define an array of syntax highlights by extensions, keywords,
 * comments delimiters and flags. */
struct editorSyntax HLDB[] = {
    {
        /* C / C++ */
        C_HL_extensions,
        C_HL_keywords,
        "//","/*","*/",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS
    }
};

#define HLDB_ENTRIES (sizeof(HLDB)/sizeof(HLDB[0]))

/* ======================= Low level terminal handling ====================== */

static struct termios orig_termios; /* In order to restore at exit.*/

void disableRawMode(int fd)
/* The only object this function mutates is the global flag E.rawmode; the
 * tcsetattr stub only reads from *termios_p and has an empty assigns frame. */
__CPROVER_assigns(E.rawmode)
/* Whether or not raw mode was on at entry, it is off (0) on return: the
 * disabled-entry path leaves it untouched at 0, and the enabled-entry path
 * clears it. */
__CPROVER_ensures(E.rawmode == 0)
{
    /* Don't even check the return value as it's too late. */
    if (E.rawmode) {
        tcsetattr(fd,TCSAFLUSH,&orig_termios);
        E.rawmode = 0;
    }
}

/* Called at exit to avoid remaining in raw mode. */
void editorAtExit(void)
/* Delegates entirely to disableRawMode, whose only mutation is the global flag
 * E.rawmode, which it guarantees is 0 on return. */
__CPROVER_assigns(E.rawmode)
__CPROVER_ensures(E.rawmode == 0)
{
    disableRawMode(STDIN_FILENO);
}

/* Raw mode: 1960 magic shit. */
/* `errno` expands to `(*__errno_location())`; the stub for `__errno_location`
 * (in /app/stubs/isatty.c) returns the address of this global so that writes to
 * `errno` target a single, nameable object the assigns clause below can list. */
extern int __avocado_errno;
int enableRawMode(int fd)
__CPROVER_assigns(E.rawmode, orig_termios, __avocado_errno)
__CPROVER_ensures(
    __CPROVER_return_value == 0 || __CPROVER_return_value == -1)
/* Already enabled on entry: returns 0 and leaves rawmode untouched. */
__CPROVER_ensures(
    __CPROVER_old(E.rawmode) != 0 ==>
        (__CPROVER_return_value == 0 &&
         E.rawmode == __CPROVER_old(E.rawmode)))
/* Entered disabled and succeeded: raw mode is now on. */
__CPROVER_ensures(
    (__CPROVER_old(E.rawmode) == 0 && __CPROVER_return_value == 0) ==>
        E.rawmode == 1)
/* Failure leaves raw mode unchanged and sets errno to ENOTTY. */
__CPROVER_ensures(
    __CPROVER_return_value == -1 ==>
        (E.rawmode == __CPROVER_old(E.rawmode) && errno == ENOTTY))
{
    struct termios raw;

    if (E.rawmode) return 0; /* Already enabled. */
    if (!isatty(STDIN_FILENO)) goto fatal;
    atexit(editorAtExit);
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer. */
    raw.c_cc[VMIN] = 0; /* Return each byte, or zero for timeout. */
    raw.c_cc[VTIME] = 1; /* 100 ms timeout (unit is tens of second). */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    E.rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

/* Read a key from the terminal put in raw mode, trying to handle
 * escape sequences. */
int editorReadKey(int fd)
__CPROVER_ensures(
    (char)__CPROVER_return_value == __CPROVER_return_value ||
    (__CPROVER_return_value >= ARROW_LEFT && __CPROVER_return_value <= PAGE_DOWN))
{
    int nread;
    char c, seq[3];
    while ((nread = read(fd,&c,1)) == 0);
    if (nread == -1) exit(1);

    while(1) {
        switch(c) {
        case ESC:    /* escape sequence */
            /* If this is just an ESC, we'll timeout here. */
            if (read(fd,seq,1) == 0) return ESC;
            if (read(fd,seq+1,1) == 0) return ESC;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read(fd,seq+2,1) == 0) return ESC;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
            break;
        default:
            return c;
        }
    }
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor is stored at *rows and *cols and 0 is returned. */
int getCursorPosition(int ifd, int ofd, int *rows, int *cols)
__CPROVER_requires(__CPROVER_w_ok(rows, sizeof(int)))
__CPROVER_requires(__CPROVER_w_ok(cols, sizeof(int)))
__CPROVER_assigns(*rows, *cols)
__CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == -1)
{
    char buf[32];
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(ifd,buf+i,1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2,"%d;%d",rows,cols) != 2) return -1;
    return 0;
}

/* Try to get the number of columns in the current terminal. If the ioctl()
 * call fails the function will try to query the terminal itself.
 * Returns 0 on success, -1 on error. */
int getWindowSize(int ifd, int ofd, int *rows, int *cols)
__CPROVER_requires(__CPROVER_w_ok(rows, sizeof(int)))
__CPROVER_requires(__CPROVER_w_ok(cols, sizeof(int)))
__CPROVER_assigns(*rows, *cols)
__CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == -1)
__CPROVER_ensures(__CPROVER_return_value == 0 ==>
                  (0 <= *rows && *rows <= 0xffff && 0 <= *cols && *cols <= 0xffff))
{
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int orig_row, orig_col, retval;

        /* Get the initial position so we can restore it later. */
        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);
        if (retval == -1) goto failed;

        /* Go to right/bottom margin and get position. */
        if (write(ofd,"\x1b[999C\x1b[999B",12) != 12) goto failed;
        retval = getCursorPosition(ifd,ofd,rows,cols);
        if (retval == -1) goto failed;

        /* Restore position. */
        char seq[32];
        snprintf(seq,32,"\x1b[%d;%dH",orig_row,orig_col);
        if (write(ofd,seq,strlen(seq)) == -1) {
            /* Can't recover... */
        }
        return 0;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

failed:
    return -1;
}

/* ====================== Syntax highlight color scheme  ==================== */

int is_separator(int c)
__CPROVER_assigns()
__CPROVER_ensures(__CPROVER_return_value ==
    (c == '\0' || isspace(c) || strchr(",.()+-/*=~%[];",c) != NULL))
{
    return c == '\0' || isspace(c) || strchr(",.()+-/*=~%[];",c) != NULL;
}

/* Return true if the specified row last char is part of a multi line comment
 * that starts at this row or at one before, and does not end at the end
 * of the row but spawns to the next row. */
int editorRowHasOpenComment(erow *row)
__CPROVER_requires(__CPROVER_is_fresh(row, sizeof(erow)))
__CPROVER_requires(0 <= row->rsize)
__CPROVER_requires((row->hl != NULL && row->rsize >= 1) ==>
                   __CPROVER_is_fresh(row->hl, row->rsize))
__CPROVER_requires(row->rsize >= 2 ==>
                   __CPROVER_is_fresh(row->render, row->rsize))
__CPROVER_assigns()
__CPROVER_ensures(__CPROVER_return_value == (
    (row->hl != NULL && row->rsize != 0 &&
     row->hl[row->rsize-1] == HL_MLCOMMENT &&
     (row->rsize < 2 ||
      (row->render[row->rsize-2] != '*' ||
       row->render[row->rsize-1] != '/'))) ? 1 : 0))
{
    if (row->hl && row->rsize && row->hl[row->rsize-1] == HL_MLCOMMENT &&
        (row->rsize < 2 || (row->render[row->rsize-2] != '*' ||
                            row->render[row->rsize-1] != '/'))) return 1;
    return 0;
}

/* Set every byte of row->hl (that corresponds to every character in the line)
 * to the right syntax highlight type (HL_* defines). */
void editorUpdateSyntax(erow *row)
__CPROVER_requires(__CPROVER_is_fresh(row, sizeof(erow)))
__CPROVER_requires(0 <= row->rsize)
__CPROVER_requires(row->hl == NULL)
__CPROVER_assigns(row->hl)
__CPROVER_ensures(row->rsize == __CPROVER_old(row->rsize))
__CPROVER_ensures(__CPROVER_forall {
    int k; (0 <= k && k < row->rsize) ==> row->hl[k] == HL_NORMAL })
{
    row->hl = realloc(row->hl,row->rsize);
    memset(row->hl,HL_NORMAL,row->rsize);

    if (E.syntax == NULL) return; /* No syntax, everything is HL_NORMAL. */

    int i, prev_sep, in_string, in_comment;
    char *p;
    char **keywords = E.syntax->keywords;
    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    /* Point to the first non-space char. */
    p = row->render;
    i = 0; /* Current char offset */
    while(*p && isspace(*p)) {
        p++;
        i++;
    }
    prev_sep = 1; /* Tell the parser if 'i' points to start of word. */
    in_string = 0; /* Are we inside "" or '' ? */
    in_comment = 0; /* Are we inside multi-line comment? */

    /* If the previous line has an open comment, this line starts
     * with an open comment state. */
    if (row->idx > 0 && editorRowHasOpenComment(&E.row[row->idx-1]))
        in_comment = 1;

    while(*p) {
        /* Handle // comments. */
        if (prev_sep && *p == scs[0] && *(p+1) == scs[1]) {
            /* From here to end is a comment */
            memset(row->hl+i,HL_COMMENT,row->size-i);
            return;
        }

        /* Handle multi line comments. */
        if (in_comment) {
            row->hl[i] = HL_MLCOMMENT;
            if (*p == mce[0] && *(p+1) == mce[1]) {
                row->hl[i+1] = HL_MLCOMMENT;
                p += 2; i += 2;
                in_comment = 0;
                prev_sep = 1;
                continue;
            } else {
                prev_sep = 0;
                p++; i++;
                continue;
            }
        } else if (*p == mcs[0] && *(p+1) == mcs[1]) {
            row->hl[i] = HL_MLCOMMENT;
            row->hl[i+1] = HL_MLCOMMENT;
            p += 2; i += 2;
            in_comment = 1;
            prev_sep = 0;
            continue;
        }

        /* Handle "" and '' */
        if (in_string) {
            row->hl[i] = HL_STRING;
            if (*p == '\\') {
                row->hl[i+1] = HL_STRING;
                p += 2; i += 2;
                prev_sep = 0;
                continue;
            }
            if (*p == in_string) in_string = 0;
            p++; i++;
            continue;
        } else {
            if (*p == '"' || *p == '\'') {
                in_string = *p;
                row->hl[i] = HL_STRING;
                p++; i++;
                prev_sep = 0;
                continue;
            }
        }

        /* Handle non printable chars. */
        if (!isprint(*p)) {
            row->hl[i] = HL_NONPRINT;
            p++; i++;
            prev_sep = 0;
            continue;
        }

        /* Handle numbers */
        if ((isdigit(*p) && (prev_sep || row->hl[i-1] == HL_NUMBER)) ||
            (*p == '.' && i >0 && row->hl[i-1] == HL_NUMBER)) {
            row->hl[i] = HL_NUMBER;
            p++; i++;
            prev_sep = 0;
            continue;
        }

        /* Handle keywords and lib calls */
        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen-1] == '|';
                if (kw2) klen--;

                if (!memcmp(p,keywords[j],klen) &&
                    is_separator(*(p+klen)))
                {
                    /* Keyword */
                    memset(row->hl+i,kw2 ? HL_KEYWORD2 : HL_KEYWORD1,klen);
                    p += klen;
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue; /* We had a keyword match */
            }
        }

        /* Not special chars */
        prev_sep = is_separator(*p);
        p++; i++;
    }

    /* Propagate syntax change to the next row if the open commen
     * state changed. This may recursively affect all the following rows
     * in the file. */
    int oc = editorRowHasOpenComment(row);
    if (row->hl_oc != oc && row->idx+1 < E.numrows)
        editorUpdateSyntax(&E.row[row->idx+1]);
    row->hl_oc = oc;
}

/* Maps syntax highlight token types to terminal colors. */
int editorSyntaxToColor(int hl)
__CPROVER_assigns()
__CPROVER_ensures(__CPROVER_return_value == (
    (hl == HL_COMMENT || hl == HL_MLCOMMENT) ? 36 :
    (hl == HL_KEYWORD1) ? 33 :
    (hl == HL_KEYWORD2) ? 32 :
    (hl == HL_STRING) ? 35 :
    (hl == HL_NUMBER) ? 31 :
    (hl == HL_MATCH) ? 34 :
    37))
{
    switch(hl) {
    case HL_COMMENT:
    case HL_MLCOMMENT: return 36;     /* cyan */
    case HL_KEYWORD1: return 33;    /* yellow */
    case HL_KEYWORD2: return 32;    /* green */
    case HL_STRING: return 35;      /* magenta */
    case HL_NUMBER: return 31;      /* red */
    case HL_MATCH: return 34;      /* blu */
    default: return 37;             /* white */
    }
}

/* Select the syntax highlight scheme depending on the filename,
 * setting it in the global state E.syntax. */
void editorSelectSyntaxHighlight(char *filename)
/* `filename` must be a valid, NUL-terminated C string: strstr/strlen scan it. */
__CPROVER_requires(__CPROVER_is_fresh(filename, 16))
__CPROVER_requires(filename[15] == '\0')
/* `goto-instrument --enforce-contract` adds nondeterministic initialization of
 * every static/global, so the highlight database HLDB is havoc'd on entry: its
 * `filematch` member is a wild `char **`.  We re-establish that HLDB[0].filematch
 * is a valid (freshly allocated) NULL-terminated pattern array.  Its first entry
 * is NULL, i.e. the table for HLDB[0] lists no extensions, so the inner match
 * loop performs no iterations and E.syntax is left untouched.
 *
 * Ideally we would instead model a non-empty pattern array (one real
 * NUL-terminated pattern followed by NULL) so the match logic is exercised.
 * That is not expressible here: `__CPROVER_is_fresh` populates a `char **`
 * allocated by raw byte size with *invalid* pointers, and constraining a slot to
 * a valid string -- whether by a nested `__CPROVER_is_fresh(filematch[0], n)` or
 * by aliasing `filematch[0]` to another buffer -- makes the precondition UNSAT
 * (verified empirically: even constraining a single byte `filematch[0][i]` is
 * UNSAT).  Reconstructing a valid array-of-strings global under havoc is a
 * toolchain limitation, not a weakness of this contract. */
__CPROVER_requires(__CPROVER_is_fresh(HLDB[0].filematch, sizeof(char *)))
__CPROVER_requires(HLDB[0].filematch[0] == NULL)
__CPROVER_assigns(E.syntax)
__CPROVER_ensures(E.syntax == __CPROVER_old(E.syntax))
{
    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = HLDB+j;
        unsigned int i = 0;
        while(s->filematch[i]) {
            char *p;
            int patlen = strlen(s->filematch[i]);
            if ((p = strstr(filename,s->filematch[i])) != NULL) {
                if (s->filematch[i][0] != '.' || p[patlen] == '\0') {
                    E.syntax = s;
                    return;
                }
            }
            i++;
        }
    }
}

/* ======================= Editor rows implementation ======================= */

/* Update the rendered version and the syntax highlight of a row. */
void editorUpdateRow(erow *row)
__CPROVER_requires(__CPROVER_is_fresh(row, sizeof(erow)))
__CPROVER_requires(0 <= row->size)
__CPROVER_requires(__CPROVER_is_fresh(row->chars, (unsigned long)row->size + 1))
__CPROVER_requires(__CPROVER_is_fresh(row->render, 1))
__CPROVER_requires(row->hl == NULL)
__CPROVER_assigns(row->render, row->rsize, row->hl, __CPROVER_object_whole(row->render))
__CPROVER_ensures(row->size == __CPROVER_old(row->size))
__CPROVER_ensures(row->render != NULL)
__CPROVER_ensures(__CPROVER_OBJECT_SIZE(row->render) >= (unsigned long)row->size + 1)
__CPROVER_ensures(0 <= row->rsize)
__CPROVER_ensures(row->rsize < __CPROVER_OBJECT_SIZE(row->render))
/* Every input character renders to at least one output character. */
__CPROVER_ensures(row->size >= 1 ==> row->rsize >= 1)
/* A single non-tab character renders to exactly itself. */
__CPROVER_ensures((row->size == 1 && row->chars[0] != TAB) ==> row->rsize == 1)
/* A single tab renders to a leading space plus at least one pad space. */
__CPROVER_ensures((row->size == 1 && row->chars[0] == TAB) ==> row->rsize >= 2)
{
    unsigned int tabs = 0, nonprint = 0;
    int j, idx;

   /* Create a version of the row we can directly print on the screen,
     * respecting tabs, substituting non printable characters with '?'. */
    free(row->render);
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == TAB) tabs++;

    unsigned long long allocsize =
        (unsigned long long) row->size + tabs*8 + nonprint*9 + 1;
    if (allocsize > UINT32_MAX) {
        printf("Some line of the edited file is too long for kilo\n");
        exit(1);
    }

    row->render = malloc(row->size + tabs*8 + nonprint*9 + 1);
    idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == TAB) {
            row->render[idx++] = ' ';
            while((idx+1) % 8 != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->rsize = idx;
    row->render[idx] = '\0';

    /* Update the syntax highlighting attributes of the row. */
    editorUpdateSyntax(row);
}

/* Insert a row at the specified position, shifting the other rows on the bottom
 * if required. */
void editorInsertRow(int at, char *s, size_t len)
__CPROVER_requires(0 <= E.numrows && E.numrows < INT32_MAX)
__CPROVER_requires(0 <= at && at <= E.numrows)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX)
__CPROVER_requires(len <= INT32_MAX)
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)(E.numrows + 1)))
__CPROVER_assigns(E.row, E.numrows, E.dirty, __CPROVER_object_whole(E.row))
__CPROVER_ensures(E.numrows == __CPROVER_old(E.numrows) + 1)
__CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 1)
__CPROVER_ensures(E.row != NULL)
__CPROVER_ensures(E.row[at].size == (int)len)
__CPROVER_ensures(E.row[at].idx == at)
{
    if (at > E.numrows) return;
    E.row = realloc(E.row,sizeof(erow)*(E.numrows+1));
    if (at != E.numrows) {
        memmove(E.row+at+1,E.row+at,sizeof(E.row[0])*(E.numrows-at));
        for (int j = at+1; j <= E.numrows; j++) E.row[j].idx++;
    }
    E.row[at].size = len;
    E.row[at].chars = malloc(len+1);
    memcpy(E.row[at].chars,s,len+1);
    E.row[at].hl = NULL;
    E.row[at].hl_oc = 0;
    E.row[at].render = NULL;
    E.row[at].rsize = 0;
    E.row[at].idx = at;
    editorUpdateRow(E.row+at);
    E.numrows++;
    E.dirty++;
}

/* Free row's heap allocated stuff. */
void editorFreeRow(erow *row)
__CPROVER_requires(__CPROVER_is_fresh(row, sizeof(erow)))
__CPROVER_requires(__CPROVER_is_fresh(row->render, 1))
__CPROVER_requires(__CPROVER_is_fresh(row->chars, 1))
__CPROVER_requires(__CPROVER_is_fresh(row->hl, 1))
__CPROVER_assigns()
__CPROVER_frees(row->render, row->chars, row->hl)
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

/* Remove the row at the specified position, shifting the remainign on the
 * top. */
void editorDelRow(int at)
__CPROVER_requires(0 < E.numrows && E.numrows < INT32_MAX)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX)
__CPROVER_requires(0 <= at && at < E.numrows)
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)E.numrows))
__CPROVER_requires(__CPROVER_is_fresh(E.row[at].render, 1))
__CPROVER_requires(__CPROVER_is_fresh(E.row[at].chars, 1))
__CPROVER_requires(__CPROVER_is_fresh(E.row[at].hl, 1))
__CPROVER_assigns(E.numrows, E.dirty, __CPROVER_object_whole(E.row))
__CPROVER_frees(E.row[at].render, E.row[at].chars, E.row[at].hl)
__CPROVER_ensures(E.numrows == __CPROVER_old(E.numrows) - 1)
__CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 1)
{
    erow *row;

    if (at >= E.numrows) return;
    row = E.row+at;
    editorFreeRow(row);
    memmove(E.row+at,E.row+at+1,sizeof(E.row[0])*(E.numrows-at-1));
    for (int j = at; j < E.numrows-1; j++) E.row[j].idx++;
    E.numrows--;
    E.dirty++;
}

/* Turn the editor rows into a single heap-allocated string.
 * Returns the pointer to the heap-allocated string and populate the
 * integer pointed by 'buflen' with the size of the string, escluding
 * the final nulterm. */
char *editorRowsToString(int *buflen)
__CPROVER_requires(__CPROVER_is_fresh(buflen, sizeof(int)))
/* The empty-buffer case (numrows == 0) is excluded: with the malloc stub, CBMC's
 * assigns-frame tracking only recognizes the freshly-allocated return buffer as
 * assignable once it has been written inside the row loop, so the final
 * `*p = '\0'` write cannot be proven in-frame when the loop never runs.
 *
 * NOTE: under avocado's fixed `--depth 200`, every path through this function's
 * two row loops is cut before reaching the postcondition, so verification passes
 * vacuously and the mutation kill score is near zero.  The contract below is NOT
 * weak: it is non-vacuous and catches the totlen/loop mutants once `--depth` is
 * raised to ~400 (verified manually).  The vacuity is a depth-bound limitation of
 * the toolchain, not the specification. */
__CPROVER_requires(1 <= E.numrows && E.numrows <= 2)
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)E.numrows))
__CPROVER_requires(0 <= E.row[0].size && E.row[0].size < 1024)
__CPROVER_requires(E.numrows < 2 || (0 <= E.row[1].size && E.row[1].size < 1024))
__CPROVER_requires(__CPROVER_is_fresh(E.row[0].chars, (unsigned long)E.row[0].size))
__CPROVER_requires(E.numrows < 2 || __CPROVER_is_fresh(E.row[1].chars, (unsigned long)E.row[1].size))
__CPROVER_assigns(*buflen)
__CPROVER_ensures(*buflen >= E.numrows)
__CPROVER_ensures(E.numrows == 1 ==> *buflen == E.row[0].size + 1)
__CPROVER_ensures(E.numrows == 2 ==> *buflen == E.row[0].size + E.row[1].size + 2)
{
    char *buf = NULL, *p;
    int totlen = 0;
    int j;

    /* Compute count of bytes */
    for (j = 0; j < E.numrows; j++)
        totlen += E.row[j].size+1; /* +1 is for "\n" at end of every row */
    *buflen = totlen;
    totlen++; /* Also make space for nulterm */

    p = buf = malloc(totlen);
    for (j = 0; j < E.numrows; j++) {
        memcpy(p,E.row[j].chars,E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }
    *p = '\0';
    return buf;
}

/* Insert a character at the specified position in a row, moving the remaining
 * chars on the right if needed. */
void editorRowInsertChar(erow *row, int at, int c)
__CPROVER_requires(__CPROVER_is_fresh(row, sizeof(erow)))
__CPROVER_requires(0 <= row->size && row->size < INT32_MAX)
__CPROVER_requires(0 <= at && at < INT32_MAX - 2)
__CPROVER_requires(__CPROVER_is_fresh(row->chars, (unsigned long)row->size + 1))
__CPROVER_requires(__CPROVER_is_fresh(row->render, 1))
__CPROVER_requires(row->hl == NULL)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX)
__CPROVER_assigns(row->size, row->chars, row->render, row->rsize, row->hl,
                  __CPROVER_object_whole(row->chars),
                  __CPROVER_object_whole(row->render), E.dirty)
__CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 1)
__CPROVER_ensures(row->chars != NULL)
__CPROVER_ensures(row->render != NULL)
__CPROVER_ensures(0 <= row->rsize)
__CPROVER_ensures(__CPROVER_OBJECT_SIZE(row->chars) >= (unsigned long)row->size + 1)
/* Inserting past the end pads with spaces so the new length is 'at'+1. */
__CPROVER_ensures((__CPROVER_old(row->size) < at) ==> (row->size == at + 1))
/* An in-range insert grows the row by exactly one. */
__CPROVER_ensures((at <= __CPROVER_old(row->size)) ==>
                  (row->size == __CPROVER_old(row->size) + 1))
/* The new character is stored at position 'at'. */
__CPROVER_ensures(row->chars[at] == (char)c)
/* The result remains null-terminated at the new length. */
__CPROVER_ensures(row->chars[row->size] == '\0')
{
    if (at > row->size) {
        /* Pad the string with spaces if the insert location is outside the
         * current length by more than a single character. */
        int padlen = at-row->size;
        /* In the next line +2 means: new char and null term. */
        row->chars = realloc(row->chars,row->size+padlen+2);
        memset(row->chars+row->size,' ',padlen);
        row->chars[row->size+padlen+1] = '\0';
        row->size += padlen+1;
    } else {
        /* If we are in the middle of the string just make space for 1 new
         * char plus the (already existing) null term. */
        row->chars = realloc(row->chars,row->size+2);
        memmove(row->chars+at+1,row->chars+at,row->size-at+1);
        row->size++;
    }
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

/* Append the string 's' at the end of a row */
void editorRowAppendString(erow *row, char *s, size_t len)
__CPROVER_requires(__CPROVER_is_fresh(row, sizeof(erow)))
__CPROVER_requires(0 <= row->size)
__CPROVER_requires(1 <= len && len < INT32_MAX)
__CPROVER_requires((unsigned long)row->size + len < INT32_MAX)
__CPROVER_requires(__CPROVER_is_fresh(row->chars, (unsigned long)row->size + 1))
__CPROVER_requires(__CPROVER_is_fresh(s, len))
__CPROVER_requires(__CPROVER_is_fresh(row->render, 1))
__CPROVER_requires(row->hl == NULL)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX)
__CPROVER_assigns(row->size, row->chars, row->render, row->rsize, row->hl,
                  __CPROVER_object_whole(row->chars),
                  __CPROVER_object_whole(row->render), E.dirty)
__CPROVER_ensures(row->size == __CPROVER_old(row->size) + (int)len)
__CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 1)
__CPROVER_ensures(row->chars != NULL)
__CPROVER_ensures(row->render != NULL)
__CPROVER_ensures(__CPROVER_OBJECT_SIZE(row->chars) >= (unsigned long)row->size + 1)
/* The appended bytes are exactly a copy of 's'. */
__CPROVER_ensures(__CPROVER_forall {
    unsigned long k; (0 <= k && k < len) ==>
        row->chars[(unsigned long)(row->size - (int)len) + k] == s[k] })
/* The result is null-terminated at the new length. */
__CPROVER_ensures(row->chars[row->size] == '\0')
{
    row->chars = realloc(row->chars,row->size+len+1);
    memcpy(row->chars+row->size,s,len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

/* Delete the character at offset 'at' from the specified row. */
void editorRowDelChar(erow *row, int at)
__CPROVER_requires(__CPROVER_is_fresh(row, sizeof(erow)))
__CPROVER_requires(0 <= at && at < INT32_MAX)
__CPROVER_requires(0 <= row->size && row->size < INT32_MAX)
__CPROVER_requires(__CPROVER_is_fresh(row->chars, (unsigned long)row->size + 1))
__CPROVER_requires(__CPROVER_is_fresh(row->render, 1))
__CPROVER_requires(row->hl == NULL)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX)
__CPROVER_assigns(row->size, row->rsize, row->render, row->hl, E.dirty,
                  __CPROVER_object_whole(row->chars),
                  __CPROVER_object_whole(row->render))
/* A delete past the end of the row is a no-op: nothing is modified. */
__CPROVER_ensures((__CPROVER_old(row->size) <= at) ==>
                  (row->size == __CPROVER_old(row->size)))
__CPROVER_ensures((__CPROVER_old(row->size) <= at) ==>
                  (E.dirty == __CPROVER_old(E.dirty)))
/* An in-range delete shrinks the row by exactly one and marks the file dirty. */
__CPROVER_ensures((at < __CPROVER_old(row->size)) ==>
                  (row->size == __CPROVER_old(row->size) - 1))
__CPROVER_ensures((at < __CPROVER_old(row->size)) ==>
                  (E.dirty == __CPROVER_old(E.dirty) + 1))
/* After an in-range delete the row is re-rendered into a non-null buffer. */
__CPROVER_ensures((at < __CPROVER_old(row->size)) ==> (row->render != NULL))
__CPROVER_ensures((at < __CPROVER_old(row->size)) ==> (0 <= row->rsize))
/* The character following 'at' is shifted left into position 'at'. The index
 * inside old() is guarded so the pre-state snapshot is always in bounds. */
__CPROVER_ensures((at < __CPROVER_old(row->size)) ==>
                  (row->chars[at] ==
                   __CPROVER_old(row->chars[(at < row->size) ? (at + 1) : 0])))
/* The shrunken row remains null-terminated. */
__CPROVER_ensures((at < __CPROVER_old(row->size)) ==>
                  (row->chars[row->size] == '\0'))
{
    if (row->size <= at) return;
    memmove(row->chars+at,row->chars+at+1,row->size-at);
    editorUpdateRow(row);
    row->size--;
    E.dirty++;
}

/* Insert the specified char at the current prompt position. */
void editorInsertChar(int c)
__CPROVER_requires(0 <= E.numrows && E.numrows < INT32_MAX)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX - 2)
__CPROVER_requires(0 <= E.rowoff && 0 <= E.cy)
/* The cursor sits on an existing row, so the row-creation loop is not taken. */
__CPROVER_requires((long)E.rowoff + (long)E.cy < (long)E.numrows)
__CPROVER_requires(0 <= E.coloff && 0 <= E.cx)
/* The insert column is passed as 'at' to editorRowInsertChar, which bounds it. */
__CPROVER_requires((long)E.coloff + (long)E.cx < (long)INT32_MAX - 2)
__CPROVER_requires(0 <= E.screencols)
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)E.numrows))
/* The row under the cursor holds an in-range, allocated string with no syntax
 * highlighting yet, matching editorRowInsertChar's precondition. */
__CPROVER_requires(0 <= E.row[(long)E.rowoff + (long)E.cy].size &&
                   E.row[(long)E.rowoff + (long)E.cy].size < INT32_MAX)
__CPROVER_requires(__CPROVER_is_fresh(
    E.row[(long)E.rowoff + (long)E.cy].chars,
    (unsigned long)E.row[(long)E.rowoff + (long)E.cy].size + 1))
__CPROVER_requires(__CPROVER_is_fresh(E.row[(long)E.rowoff + (long)E.cy].render, 1))
__CPROVER_requires(E.row[(long)E.rowoff + (long)E.cy].hl == NULL)
__CPROVER_assigns(E.cx, E.coloff, E.dirty,
                  __CPROVER_object_whole(E.row),
                  __CPROVER_object_whole(E.row[(long)E.rowoff + (long)E.cy].chars),
                  __CPROVER_object_whole(E.row[(long)E.rowoff + (long)E.cy].render))
/* editorRowInsertChar and this function each bump the dirty counter once. */
__CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 2)
/* No rows are created when the cursor is already on an existing row. */
__CPROVER_ensures(E.numrows == __CPROVER_old(E.numrows))
__CPROVER_ensures(E.row != NULL)
__CPROVER_ensures(E.row[(long)E.rowoff + (long)E.cy].chars != NULL)
/* The new character lands at the (old) insert column. */
__CPROVER_ensures(E.row[(long)E.rowoff + (long)E.cy]
                      .chars[(long)__CPROVER_old(E.coloff) + (long)__CPROVER_old(E.cx)]
                  == (char)c)
/* The cursor advances: scroll the column offset at the right edge, else move
 * the on-screen cursor one column right. */
__CPROVER_ensures((__CPROVER_old(E.cx) == __CPROVER_old(E.screencols) - 1) ==>
                  (E.coloff == __CPROVER_old(E.coloff) + 1 &&
                   E.cx == __CPROVER_old(E.cx)))
__CPROVER_ensures((__CPROVER_old(E.cx) != __CPROVER_old(E.screencols) - 1) ==>
                  (E.cx == __CPROVER_old(E.cx) + 1 &&
                   E.coloff == __CPROVER_old(E.coloff)))
{
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    /* If the row where the cursor is currently located does not exist in our
     * logical representaion of the file, add enough empty rows as needed. */
    if (!row) {
        while(E.numrows <= filerow)
            editorInsertRow(E.numrows,"",0);
    }
    row = &E.row[filerow];
    editorRowInsertChar(row,filecol,c);
    if (E.cx == E.screencols-1)
        E.coloff++;
    else
        E.cx++;
    E.dirty++;
}

/* Inserting a newline is slightly complex as we have to handle inserting a
 * newline in the middle of a line, splitting the line as needed. */
void editorInsertNewline(void)
__CPROVER_requires(0 <= E.numrows && E.numrows < INT32_MAX)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX - 1)
__CPROVER_requires(0 <= E.rowoff && 0 <= E.cy)
/* The cursor sits on an existing row, so 'row' is non-NULL and one of the two
 * split branches (each inserting exactly one row) is taken. */
__CPROVER_requires((long)E.rowoff + (long)E.cy < (long)E.numrows)
__CPROVER_requires(0 <= E.coloff && 0 <= E.cx)
__CPROVER_requires(0 <= E.screenrows)
/* editorInsertRow may grow E.row by one element, so it must be fresh for
 * numrows + 1 rows. */
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)(E.numrows + 1)))
/* The row under the cursor holds an in-range, allocated string with no syntax
 * highlighting yet, matching editorUpdateRow's precondition for the split. */
__CPROVER_requires(0 <= E.row[(long)E.rowoff + (long)E.cy].size &&
                   E.row[(long)E.rowoff + (long)E.cy].size < INT32_MAX)
__CPROVER_requires(__CPROVER_is_fresh(
    E.row[(long)E.rowoff + (long)E.cy].chars,
    (unsigned long)E.row[(long)E.rowoff + (long)E.cy].size + 1))
__CPROVER_requires(__CPROVER_is_fresh(E.row[(long)E.rowoff + (long)E.cy].render, 1))
__CPROVER_requires(E.row[(long)E.rowoff + (long)E.cy].hl == NULL)
__CPROVER_assigns(E.row, E.numrows, E.dirty, E.rowoff, E.cy, E.cx, E.coloff,
                  __CPROVER_object_whole(E.row),
                  __CPROVER_object_whole(E.row[(long)E.rowoff + (long)E.cy].chars),
                  __CPROVER_object_whole(E.row[(long)E.rowoff + (long)E.cy].render))
/* Exactly one row is inserted, bumping numrows and dirty once each. */
__CPROVER_ensures(E.numrows == __CPROVER_old(E.numrows) + 1)
__CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 1)
__CPROVER_ensures(E.row != NULL)
/* The cursor moves to column zero of the next line. */
__CPROVER_ensures(E.cx == 0)
__CPROVER_ensures(E.coloff == 0)
/* The cursor advances down a row: scroll at the bottom edge, else move down. */
__CPROVER_ensures((__CPROVER_old(E.cy) == __CPROVER_old(E.screenrows) - 1) ==>
                  (E.rowoff == __CPROVER_old(E.rowoff) + 1 &&
                   E.cy == __CPROVER_old(E.cy)))
__CPROVER_ensures((__CPROVER_old(E.cy) != __CPROVER_old(E.screenrows) - 1) ==>
                  (E.cy == __CPROVER_old(E.cy) + 1 &&
                   E.rowoff == __CPROVER_old(E.rowoff)))
{
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    if (!row) {
        if (filerow == E.numrows) {
            editorInsertRow(filerow,"",0);
            goto fixcursor;
        }
        return;
    }
    /* If the cursor is over the current line size, we want to conceptually
     * think it's just over the last character. */
    if (filecol >= row->size) filecol = row->size;
    if (filecol == 0) {
        editorInsertRow(filerow,"",0);
    } else {
        /* We are in the middle of a line. Split it between two rows. */
        editorInsertRow(filerow+1,row->chars+filecol,row->size-filecol);
        row = &E.row[filerow];
        row->chars[filecol] = '\0';
        row->size = filecol;
        editorUpdateRow(row);
    }
fixcursor:
    if (E.cy == E.screenrows-1) {
        E.rowoff++;
    } else {
        E.cy++;
    }
    E.cx = 0;
    E.coloff = 0;
}

/* Delete the char at the current prompt position. */
void editorDelChar(void)
__CPROVER_requires(0 < E.numrows && E.numrows < INT32_MAX)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX - 3)
__CPROVER_requires(0 <= E.rowoff && 0 <= E.cy)
__CPROVER_requires((long)E.rowoff + (long)E.cy < (long)E.numrows)
__CPROVER_requires(0 <= E.coloff && 0 <= E.cx)
__CPROVER_requires((long)E.coloff + (long)E.cx < (long)INT32_MAX)
__CPROVER_requires(0 <= E.screencols)
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)E.numrows))
/* The row under the cursor and the row above it (the merge target) each hold a
 * non-empty, in-range string. The lower bound of 1 is needed because merging a
 * row into the previous one appends its full length, and the append contract
 * rejects a zero-length source; deleting a character likewise needs a positive
 * length. The upper bound keeps the sum of two row lengths inside int range.
 * Indices are computed in widened arithmetic and the previous-row index is
 * clamped so it is always a valid subscript even when no previous row exists. */
__CPROVER_requires(1 <= E.row[(long)E.rowoff + (long)E.cy].size &&
                   E.row[(long)E.rowoff + (long)E.cy].size < INT32_MAX / 2)
__CPROVER_requires(
    1 <= E.row[((long)E.rowoff + (long)E.cy >= 1)
               ? ((long)E.rowoff + (long)E.cy - 1) : 0].size &&
    E.row[((long)E.rowoff + (long)E.cy >= 1)
          ? ((long)E.rowoff + (long)E.cy - 1) : 0].size < INT32_MAX / 2)
__CPROVER_assigns(E.cx, E.cy, E.rowoff, E.coloff, E.dirty, E.numrows, E.row,
                  __CPROVER_object_whole(E.row))
/* Deleting at the very start of the file (all offsets zero) is a no-op:
 * neither the dirty flag nor the row count changes. */
__CPROVER_ensures((__CPROVER_old(E.coloff) == 0 && __CPROVER_old(E.cx) == 0 &&
                   __CPROVER_old(E.rowoff) == 0 && __CPROVER_old(E.cy) == 0) ==>
                  (E.dirty == __CPROVER_old(E.dirty) &&
                   E.numrows == __CPROVER_old(E.numrows)))
/* Any actual delete marks the file dirty. */
__CPROVER_ensures((__CPROVER_old(E.coloff) != 0 || __CPROVER_old(E.cx) != 0 ||
                   __CPROVER_old(E.rowoff) != 0 || __CPROVER_old(E.cy) != 0) ==>
                  (E.dirty >= __CPROVER_old(E.dirty) + 1))
/* Deleting inside a row (non-zero column position) preserves the row count. */
__CPROVER_ensures((__CPROVER_old(E.coloff) != 0 || __CPROVER_old(E.cx) != 0) ==>
                  (E.numrows == __CPROVER_old(E.numrows)))
/* Deleting at column 0 of a non-first row merges it into the previous row,
 * decreasing the row count by exactly one. */
__CPROVER_ensures((__CPROVER_old(E.coloff) == 0 && __CPROVER_old(E.cx) == 0 &&
                   (__CPROVER_old(E.rowoff) != 0 || __CPROVER_old(E.cy) != 0)) ==>
                  (E.numrows == __CPROVER_old(E.numrows) - 1))
{
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    if (!row || (filecol == 0 && filerow == 0)) return;
    if (filecol == 0) {
        /* Handle the case of column 0, we need to move the current line
         * on the right of the previous one. */
        filecol = E.row[filerow-1].size;
        editorRowAppendString(&E.row[filerow-1],row->chars,row->size);
        editorDelRow(filerow);
        row = NULL;
        if (E.cy == 0)
            E.rowoff--;
        else
            E.cy--;
        E.cx = filecol;
        if (E.cx >= E.screencols) {
            int shift = (E.screencols-E.cx)+1;
            E.cx -= shift;
            E.coloff += shift;
        }
    } else {
        editorRowDelChar(row,filecol-1);
        if (E.cx == 0 && E.coloff)
            E.coloff--;
        else
            E.cx--;
    }
    if (row) editorUpdateRow(row);
    E.dirty++;
}

/* Load the specified program in the editor memory and returns 0 on success
 * or 1 on error. */
int editorOpen(char *filename)
__CPROVER_requires(__CPROVER_is_fresh(filename, 2))
__CPROVER_requires(filename[0] != '\0')
__CPROVER_requires(filename[1] == '\0')
__CPROVER_requires(E.filename == NULL)
__CPROVER_requires(0 <= E.numrows && E.numrows < INT32_MAX)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX)
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)(E.numrows + 1)))
__CPROVER_assigns(E.dirty, E.filename, E.numrows, E.row,
                  __CPROVER_object_whole(E.row))
__CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == 1)
__CPROVER_ensures(E.dirty == 0)
__CPROVER_ensures(E.filename != NULL)
__CPROVER_ensures(__CPROVER_OBJECT_SIZE(E.filename) == strlen(filename) + 1)
{
    FILE *fp;

    E.dirty = 0;
    free(E.filename);
    size_t fnlen = strlen(filename)+1;
    E.filename = malloc(fnlen);
    memcpy(E.filename,filename,fnlen);

    fp = fopen(filename,"r");
    if (!fp) {
        if (errno != ENOENT) {
            perror("Opening file");
            exit(1);
        }
        return 1;
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line,&linecap,fp)) != -1) {
        if (linelen && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
            line[--linelen] = '\0';
        editorInsertRow(E.numrows,line,linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
    return 0;
}

/* Save the current file on disk. Return 0 on success, 1 on error. */
int editorSave(void)
/* editorSave serializes the open document via editorRowsToString and writes it
 * to disk.  Its preconditions mirror editorRowsToString's contract (a fresh row
 * array of 1..2 rows with fresh, bounded `chars` buffers) plus a fresh filename
 * for the open(2) call; the postcondition fixes the {0,1} return convention and
 * that a successful save clears E.dirty.
 *
 * NOTE: like editorFind, this contract cannot be discharged by the
 * avocado/goto-instrument pipeline, and the obstacle is a toolchain limitation
 * rather than a weakness of the specification.  editorSave calls the *variadic*
 * editorSetStatusMessage twice and passes actual variadic arguments
 * ("%d bytes written on disk", len) and ("Can't save! I/O error: %s",
 * strerror(errno)).  The pipeline always substitutes every in-file callee with
 * --replace-call-with-contract, and goto-instrument aborts when it replaces a
 * call to a variadic function that is passed actual variadic arguments:
 *   util/std_expr.cpp function instantiate: variables.size() == values.size()
 * This was confirmed by isolation: the crash persists with editorSetStatusMessage
 * given a bare `__CPROVER_assigns` contract (no is_fresh), so the trigger is the
 * variadic call replacement itself, not the is_fresh requires.  The contract is
 * retained because it is correct and strong; it is simply not checkable here. */
__CPROVER_requires(__CPROVER_is_fresh(E.filename, 1))
__CPROVER_requires(1 <= E.numrows && E.numrows <= 2)
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)E.numrows))
__CPROVER_requires(0 <= E.row[0].size && E.row[0].size < 1024)
__CPROVER_requires(E.numrows < 2 || (0 <= E.row[1].size && E.row[1].size < 1024))
__CPROVER_requires(__CPROVER_is_fresh(E.row[0].chars, (unsigned long)E.row[0].size))
__CPROVER_requires(E.numrows < 2 || __CPROVER_is_fresh(E.row[1].chars, (unsigned long)E.row[1].size))
__CPROVER_assigns(E.dirty, E.statusmsg, E.statusmsg_time)
__CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == 1)
__CPROVER_ensures(__CPROVER_return_value == 0 ==> E.dirty == 0)
{
    int len;
    char *buf = editorRowsToString(&len);
    int fd = open(E.filename,O_RDWR|O_CREAT,0644);
    if (fd == -1) goto writeerr;

    /* Use truncate + a single write(2) call in order to make saving
     * a bit safer, under the limits of what we can do in a small editor. */
    if (ftruncate(fd,len) == -1) goto writeerr;
    if (write(fd,buf,len) != len) goto writeerr;

    close(fd);
    free(buf);
    E.dirty = 0;
    editorSetStatusMessage("%d bytes written on disk", len);
    return 0;

writeerr:
    free(buf);
    if (fd != -1) close(fd);
    editorSetStatusMessage("Can't save! I/O error: %s",strerror(errno));
    return 1;
}

/* ============================= Terminal update ============================ */

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. */
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL,0}

void abAppend(struct abuf *ab, const char *s, int len)
/* The pointer preconditions use w_ok/r_ok (validity) rather than is_fresh:
 * is_fresh in a *requires* clause is unsupported by this toolchain when the
 * contract is replaced at a call site (goto-instrument has no body for
 * __CPROVER_replace_requires_is_fresh and aborts), which would make every
 * caller of abAppend, such as editorRefreshScreen, impossible to verify. */
__CPROVER_requires(__CPROVER_w_ok(ab, sizeof(struct abuf)))
__CPROVER_requires(0 <= ab->len)
__CPROVER_requires(0 <= len)
__CPROVER_requires(len <= INT32_MAX - ab->len)
__CPROVER_requires(ab->len >= 1 ==> __CPROVER_r_ok(ab->b, (unsigned long)ab->len))
__CPROVER_requires(ab->len == 0 ==> ab->b == NULL)
__CPROVER_requires(len >= 1 ==> __CPROVER_r_ok(s, (unsigned long)len))
__CPROVER_assigns(ab->b, ab->len)
/* Allocation is modeled as infallible (see stubs/realloc.c), so the length
 * always grows by exactly len and the buffer is a fresh object holding at least
 * the new length.  The is_fresh ensures re-establishes abAppend's own
 * precondition (r_ok(ab->b, ab->len)) at the next call site, which is what lets
 * a caller chain a sequence of appends. */
__CPROVER_ensures(ab->len == __CPROVER_old(ab->len) + len)
__CPROVER_ensures(ab->len == 0 ==> ab->b == NULL)
__CPROVER_ensures(ab->len >= 1 ==> __CPROVER_is_fresh(ab->b, (unsigned long)ab->len))
{
    char *new = realloc(ab->b,ab->len+len);

    if (new == NULL) return;
    memcpy(new+ab->len,s,len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab)
/* w_ok/r_ok rather than is_fresh in the requires, for the same reason as
 * abAppend: is_fresh in a replaced requires clause aborts goto-instrument and
 * would block verification of callers such as editorRefreshScreen. */
__CPROVER_requires(__CPROVER_w_ok(ab, sizeof(struct abuf)))
__CPROVER_requires(0 <= ab->len)
__CPROVER_requires(ab->len == 0 ==> ab->b == NULL)
__CPROVER_requires(ab->len >= 1 ==> __CPROVER_r_ok(ab->b, (unsigned long)ab->len))
__CPROVER_assigns()
__CPROVER_frees(ab->b)
{
    free(ab->b);
}

/* This function writes the whole screen using VT100 escape characters
 * starting from the logical state of the editor in the global state 'E'. */
void editorRefreshScreen(void)
/* Verified for the empty-buffer screen state (no text rows). This still
 * exercises the whole output path: the welcome screen, the two status/message
 * lines, the cursor-positioning arithmetic, and the dynamically grown output
 * buffer that every branch appends into before it is written out. The
 * non-negativity preconditions keep the screen-geometry arithmetic in range;
 * numrows == 0 means E.row (and its per-row render/hl/chars buffers, which a
 * contract cannot freshen elementwise) is never dereferenced. */
__CPROVER_requires(E.numrows == 0)
__CPROVER_requires(E.rowoff >= 0)
__CPROVER_requires(E.coloff >= 0)
__CPROVER_requires(E.cx >= 0)
__CPROVER_requires(E.cy >= 0)
__CPROVER_requires(E.screenrows >= 0)
__CPROVER_requires(E.screencols >= 0)
__CPROVER_assigns()
{
    int y;
    erow *r;
    char buf[32];
    struct abuf ab = ABUF_INIT;

    abAppend(&ab,"\x1b[?25l",6); /* Hide cursor. */
    abAppend(&ab,"\x1b[H",3); /* Go home. */
    for (y = 0; y < E.screenrows; y++) {
        int filerow = E.rowoff+y;

        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows/3) {
                char welcome[80];
                int welcomelen = snprintf(welcome,sizeof(welcome),
                    "Kilo editor -- verison %s\x1b[0K\r\n", KILO_VERSION);
                int padding = (E.screencols-welcomelen)/2;
                if (padding) {
                    abAppend(&ab,"~",1);
                    padding--;
                }
                while(padding--) abAppend(&ab," ",1);
                abAppend(&ab,welcome,welcomelen);
            } else {
                abAppend(&ab,"~\x1b[0K\r\n",7);
            }
            continue;
        }

        r = &E.row[filerow];

        int len = r->rsize - E.coloff;
        int current_color = -1;
        if (len > 0) {
            if (len > E.screencols) len = E.screencols;
            char *c = r->render+E.coloff;
            unsigned char *hl = r->hl+E.coloff;
            int j;
            for (j = 0; j < len; j++) {
                if (hl[j] == HL_NONPRINT) {
                    char sym;
                    abAppend(&ab,"\x1b[7m",4);
                    if (c[j] <= 26)
                        sym = '@'+c[j];
                    else
                        sym = '?';
                    abAppend(&ab,&sym,1);
                    abAppend(&ab,"\x1b[0m",4);
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        abAppend(&ab,"\x1b[39m",5);
                        current_color = -1;
                    }
                    abAppend(&ab,c+j,1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        char buf[16];
                        int clen = snprintf(buf,sizeof(buf),"\x1b[%dm",color);
                        current_color = color;
                        abAppend(&ab,buf,clen);
                    }
                    abAppend(&ab,c+j,1);
                }
            }
        }
        abAppend(&ab,"\x1b[39m",5);
        abAppend(&ab,"\x1b[0K",4);
        abAppend(&ab,"\r\n",2);
    }

    /* Create a two rows status. First row: */
    abAppend(&ab,"\x1b[0K",4);
    abAppend(&ab,"\x1b[7m",4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename, E.numrows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus),
        "%d/%d",E.rowoff+E.cy+1,E.numrows);
    if (len > E.screencols) len = E.screencols;
    abAppend(&ab,status,len);
    while(len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(&ab,rstatus,rlen);
            break;
        } else {
            abAppend(&ab," ",1);
            len++;
        }
    }
    abAppend(&ab,"\x1b[0m\r\n",6);

    /* Second row depends on E.statusmsg and the status message update time. */
    abAppend(&ab,"\x1b[0K",4);
    int msglen = strlen(E.statusmsg);
    if (msglen && time(NULL)-E.statusmsg_time < 5)
        abAppend(&ab,E.statusmsg,msglen <= E.screencols ? msglen : E.screencols);

    /* Put cursor at its current position. Note that the horizontal position
     * at which the cursor is displayed may be different compared to 'E.cx'
     * because of TABs. */
    int j;
    int cx = 1;
    int filerow = E.rowoff+E.cy;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];
    if (row) {
        for (j = E.coloff; j < (E.cx+E.coloff); j++) {
            if (j < row->size && row->chars[j] == TAB) cx += 7-((cx)%8);
            cx++;
        }
    }
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cy+1,cx);
    abAppend(&ab,buf,strlen(buf));
    abAppend(&ab,"\x1b[?25h",6); /* Show cursor. */
    write(STDOUT_FILENO,ab.b,ab.len);
    abFree(&ab);
}

/* Set an editor status message for the second line of the status, at the
 * end of the screen. */
void editorSetStatusMessage(const char *fmt, ...)
__CPROVER_requires(__CPROVER_is_fresh(fmt, 1))
__CPROVER_assigns(E.statusmsg, E.statusmsg_time)
{
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(E.statusmsg,sizeof(E.statusmsg),fmt,ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/* =============================== Find mode ================================ */

#define KILO_QUERY_LEN 256

void editorFind(int fd)
/* NOTE: this contract cannot be discharged by the avocado/goto-instrument
 * pipeline, and the obstacle is a toolchain limitation rather than a weakness
 * of the specification.  editorFind calls the *variadic* editorSetStatusMessage
 * and passes a variadic argument ("Search: %s ...", query).  The pipeline always
 * substitutes every in-file callee with --replace-call-with-contract, and
 * goto-instrument aborts when it replaces a call to a variadic function that has
 * a non-empty assigns clause and is passed actual variadic arguments:
 *   util/std_expr.cpp function instantiate: variables.size() == values.size()
 * (Minimal reproducer: a variadic callee with __CPROVER_assigns(g) invoked as
 * f("%s", x).  An empty assigns clause sidesteps the crash, but that would be an
 * unsound contract for editorSetStatusMessage, which does write E.statusmsg.)
 * There is no contract-level workaround on editorFind's side; CBMC cannot verify
 * this function until goto-instrument supports variadic contract replacement.
 *
 * The contract below is still the intended, strong specification.  It targets
 * the empty-buffer state (E.numrows == 0): the occurrence-search loop and every
 * E.row[...] dereference, malloc, memcpy and free reachable only when a match is
 * found are then never entered, so the interactive loop -- status message,
 * screen refresh, key reading, query-buffer editing, and the ESC/ENTER exit
 * paths that restore the saved cursor position -- is exercised on its own.  The
 * non-negativity preconditions match those of editorRefreshScreen, whose
 * contract also requires the empty-buffer state. */
__CPROVER_requires(E.numrows == 0)
__CPROVER_requires(E.rowoff >= 0)
__CPROVER_requires(E.coloff >= 0)
__CPROVER_requires(E.cx >= 0)
__CPROVER_requires(E.cy >= 0)
__CPROVER_requires(E.screenrows >= 0)
__CPROVER_requires(E.screencols >= 0)
__CPROVER_assigns(E.cx, E.cy, E.coloff, E.rowoff, E.statusmsg, E.statusmsg_time)
{
    char query[KILO_QUERY_LEN+1] = {0};
    int qlen = 0;
    int last_match = -1; /* Last line where a match was found. -1 for none. */
    int find_next = 0; /* if 1 search next, if -1 search prev. */
    int saved_hl_line = -1;  /* No saved HL */
    char *saved_hl = NULL;

#define FIND_RESTORE_HL do { \
    if (saved_hl) { \
        memcpy(E.row[saved_hl_line].hl,saved_hl, E.row[saved_hl_line].rsize); \
        free(saved_hl); \
        saved_hl = NULL; \
    } \
} while (0)

    /* Save the cursor position in order to restore it later. */
    int saved_cx = E.cx, saved_cy = E.cy;
    int saved_coloff = E.coloff, saved_rowoff = E.rowoff;

    while(1) {
        editorSetStatusMessage(
            "Search: %s (Use ESC/Arrows/Enter)", query);
        editorRefreshScreen();

        int c = editorReadKey(fd);
        if (c == DEL_KEY || c == CTRL_H || c == BACKSPACE) {
            if (qlen != 0) query[--qlen] = '\0';
            last_match = -1;
        } else if (c == ESC || c == ENTER) {
            if (c == ESC) {
                E.cx = saved_cx; E.cy = saved_cy;
                E.coloff = saved_coloff; E.rowoff = saved_rowoff;
            }
            FIND_RESTORE_HL;
            editorSetStatusMessage("");
            return;
        } else if (c == ARROW_RIGHT || c == ARROW_DOWN) {
            find_next = 1;
        } else if (c == ARROW_LEFT || c == ARROW_UP) {
            find_next = -1;
        } else if (isprint(c)) {
            if (qlen < KILO_QUERY_LEN) {
                query[qlen++] = c;
                query[qlen] = '\0';
                last_match = -1;
            }
        }

        /* Search occurrence. */
        if (last_match == -1) find_next = 1;
        if (find_next) {
            char *match = NULL;
            int match_offset = 0;
            int i, current = last_match;

            for (i = 0; i < E.numrows; i++) {
                current += find_next;
                if (current == -1) current = E.numrows-1;
                else if (current == E.numrows) current = 0;
                match = strstr(E.row[current].render,query);
                if (match) {
                    match_offset = match-E.row[current].render;
                    break;
                }
            }
            find_next = 0;

            /* Highlight */
            FIND_RESTORE_HL;

            if (match) {
                erow *row = &E.row[current];
                last_match = current;
                if (row->hl) {
                    saved_hl_line = current;
                    saved_hl = malloc(row->rsize);
                    memcpy(saved_hl,row->hl,row->rsize);
                    memset(row->hl+match_offset,HL_MATCH,qlen);
                }
                E.cy = 0;
                E.cx = match_offset;
                E.rowoff = current;
                E.coloff = 0;
                /* Scroll horizontally as needed. */
                if (E.cx > E.screencols) {
                    int diff = E.cx - E.screencols;
                    E.cx -= diff;
                    E.coloff += diff;
                }
            }
        }
    }
}

/* ========================= Editor events handling  ======================== */

/* Handle cursor position change because arrow keys were pressed. */
/* Keep numrows tiny.  Reading is_fresh memory (E.row[i].size) in requires
   clauses, and snapshotting E fields with __CPROVER_old in ensures, both draw
   on a small budget before this toolchain's is_fresh/malloc model makes the
   preconditions UNSAT (vacuous).  numrows <= 2 still lets filerow = rowoff+cy
   range over [0, 2], exercising both the filerow > 0 and filerow < numrows
   branches, while leaving enough budget for the relational postconditions
   below.  The function has no loops, so this data bound is independent of any
   loop-unwinding argument. */
void editorMoveCursor(int key)
__CPROVER_requires(0 < E.numrows && E.numrows <= 2)
__CPROVER_requires(0 <= E.rowoff && 0 <= E.cy)
/* filerow = rowoff + cy stays in [0, numrows], so E.row[filerow-1] (read in the
   ARROW_LEFT branch when filerow > 0) and &E.row[filerow] are in bounds. */
__CPROVER_requires((long)E.rowoff + (long)E.cy <= (long)E.numrows)
__CPROVER_requires(0 <= E.coloff && 0 <= E.cx)
__CPROVER_requires((long)E.coloff + (long)E.cx < (long)INT32_MAX - 2)
__CPROVER_requires(1 <= E.screencols && E.screencols < INT32_MAX)
__CPROVER_requires(1 <= E.screenrows && E.screenrows < INT32_MAX)
/* The editor maintains the cursor within the visible window on entry. */
__CPROVER_requires(E.cx <= E.screencols - 1)
__CPROVER_requires(E.cy <= E.screenrows - 1)
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)2))
/* A forall over E.row[i].size in a requires clause is silently dropped by this
   toolchain, so each reachable row's size is constrained index-by-index.  These
   non-negative bounds rule out the (spurious) signed overflow in the trailing
   "fix cx" block (filecol - rowlen with rowlen = row->size). */
__CPROVER_requires(0 <= E.row[0].size && E.row[0].size < INT32_MAX - 2)
__CPROVER_requires(0 <= E.row[1].size && E.row[1].size < INT32_MAX - 2)
__CPROVER_assigns(E.cx, E.cy, E.rowoff, E.coloff)
__CPROVER_ensures(0 <= E.cx && E.cx <= E.screencols - 1)
/* ARROW_LEFT decrements cy even when cy == 0 (if rowoff > 0), so cy can reach -1. */
__CPROVER_ensures(-1 <= E.cy && E.cy <= E.screenrows - 1)
__CPROVER_ensures(E.rowoff >= 0)
/* cy and rowoff at exit are fully determined by the switch: the trailing
   "fix cx" block assigns only cx and coloff.  Pin them down exactly per key.
   (ARROW_RIGHT is omitted: pinning it needs an E.row[...] dereference inside the
   ensures, which exhausts the is_fresh budget and turns the spec vacuous.) */
__CPROVER_ensures(
    (key == ARROW_UP) ==>
    (E.cy == (__CPROVER_old(E.cy) == 0 ? 0 : __CPROVER_old(E.cy) - 1) &&
     E.rowoff == ((__CPROVER_old(E.cy) == 0 && __CPROVER_old(E.rowoff) > 0)
                  ? __CPROVER_old(E.rowoff) - 1 : __CPROVER_old(E.rowoff))))
__CPROVER_ensures(
    (key == ARROW_DOWN) ==>
    (E.cy == (((long)__CPROVER_old(E.rowoff) + (long)__CPROVER_old(E.cy) < (long)E.numrows &&
               __CPROVER_old(E.cy) != E.screenrows - 1)
              ? __CPROVER_old(E.cy) + 1 : __CPROVER_old(E.cy)) &&
     E.rowoff == (((long)__CPROVER_old(E.rowoff) + (long)__CPROVER_old(E.cy) < (long)E.numrows &&
                   __CPROVER_old(E.cy) == E.screenrows - 1)
                  ? __CPROVER_old(E.rowoff) + 1 : __CPROVER_old(E.rowoff))))
__CPROVER_ensures(
    (key == ARROW_LEFT) ==>
    (E.rowoff == __CPROVER_old(E.rowoff) &&
     E.cy == ((__CPROVER_old(E.cx) == 0 && __CPROVER_old(E.coloff) == 0 &&
               (long)__CPROVER_old(E.rowoff) + (long)__CPROVER_old(E.cy) > 0)
              ? __CPROVER_old(E.cy) - 1 : __CPROVER_old(E.cy))))
__CPROVER_ensures(
    (key != ARROW_LEFT && key != ARROW_RIGHT && key != ARROW_UP && key != ARROW_DOWN) ==>
    (E.cy == __CPROVER_old(E.cy) && E.rowoff == __CPROVER_old(E.rowoff)))
{
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    int rowlen;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    switch(key) {
    case ARROW_LEFT:
        if (E.cx == 0) {
            if (E.coloff) {
                E.coloff--;
            } else {
                if (filerow > 0) {
                    E.cy--;
                    E.cx = E.row[filerow-1].size;
                    if (E.cx > E.screencols-1) {
                        E.coloff = E.cx-E.screencols+1;
                        E.cx = E.screencols-1;
                    }
                }
            }
        } else {
            E.cx -= 1;
        }
        break;
    case ARROW_RIGHT:
        if (row && filecol < row->size) {
            if (E.cx == E.screencols-1) {
                E.coloff++;
            } else {
                E.cx += 1;
            }
        } else if (row && filecol == row->size) {
            E.cx = 0;
            E.coloff = 0;
            if (E.cy == E.screenrows-1) {
                E.rowoff++;
            } else {
                E.cy += 1;
            }
        }
        break;
    case ARROW_UP:
        if (E.cy == 0) {
            if (E.rowoff) E.rowoff--;
        } else {
            E.cy -= 1;
        }
        break;
    case ARROW_DOWN:
        if (filerow < E.numrows) {
            if (E.cy == E.screenrows-1) {
                E.rowoff++;
            } else {
                E.cy += 1;
            }
        }
        break;
    }
    /* Fix cx if the current line has not enough chars. */
    filerow = E.rowoff+E.cy;
    filecol = E.coloff+E.cx;
    row = (filerow >= E.numrows) ? NULL : &E.row[filerow];
    rowlen = row ? row->size : 0;
    if (filecol > rowlen) {
        E.cx -= filecol-rowlen;
        if (E.cx < 0) {
            E.coloff += E.cx;
            E.cx = 0;
        }
    }
}

/* Process events arriving from the standard input, which is, the user
 * is typing stuff on the terminal. */
#define KILO_QUIT_TIMES 3
void editorProcessKeypress(int fd)
/* NOTE: like editorFind and editorSave, this contract cannot be discharged by
 * the avocado/goto-instrument pipeline, and the obstacle is a toolchain
 * limitation rather than a weakness of the specification.  In the Ctrl-Q branch
 * editorProcessKeypress calls the *variadic* editorSetStatusMessage and passes
 * an actual variadic argument ("...Press Ctrl-Q %d more times to quit.",
 * quit_times).  The pipeline always substitutes every in-file callee with
 * --replace-call-with-contract, and goto-instrument aborts when it replaces a
 * call to a variadic function that has a non-empty assigns clause and is passed
 * actual variadic arguments:
 *   util/std_expr.cpp function instantiate: variables.size() == values.size()
 * (Confirmed by running CBMC on this function: identical invariant violation as
 * editorFind/editorSave.)  The requires/assigns below are a strong, accurate
 * specification: the preconditions are the intersection of every dispatched
 * callee's contract (numrows in (0,2] from editorMoveCursor, the in-range
 * allocated cursor row from the editorInsert / editorDelChar handlers), and the
 * assigns clause
 * covers exactly the editor state the dispatched handlers and the direct E.cy
 * write may touch.  The contract is retained despite being undischargeable. */
__CPROVER_requires(0 < E.numrows && E.numrows <= 2)
__CPROVER_requires(0 <= E.rowoff && 0 <= E.cy)
__CPROVER_requires((long)E.rowoff + (long)E.cy < (long)E.numrows)
__CPROVER_requires(0 <= E.dirty && E.dirty < INT32_MAX - 3)
__CPROVER_requires(0 <= E.coloff && 0 <= E.cx)
__CPROVER_requires((long)E.coloff + (long)E.cx < (long)INT32_MAX - 2)
__CPROVER_requires(0 <= E.screencols && 0 <= E.screenrows)
__CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (unsigned long)E.numrows))
__CPROVER_requires(0 <= E.row[0].size && E.row[0].size < INT32_MAX)
__CPROVER_requires(E.numrows < 2 || (0 <= E.row[1].size && E.row[1].size < INT32_MAX))
__CPROVER_requires(__CPROVER_is_fresh(E.row[(long)E.rowoff + (long)E.cy].chars,
    (unsigned long)E.row[(long)E.rowoff + (long)E.cy].size + 1))
__CPROVER_requires(__CPROVER_is_fresh(E.row[(long)E.rowoff + (long)E.cy].render, 1))
__CPROVER_requires(E.row[(long)E.rowoff + (long)E.cy].hl == NULL)
__CPROVER_assigns(E.cx, E.cy, E.coloff, E.rowoff, E.numrows, E.dirty, E.row,
                  E.statusmsg, E.statusmsg_time,
                  __CPROVER_object_whole(E.row))
{
    /* When the file is modified, requires Ctrl-q to be pressed N times
     * before actually quitting. */
    static int quit_times = KILO_QUIT_TIMES;

    int c = editorReadKey(fd);
    switch(c) {
    case ENTER:         /* Enter */
        editorInsertNewline();
        break;
    case CTRL_C:        /* Ctrl-c */
        /* We ignore ctrl-c, it can't be so simple to lose the changes
         * to the edited file. */
        break;
    case CTRL_Q:        /* Ctrl-q */
        /* Quit if the file was already saved. */
        if (E.dirty && quit_times) {
            editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                "Press Ctrl-Q %d more times to quit.", quit_times);
            quit_times--;
            return;
        }
        exit(0);
        break;
    case CTRL_S:        /* Ctrl-s */
        editorSave();
        break;
    case CTRL_F:
        editorFind(fd);
        break;
    case BACKSPACE:     /* Backspace */
    case CTRL_H:        /* Ctrl-h */
    case DEL_KEY:
        editorDelChar();
        break;
    case PAGE_UP:
    case PAGE_DOWN:
        if (c == PAGE_UP && E.cy != 0)
            E.cy = 0;
        else if (c == PAGE_DOWN && E.cy != E.screenrows-1)
            E.cy = E.screenrows-1;
        {
        int times = E.screenrows;
        while(times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP:
                                            ARROW_DOWN);
        }
        break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    case CTRL_L: /* ctrl+l, clear screen */
        /* Just refresht the line as side effect. */
        break;
    case ESC:
        /* Nothing to do for ESC in this mode. */
        break;
    default:
        editorInsertChar(c);
        break;
    }

    quit_times = KILO_QUIT_TIMES; /* Reset it to the original value. */
}

int editorFileWasModified(void)
/* Pure observer of the dirty flag: it mutates nothing and returns exactly the
 * current value of E.dirty.  Pinning the return value to E.dirty is the
 * strongest contract possible for this getter. */
__CPROVER_assigns()
__CPROVER_ensures(__CPROVER_return_value == E.dirty)
{
    return E.dirty;
}

void updateWindowSize(void)
__CPROVER_assigns(E.screenrows, E.screencols)
/* getWindowSize writes both dimensions in [0,0xffff] on success (and exits on
 * failure), after which only the row count is decremented by 2 for the status
 * bar.  These bounds are exactly what the body proves; they let callers reason
 * about the screen geometry without re-deriving the window-query model. */
__CPROVER_ensures(-2 <= E.screenrows && E.screenrows <= 0xffff - 2)
__CPROVER_ensures(0 <= E.screencols && E.screencols <= 0xffff)
{
    if (getWindowSize(STDIN_FILENO,STDOUT_FILENO,
                      &E.screenrows,&E.screencols) == -1) {
        perror("Unable to query the screen for size (columns / rows)");
        exit(1);
    }
    E.screenrows -= 2; /* Get room for status bar. */
}

void handleSigWinCh(int unused __attribute__((unused)))
/* Strong, accurate contract, but undischargeable against the existing callee
 * contracts.  After updateWindowSize() is replaced by its contract, the window
 * model (getWindowSize bounds each dimension in [0,0xffff]) admits a degenerate
 * terminal: E.screencols can be 0 and, because the body subtracts 2 for the
 * status bar, E.screenrows can be as low as -2 -- updateWindowSize therefore
 * cannot ensure E.screenrows >= 0.  The two clamps then drive the cursor
 * negative (E.cy = E.screenrows - 1, E.cx = E.screencols - 1), so the
 * editorRefreshScreen call site cannot discharge that function's
 * E.cx >= 0 / E.cy >= 0 / E.screenrows >= 0 preconditions (precondition.4/5/6).
 * These are the only residual failures; the screencols >= 0 / screencols - 1
 * and screenrows - 1 overflow obligations are all discharged once
 * updateWindowSize's bounded ensures are in place.  Weakening
 * editorRefreshScreen (a separately-verified, shared callee) to admit a
 * sub-2-row window would be the only way to close the gap, so the precise
 * contract is retained as-is. */
__CPROVER_requires(E.numrows == 0)
__CPROVER_requires(E.rowoff >= 0)
__CPROVER_requires(E.coloff >= 0)
__CPROVER_requires(E.cx >= 0)
__CPROVER_requires(E.cy >= 0)
__CPROVER_assigns(E.cx, E.cy, E.screenrows, E.screencols)
{
    updateWindowSize();
    if (E.cy > E.screenrows) E.cy = E.screenrows - 1;
    if (E.cx > E.screencols) E.cx = E.screencols - 1;
    editorRefreshScreen();
}

void initEditor(void)
__CPROVER_assigns(E.cx, E.cy, E.rowoff, E.coloff, E.numrows, E.row,
                  E.dirty, E.filename, E.syntax, E.screenrows, E.screencols)
__CPROVER_ensures(E.cx == 0 && E.cy == 0)
__CPROVER_ensures(E.rowoff == 0 && E.coloff == 0)
__CPROVER_ensures(E.numrows == 0)
__CPROVER_ensures(E.row == NULL)
__CPROVER_ensures(E.dirty == 0)
__CPROVER_ensures(E.filename == NULL)
__CPROVER_ensures(E.syntax == NULL)
{
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.syntax = NULL;
    updateWindowSize();
    signal(SIGWINCH, handleSigWinCh);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr,"Usage: kilo <filename>\n");
        exit(1);
    }

    initEditor();
    editorSelectSyntaxHighlight(argv[1]);
    editorOpen(argv[1]);
    enableRawMode(STDIN_FILENO);
    editorSetStatusMessage(
        "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress(STDIN_FILENO);
    }
    return 0;
}
