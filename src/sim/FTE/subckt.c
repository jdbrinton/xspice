
/* RCS Info: $Revision: 1.2 $ on $Date: 91/04/10 12:18:44 $
 *           $Source: //pepper/atesse_spice/spice3/FTE/RCS/subckt.c,v $
 * Copyright (c) 1985 Wayne A. Christopher, U. C. Berkeley CAD Group
 *
 * Expand subcircuits. This is very spice-dependent. Bug fixes by Norbert 
 * Jeske on 10/5/85.
 */

#include "prefix.h"
#include "CPdefs.h"
#include "FTEdefs.h"
#include "FTEinp.h"

/* gtri - add - wbk - 11/9/90 - include MIF function prototypes */
#include "MIFproto.h"
/* gtri - end - wbk - 11/9/90 */

#include "suffix.h"

static struct line *doit();
static void translate();
static void settrans();
static void finishLine();
static char *gettrans();
static int numnodes(), numdevs();
static bool modtranslate();
static void devmodtranslate();

struct subs {
    char *su_name;      /* The name. */
    char *su_args;      /* The arguments, space seperated. */
    int su_numargs;
    struct line *su_def;    /* The deck that is to be substituted. */
    struct subs *su_next;
} ;

/* Expand all subcircuits in the deck. This handles imbedded .subckt
 * definitions. The variables substart, subend, and subinvoke can be used
 * to redefine the controls used. The syntax is invariant though.
 * NOTE: the deck must be passed without the title card.
 * What we do is as follows: first make one pass through the circuit
 * and collect all of the subcircuits. Then, whenever a card that starts
 * with 'x' is found, copy the subcircuit associated with that name and
 * splice it in. A few of the problems: the nodes in the spliced-in
 * stuff must be unique, so when we copy it, append "subcktname:" to
 * each node. If we are in a nested subcircuit, use foo:bar:...:node.
 * Then we have to systematically change all references to the renamed
 * nodes. On top of that, we have to know how many args BJT's have,
 * so we have to keep track of model names.
 */

static wordlist *modnames, *submod;
static struct subs *subs = NULL;
static bool nobjthack = false;

static char start[32], sbend[32], invoke[32], model[32];

struct line *
inp_subcktexpand(deck)
    struct line *deck;
{
    struct line *ll, *c;
    char *s;

    if(!cp_getvar("substart", VT_STRING, start))
        (void) strcpy(start, ".subckt");
    if(!cp_getvar("subend", VT_STRING, sbend))
        (void) strcpy(sbend, ".ends");
    if(!cp_getvar("subinvoke", VT_STRING, invoke))
        (void) strcpy(invoke, "X");
    if(!cp_getvar("modelcard", VT_STRING, model))
        (void) strcpy(model, ".model");
    (void) cp_getvar("nobjthack", VT_BOOL, (char *) &nobjthack);

    /* Let's do a few cleanup things first... Get rid of ( ) around node
     * lists...
     */

/* gtri - modify - wbk - 10/22/90 - fix so that all '(' and ')' are */
/* changed to white space (i.e. ' ').  Previous code only removed   */
/* one instance of '(' or ')', and did not replace it with white    */
/* space.                                                           */

    /* bypass old code that eliminated a single ')' and '(' */

#if 0
    for (c = deck; c; c = c->li_next) {
        if (prefix(start, c->li_line)) {
            for (s = c->li_line; *s && (*s != '('); s++)
                ;
            if (*s) {
                while (s[0] && (s[1] != ')')) {
                    s[0] = s[1];
                    s++;
                }
                while (s[1]) {
                    s[0] = s[2];
                    s++;
                }
            }
        } else {
            for (s = c->li_line; *s && !isspace(*s); s++)
                ;
            while (isspace(*s))
                s++;
            if (*s == '(') {
                while (s[0] && (s[1] != ')')) {
                    s[0] = s[1];
                    s++;
                }
                while (s[1]) {
                    s[0] = s[2];
                    s++;
                }
            }
        }
    }
#endif

    /* new code - replace all '(' and ')' with ' ' */

