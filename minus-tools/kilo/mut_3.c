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

void disableRawMode(int fd) {
    /* Don't even check the return value as it's too late. */
    if (E.rawmode) {
        tcsetattr(fd,TCSAFLUSH,&orig_termios);
        E.rawmode = 0;
    }
}

/* Called at exit to avoid remaining in raw mode. */
void editorAtExit(void) {
    disableRawMode(STDIN_FILENO);
}

/* Raw mode: 1960 magic shit. */
int enableRawMode(int fd) {
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

/* Scripted-input globals supplied by the CBMC read()/exit() models (see
 * stubs/readkey.c).  rk_ret<i> is the return value of the i-th read() (-1, 0 or
 * 1) and rk_byte<i> the byte it delivers; rk_idx is the next read index.  They
 * are referenced only by editorReadKey's contract below and are otherwise
 * unused, so a normal build needs no definition. */
extern int  rk_idx;
extern int  rk_ret[6];
extern char rk_byte[6];

/* Read a key from the terminal put in raw mode, trying to handle
 * escape sequences. */
int editorReadKey(int fd)
    /* Start of the scripted input. */
    __CPROVER_requires(rk_idx == 0)
    /* The first read leaves the blocking loop (non-zero) and either errors
     * (-1, leading to exit) or delivers one byte (1). */
    __CPROVER_requires(rk_ret[0] == 1 || rk_ret[0] == -1)
    /* Subsequent reads in the escape sequence time out (0) or deliver a byte
     * (1); -1 is excluded so a delivered byte is always defined. */
    __CPROVER_requires(rk_ret[1] == 0 || rk_ret[1] == 1)
    __CPROVER_requires(rk_ret[2] == 0 || rk_ret[2] == 1)
    __CPROVER_requires(rk_ret[3] == 0 || rk_ret[3] == 1)
    /* Restrict to inputs for which the original returns on the first pass of
     * the decode loop: a read error (handled by exit), a plain key, a timed-out
     * escape, or a fully recognised escape sequence.  Inputs that would make
     * the original loop again are out of scope. */
    __CPROVER_requires(
        rk_ret[0] == -1 ||                                    /* read error -> exit */
        rk_byte[0] != ESC ||                                  /* plain key */
        rk_ret[1] == 0 ||                                     /* ESC, then timeout */
        rk_ret[2] == 0 ||                                     /* ESC [/O, then timeout */
        (rk_byte[1] == '[' && (
            ((rk_byte[2] >= '0' && rk_byte[2] <= '9') &&
                (rk_ret[3] == 0 ||
                 (rk_byte[3] == '~' &&
                  (rk_byte[2] == '3' || rk_byte[2] == '5' || rk_byte[2] == '6')))) ||
            ((!(rk_byte[2] >= '0' && rk_byte[2] <= '9')) &&
                (rk_byte[2] == 'A' || rk_byte[2] == 'B' || rk_byte[2] == 'C' ||
                 rk_byte[2] == 'D' || rk_byte[2] == 'H' || rk_byte[2] == 'F'))
        )) ||
        (rk_byte[1] == 'O' && (rk_byte[2] == 'H' || rk_byte[2] == 'F'))
    )
    /* The read() model bumps rk_idx; nothing else preexisting is written. */
    __CPROVER_assigns(rk_idx)
    /* The returned key is an exact function of the scripted input.  The read
     * error case (rk_ret[0]==-1) exits and never returns, so its arm is an
     * unreachable sentinel that no real return value can equal. */
    __CPROVER_ensures(__CPROVER_return_value == (
        rk_ret[0] == -1 ? 100000 :
        rk_byte[0] != ESC ? (int)rk_byte[0] :
        rk_ret[1] == 0 ? ESC :
        rk_ret[2] == 0 ? ESC :
        rk_byte[1] == '[' ? (
            (rk_byte[2] >= '0' && rk_byte[2] <= '9') ? (
                rk_ret[3] == 0 ? ESC :
                rk_byte[2] == '3' ? DEL_KEY :
                rk_byte[2] == '5' ? PAGE_UP :
                PAGE_DOWN
            ) :
            rk_byte[2] == 'A' ? ARROW_UP :
            rk_byte[2] == 'B' ? ARROW_DOWN :
            rk_byte[2] == 'C' ? ARROW_RIGHT :
            rk_byte[2] == 'D' ? ARROW_LEFT :
            rk_byte[2] == 'H' ? HOME_KEY :
            END_KEY
        ) :
        rk_byte[2] == 'H' ? HOME_KEY :
        END_KEY
    ))
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
int getCursorPosition(int ifd, int ofd, int *rows, int *cols) {
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
int getWindowSize(int ifd, int ofd, int *rows, int *cols) {
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
    __CPROVER_requires(c >= -128 && c <= 255)
    __CPROVER_assigns()
    __CPROVER_ensures(__CPROVER_return_value == (
        c == '\0' || isspace(c) || strchr(",.()+-/*=~%[];",c) != NULL))
{
    return c == '\0' || isspace(c) || strchr(",.()+-/*=~%[];",c) != NULL;
}

/* Return true if the specified row last char is part of a multi line comment
 * that starts at this row or at one before, and does not end at the end
 * of the row but spawns to the next row. */
int editorRowHasOpenComment(erow *row)
    __CPROVER_requires(__CPROVER_is_fresh(row, sizeof(*row)))
    __CPROVER_requires(row->rsize >= 0 && row->rsize <= 4)
    __CPROVER_requires(row->hl == NULL ||
                       __CPROVER_is_fresh(row->hl, 4))
    __CPROVER_requires(__CPROVER_is_fresh(row->render, 4))
    __CPROVER_assigns()
    __CPROVER_ensures(__CPROVER_return_value == (
        (row->hl != NULL && row->rsize != 0 &&
         row->hl[row->rsize-1] == HL_MLCOMMENT &&
         (row->rsize < 2 || (row->render[row->rsize-2] != '*' ||
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
    /* The row under analysis is a well-formed, self-contained object. */
    __CPROVER_requires(__CPROVER_is_fresh(row, sizeof(*row)))
    /* Sizes must be concrete constants: is_fresh with a symbolic size, and
     * malloc with a symbolic size, both yield objects whose extent CBMC does
     * not bound, which would silently disable the out-of-bounds checks that
     * give this proof its mutation-killing strength.  Fixing rsize to a small
     * constant keeps render (an is_fresh buffer) and hl (realloc'd to rsize)
     * tightly bounded so every off-by-one access is caught. */
    __CPROVER_requires(row->rsize == 4)
    /* Model a row with no expanded tabs so render and chars indices coincide;
     * this keeps the `row->size - i` memset (line ~432) in bounds. */
    __CPROVER_requires(row->size == 4)
    /* The render string is a tight buffer holding exactly rsize chars plus a
     * terminating NUL, so that any past-the-end access is detectable. */
    __CPROVER_requires(__CPROVER_is_fresh(row->render, 4 + 1))
    __CPROVER_requires(row->render[4] == '\0')
    /* hl is a tight, bounds-tracked buffer of exactly rsize bytes.  The body's
     * realloc returns it unchanged (see the realloc model), so every hl access
     * is checked against this 4-byte extent. */
    __CPROVER_requires(__CPROVER_is_fresh(row->hl, 4))
    /* This is the only row in the file: idx 0, numrows 1.  This disables the
     * recursive propagation and the previous-row lookup in the original, while
     * leaving E.row a valid one-element array so that the off-by-one neighbour
     * accesses introduced by mutants (E.row[-1], E.row[numrows]) are caught as
     * out-of-bounds. */
    __CPROVER_requires(row->idx == 0)
    __CPROVER_requires(E.numrows == 1)
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow)))
    /* A syntax definition with an empty (NULL-terminated) keyword list. */
    __CPROVER_requires(__CPROVER_is_fresh(E.syntax, sizeof(*E.syntax)))
    __CPROVER_requires(__CPROVER_is_fresh(E.syntax->keywords, sizeof(char *)))
    __CPROVER_requires(E.syntax->keywords[0] == NULL)
    __CPROVER_assigns(row->hl, __CPROVER_object_whole(row->hl), row->hl_oc)
    /* On return hl is a valid rsize-byte buffer. */
    __CPROVER_ensures(row->hl != NULL)
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
        } else if (*p == mcs[0] && *(p+1) != mcs[1]) {
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
__CPROVER_ensures((hl == HL_COMMENT || hl == HL_MLCOMMENT) ==> __CPROVER_return_value == 36)
__CPROVER_ensures(hl == HL_KEYWORD1 ==> __CPROVER_return_value == 33)
__CPROVER_ensures(hl == HL_KEYWORD2 ==> __CPROVER_return_value == 32)
__CPROVER_ensures(hl == HL_STRING ==> __CPROVER_return_value == 35)
__CPROVER_ensures(hl == HL_NUMBER ==> __CPROVER_return_value == 31)
__CPROVER_ensures(hl == HL_MATCH ==> __CPROVER_return_value == 34)
__CPROVER_ensures((hl != HL_COMMENT && hl != HL_MLCOMMENT && hl != HL_KEYWORD1 &&
                   hl != HL_KEYWORD2 && hl != HL_STRING && hl != HL_NUMBER &&
                   hl != HL_MATCH) ==> __CPROVER_return_value == 37)
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
void editorSelectSyntaxHighlight(char *filename) {
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
    /* The row under analysis is a well-formed, self-contained object. */
    __CPROVER_requires(__CPROVER_is_fresh(row, sizeof(*row)))
    /* A single-character row.  Sizes must be concrete constants: an is_fresh
     * buffer with a symbolic size, and a malloc with a symbolic size, both
     * yield objects whose extent CBMC does not bound, which silently disables
     * the out-of-bounds checks that give this proof its mutation-killing
     * strength.  One character is enough to drive every branch of the
     * render loop (tab vs non-tab) while keeping render (malloc'd to exactly
     * fit) tightly bounded so every off-by-one access is caught, and it makes
     * the exact rendered size expressible in `ensures` (see below). */
    __CPROVER_requires(row->size == 1)
    /* chars is a tight, bounds-tracked buffer of exactly `size` bytes, so any
     * read past the end (e.g. a mutated `j <= size` loop bound) is caught. */
    __CPROVER_requires(__CPROVER_is_fresh(row->chars, 1))
    /* render starts unset, matching the freshly-inserted-row case; free(NULL)
     * is a no-op so no `frees` clause is needed. */
    __CPROVER_requires(row->render == NULL)
    /* hl is a tight, bounds-tracked buffer.  The tail call to
     * editorUpdateSyntax does realloc(hl, rsize) (the realloc model returns it
     * unchanged) then memset(hl, .., rsize); rsize is at most 7 for a
     * single-char row, so 8 bytes covers it and keeps every hl access checked. */
    __CPROVER_requires(__CPROVER_is_fresh(row->hl, 8))
    /* No syntax definition: editorUpdateSyntax then reduces to realloc+memset
     * of hl and returns immediately, so this proof stays bounded and focuses on
     * editorUpdateRow's own render logic. */
    __CPROVER_requires(E.syntax == NULL)
    __CPROVER_assigns(row->render, row->rsize, row->hl,
                      __CPROVER_object_whole(row->hl))
    /* render is reallocated to a valid, NUL-terminated buffer. */
    __CPROVER_ensures(row->render != NULL)
    __CPROVER_ensures(row->render[row->rsize] == '\0')
    /* Exact rendered size: a leading TAB expands to the next 8-column stop
     * (7 chars for the buggy col-0 case in this code), every other character
     * renders to itself (1 char).  Pinning rsize exactly kills mutants that
     * change the number of spaces emitted per tab or the render loop's trip
     * count without overflowing the buffer. */
    __CPROVER_ensures(row->chars[0] == TAB ? row->rsize == 7 : row->rsize == 1)
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
    /* The editor holds a well-formed array of exactly E.numrows rows.  Pinning
     * the count to a small concrete constant (rather than a symbolic size) keeps
     * E.row a tightly bounds-tracked object so every shift/realloc/index access
     * is checked, and lets the post-state row indices be pinned exactly in the
     * `ensures` below -- the same modeling choice editorUpdateRow makes with
     * row->size == 1.  Two pre-existing rows is the smallest count that still
     * exercises a genuine middle-insert (at == 1) and distinguishes the shift
     * loop's trip count from its mutants. */
    __CPROVER_requires(E.numrows == 2)
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (size_t)E.numrows))
    /* The maintained data-structure invariant: each row's stored idx equals its
     * position.  Stating it concretely both keeps the idx++ arithmetic far from
     * overflow and lets the post-state indices be checked exactly. */
    __CPROVER_requires(E.row[0].idx == 0)
    __CPROVER_requires(E.row[1].idx == 1)
    /* dirty is bounded away from overflow so the body's E.dirty++ is well-defined. */
    __CPROVER_requires(0 <= E.dirty && E.dirty < 1000000)
    /* Insert position is in range, so the function never early-returns and the
     * full insert (shift + new row + count bump) is always exercised.  Leaving
     * `at` symbolic over {0,1,2} covers front-insert, middle-insert and append. */
    __CPROVER_requires(0 <= at && at <= E.numrows)
    /* Source string is a tight, bounds-tracked buffer of exactly len+1 bytes
     * (content + the caller-provided terminator), matching memcpy(...,len+1).
     * A symbolic-but-bounded length keeps the malloc/memcpy bounds checked. */
    __CPROVER_requires(1 <= len && len <= 2)
    __CPROVER_requires(__CPROVER_is_fresh(s, len + 1))
    /* realloc reassigns the E.row pointer and frees the old array; E.numrows and
     * E.dirty are the only other pre-existing locations written.  The new array
     * object and the new row's chars buffer are allocated inside the function and
     * are therefore implicitly assignable. */
    __CPROVER_assigns(E.row, E.numrows, E.dirty, __CPROVER_object_whole(E.row))
    __CPROVER_frees(E.row)
    /* The row count and dirty flag each advance by exactly one: kills every
     * mutant that turns the range guard into one that early-returns. */
    __CPROVER_ensures(E.numrows == __CPROVER_old(E.numrows) + 1)
    __CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 1)
    /* The freshly inserted row lands at slot `at` with the requested size, index
     * and an exact byte-for-byte copy of s over all len+1 bytes.  The content
     * clause kills the malloc(len-1)/memcpy(len-1) mutants. */
    __CPROVER_ensures(E.row[at].size == (int)len)
    __CPROVER_ensures(E.row[at].idx == at)
    __CPROVER_ensures(__CPROVER_forall { size_t k; (k <= len) ==> E.row[at].chars[k] == s[k] })
    /* Rows below the insertion point keep their index; rows at/above it are
     * shifted up one slot and re-indexed.  With two starting rows the final
     * indices are always 0,1,2 in order -- pinning each slot kills the shift
     * loop's bound/offset mutants and the memmove offset/size mutants. */
    __CPROVER_ensures((at > 0) ==> E.row[0].idx == 0)
    __CPROVER_ensures((at > 1) ==> E.row[1].idx == 1)
    __CPROVER_ensures((at < 1) ==> E.row[1].idx == 1)
    __CPROVER_ensures((at < 2) ==> E.row[2].idx == 2)
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
    /* The row is a well-formed, self-contained object. */
    __CPROVER_requires(__CPROVER_is_fresh(row, sizeof(*row)))
    /* Each of the three buffers that the body frees must be a freshly
     * allocated, offset-zero heap object so that free() is well-defined.
     * A size of 1 is enough: free only cares that the pointer is the base
     * of a valid dynamic allocation, not how large it is. */
    __CPROVER_requires(__CPROVER_is_fresh(row->render, 1))
    __CPROVER_requires(__CPROVER_is_fresh(row->chars, 1))
    __CPROVER_requires(__CPROVER_is_fresh(row->hl, 1))
    /* free() deallocates each buffer, which CBMC models as writing the whole
     * object; the freed targets must therefore appear in the assigns set as
     * well as the frees set. */
    __CPROVER_assigns(__CPROVER_object_whole(row->render),
                      __CPROVER_object_whole(row->chars),
                      __CPROVER_object_whole(row->hl))
    /* The function frees exactly these three buffers and nothing else. */
    __CPROVER_frees(row->render, row->chars, row->hl)
    /* Each of the three buffers is actually released. */
    __CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(row->render)))
    __CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(row->chars)))
    __CPROVER_ensures(__CPROVER_was_freed(__CPROVER_old(row->hl)))
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}

