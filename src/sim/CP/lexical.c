
/* RCS Info: $Revision: 1.1 $ on $Date: 91/04/02 12:04:20 $
 *           $Source: //pepper/atesse_spice/spice3/CP/RCS/lexical.c,v $
 * Copyright (c) 1985 Wayne A. Christopher, U. C. Berkeley CAD Group
 *
 * Initial lexer.
 */

#include "prefix.h"
#include "CPdefs.h"
#ifdef UNIX
#include <sys/types.h>
#include <sgtty.h>
#include <sys/dir.h>
#include <pwd.h>
#include <errno.h>
#include <sys/time.h>
#endif
#ifdef GI_X
#include "FTEgraf.h"
#include <X/Xlib.h>
#endif
#include "FTEinput.h"
#include "suffix.h"

static void prompt();

FILE *cp_inp_cur = NULL;
int cp_event = 1;
bool cp_interactive = true;
bool cp_bqflag = false;
char *cp_promptstring = NULL;
char *cp_altprompt = NULL;
char cp_hash = '#';

static int numeofs = 0;

static int input();
extern void Input();

#define ESCAPE  '\033'

/* Return a list of words, with backslash quoting and '' quoting done.
 * Strings en(void) closed in "" or `` are made single words and returned,
 * but with the "" or `` still present. For the \ and '' cases, the
 * 8th bit is turned on (as in csh) to prevent them from being recogized,
 * and stripped off once all processing is done. We also have to deal with
 * command, filename, and keyword completion here.
 * If string is non-NULL, then use it instead of the fp. Escape and EOF
 * have no business being in the string.
 */

#define newword cw->wl_word = copy(buf); \
        cw->wl_next = alloc(wordlist); \
        cw->wl_next->wl_prev = cw; \
        cw = cw->wl_next; \
        bzero(buf, BSIZE); \
        i = 0;

wordlist *
cp_lexer(string)
    char *string;
{
    int c;
    int i, j;
    wordlist *wlist = NULL, *cw = NULL;
    char buf[BSIZE], linebuf[BSIZE], d;

    if (cp_inp_cur == NULL)
        cp_inp_cur = cp_in;

    if (!string && cp_interactive) {
        cp_ccon(true);
        prompt();
    }
nloop:  i = 0;
    j = 0;
    bzero(linebuf, BSIZE);
    bzero(buf, BSIZE);
    wlist = cw = alloc(wordlist);
    cw->wl_next = cw->wl_prev = NULL;
    for (;;) {
        if (string) {
            c = *string++;
            if (c == '\0')
                c = '\n';
            if (c == ESCAPE)
                c = '[';
        } else
            c = input(cp_inp_cur);
gotchar:    if ((c != EOF) && (c != ESCAPE))
            linebuf[j++] = c;
        if (c != EOF)
            numeofs = 0;
        if (i == BSIZE - 1) {
            fprintf(cp_err, "Warning: word too long.\n");
            c = ' ';
        }
        if (j == BSIZE - 1) {
            fprintf(cp_err, "Warning: line too long.\n");
            if (cp_bqflag)
                c = EOF;
            else
                c = '\n';
        }
        if (c != EOF)
            c = strip(c);   /* Don't need to do this really. */
        if (c == '\\') {
            c = quote(string ? *string++ : input(cp_inp_cur));
            linebuf[j++] = strip(c);
        }
        if ((c == '\n') && cp_bqflag)
            c = ' ';
        if ((c == EOF) && cp_bqflag)
            c = '\n';
        if ((c == cp_hash) && !cp_interactive && (j == 1)) {
            if (string)
                return (NULL);
            while (((c = input(cp_inp_cur)) != '\n') &&
                    (c != EOF));
            goto nloop;
        }
        switch (c) {

            case ' ':
            case '\t':
            if (i > 0) {
                newword;
            }
            break;

            case '\n':
            if (i) {
                buf[i] = '\0';
                cw->wl_word = copy(buf);
            } else if (cw->wl_prev) {
                cw->wl_prev->wl_next = NULL;
                tfree(cw);
            } else {
                cw->wl_word = NULL;
            }
            goto done;

            case '\'':
            while (((c = (string ? *string++ : 
                    input(cp_inp_cur))) != '\'')
                    && (i < BSIZE - 1)) {
                if ((c == '\n') || (c == EOF) || (c == ESCAPE))
                    goto gotchar;
                else {
                    buf[i++] = quote(c);
                    linebuf[j++] = c;
                }
            }
            linebuf[j++] = '\'';
            break;

            case '"':
            case '`':
            d = c;
            buf[i++] = d;
            while (((c = (string ? *string++ : input(cp_inp_cur)))
                    != d) && (i < BSIZE - 2)) {
                if ((c == '\n') || (c == EOF) || (c == ESCAPE))
                    goto gotchar;
                else if (c == '\\') {
                    linebuf[j++] = c;
                    c = (string ? *string++ :
                            input(cp_inp_cur));
                    buf[i++] = quote(c);
                    linebuf[j++] = c;
                } else {
                    buf[i++] = c;
                    linebuf[j++] = c;
                }
            }
            buf[i++] = d;
            linebuf[j++] = d;
            break;

            case EOF:
#ifdef BSD
            if (cp_interactive && !cp_nocc && 
                    (string == NULL)) {
                if (j == 0) {
                    if (cp_ignoreeof && (numeofs++
                            < 23)) {
                        fputs(
                    "Use \"quit\" to quit.\n",
                            stdout);
                    } else {
                        fputs("quit\n", stdout);
                        cp_doquit();
                    }
                    goto done;
                }
                cp_ccom(wlist, buf, false);
                wl_free(wlist);
                (void) putchar('\r');
                prompt();
                for (j = 0; linebuf[j]; j++)
                    (void) ioctl(fileno(cp_out), TIOCSTI,
                            linebuf + j);
                goto nloop;
            } else {    /* EOF during a source... */
#endif
                if (cp_interactive) {
                    fputs("quit\n", stdout);
                    cp_doquit();
                    goto done;
                } else
                    return (NULL);
#ifdef BSD  /* Ack... */
            }
#endif
            case ESCAPE:
#ifdef BSD
            if (cp_interactive && !cp_nocc) {
                fputs("\b\b  \b\b\r", cp_out);
                prompt();
                for (j = 0; linebuf[j]; j++)
                    (void) ioctl(fileno(cp_out), TIOCSTI,
                            linebuf + j);
                cp_ccom(wlist, buf, true);
                wl_free(wlist);
                goto nloop;
            } /* Else fall through... */
#endif
            default:
            /* We have to remember the special case $<
             * here... Drag...
             */
            if ((cp_chars[c] & CPC_BRL) && (i > 0)) {
                if ((c != '<') || (buf[i - 1] != '$')) {
                    newword;
                }
            }
            buf[i++] = c;
            if (cp_chars[c] & CPC_BRR) {
                if ((c != '<') || (i < 2) ||
                        (buf[i - 2] != '$')) {
                    newword;
                }
            }
        }
    }
done:
    return (wlist);
}