    for(c = deck; c; c = c->li_next)
        for(s = c->li_line; *s; s++)
            if( (*s == '(') || (*s == ')') )
                *s = ' ';

/* gtri - end - wbk - 10/22/90 */

    
    ll = doit(deck);

    /* Now check to see if there are still subckt instances undefined... */
    if (ll) {
        for (c = ll; c; c = c->li_next)
            if (ciprefix(invoke, c->li_line)) {
                fprintf(cp_err, "Error: unknown subckt: %s\n",
                        c->li_line);
                ll = NULL;
            }
    }

    return (ll);
}

#define MAXNEST 21

static struct line *
doit(deck)
    struct line *deck;
{
    struct line *c, *last, *lc, *lcc;
    struct subs *sss = (struct subs *) NULL, *ks;
    char *s, *t, *scname, *subname;
    int nest, numpasses = MAXNEST, i;
    bool gotone;
    wordlist *wl;
    wordlist *tmodnames = modnames;
    wordlist *tsubmod = submod;
    struct subs *ts = subs;

    /* Save all the old stuff... */
    modnames = NULL;
    subs = NULL;
    submod = NULL;

    /* First extract all the subckts. */
    for (last = deck, lc = NULL; last; ) {
        if (prefix(sbend, last->li_line)) {
            fprintf(cp_err, "Error: misplaced %s card: %s\n", sbend,
                    last->li_line);
            return (NULL);
        } else if (prefix(start, last->li_line)) {
            /* We've got one now. */
            if (last->li_next == NULL) {
                fprintf(cp_err, "Error: no %s card.\n", sbend);
                return (NULL);
            }
            lcc = NULL;
            wl_free(submod);
            submod = NULL;
            gotone = false;
            for (nest = 0, c = last->li_next; c; c = c->li_next) {
                if (prefix(sbend, c->li_line)) {
                    if (!nest)
                        break;
                    else {
                        nest--;
                        lcc = c;
                        continue;
                    }
                } else if (prefix(start, c->li_line))
                    nest++;
                lcc = c;
            }
            if (!c) {
                fprintf(cp_err, "Error: no %s card.\n", sbend);
                return (NULL);
            }
            sss = alloc(subs);
            lcc->li_next = NULL;
            if (lc)
                lc->li_next = c->li_next;
            else
                deck = c->li_next;
            sss->su_def = last->li_next;
            s = last->li_line;
            (void) gettok(&s);
            sss->su_name = gettok(&s);
            sss->su_args = copy(s);
            for (sss->su_numargs = 0, i = 0; s[i]; ) {
                while (isspace(s[i]))
                    i++;
                if (s[i]) {
                    sss->su_numargs++;
                    while (s[i] && !isspace(s[i]))
                        i++;
                }
            }
            sss->su_next = subs;
            subs = sss;
            last = c->li_next;
            lcc = subs->su_def;
        } else {
            lc = last;
            last = last->li_next;
        }
    }

    if (!sss)
        return (deck);

    /* Expand sub-subcircuits. */
    for (ks = sss = subs; sss; sss = sss->su_next)
        if (!(sss->su_def = doit(sss->su_def)))
            return (NULL);
    subs = ks;
    
    /* Get all the model names so we can deal with BJT's. */
    for (c = deck; c; c = c->li_next)
        if (prefix(model, c->li_line)) {
            s = c->li_line;
            (void) gettok(&s);
            wl = alloc(wordlist);
            wl->wl_next = modnames;
            if (modnames)
                modnames->wl_prev = wl;
            modnames = wl;
            wl->wl_word = gettok(&s);
        }