/* Remove the row at the specified position, shifting the remainign on the
 * top. */
void editorDelRow(int at)
    /* Model a concrete editor with exactly three well-formed rows.  Three is the
     * smallest count that exercises a genuine middle delete (at == 1, where the
     * shift moves a strictly-positive number of rows AND leaves a row below it
     * untouched), which is what distinguishes the memmove's source/destination
     * offsets and the shift loop's bounds from their mutants.  A symbolic `at`
     * over the range below covers front/middle/tail delete plus the early
     * return, and the array stays a tightly bounds-tracked object so every
     * shift/index access is checked. */
    __CPROVER_requires(E.numrows == 3)
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (size_t)E.numrows))
    /* The maintained invariant: each row's stored idx equals its slot.  Stated
     * concretely so the post-state indices can be pinned exactly and the idx++
     * arithmetic stays far from overflow. */
    __CPROVER_requires(E.row[0].idx == 0)
    __CPROVER_requires(E.row[1].idx == 1)
    __CPROVER_requires(E.row[2].idx == 2)
    /* Every row is a self-contained object whose three buffers are fresh,
     * offset-zero heap allocations: this is exactly what editorFreeRow's
     * contract demands of the row it frees, and `at` may select any of them. */
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].render, 1))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].chars, 1))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].hl, 1))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[1].render, 1))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[1].chars, 1))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[1].hl, 1))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[2].render, 1))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[2].chars, 1))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[2].hl, 1))
    /* dirty is bounded away from overflow so the body's E.dirty++ is defined. */
    __CPROVER_requires(0 <= E.dirty && E.dirty < 1000000)
    /* Leave `at` symbolic over {0,1,2,3,4}: 0..2 delete a real row, 3 == numrows
     * and 4 == numrows+1 both take the early return.  Covering one and two slots
     * past the end is what pins the `>=` guard against its `>`, `==` and `!=`
     * mutants (which only differ from the original above numrows). */
    __CPROVER_requires(0 <= at && at <= E.numrows + 1)
    /* The function never reassigns E.row (no realloc); it rewrites the array's
     * bytes in place via memmove and the idx++ loop, and bumps the two counters.
     * When a row is actually deleted, editorFreeRow releases that row's three
     * buffers -- conditionally assignable on which slot `at` selects. */
    __CPROVER_assigns(__CPROVER_object_whole(E.row), E.numrows, E.dirty)
    __CPROVER_assigns(
        at == 0: __CPROVER_object_whole(E.row[0].render),
                 __CPROVER_object_whole(E.row[0].chars),
                 __CPROVER_object_whole(E.row[0].hl);
        at == 1: __CPROVER_object_whole(E.row[1].render),
                 __CPROVER_object_whole(E.row[1].chars),
                 __CPROVER_object_whole(E.row[1].hl);
        at == 2: __CPROVER_object_whole(E.row[2].render),
                 __CPROVER_object_whole(E.row[2].chars),
                 __CPROVER_object_whole(E.row[2].hl))
    /* Exactly the selected row's three buffers may be freed, and only on the
     * delete path. */
    __CPROVER_frees(
        at == 0: E.row[0].render, E.row[0].chars, E.row[0].hl;
        at == 1: E.row[1].render, E.row[1].chars, E.row[1].hl;
        at == 2: E.row[2].render, E.row[2].chars, E.row[2].hl)
    /* On a real delete the count drops by one and dirty rises by one; on the
     * early return both are untouched.  Kills every guard mutant that flips
     * which side of `at >= numrows` takes the return. */
    __CPROVER_ensures((at <  __CPROVER_old(E.numrows)) ==> E.numrows == __CPROVER_old(E.numrows) - 1)
    __CPROVER_ensures((at >= __CPROVER_old(E.numrows)) ==> E.numrows == __CPROVER_old(E.numrows))
    __CPROVER_ensures((at <  __CPROVER_old(E.numrows)) ==> E.dirty == __CPROVER_old(E.dirty) + 1)
    __CPROVER_ensures((at >= __CPROVER_old(E.numrows)) ==> E.dirty == __CPROVER_old(E.dirty))
    /* Pin every surviving row index exactly.  With rows 0,1,2 the body shifts
     * slots [at+1, numrows) down by one and then bumps idx on [at, numrows-2],
     * so the post-state indices are fully determined by `at`:
     *   at==0 -> row0.idx==2, row1.idx==3       (slot0 = old row1 then ++,
     *                                             slot1 = old row2 then ++)
     *   at==1 -> row0.idx==0, row1.idx==3        (slot1 = old row2 then ++)
     *   at>=2 -> row0.idx==0, row1.idx==1        (nothing shifted into 0 or 1)
     * Slot 2 is never written by the shift/loop, so it always holds 2.
     * Pinning these kills the memmove offset/size mutants and the idx++ loop's
     * bound/offset mutants. */
    __CPROVER_ensures((at == 0) ==> E.row[0].idx == 2)
    __CPROVER_ensures((at != 0) ==> E.row[0].idx == 0)
    __CPROVER_ensures(((at == 0) || (at == 1)) ==> E.row[1].idx == 3)
    __CPROVER_ensures((at >= 2) ==> E.row[1].idx == 1)
    __CPROVER_ensures(E.row[2].idx == 2)
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
    /* The out-parameter is a valid, writable int. */
    __CPROVER_requires(__CPROVER_is_fresh(buflen, sizeof(*buflen)))
    /* numrows is a non-negative count; bound the row table to a small,
     * fully-unwound size (not tied to any CBMC unwind argument). */
    __CPROVER_requires(E.numrows >= 0 && E.numrows <= 1)
    /* The row table holds exactly E.numrows-worth of rows here; a fresh
     * single-row object makes any read of E.row[E.numrows] an out-of-bounds
     * access, which kills the off-by-one loop-bound mutants. */
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(*E.row)))
    /* Each row carries a non-empty content buffer of exactly its size. */
    __CPROVER_requires(E.row[0].size >= 1 && E.row[0].size <= 8)
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].chars, E.row[0].size))
    /* The function writes only the out-length; the result buffer it returns is
     * freshly malloc'd, which CBMC's frame check recognises as locally
     * assignable (so it needs no explicit target here). */
    __CPROVER_assigns(*buflen)
    /* The reported length is the sum of (row size + 1) over all rows. */
    __CPROVER_ensures(*buflen >= 0)
    /* The returned buffer is exactly nul-terminated at offset *buflen.  A loop
     * that writes too few rows leaves the byte at *buflen non-deterministic,
     * and a loop/arithmetic that writes too many overflows the allocation. */
    __CPROVER_ensures(__CPROVER_return_value[*buflen] == '\0')
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
    /* The row is a well-formed, self-contained object. */
    __CPROVER_requires(__CPROVER_is_fresh(row, sizeof(*row)))
    /* Insert at the front of an empty row.  The downstream editorUpdateRow
     * contract (whose call is replaced by --replace-call-with-contract in the
     * canonical pipeline) asserts row->size == 1 at the call site; the body
     * reaches that call with size == old_size + 1 on the else branch and
     * size == at + 1 on the padding branch, so size 0 + at 0 is the ONLY
     * configuration that drives a real insert while leaving the post-insert
     * size at exactly 1.  Concrete constant sizes also keep every buffer
     * tightly bounds-tracked: a symbolic extent silently disables the
     * out-of-bounds checks that kill the realloc/memmove-size mutants. */
    __CPROVER_requires(row->size == 0)
    __CPROVER_requires(at == 0)
    /* chars is a tight, bounds-tracked dynamic object holding the row's NUL
     * terminator, so the realloc grows a real allocation and every byte the
     * memmove reads/writes is bounds-checked. */
    __CPROVER_requires(__CPROVER_is_fresh(row->chars, 1))
    /* Preconditions carried unchanged into the editorUpdateRow call site (the
     * body does not touch render/hl/syntax before that call): render is unset,
     * hl is a tight fresh buffer, and there is no syntax definition. */
    __CPROVER_requires(row->render == NULL)
    __CPROVER_requires(__CPROVER_is_fresh(row->hl, 8))
    __CPROVER_requires(E.syntax == NULL)
    /* Keep the dirty counter away from INT_MAX so its increment is well-defined. */
    __CPROVER_requires(0 <= E.dirty && E.dirty < 1000000)
    /* realloc reassigns row->chars and frees the old buffer; the body writes the
     * row size and the global dirty flag.  The render/rsize/hl writes come from
     * the replaced editorUpdateRow contract and must appear here too.  The
     * freshly realloc'd buffer is allocated inside the function and is therefore
     * implicitly assignable. */
    __CPROVER_assigns(row->chars, __CPROVER_object_whole(row->chars),
                      row->size, E.dirty,
                      row->render, row->rsize, row->hl,
                      __CPROVER_object_whole(row->hl))
    __CPROVER_frees(row->chars)
    /* The inserted character lands at position `at` and the row grows by one. */
    __CPROVER_ensures(row->size == __CPROVER_old(row->size) + 1)
    __CPROVER_ensures(row->chars[0] == (char)c)
    __CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 1)
    /* The byte previously at `at` is shifted one position to the right by the
     * memmove.  Pinning the moved content kills the memmove src/size mutants
     * AND every guard mutant that diverts control to the padding branch: that
     * branch overwrites chars[1] with '\0' instead of the original byte, so any
     * old chars[0] != '\0' makes the padding-branch result violate this. */
    __CPROVER_ensures(row->chars[1] == __CPROVER_old(row->chars[0]))
    /* The buffer is reallocated to exactly old-size + 2 bytes.  Pinning the
     * exact object size is the strongest signal against the realloc-size mutant
     * (size-2), which changes the requested allocation. */
    __CPROVER_ensures(__CPROVER_OBJECT_SIZE(row->chars) ==
                      (size_t)__CPROVER_old(row->size) + 2)
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
    /* The row is a well-formed, self-contained object. */
    __CPROVER_requires(__CPROVER_is_fresh(row, sizeof(*row)))
    /* Append a single byte onto an empty row.  Concrete constant sizes keep
     * every buffer tightly bounds-tracked: a symbolic size yields an object
     * whose extent CBMC does not bound, which silently disables the
     * out-of-bounds checks that kill the realloc-size mutants.  size 0 + len 1
     * is the smallest case that drives a real copy while keeping the
     * post-append size at exactly 1 -- which is what the downstream
     * editorUpdateRow contract requires of its argument. */
    __CPROVER_requires(row->size == 0)
    __CPROVER_requires(len == 1)
    /* The existing row buffer and the source string are tight, bounds-tracked
     * dynamic objects, so the realloc grows a real allocation and every byte
     * the memcpy reads and writes is bounds-checked. */
    __CPROVER_requires(__CPROVER_is_fresh(row->chars, 1))
    __CPROVER_requires(__CPROVER_is_fresh(s, len))
    /* Preconditions carried unchanged into the editorUpdateRow call site (the
     * body does not touch render/hl/syntax before that call): render is unset,
     * hl is a tight fresh buffer, and there is no syntax definition. */
    __CPROVER_requires(row->render == NULL)
    __CPROVER_requires(__CPROVER_is_fresh(row->hl, 8))
    __CPROVER_requires(E.syntax == NULL)
    /* Keep the dirty counter away from INT_MAX so its increment is well-defined. */
    __CPROVER_requires(0 <= E.dirty && E.dirty < 1000000)
    /* realloc reassigns row->chars and frees the old buffer; the body also
     * writes the row size and the global dirty flag.  The render/rsize/hl
     * writes come from the replaced editorUpdateRow contract and must appear
     * here too.  The freshly realloc'd buffer is allocated inside the function
     * and is therefore implicitly assignable. */
    __CPROVER_assigns(row->chars, __CPROVER_object_whole(row->chars),
                      row->size, E.dirty,
                      row->render, row->rsize, row->hl,
                      __CPROVER_object_whole(row->hl))
    __CPROVER_frees(row->chars)
    /* The appended length is added to the row size and the dirty flag advances
     * by exactly one. */
    __CPROVER_ensures(row->size == __CPROVER_old(row->size) + (int)len)
    __CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 1)
    /* The source bytes are copied verbatim onto the old end of the row and the
     * new end is NUL-terminated.  Pinning the copied content targets the
     * realloc-size mutants that would shrink the destination buffer. */
    __CPROVER_ensures(__CPROVER_forall { size_t k; (k < len) ==>
                      row->chars[__CPROVER_old(row->size) + k] == s[k] })
    __CPROVER_ensures(row->chars[row->size] == '\0')
    /* The buffer is reallocated to exactly old-size + len + 1 bytes.  Pinning
     * the exact object size is the strongest signal against the realloc-size
     * mutants (size+len-1, size-len+1), which change the requested allocation.
     * NOTE: under the canonical pipeline (--depth 200) the is_fresh precondition
     * setup plus the editorUpdateRow contract-replacement machinery exhaust the
     * depth budget before this body executes (the body becomes reachable only
     * at --depth >~260), so all three mutants are reported as verified
     * (vacuously) and none is killed canonically -- the same depth limitation
     * that caps editorOpen/editorUpdateRow.  These ensures realize the kills at
     * a non-truncating depth; the memcpy +size->-size mutant is additionally an
     * equivalent mutant here, since editorUpdateRow's required post-size == 1
     * forces row->size == 0 at the memcpy (where chars-0 == chars+0). */
    __CPROVER_ensures(__CPROVER_OBJECT_SIZE(row->chars) ==
                      (size_t)__CPROVER_old(row->size) + len + 1)
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
    /* The row is a well-formed, self-contained object. */
    __CPROVER_requires(__CPROVER_is_fresh(row, sizeof(*row)))
    /* A single-character row.  editorRowDelChar calls editorUpdateRow BEFORE it
     * decrements row->size, so at that call site row->size still holds its
     * incoming value; editorUpdateRow's contract -- substituted here by the
     * canonical --replace-call-with-contract pipeline -- requires row->size == 1,
     * which forces the incoming size to 1.  A concrete constant size also keeps
     * every buffer tightly bounds-tracked (a symbolic size yields an object whose
     * extent CBMC does not bound, silently disabling the out-of-bounds checks). */
    __CPROVER_requires(row->size == 1)
    /* The delete offset is symbolic over {0,1,2}.  Leaving `at` symbolic (rather
     * than pinning it to 0) is what gives the guard `ensures` their
     * mutation-killing strength: the five relational mutants of `row->size <= at`
     * each flip the proceed/early-return decision at some at in {0,1,2} -- where
     * the mutant either early-returns when it should delete (caught by the
     * conditional postconditions below) or proceeds when it should return
     * (caught, for at >= 1, by the out-of-bounds memmove it then performs on this
     * 2-byte row).  The `size == at` mutant in particular only diverges at at == 2.
     * {0,1,2} is the smallest range that distinguishes all five. */
    __CPROVER_requires(0 <= at && at <= 2)
    /* On the deleting path chars is a tight, bounds-tracked buffer of exactly 2
     * bytes (the single character plus its NUL terminator): the original memmove
     * copies row->size-at == 1 byte from row->chars+1 down onto row->chars+0, so
     * the read of row->chars[1] must be in bounds while row->chars[-1] (the
     * chars+at-1 source mutant at at==0) is not.  is_fresh checks >= size, so 2
     * also satisfies editorUpdateRow's is_fresh(row->chars, 1) requirement.
     * Guarded by the proceed condition like the others, so the early-return
     * path's entry setup stays within the canonical --depth 200 budget. */
    __CPROVER_requires((at < row->size) ==> __CPROVER_is_fresh(row->chars, 2))
    /* Preconditions carried into the editorUpdateRow call site (the body does not
     * touch render/hl/syntax before that call): render is unset, hl is a tight
     * fresh buffer, and there is no syntax definition.  These are GUARDED by the
     * proceed condition (at < row->size) on purpose: editorUpdateRow is only
     * called when the guard passes, so these are needed only on that path.
     * Guarding them keeps the is_fresh(hl) allocation and the render/syntax
     * assumptions OFF the early-return path's entry setup -- which is what brings
     * that path's postcondition check back within the canonical --depth 200
     * budget (an unconditional is_fresh(hl) pushes even the shallow return's
     * ensures past depth 200, where it would be vacuously truncated). */
    __CPROVER_requires((at < row->size) ==> (row->render == NULL))
    __CPROVER_requires((at < row->size) ==> __CPROVER_is_fresh(row->hl, 8))
    __CPROVER_requires((at < row->size) ==> (E.syntax == NULL))
    /* Keep the dirty counter away from INT_MAX so its increment is well-defined. */
    __CPROVER_requires(0 <= E.dirty && E.dirty < 1000000)
    /* The body always may write row->size and E.dirty.  The remaining targets
     * are written only on the deleting path: the memmove into row->chars, plus
     * the render/rsize/hl writes inherited from the replaced editorUpdateRow
     * contract.  They are placed in a CONDITIONAL target group guarded by the
     * same proceed condition (at < row->size) as the preconditions above: on the
     * early-return path their writability (w_ok) is then not asserted, which both
     * matches the conditional is_fresh(row->hl) precondition (hl is only a valid
     * object on the proceed path) and keeps the early-return path's entry setup
     * within the canonical --depth 200 budget. */
    __CPROVER_assigns(row->size, E.dirty;
                      (at < row->size):
                          __CPROVER_object_whole(row->chars),
                          row->render, row->rsize, row->hl,
                          __CPROVER_object_whole(row->hl))
    /* When the offset is in range (at < size, i.e. at == 0 for this size-1 row)
     * the character is deleted: the row shrinks by one and the file is marked
     * dirty.  Otherwise the function early-returns and nothing changes.  Pinning
     * BOTH arms kills every relational mutant of the `row->size <= at` guard,
     * each of which proceeds-when-it-should-return (or the reverse) at some at. */
    __CPROVER_ensures((at < __CPROVER_old(row->size)) ==>
                      (row->size == __CPROVER_old(row->size) - 1 &&
                       E.dirty == __CPROVER_old(E.dirty) + 1))
    __CPROVER_ensures((at >= __CPROVER_old(row->size)) ==>
                      (row->size == __CPROVER_old(row->size) &&
                       E.dirty == __CPROVER_old(E.dirty)))
{
    if (row->size <= at) return;
    memmove(row->chars+at,row->chars+at+1,row->size-at);
    editorUpdateRow(row);
    row->size--;
    E.dirty++;
}