static void
prompt()
{
    char *s;

    if (cp_interactive == false)
        return;
    if (cp_promptstring == NULL)
        s = "-> ";
    else
        s = cp_promptstring;
    if (cp_altprompt)
        s = cp_altprompt;
#ifdef VMS
    /* this is for VMS/RMS which otherwise won't output the LF
     * part of the newline on the previous line if this line
     * doesn't also end in newline, and most prompts don't, so...
     * we force an extra line here.
     */
    fprintf(cp_out,"\n");
#endif
    while (*s) {
        switch (strip(*s)) {
            case '!':
                fprintf(cp_out, "%d", cp_event);
                break;
            case '\\':
                if (*(s + 1))
                    (void) putc(strip(*++s), cp_out);
            default:
                (void) putc(strip(*s), cp_out);
        }
        s++;
    }
    (void) fflush(cp_out);
    return;
}


/* A special 'getc' so that we can deal with ^D properly. There is no way for
 * stdio to know if we have typed a ^D after some other characters, so
 * don't use buffering at all...
 */
int
inchar(fp)
FILE *fp;
{

    char c;
    int i;
    static int errno;

#ifdef BSD
    if (cp_interactive && !cp_nocc) {
      i = read((int) fileno(fp), &c, 1);
      if (i == 0)
        return (EOF);
      else if (i == -1) {
        perror("read");
        return (EOF);
      } else
        return ((int) c);
    }
#endif
#ifdef HPUX
yoyo:
    errno = 0;
#endif
    c = getc(fp);
#ifdef HPUX
    if ((c == EOF) && (errno = EINTR))
        goto yoyo;
#endif
    return ((int) c);
}

static int input(fp)
FILE *fp;
{

    REQUEST request;
    RESPONSE response;

    request.option = char_option;
    request.fp = fp;
    Input(&request, &response);
    return(response.reply.ch);

}