    /* Now do the replacements. */
    do {
        gotone = false;
        for (c = deck, lc = NULL; c; ) {
            if (ciprefix(invoke, c->li_line)) {
                gotone = true;
                t = s = copy(c->li_line);
                scname = gettok(&s);
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* comment out so that whole instance name is used in pathnames   */
/*
                scname += strlen(invoke);
                while ((*scname == ' ') || (*scname == '\t') ||
                        (*scname == ':'))
                    scname++;
*/
/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
                while(*s)
                    s++;
                s--;
                while ((*s == ' ') || (*s == '\t'))
                    *s-- = '\0';
                while ((*s != ' ') && (*s != '\t'))
                    s--;
                s++;
                for (sss = subs; sss; sss = sss->su_next)
                    if (eq(sss->su_name, s))
                        break;
                /* Don't complain -- this might be an
                 * instance of a subckt that is defined above.
                 */
                if (!sss) {
                    lc = c;
                    c = c->li_next;
                    continue;
                }
                /* Now we have to replace this card with the
                 * macro definition.
                 */
                subname = copy(sss->su_name);
                lcc = inp_deckcopy(sss->su_def);

                /* Change the names of the models... */
                if (modtranslate(lcc, scname))
                    devmodtranslate(lcc, scname);

                s = sss->su_args;
                (void) gettok(&t); /* Throw out the name. */

                translate(lcc, s, t, scname, subname);

                /* Now splice the decks together. */
                if (lc)
                    lc->li_next = lcc;
                else
                    deck = lcc;
                while (lcc->li_next != NULL)
                    lcc = lcc->li_next;
                lcc->li_next = c->li_next;
                c = lcc->li_next;
                lc = lcc;
            } else {
                lc = c;
                c = c->li_next;
            }
        }
    } while (numpasses-- && gotone);

    if (!numpasses) {
        fprintf(cp_err, "Error: infinite subckt recursion\n");
        return (NULL);
    }

    subs = ts;
    modnames = tmodnames;
    submod = tsubmod;

    return (deck);
}

/* Copy a deck, including the actual lines. */

struct line *
inp_deckcopy(deck)
    struct line *deck;
{
    struct line *d = NULL, *nd = NULL;

    while (deck) {
        if (nd) {
            d->li_next = alloc(line);
            d = d->li_next;
        } else
            nd = d = alloc(line);
        d->li_linenum = deck->li_linenum;
        d->li_line = copy(deck->li_line);
        if (deck->li_error)
            d->li_error = copy(deck->li_error);
        d->li_actual = inp_deckcopy(deck->li_actual);
        deck = deck->li_next;
    }
    return (nd);
}

/* Translate all of the device names and node names in the deck. They are
 * pre-pended with subname:, unless they are in the formal list, in which case
 * they are replaced with the corresponding entry in the actual list.
 * The one special case is node 0 -- this is always ground and we don't
 * touch it.
 */