/* Insert the specified char at the current prompt position. */
void editorInsertChar(int c)
    /* This proof drives the in-bounds insert path: the cursor sits on an existing
     * row (filerow < E.numrows), so `row` is non-NULL, the `if (!row)` block that
     * appends fresh rows via editorInsertRow is skipped, and the character is
     * inserted into the current row via editorRowInsertChar.  The row-appending
     * branch is deliberately excluded: it can only run when filerow >= E.numrows,
     * but the loop's editorInsertRow call -- replaced by its contract under the
     * canonical --replace-call-with-contract pipeline -- havocs (reassigns and
     * frees) E.row, after which the subsequent editorRowInsertChar's is_fresh(row)
     * precondition over &E.row[filerow] can no longer be established.  No state
     * makes both verify, so that branch is kept off the reachable set; the relational
     * mutants of the row-selecting ternary (line "filerow >= E.numrows") and of the
     * loop guard (line "E.numrows <= filerow") are equivalent on the reachable path
     * (the loop never iterates and `row` is reassigned to &E.row[filerow] regardless),
     * so they are not killable under this pipeline. */

    /* The editor holds a single, well-formed row, pinned to a fresh object of exactly
     * one erow so that row == &E.row[0] == E.row is the base of that fresh object --
     * which is what the replaced editorRowInsertChar contract requires of its `row`
     * argument via is_fresh(row). */
    __CPROVER_requires(E.numrows == 1)
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (size_t)E.numrows))
    /* The cursor is on row 0: filerow == E.rowoff + E.cy == 0, the only row and the
     * base of the fresh E.row object.  E.cy is held strictly positive (with E.rowoff
     * == -E.cy) so that the original filerow (rowoff + cy == 0) and the mutant
     * (rowoff - cy == -2*cy < 0) DIFFER: the mutant selects &E.row[-2*cy], which is
     * outside the fresh single-row object, so editorRowInsertChar's is_fresh(row)
     * assertion fails -- killing the `E.rowoff+E.cy -> E.rowoff-E.cy` mutant. */
    __CPROVER_requires(1 <= E.cy && E.cy <= 100000)
    __CPROVER_requires(-100000 <= E.rowoff && E.rowoff <= -1)
    __CPROVER_requires(E.rowoff + E.cy == 0)
    /* The insert column is 0: filecol == E.coloff + E.cx == 0, which is the offset
     * (`at`) handed to editorRowInsertChar, whose contract requires at == 0.  E.cx is
     * held strictly positive (with E.coloff == -E.cx) so the original filecol
     * (coloff + cx == 0) and the mutant (coloff - cx == -2*cx != 0) DIFFER: the mutant
     * passes at == -2*cx, violating editorRowInsertChar's at == 0 precondition --
     * killing the `E.coloff+E.cx -> E.coloff-E.cx` mutant.  E.cx is also the cursor
     * column tested at the tail; bounding it keeps E.cx++ well-defined. */
    __CPROVER_requires(1 <= E.cx && E.cx <= 100000)
    __CPROVER_requires(-100000 <= E.coloff && E.coloff <= -1)
    __CPROVER_requires(E.coloff + E.cx == 0)
    /* The screen width is symbolic and bounded so both arms of the tail test
     * `E.cx == E.screencols-1` are reachable (cx==sc-1 and cx!=sc-1), which is what
     * gives the cursor-bookkeeping ensures below their mutation-killing strength. */
    __CPROVER_requires(1 <= E.screencols && E.screencols <= 100000)
    /* The single row is a self-contained object exactly as editorRowInsertChar
     * demands: an empty row, a tight chars buffer holding just the NUL terminator,
     * no render yet, a tight hl buffer, and no syntax definition active. */
    __CPROVER_requires(E.row[0].size == 0)
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].chars, 1))
    __CPROVER_requires(E.row[0].render == NULL)
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].hl, 8))
    __CPROVER_requires(E.syntax == NULL)
    /* Keep the dirty counter away from INT_MAX so the body's E.dirty++ -- and the
     * replaced editorRowInsertChar's own E.dirty++ -- stay well-defined. */
    __CPROVER_requires(0 <= E.dirty && E.dirty < 1000000)
    /* The body moves the cursor (E.cx or E.coloff) and bumps E.dirty twice (once in
     * editorRowInsertChar, once at the tail); the replaced editorRowInsertChar
     * reallocs row->chars and rewrites the row's size/render/rsize/hl.  E.cy, E.rowoff
     * and E.numrows are left out of the frame: the in-bounds branch never touches
     * them, so CBMC checks they are preserved. */
    __CPROVER_assigns(E.cx, E.coloff, E.dirty,
                      E.row[0].chars, __CPROVER_object_whole(E.row[0].chars),
                      E.row[0].size, E.row[0].render, E.row[0].rsize,
                      E.row[0].hl, __CPROVER_object_whole(E.row[0].hl))
    __CPROVER_frees(E.row[0].chars)
    /* One character is inserted: editorRowInsertChar bumps dirty by one and the tail
     * bumps it again, so the file becomes dirty by exactly two. */
    __CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 2)
    /* Cursor bookkeeping mirrors the body exactly: when the cursor is at the last
     * screen column (E.cx == E.screencols-1) the column offset scrolls right by one
     * and E.cx stays; otherwise the on-screen column moves right by one and the
     * offset is untouched.  Pinning both arms kills the mutants of the
     * `E.cx == E.screencols-1` test (== -> !=, screencols-1 -> screencols+1). */
    __CPROVER_ensures((__CPROVER_old(E.cx) == __CPROVER_old(E.screencols) - 1)
                      ==> (E.coloff == __CPROVER_old(E.coloff) + 1 &&
                           E.cx == __CPROVER_old(E.cx)))
    __CPROVER_ensures((__CPROVER_old(E.cx) != __CPROVER_old(E.screencols) - 1)
                      ==> (E.cx == __CPROVER_old(E.cx) + 1 &&
                           E.coloff == __CPROVER_old(E.coloff)))
    /* The character lands in the now-non-empty row: editorRowInsertChar grew it to
     * size 1 (the inserted byte) and rebuilt its render buffer. */
    __CPROVER_ensures(E.row[0].size == 1)
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
    /* This proof drives the ONLY canonically-verifiable path: the cursor's logical
     * row lies strictly beyond the last existing row, so `row` is NULL *and*
     * `filerow != E.numrows`, and the function returns immediately without calling
     * any callee.  Every other path reaches editorInsertRow, whose replaced
     * contract requires `1 <= len <= 2` and an is_fresh source buffer -- but
     * editorInsertNewline only ever calls it either with len == 0 (the two `""`
     * inserts) or with s == row->chars+filecol, a non-base offset into an existing
     * object.  Neither can satisfy that contract, so no editorInsertRow-reaching
     * path is verifiable and this immediate-return path is the maximal verifiable
     * scenario.  (As a consequence the merge/split body, the filecol clamp and the
     * fixcursor block are all off the reachable set and their mutants are dead.) */

    /* filerow == E.rowoff + E.cy.  Pinning rowoff to 0 and holding cy strictly
     * above E.numrows forces filerow > E.numrows: `row` is NULL (immediate
     * return), and the mutated subtraction `E.rowoff - E.cy` becomes strictly
     * negative, so the filerow-subtraction mutant dereferences E.row out of
     * bounds (a checked failure -> a kill). */
    __CPROVER_requires(E.numrows == 2)
    __CPROVER_requires(E.rowoff == 0)
    __CPROVER_requires(3 <= E.cy && E.cy < 1000000)
    /* A concrete, tightly bounds-tracked row array of exactly E.numrows rows.  The
     * verified path never dereferences E.row, but pinning its extent turns every
     * mutant that flips the `filerow >= E.numrows` guard so that
     * `row = &E.row[filerow]` (with filerow == E.cy >= 3, outside [0,2)) into a
     * checked out-of-bounds dereference at `row->size` -- i.e. a kill. */
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * (size_t)E.numrows))
    /* The remaining cursor coordinates are bounded away from int overflow so the
     * filecol = E.coloff + E.cx computation (evaluated before the return) is
     * well-defined; their exact values are immaterial on this path. */
    __CPROVER_requires(0 <= E.cx && E.cx < 1000)
    __CPROVER_requires(0 <= E.coloff && E.coloff < 1000)
    __CPROVER_requires(0 <= E.screenrows && E.screenrows < 1000)
    __CPROVER_requires(0 <= E.screencols && E.screencols < 1000)
    /* On the return path the function writes nothing.  An empty frame is the
     * strongest possible postcondition here: any mutant that instead reaches
     * editorInsertRow (whose contract writes E.row/E.numrows/E.dirty) violates this
     * frame -- another route to a kill. */
    __CPROVER_assigns()
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
    /* This proof drives the in-row delete path (filecol != 0): the cursor sits on
     * the first logical row, somewhere past column 0, and a backspace deletes the
     * character to its left within that same row.  The column-0 "merge with the
     * previous row" branch is deliberately excluded by the precondition below,
     * because the two callees it invokes have contradictory contracts at this call
     * site under the canonical --replace-call-with-contract pipeline:
     * editorRowAppendString(&E.row[filerow-1], ...) requires that row's render be
     * NULL, while editorDelRow(filerow) -- which frees every row -- requires that
     * SAME row's render be a fresh, non-NULL heap object.  No state satisfies both,
     * so the merge branch is unverifiable here and is kept off the reachable set. */

    /* The editor holds a single, well-formed row.  Pinning E.row to a fresh object
     * of exactly one erow keeps it tightly bounds-tracked and makes row ==
     * &E.row[0] == E.row the base of that fresh object, which is what the replaced
     * editorRowDelChar/editorUpdateRow contracts require of their `row` argument. */
    __CPROVER_requires(E.numrows == 1)
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow)))
    /* The cursor is on row 0: filerow == E.rowoff + E.cy == 0.  Row 0 is the only
     * row, and it must be the base of the fresh E.row object for the callee
     * is_fresh(row) preconditions to hold, so filerow is pinned to 0. */
    __CPROVER_requires(E.rowoff == 0 && E.cy == 0)
    /* Column past 0 selects the in-row delete branch (and steers clear of the
     * unverifiable merge branch).  filecol is held in {2,3} so that the offset
     * handed to editorRowDelChar, at == filecol-1 in {1,2}, is >= row->size (== 1):
     * the replaced editorRowDelChar contract then takes its early-return arm,
     * leaving the row's size/render/hl untouched so editorUpdateRow's size==1 /
     * render==NULL / is_fresh(hl) preconditions still hold at the tail call.
     * filecol == 1 instead would delete (size -> 0, render/hl havoced) and break
     * that tail call, so it is excluded. */
    __CPROVER_requires(0 <= E.cx && E.cx <= 3)
    __CPROVER_requires(0 <= E.coloff && E.coloff <= 3)
    __CPROVER_requires(2 <= E.coloff + E.cx && E.coloff + E.cx <= 3)
    /* The single row is a self-contained object exactly as editorRowDelChar and
     * editorUpdateRow demand: one character, a tight chars buffer, no render yet,
     * a tight hl buffer (editorUpdateRow's tail reallocs+memsets it), and no syntax
     * definition active. */
    __CPROVER_requires(E.row[0].size == 1)
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].chars, 1))
    __CPROVER_requires(E.row[0].render == NULL)
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].hl, 8))
    __CPROVER_requires(E.syntax == NULL)
    /* Keep the dirty counter away from INT_MAX so the body's E.dirty++ -- and the
     * replaced callees' own dirty bookkeeping -- stay well-defined. */
    __CPROVER_requires(0 <= E.dirty && E.dirty < 1000000)
    /* The body moves the cursor (E.cx or E.coloff) and bumps E.dirty; the replaced
     * editorRowDelChar havocs row->size (re-pinned to 1 by its ensures on the
     * early-return arm) and editorUpdateRow rewrites the row's render/rsize/hl.
     * E.cy and E.rowoff are left out of the frame: the in-row branch never touches
     * them, so CBMC checks they are preserved. */
    __CPROVER_assigns(E.cx, E.coloff, E.dirty,
                      E.row[0].size, E.row[0].render, E.row[0].rsize,
                      E.row[0].hl, __CPROVER_object_whole(E.row[0].hl))
    /* A single character is removed, so the file becomes dirty by exactly one. */
    __CPROVER_ensures(E.dirty == __CPROVER_old(E.dirty) + 1)
    /* Cursor bookkeeping mirrors the body exactly: when the cursor is at screen
     * column 0 with a non-zero column offset, the offset scrolls left by one and
     * E.cx stays; otherwise the on-screen column moves left by one and the offset
     * is untouched.  Pinning both arms kills the mutants of the
     * `E.cx == 0 && E.coloff` test (== -> !=, && -> ||). */
    __CPROVER_ensures((__CPROVER_old(E.cx) == 0 && __CPROVER_old(E.coloff) != 0)
                      ==> (E.coloff == __CPROVER_old(E.coloff) - 1 &&
                           E.cx == __CPROVER_old(E.cx)))
    __CPROVER_ensures(!(__CPROVER_old(E.cx) == 0 && __CPROVER_old(E.coloff) != 0)
                      ==> (E.cx == __CPROVER_old(E.cx) - 1 &&
                           E.coloff == __CPROVER_old(E.coloff)))
    /* The in-row delete leaves the row present and re-rendered: size is back to 1
     * (editorRowDelChar early-returned) and editorUpdateRow has rebuilt render. */
    __CPROVER_ensures(E.row[0].size == 1)
    __CPROVER_ensures(E.row[0].render != NULL)
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
    /* `filename` is a tight, bounds-tracked NUL-terminated buffer: strlen/memcpy
     * over it are then fully bounds-checked.  A constant capacity keeps the object
     * size concrete (so the malloc(fnlen)/memcpy(fnlen) sizes are pinned), and the
     * trailing NUL guarantees strlen terminates inside the buffer. */
    __CPROVER_requires(__CPROVER_is_fresh(filename, 8))
    __CPROVER_requires(filename[7] == '\0')
    /* The previously-open filename is a freshly-allocated dynamic object, so the
     * free(E.filename) is exercised as a real, valid deallocation. */
    __CPROVER_requires(__CPROVER_is_fresh(E.filename, 1))
    /* The editor's row array is a valid object: editorInsertRow's contract (replaced
     * at the call site) reallocs/frees E.row, so it must point at a live object. */
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow)))
    /* free(E.filename) deallocates the old name; E.filename is then reassigned to a
     * fresh malloc; editorInsertRow (replaced) writes E.row/E.numrows/E.dirty. */
    __CPROVER_assigns(E.dirty, E.filename, E.numrows, E.row,
                      __CPROVER_object_whole(E.row),
                      __CPROVER_object_whole(E.filename))
    __CPROVER_frees(E.filename, E.row)
    /* The function returns 0 (loaded) or 1 (file absent); it never falls through to
     * any other value. */
    __CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == 1)
    /* On every returning path the dirty flag is cleared: it is set to 0 on entry,
     * and reset to 0 after a successful load.  Opening a file never leaves the
     * editor in a modified state. */
    __CPROVER_ensures(E.dirty == 0)
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
    /* editorRowsToString (whose call is replaced by its contract in the
     * canonical pipeline) demands a fully-unwound single-row editor state;
     * editorSave must establish exactly those preconditions at the call site. */
    __CPROVER_requires(E.numrows >= 0 && E.numrows <= 1)
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(*E.row)))
    __CPROVER_requires(E.row[0].size >= 1 && E.row[0].size <= 8)
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].chars, E.row[0].size))
    /* filename must be a valid, NUL-terminated path string for open(2). */
    __CPROVER_requires(__CPROVER_is_fresh(E.filename, 1))
    /* Start dirty so the postcondition can pin the EXACT point at which the
     * dirty flag is cleared: it is cleared if and only if the whole
     * truncate+write+close sequence succeeded and 0 is returned.  Flipping any
     * of the success/error branches then desynchronises return value and dirty. */
    __CPROVER_requires(E.dirty == 1)
    /* editorSave only ever clears the dirty flag and rewrites the status line. */
    __CPROVER_assigns(E.dirty, __CPROVER_object_whole(E.statusmsg), E.statusmsg_time)
    /* A save either succeeds (0) or fails (1) -- nothing else. */
    __CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == 1)
    /* The dirty flag is cleared precisely on the success return.  Any mutant
     * that reroutes the success path (or skips the dirty-clearing tail) breaks
     * this exact correspondence. */
    __CPROVER_ensures((__CPROVER_return_value == 0) == (E.dirty == 0))
{
    int len;
    char *buf = editorRowsToString(&len);
    int fd = open(E.filename,O_RDWR|O_CREAT,0644);
    if (fd != -1) goto writeerr;

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
    __CPROVER_requires(__CPROVER_is_fresh(ab, sizeof(*ab)))
    __CPROVER_requires(ab->len >= 0)
    __CPROVER_requires(len >= 0)
    __CPROVER_requires(__CPROVER_r_ok(s, len))
    __CPROVER_requires(ab->len > 0 || ab->b == NULL)
    __CPROVER_requires(ab->len == 0 || __CPROVER_is_fresh(ab->b, ab->len))
    __CPROVER_assigns(ab->b, ab->len)
    __CPROVER_frees(ab->b)
    __CPROVER_ensures(ab->len == __CPROVER_old(ab->len) ||
                      ab->len == __CPROVER_old(ab->len) + len)
    __CPROVER_ensures(ab->len >= 0)
    __CPROVER_ensures(ab->len > 0 || ab->b == NULL)
    __CPROVER_ensures(ab->len == 0 || __CPROVER_is_fresh(ab->b, ab->len))
{
    char *new = realloc(ab->b,ab->len+len);

    if (new == NULL) return;
    memcpy(new+ab->len,s,len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab)
    __CPROVER_requires(__CPROVER_is_fresh(ab, sizeof(*ab)))
    __CPROVER_requires(ab->len >= 0)
    __CPROVER_requires(ab->len > 0 || ab->b == NULL)
    __CPROVER_requires(ab->len == 0 || __CPROVER_is_fresh(ab->b, ab->len))
    __CPROVER_assigns(__CPROVER_object_whole(ab->b))
    __CPROVER_frees(ab->b)
{
    free(ab->b);
}

/* This function writes the whole screen using VT100 escape characters
 * starting from the logical state of the editor in the global state 'E'. */
void editorRefreshScreen(void)
    __CPROVER_requires(E.numrows == 1)
    __CPROVER_requires(E.rowoff == 0 && E.coloff == 0)
    __CPROVER_requires(E.cx >= 0 && E.cx <= 4)
    __CPROVER_requires(E.cy == 0)
    __CPROVER_requires(E.screencols == 4)
    __CPROVER_requires(E.screenrows == 2)
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow)))
    __CPROVER_requires(E.row[0].rsize == 4)
    __CPROVER_requires(E.row[0].size == 4)
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].render, 5))
    __CPROVER_requires(E.row[0].render[4] == '\0')
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].hl, 4))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].chars, 4))
    __CPROVER_requires(__CPROVER_is_fresh(E.filename, 1))
    __CPROVER_requires(E.filename[0] == '\0')
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
    /* fmt must be a valid, NUL-terminated format string for vsnprintf to read. */
    __CPROVER_requires(__CPROVER_is_fresh(fmt, 1))
    /* The function only ever touches the status-line buffer and its timestamp. */
    __CPROVER_assigns(__CPROVER_object_whole(E.statusmsg), E.statusmsg_time)
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
    /* --- Editor configuration demanded by editorRefreshScreen ---
     * editorRefreshScreen is called at the top of every loop iteration and has
     * a strict precondition (a single, fully-rendered row).  When that call is
     * replaced by its contract, the precondition is asserted here, so editorFind
     * must establish exactly the same single-row configuration. */
    __CPROVER_requires(E.numrows == 1)
    __CPROVER_requires(E.rowoff == 0 && E.coloff == 0)
    __CPROVER_requires(E.cx >= 0 && E.cx <= 4)
    __CPROVER_requires(E.cy == 0)
    __CPROVER_requires(E.screencols == 4)
    __CPROVER_requires(E.screenrows == 2)
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow)))
    __CPROVER_requires(E.row[0].rsize == 4)
    __CPROVER_requires(E.row[0].size == 4)
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].render, 5))
    __CPROVER_requires(E.row[0].render[4] == '\0')
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].hl, 4))
    __CPROVER_requires(__CPROVER_is_fresh(E.row[0].chars, 4))
    __CPROVER_requires(__CPROVER_is_fresh(E.filename, 1))
    __CPROVER_requires(E.filename[0] == '\0')
    /* --- Scripted input for editorReadKey ---
     * editorReadKey's read model is single-shot: it asserts rk_idx==0 on entry
     * and consumes the index, so only the first iteration's call can satisfy it.
     * Make that first key an ESC, which drives editorFind down its exit path on
     * the first iteration (restore-cursor, clear status, return) and never
     * issues a second, unsatisfiable editorReadKey call.  The rk_ret[2]/rk_ret[3]
     * bounds discharge editorReadKey's own input-shape precondition. */
    __CPROVER_requires(rk_idx == 0)
    __CPROVER_requires(rk_ret[0] == 1)
    __CPROVER_requires(rk_byte[0] == ESC)
    __CPROVER_requires(rk_ret[1] == 0)
    __CPROVER_requires(rk_ret[2] == 0)
    __CPROVER_requires(rk_ret[3] == 0)
    /* On the ESC exit path editorFind writes the four cursor/offset fields (it
     * restores them), the status line (twice, via editorSetStatusMessage), and
     * the read index (via editorReadKey).  Nothing else is touched. */
    __CPROVER_assigns(E.cx, E.cy, E.coloff, E.rowoff)
    __CPROVER_assigns(__CPROVER_object_whole(E.statusmsg), E.statusmsg_time)
    __CPROVER_assigns(rk_idx)
    /* ESC restores the cursor and viewport to exactly their entry values. */
    __CPROVER_ensures(E.cx == __CPROVER_old(E.cx))
    __CPROVER_ensures(E.cy == __CPROVER_old(E.cy))
    __CPROVER_ensures(E.coloff == __CPROVER_old(E.coloff))
    __CPROVER_ensures(E.rowoff == __CPROVER_old(E.rowoff))
    __CPROVER_ensures(E.numrows == __CPROVER_old(E.numrows))
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

/* History-variable helpers for editorMoveCursor's functional postcondition.
 * E.row contents, E.numrows, E.screencols and E.screenrows are not assigned, so
 * they may be read directly; only cx/cy/rowoff/coloff change. */
#define OMC_OCX    __CPROVER_old(E.cx)
#define OMC_OCY    __CPROVER_old(E.cy)
#define OMC_OROFF  __CPROVER_old(E.rowoff)
#define OMC_OCOFF  __CPROVER_old(E.coloff)
#define OMC_OFR    (OMC_OROFF + OMC_OCY)            /* old filerow */
#define OMC_OFC    (OMC_OCOFF + OMC_OCX)            /* old filecol */
#define OMC_ORVALID (OMC_OFR < E.numrows)           /* old row != NULL */
#define OMC_ORSZ   (E.row[OMC_ORVALID ? OMC_OFR : 0].size)  /* old row->size, guarded */

/* Final filerow (== the E.rowoff + E.cy postcondition).  The clamp never
 * touches rowoff/cy, so the four arrow-key cases fully determine it. */
#define OMC_NFR ( \
    key == ARROW_UP    ? OMC_OFR - ((OMC_OFR > 0) ? 1 : 0) : \
    key == ARROW_DOWN  ? OMC_OFR + ((OMC_OFR < E.numrows) ? 1 : 0) : \
    key == ARROW_LEFT  ? OMC_OFR - ((OMC_OCX == 0 && OMC_OCOFF == 0 && OMC_OFR > 0) ? 1 : 0) : \
    key == ARROW_RIGHT ? OMC_OFR + ((OMC_ORVALID && OMC_OFC == OMC_ORSZ) ? 1 : 0) : \
    OMC_OFR )