static void
translate(deck, formal, actual, scname, subname)
    struct line *deck;
    char *formal, *actual, *scname, *subname;
{
    struct line *c;
    char *buffer, *name, *s, *t, ch;
    int nnodes;

/* gtri - add - wbk - 10/23/90 - add new local variables */

    char    *next_name;     /* for look-ahead during tokenizing */

/* gtri - end - wbk - 10/23/90 */


    settrans(formal, actual, subname);
    for (c = deck; c; c = c->li_next) {
        /* Rename the device. */
        switch (*c->li_line) {
        case '\0':
        case '*':
        case '.':
            /* Nothing any good here. */
            continue;

/* gtri - add - wbk - 10/23/90 - process A devices specially */
/* since they have a more involved and variable length node syntax */

        case 'a':
        case 'A':

            /* translate the instance name according to normal rules */

            buffer = tmalloc(10000);     /* XXXXX */

            s = c->li_line;
            name = MIFgettok(&s);

            sprintf(buffer, "%s:%s ", name, scname);

            /* Now translate the nodes, looking ahead one token to recognize */
            /* when we reach the model name which should not be translated   */
            /* here.                                                         */

            next_name = MIFgettok(&s);

            while(1) {

                /* rotate the tokens and get the the next one */

                name = next_name;
                next_name = MIFgettok(&s);

                /* if next token is NULL, name holds the model name, so exit */

                if(next_name == NULL)
                    break;

                /* Process the token in name.  If it is special, then don't */
                /* translate it.                                            */

                switch(*name) {

                case '[':
                case ']':
                case '~':
                    sprintf(buffer + strlen(buffer), "%s ", name);
                    break;

                case '%':

                    sprintf(buffer + strlen(buffer), "%%");

                    /* don't translate the port type identifier */

                    name = next_name;
                    next_name = MIFgettok(&s);

                    sprintf(buffer + strlen(buffer), "%s ", name);
                    break;

                default:

                    /* must be a node name at this point, so translate it */

                    t = gettrans(name);
                    if (t)
                        sprintf(buffer + strlen(buffer), "%s ", t);
                    else
                        sprintf(buffer + strlen(buffer), "%s:%s ", name, scname);

                    break;

                } /* switch */

            } /* while */


            /* copy in the last token, which is the model name */

            if(name)
                sprintf(buffer + strlen(buffer), "%s ", name);

            /* Set s to null string for compatibility with code */
            /* after switch statement                           */

            s = "";

            break;

/* gtri - end - wbk - 10/23/90 */


        default:
                s = c->li_line;
            name = gettok(&s);
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
            ch = *name;
            buffer = tmalloc(10000);
            name++;
            if (*name == ':')
            name++;
            if (*name)
                (void) sprintf(buffer, "%c:%s:%s ", ch, scname,
                    name);
            else
                (void) sprintf(buffer, "%c:%s ", ch, scname);
*/
            buffer = tmalloc(10000);    /* XXXXX */
            (void) sprintf(buffer, "%s:%s ", name, scname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */

            nnodes = numnodes(c->li_line);
            while (nnodes-- > 0) {
            name = gettok(&s);
            if (name == NULL) {
                fprintf(cp_err, "Warning: too few nodes: %s\n",
                        c->li_line);
                return;
            }
            t = gettrans(name);
            if (t)
                (void) sprintf(buffer + strlen(buffer), "%s ",
                        t);
            else
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
                (void) sprintf(buffer + strlen(buffer),
                        "%s:%s ", scname, name);
*/
                (void) sprintf(buffer + strlen(buffer),
                        "%s:%s ", name, scname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
            }    
            nnodes = numdevs(c->li_line);
            while (nnodes-- > 0) {
            name = gettok(&s);
            if (name == NULL) {
                fprintf(cp_err, "Warning: too few devs: %s\n",
                        c->li_line);
                return;
            }
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
            ch = *name;
            name++;
            if (*name == ':')
                name++;
            if (*name)
                (void) sprintf(buffer + strlen(buffer),
                        "%c:%s:%s ", ch, scname, name);
            else
                (void) sprintf(buffer + strlen(buffer),
                        "%c:%s ", ch, scname);
*/
            (void) sprintf(buffer + strlen(buffer),
                    "%s:%s ", name, scname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
            }
            /* Now scan through the line for v(something) and
             * i(something)...
             */
            finishLine(buffer + strlen(buffer), s, scname);
            s = "";
        }
            (void) strcat(buffer, s);
        tfree(c->li_line);
        c->li_line = copy(buffer);
        tfree(buffer);
    }
    return;
}

static void
finishLine(dst, src, scname)
    char *dst;
    char *src;
    char *scname;
{
    char buf[BSIZE], which;
    char *s;
    int i;

    while (*src) {
        /* Find the next instance of "<non-alpha>[vi]<opt spaces>(" in
         * this string.
         */
        if (((*src != 'v') && (*src != 'V') &&
                (*src != 'i') && (*src != 'I')) ||
                isalpha(src[-1])) {
            *dst++ = *src++;
            continue;
        }
        for (s = src + 1; *s && isspace(*s); s++)
            ;
        if (!*s || (*s != '(')) {
            *dst++ = *src++;
            continue;
        }
        which = *dst++ = *src;
        src = s;
        *dst++ = *src++;
        while (isspace(*src))
            src++;
        for (i = 0; *src && !isspace(*src) && (*src != ')'); i++)
            buf[i] = *src++;
        buf[i] = '\0';
printf("which = %c, buf = '%s'\n", which, buf);
        if ((which == 'v') || (which == 'V'))
            s = gettrans(buf);
        else
            s = NULL;
        if (s) {
            while (*s)
                *dst++ = *s++;
        } else {
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order of building pathnames so innermost is always first */
/*
            for (s = scname; *s; )
                *dst++ = *s++;
            *dst++ = ':';
            for (s = buf; *s; )
                *dst++ = *s++;
*/
            for (s = buf; *s; )
                *dst++ = *s++;
            *dst++ = ':';
            for (s = scname; *s; )
                *dst++ = *s++;

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
        }
    }

    return;
}

static struct tab {
    char *t_old;
    char *t_new;
} table[512];   /* That had better be enough. */

static void
settrans(formal, actual, subname)
    char *formal, *actual, *subname;
{
    int i;

    for (i = 0; ; i++) {
        table[i].t_old = gettok(&formal);
        table[i].t_new = gettok(&actual);
        if ((table[i].t_old == NULL) && eq(table[i].t_new, subname))
            break;
        if ((table[i].t_old == NULL) || (table[i].t_new == NULL)) {
            fprintf(cp_err, 
            "settrans: Internal Error: wrong number of params\n");
            return;
        }
    }
    return;
}

static char *
gettrans(name)
    char *name;
{
    int i;

    if (eq(name, "0"))
        return (name);

/* gtri - wbk - 2/27/91 - don't translate the reserved word 'null' */

    if (eq(name, "null"))
        return (name);

/* gtri - end */

    for (i = 0; table[i].t_old; i++)
        if (eq(table[i].t_old, name))
            return (table[i].t_new);
    return (NULL);
}

static int
numnodes(name)
    char *name;
{
    char c = (isupper(*name) ? tolower(*name) : *name);
    struct subs *sss;
    char *s, *t, buf[BSIZE];
    wordlist *wl;
    int n, i;

    (void) strncpy(buf, name, BSIZE);
    s = buf;
    if (c == 'x') {     /* Handle this ourselves. */
        while(*s)
            s++;
        s--;
        while ((*s == ' ') || (*s == '\t'))
            *s-- = '\0';
        while ((*s != ' ') && (*s != '\t'))
            s--;
        s++;
        for (sss = subs; sss; sss = sss->su_next)
            if (eq(sss->su_name, s))
                break;
        if (!sss) {
            fprintf(cp_err, "Error: no such subcircuit: %s\n", s);
            return (0);
        }
        return (sss->su_numargs);
    }
    n = inp_numnodes(c);
    if (nobjthack || (c != 'q'))
        return (n);
    for (s = buf, i = 0; *s && (i < 4); i++)
        (void) gettok(&s);
    if (i == 3)
        return (3);
    else if (i < 4) {
        fprintf(cp_err, "Error: too few nodes for BJT: %s\n", name);
        return (0);
    }
    /* Now, is this a model? */
    t = gettok(&s);
    for (wl = modnames; wl; wl = wl->wl_next)
        if (eq(t, wl->wl_word))
            return (3);
    return (4);
}

static int 
numdevs(s)
    char *s;
{
    switch (*s) {
        case 'K':
        case 'k':
        return (2);
    
        case 'F':
        case 'f':
        case 'H':
        case 'h':
        return (1);
    
        default:
        return (0);
    }
}

static bool
modtranslate(deck, subname)
    struct line *deck;
    char *subname;
{
    struct line *c;
    char *buffer, *name, *t, model[BSIZE];
    wordlist *wl, *wlsub;
    bool gotone;

    (void) strcpy(model, ".model");
    gotone = false;
    for (c = deck; c; c = c->li_next) {
        if (prefix(model, c->li_line)) {
            gotone = true;
            t = c->li_line;
            name = gettok(&t);
            buffer = tmalloc(strlen(name) + strlen(t) +
                    strlen(subname) + 4);
            (void) sprintf(buffer, "%s ",name);
            name = gettok(&t);
            wlsub = alloc(wordlist);
            wlsub->wl_next = submod;
            if (submod)
                submod->wl_prev = wlsub;
            submod = wlsub;
            wlsub->wl_word = name;
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
            (void) sprintf(buffer + strlen(buffer), "%s:%s ",
                    subname, name);
*/
            (void) sprintf(buffer + strlen(buffer), "%s:%s ",
                    name, subname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
            (void) strcat(buffer, t);
            tfree(c->li_line);
            c->li_line = buffer;
            t = c->li_line;
            (void) gettok(&t);
            wl = alloc(wordlist);
            wl->wl_next = modnames;
            if (modnames) 
                modnames->wl_prev = wl;
            modnames = wl;
            wl->wl_word = gettok(&t);
        }
    }
    return(gotone);
}

static void
devmodtranslate(deck, subname)
    struct line *deck;
    char *subname;
{
    struct line *s;
    char *buffer, *name, *t, c;
    wordlist *wlsub;
    bool found;

    for (s = deck; s; s = s->li_next) {
        t = s->li_line;
        c = isupper(*t) ? tolower(*t) : *t;
        found = false;
        buffer = tmalloc(strlen(t) + strlen(subname) + 4);
            switch (c) {
        case 'r':
        case 'c':
            name = gettok(&t);
            (void) sprintf(buffer,"%s ",name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            if (*t) {
                name = gettok(&t);
            /* Now, is this a subcircuit model? */
            for (wlsub = submod; wlsub; wlsub = wlsub->wl_next) {
                    if (eq(name, wlsub->wl_word)) {
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
                    (void) sprintf(buffer + strlen(buffer),
                        "%s:%s ", subname, name);
*/
                    (void) sprintf(buffer + strlen(buffer),
                        "%s:%s ", name, subname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
                    found = true;
                    break;
                    }
                }
                if (!found)
                (void) sprintf(buffer + strlen(buffer), "%s ",
                    name);
            }
            found = false;
            if (*t) {
                name = gettok(&t);
                /* Now, is this a subcircuit model? */
                for (wlsub = submod; wlsub; wlsub = wlsub->wl_next) {
                if (eq(name, wlsub->wl_word)) {
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
                (void) sprintf(buffer + strlen(buffer),
                        "%s:%s ", subname, name);
*/
                (void) sprintf(buffer + strlen(buffer),
                        "%s:%s ", name, subname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
                    found = true;
                    break;
                }
                }
                if (!found)
                (void) sprintf(buffer + strlen(buffer), "%s ", 
                    name);
            }
            (void) strcat(buffer, t);
            tfree(s->li_line);
            s->li_line = buffer;
            break;
            case 'd':
            name = gettok(&t);
            (void) sprintf(buffer,"%s ",name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            /* Now, is this a subcircuit model? */
            for (wlsub = submod; wlsub; wlsub = wlsub->wl_next) {
                if (eq(name, wlsub->wl_word)) {
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
                (void) sprintf(buffer + strlen(buffer), "%s:%s ", 
                    subname, name);
*/
                (void) sprintf(buffer + strlen(buffer), "%s:%s ",
                    name, subname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
                found = true;
                break;
                }
            }
            if (!found)
                (void) sprintf(buffer + strlen(buffer), "%s ", name);
            (void) strcat(buffer, t);
            tfree(s->li_line);
            s->li_line = buffer;
            break;
            case 'u':
            case 'j':
            case 'z':
            name = gettok(&t);
            (void) sprintf(buffer,"%s ",name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            /* Now, is this a subcircuit model? */
            for (wlsub = submod; wlsub; wlsub = wlsub->wl_next) {
                if (eq(name, wlsub->wl_word)) {
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
                (void) sprintf(buffer + strlen(buffer), "%s:%s ", 
                    subname, name);
*/
                (void) sprintf(buffer + strlen(buffer), "%s:%s ",
                    name, subname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
                found = true;
                break;
                }
            }
            if (!found)
                (void) sprintf(buffer + strlen(buffer), "%s ", name);
            (void) strcat(buffer, t);
            tfree(s->li_line);
            s->li_line = buffer;
            break;
            case 'm':
            name = gettok(&t);
            (void) sprintf(buffer,"%s ",name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            /* Now, is this a subcircuit model? */
            for (wlsub = submod; wlsub; wlsub = wlsub->wl_next) {
                if (eq(name, wlsub->wl_word)) {
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
                (void) sprintf(buffer + strlen(buffer), "%s:%s ", 
                    subname, name);
*/
                (void) sprintf(buffer + strlen(buffer), "%s:%s ",
                    name, subname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
                found = true;
                break;
                }
            }
            if (!found)
                (void) sprintf(buffer + strlen(buffer), "%s ", name);
            (void) strcat(buffer, t);
            tfree(s->li_line);
            s->li_line = buffer;
            break;
            case 'q':
            name = gettok(&t);
            (void) sprintf(buffer,"%s ",name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            (void) sprintf(buffer + strlen(buffer), "%s ", name);
            name = gettok(&t);
            /* Now, is this a subcircuit model? */
            for (wlsub = submod; wlsub; wlsub = wlsub->wl_next) {
                if (eq(name, wlsub->wl_word)) {
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
                (void) sprintf(buffer + strlen(buffer), "%s:%s ", 
                    subname, name);
*/
                (void) sprintf(buffer + strlen(buffer), "%s:%s ",
                    name, subname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
                found = true;
                break;
                }
            }
            if (!found)
                (void) sprintf(buffer + strlen(buffer), "%s ", name);
            found = false;
            if (*t) {
                name = gettok(&t);
                /* Now, is this a subcircuit model? */
                for (wlsub = submod; wlsub; wlsub = wlsub->wl_next) {
                    if (eq(name, wlsub->wl_word)) {
/* gtri - modify - wbk - 4/10/91 - change way pathnames are built */
/* reverse order so that innermost name is now first */
/*
                    (void) sprintf(buffer + strlen(buffer),
                            "%s:%s ", subname, name);
*/
                    (void) sprintf(buffer + strlen(buffer),
                            "%s:%s ", name, subname);

/* gtri - end - wbk - 4/10/91 - change way pathnames are built */
                    found = true;
                    break;
                }
                }
                if (!found)
                (void) sprintf(buffer + strlen(buffer), "%s ",
                    name);
            }
            (void) strcat(buffer, t);
            tfree(s->li_line);
            s->li_line = buffer;
            break;

/* gtri - modify - wbk - 10/22/90 - add a case for the new 'a' devices */

        case 'a':

            /* set name to the beginning of the last token on the line */

            while(1) {

                name = t;
                MIFgettok(&t);

                if(*t == '\0')
                    break;
            }

            /* copy up to this token into the buffer */
            strncpy(buffer, s->li_line, name - s->li_line);
            buffer[name - s->li_line] = '\0';

            /* eliminate the possibility of trailing white space */
            t = name;
            name = MIFgettok(&t);

            /* map name if it matches any .model cards in this subckt */
            for (wlsub = submod; wlsub; wlsub = wlsub->wl_next) {
                if (eq(name, wlsub->wl_word)) {
                sprintf(buffer + strlen(buffer), "%s:%s ", name, subname);
                    found = true;
                    break;
                }
            }
            if (!found)
                sprintf(buffer + strlen(buffer), "%s ", name);

            tfree(s->li_line);
            s->li_line = buffer;
            break;

/* gtri - end - wbk - 10/22/90 */

            default:
            tfree(buffer);
            break;
        }
    }
    return;
}

/* This is a spice-dependent thing.  It should probably go somewhere
 * else, but...  Note that we pretend that dependent sources and mutual
 * inductors have more nodes than they really do...
 */



int
inp_numnodes(c)
    char c;
{

/* gtri - comment - wbk - 10/23/90 - Do not modify this routine for */
/* 'A' type devices since the callers will not know how to find the */
/* nodes even if they know how many there are.  Modify the callers  */
/* instead.                                                         */
/* gtri - end - wbk - 10/23/90 */

    if (isupper(c))
        c = tolower(c);
    switch (c) {
        case ' ':
        case '\t':
        case '.':
        case 'x':
        case '*':
        return (0);

        case 'b': return (2);
        case 'c': return (2);
        case 'd': return (2);
        case 'e': return (4);
        case 'f': return (2);
        case 'g': return (4);
        case 'h': return (2);
        case 'i': return (2);
        case 'j': return (3);
        case 'k': return (0);
        case 'l': return (2);
        case 'm': return (4);
        case 'q': return (4);
        case 'r': return (2);
        case 's': return (4);
        case 't': return (4);
        case 'u': return (3);
        case 'v': return (2);
        case 'w': return (3);
        case 'z': return (3);

        default:
        fprintf(cp_err, "Warning: unknown device type: %c\n", c);
            return (2);
    }
}