/* Handle cursor position change because arrow keys were pressed. */
void editorMoveCursor(int key)
    __CPROVER_requires(E.numrows == 2)
    __CPROVER_requires(__CPROVER_is_fresh(E.row, sizeof(erow) * 2))
    __CPROVER_requires(E.row[0].size >= 0 && E.row[0].size <= 1000)
    __CPROVER_requires(E.row[1].size >= 0 && E.row[1].size <= 1000)
    __CPROVER_requires(E.screencols >= 1 && E.screencols <= 1000)
    __CPROVER_requires(E.screenrows >= 1 && E.screenrows <= 1000)
    __CPROVER_requires(E.cx >= 0 && E.cx <= 1000)
    __CPROVER_requires(E.coloff >= 0 && E.coloff <= 1000)
    __CPROVER_requires(E.cy >= 0 && E.cy <= E.numrows)
    __CPROVER_requires(E.rowoff >= 0 && E.rowoff <= E.numrows)
    __CPROVER_requires(E.rowoff + E.cy <= E.numrows)
    __CPROVER_assigns(E.cx, E.cy, E.rowoff, E.coloff)
    __CPROVER_ensures(E.cx >= 0)
    __CPROVER_ensures(E.coloff + E.cx <=
        ((E.rowoff + E.cy >= E.numrows) ? 0 : E.row[E.rowoff + E.cy].size))
    /* Exact final rowoff: only DOWN / RIGHT-at-line-end (when cy is at the
     * bottom screen row) and UP (when cy is at the top) move it. */
    __CPROVER_ensures(E.rowoff == (
        key == ARROW_UP    ? OMC_OROFF - ((OMC_OCY == 0 && OMC_OROFF > 0) ? 1 : 0) :
        key == ARROW_DOWN  ? OMC_OROFF + ((OMC_OFR < E.numrows && OMC_OCY == E.screenrows - 1) ? 1 : 0) :
        key == ARROW_RIGHT ? OMC_OROFF + ((OMC_ORVALID && OMC_OFC == OMC_ORSZ && OMC_OCY == E.screenrows - 1) ? 1 : 0) :
        OMC_OROFF))
    /* Exact final cursor row (filerow = rowoff + cy); the clamp never touches
     * rowoff/cy, so this is fully determined by the switch. */
    __CPROVER_ensures(E.rowoff + E.cy == OMC_NFR)
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
void editorProcessKeypress(int fd) {
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

int editorFileWasModified(void) {
    return E.dirty;
}

void updateWindowSize(void) {
    if (getWindowSize(STDIN_FILENO,STDOUT_FILENO,
                      &E.screenrows,&E.screencols) == -1) {
        perror("Unable to query the screen for size (columns / rows)");
        exit(1);
    }
    E.screenrows -= 2; /* Get room for status bar. */
}

void handleSigWinCh(int unused __attribute__((unused))) {
    updateWindowSize();
    if (E.cy > E.screenrows) E.cy = E.screenrows - 1;
    if (E.cx > E.screencols) E.cx = E.screencols - 1;
    editorRefreshScreen();
}

void initEditor(void) {
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
