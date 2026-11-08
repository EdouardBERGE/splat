/*
 * SPLAT 1.0 - file splitter
 * Portable ANSI C (C89) rewrite of the Amiga utility by Stephen Cantini (1996).
 *
 * Template (AmigaDOS ReadArgs style, keywords are case-insensitive):
 *   FILE/A,S=SEARCH/K,SIZE/N,K/S,LINES/S,TO/K,R=RECURSIVE/S,QUIET/S,
 *   BUF=BUFFER/K/N,PARTS/S,NOCUT/S,QUICK/S,NOCASE/S,DOWILD/S,MAX/K/N
 *
 * Builds with any C89 compiler, e.g.:
 *   gcc -std=c89 -pedantic -Wall -O2 -o splat splat.c
 *   cl /W3 /O2 splat.c            (Visual Studio)
 *
 * All files are opened in binary mode ("rb"/"wb"): no CR/LF translation,
 * no ^Z end-of-file handling on Windows.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

/* 64-bit unsigned counter when available (files > 4 GB) */
#if defined(_MSC_VER)
typedef unsigned __int64 u64;
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
typedef unsigned long long u64;
#else
typedef unsigned long u64;
#endif

#define DEFAULT_BUFLEN  (64UL * 1024UL)
#define FALLBACK_BUFLEN 8192UL
#define MAX_SEARCH      1024
#define WILD            (-1)

static const char *version = "$VER: Splat 1.0 (11.4.96) (c) Copyright Stephen Cantini";

static const char *template_str =
    "FILE/A,S=SEARCH/K,SIZE/N,K/S,LINES/S,TO/K,R=RECURSIVE/S,QUIET/S,"
    "BUF=BUFFER/K/N,PARTS/S,NOCUT/S,QUICK/S,NOCASE/S,DOWILD/S,MAX/K/N";

static const char *const exthelp[] = {
    "Splat V1.0 (c) Stephen Cantini - portable C version\n\n",
    "FILE/A = The file to be splitted\n",
    "S=SEARCH/K = String to search for (\\t tab, \\n LF, \\r CR, \\\\ backslash, \\? question mark).\n",
    "SIZE/N = Split at this number of bytes.\n",
    "K/S = Size is expressed in kbytes.\n",
    "LINES/S = Size is expressed in lines.\n",
    "TO/K = Optional destination directory/filenames (DEFAULT:input file's).\n",
    "R=RECURSIVE/S = Split EVERY size bytes/k/lines/search.\n",
    "QUIET/S = Sshhhh!! :)\n",
    "BUF=BUFFER/K/N = I/O buffer size in kbytes.\n",
    "PARTS/S = Divide in SIZE parts.\n",
    "NOCUT/S = Do not trunc text lines.\n",
    "QUICK/S = Output JUST the number of files created.\n",
    "NOCASE/S = Make string search case insensitive.\n",
    "DOWILD/S = Allow jolly character \"?\" in search string.\n",
    "MAX/K/N = Max number of files to create.\n",
    NULL
};

/* ------------------------------------------------------------------ */
/* Argument parsing (ReadArgs-like)                                    */
/* ------------------------------------------------------------------ */

enum { A_FILE, A_SEARCH, A_SIZE, A_K, A_LINES, A_TO, A_RECURSIVE, A_QUIET,
       A_BUFFER, A_PARTS, A_NOCUT, A_QUICK, A_NOCASE, A_DOWILD, A_MAX, A_COUNT };

#define T_STR  0   /* positional or keyword string */
#define T_NUM  1   /* positional or keyword number (/N) */
#define T_KSTR 2   /* keyword-only string (/K) */
#define T_KNUM 3   /* keyword-only number (/K/N) */
#define T_SW   4   /* switch (/S) */

struct argdef {
    const char *name;
    const char *alias;
    int type;
};

static const struct argdef argdefs[A_COUNT] = {
    { "FILE",      NULL, T_STR  },
    { "SEARCH",    "S",  T_KSTR },
    { "SIZE",      NULL, T_NUM  },
    { "K",         NULL, T_SW   },
    { "LINES",     NULL, T_SW   },
    { "TO",        NULL, T_KSTR },
    { "RECURSIVE", "R",  T_SW   },
    { "QUIET",     NULL, T_SW   },
    { "BUFFER",    "BUF",T_KNUM },
    { "PARTS",     NULL, T_SW   },
    { "NOCUT",     NULL, T_SW   },
    { "QUICK",     NULL, T_SW   },
    { "NOCASE",    NULL, T_SW   },
    { "DOWILD",    NULL, T_SW   },
    { "MAX",       NULL, T_KNUM }
};

static const char   *arg_str[A_COUNT];
static unsigned long arg_num[A_COUNT];
static int           arg_set[A_COUNT];

static int str_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return *a == *b;
}

/* Returns the argument index matching keyword 'kw' (len chars), or -1 */
static int find_keyword(const char *kw, size_t len)
{
    char tmp[16];
    int i;
    if (len == 0 || len >= sizeof(tmp))
        return -1;
    memcpy(tmp, kw, len);
    tmp[len] = '\0';
    for (i = 0; i < A_COUNT; i++) {
        if (str_ieq(tmp, argdefs[i].name))
            return i;
        if (argdefs[i].alias && str_ieq(tmp, argdefs[i].alias))
            return i;
    }
    return -1;
}

static int parse_num(const char *s, unsigned long *out)
{
    char *end;
    unsigned long v;
    if (*s == '\0' || *s == '-')
        return 0;
    errno = 0;
    v = strtoul(s, &end, 10);
    if (*end != '\0' || errno == ERANGE)
        return 0;
    *out = v;
    return 1;
}

static int set_value(int idx, const char *val)
{
    int t = argdefs[idx].type;
    if (arg_set[idx])
        return 0;
    if (t == T_NUM || t == T_KNUM) {
        if (!parse_num(val, &arg_num[idx])) {
            fprintf(stderr, "Splat: bad number \"%s\" for %s\n", val, argdefs[idx].name);
            return -1;
        }
    }
    arg_str[idx] = val;
    arg_set[idx] = 1;
    return 1;
}

static void print_help(void)
{
    int i;
    printf("%s: \n\n", template_str);
    for (i = 0; exthelp[i] != NULL; i++)
        fputs(exthelp[i], stdout);
}

/* Returns 1 if OK, 0 on error (message already printed), 2 if help shown */
static int read_args(int argc, char **argv)
{
    int i, idx, r;

    if (argc == 2 && (strcmp(argv[1], "?") == 0 || strcmp(argv[1], "-h") == 0 ||
                      strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "/?") == 0)) {
        print_help();
        return 2;
    }

    for (i = 1; i < argc; i++) {
        const char *tok = argv[i];
        const char *eq = strchr(tok, '=');

        /* KEYWORD=value form */
        if (eq != NULL) {
            idx = find_keyword(tok, (size_t)(eq - tok));
            if (idx >= 0 && argdefs[idx].type != T_SW) {
                r = set_value(idx, eq + 1);
                if (r < 0) return 0;
                if (r == 0) goto wrong;
                continue;
            }
        }

        idx = find_keyword(tok, strlen(tok));
        if (idx >= 0) {
            if (argdefs[idx].type == T_SW) {
                arg_set[idx] = 1;
                continue;
            }
            if (i + 1 >= argc) {
                fprintf(stderr, "Splat: keyword %s needs a value\n", argdefs[idx].name);
                return 0;
            }
            r = set_value(idx, argv[++i]);
            if (r < 0) return 0;
            if (r == 0) goto wrong;
            continue;
        }

        /* positional: first free non-keyword argument */
        for (idx = 0; idx < A_COUNT; idx++) {
            int t = argdefs[idx].type;
            if ((t == T_STR || t == T_NUM) && !arg_set[idx])
                break;
        }
        if (idx == A_COUNT)
            goto wrong;
        r = set_value(idx, tok);
        if (r < 0) return 0;
        if (r == 0) goto wrong;
    }

    if (!arg_set[A_FILE]) {
        fprintf(stderr, "Splat: required argument missing\n");
        return 0;
    }
    return 1;

wrong:
    fprintf(stderr, "Splat: wrong number of arguments\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Search string                                                       */
/* ------------------------------------------------------------------ */

static int pattern[MAX_SEARCH];
static int patlen;

static int parse_search(const char *s, int dowild, int nocase)
{
    patlen = 0;
    while (*s) {
        int ch = (unsigned char)*s++;
        if (ch == '\\' && *s) {
            ch = (unsigned char)*s++;
            switch (ch) {
            case 'n': ch = 10; break;
            case 't': ch = 9;  break;
            case 'r': ch = 13; break;
            default:  break;       /* "\\", "\?" and others: literal char */
            }
        } else if (ch == '?' && dowild) {
            ch = WILD;
        }
        if (ch != WILD && nocase)
            ch = toupper(ch);
        if (patlen >= MAX_SEARCH) {
            fprintf(stderr, "Splat: search string too long (max %d chars)\n", MAX_SEARCH);
            return 0;
        }
        pattern[patlen++] = ch;
    }
    if (patlen == 0) {
        fprintf(stderr, "Splat: empty search string\n");
        return 0;
    }
    return 1;
}

/* Sliding window over the last patlen bytes */
static unsigned char ring[MAX_SEARCH];
static int ringpos, ringfill;

static void matcher_reset(void)
{
    ringpos = 0;
    ringfill = 0;
}

/* Feeds one byte, returns 1 when the search string ends on this byte */
static int matcher_feed(int c, int nocase)
{
    int k, p, last;

    if (nocase)
        c = toupper(c);
    ring[ringpos] = (unsigned char)c;
    ringpos = (ringpos + 1 == patlen) ? 0 : ringpos + 1;
    if (ringfill < patlen)
        ringfill++;
    if (ringfill < patlen)
        return 0;

    last = pattern[patlen - 1];
    if (last != WILD && last != c)
        return 0;

    /* ringpos now indexes the oldest byte of the window */
    p = ringpos;
    for (k = 0; k < patlen; k++) {
        if (pattern[k] != WILD && pattern[k] != ring[p])
            return 0;
        if (++p == patlen)
            p = 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Output files                                                        */
/* ------------------------------------------------------------------ */

static char *outbase;
static char *outname;
static unsigned long nfiles;

static const char *file_part(const char *path)
{
    const char *p = path, *r = path;
    for (; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
            r = p + 1;
    return r;
}

static int build_outbase(const char *in, const char *to)
{
    size_t len;
    if (to == NULL) {
        outbase = (char *)malloc(strlen(in) + 1);
        if (outbase == NULL) return 0;
        strcpy(outbase, in);
    } else {
        len = strlen(to);
        if (len > 0 && (to[len - 1] == '/' || to[len - 1] == ':' || to[len - 1] == '\\')) {
            const char *fp = file_part(in);
            outbase = (char *)malloc(len + strlen(fp) + 1);
            if (outbase == NULL) return 0;
            strcpy(outbase, to);
            strcat(outbase, fp);
        } else {
            outbase = (char *)malloc(len + 1);
            if (outbase == NULL) return 0;
            strcpy(outbase, to);
        }
    }
    outname = (char *)malloc(strlen(outbase) + 24);
    return outname != NULL;
}

static FILE *open_next(void)
{
    FILE *f;
    sprintf(outname, "%s.%lu", outbase, nfiles);
    f = fopen(outname, "wb");
    if (f == NULL) {
        fprintf(stderr, "Splat: can't create \"%s\": %s\n", outname, strerror(errno));
        return NULL;
    }
    nfiles++;
    return f;
}

/* ------------------------------------------------------------------ */
/* Ctrl-C                                                              */
/* ------------------------------------------------------------------ */

static volatile sig_atomic_t got_break = 0;

static void on_break(int sig)
{
    (void)sig;
    got_break = 1;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

enum { PH_UNITS, PH_SEARCH, PH_NOCUT, PH_REST, PH_DONE };

int main(int argc, char **argv)
{
    FILE *in = NULL, *out = NULL;
    unsigned char *buf = NULL;
    unsigned long buflen;
    u64 size = 0;           /* SIZE in bytes or lines (or number of parts) */
    u64 total = 0;          /* file length in bytes, or number of lines */
    u64 written = 0;        /* bytes written so far */
    u64 linesdone = 0;      /* newlines consumed so far */
    u64 counter = 0;        /* units left in the current size phase */
    u64 nparts = 0;         /* PARTS: number of parts requested */
    unsigned long partidx = 0, maxfiles = 0;
    int bytemode, linemode, hassearch, recursive, nocut, nocase, quiet, quick, parts;
    int phase = PH_DONE;
    int last = -1;
    int ret = EXIT_FAILURE;
    size_t n, i, seg;

    (void)version;

    switch (read_args(argc, argv)) {
    case 0: return EXIT_FAILURE;
    case 2: return EXIT_SUCCESS;
    default: break;
    }

    hassearch = arg_set[A_SEARCH];
    linemode  = arg_set[A_SIZE] && arg_set[A_LINES];
    bytemode  = arg_set[A_SIZE] && !arg_set[A_LINES];
    parts     = arg_set[A_PARTS];
    recursive = arg_set[A_RECURSIVE] || parts;
    nocut     = arg_set[A_NOCUT];
    nocase    = arg_set[A_NOCASE];
    quiet     = arg_set[A_QUIET];
    quick     = arg_set[A_QUICK];

    if (!hassearch && !arg_set[A_SIZE]) {
        fprintf(stderr, "Splat: required argument missing\n");
        return EXIT_FAILURE;
    }
    if (parts && !arg_set[A_SIZE]) {
        fprintf(stderr, "Splat: PARTS needs SIZE (number of parts)\n");
        return EXIT_FAILURE;
    }
    if (arg_set[A_SIZE]) {
        size = (u64)arg_num[A_SIZE];
        if (arg_set[A_K] && !parts)
            size *= 1024;
        if (size == 0) {
            fprintf(stderr, "Splat: SIZE must be greater than 0\n");
            return EXIT_FAILURE;
        }
    }
    if (arg_set[A_MAX]) {
        maxfiles = arg_num[A_MAX];
        if (maxfiles < 2) {
            fprintf(stderr, "Splat: MAX must be at least 2\n");
            return EXIT_FAILURE;
        }
    }
    if (hassearch && !parse_search(arg_str[A_SEARCH], arg_set[A_DOWILD], nocase))
        return EXIT_FAILURE;

    /* I/O buffer, with an 8k fallback */
    buflen = DEFAULT_BUFLEN;
    if (arg_set[A_BUFFER] && arg_num[A_BUFFER] > 0 && arg_num[A_BUFFER] <= 1024UL * 1024UL)
        buflen = arg_num[A_BUFFER] * 1024UL;
    buf = (unsigned char *)malloc(buflen);
    if (buf == NULL) {
        buflen = FALLBACK_BUFLEN;
        buf = (unsigned char *)malloc(buflen);
        if (buf == NULL) {
            fprintf(stderr, "Not enough memory.\n");
            return EXIT_FAILURE;
        }
    }

    if (!build_outbase(arg_str[A_FILE], arg_set[A_TO] ? arg_str[A_TO] : NULL)) {
        fprintf(stderr, "Not enough memory.\n");
        goto cleanup;
    }

    in = fopen(arg_str[A_FILE], "rb");
    if (in == NULL) {
        fprintf(stderr, "Splat: can't open \"%s\": %s\n", arg_str[A_FILE], strerror(errno));
        goto cleanup;
    }

    signal(SIGINT, on_break);

    /* Pre-pass: measure the file (bytes or lines) when a SIZE is given */
    if (arg_set[A_SIZE]) {
        u64 bytes = 0, nl = 0;
        int lastc = -1;
        while ((n = fread(buf, 1, buflen, in)) > 0) {
            if (linemode) {
                const unsigned char *p = buf, *e = buf + n;
                while ((p = (const unsigned char *)memchr(p, '\n', (size_t)(e - p))) != NULL) {
                    nl++;
                    p++;
                }
            }
            bytes += n;
            lastc = buf[n - 1];
            if (got_break) goto brk;
        }
        if (ferror(in)) {
            fprintf(stderr, "Splat: read error on \"%s\"\n", arg_str[A_FILE]);
            goto cleanup;
        }
        if (bytes == 0) {
            fprintf(stderr, "Splat: \"%s\" is empty\n", arg_str[A_FILE]);
            goto cleanup;
        }
        total = linemode ? nl + (lastc != '\n' ? 1 : 0) : bytes;
        if (parts ? (size > total) : (size >= total)) {
            fprintf(stderr, "SIZE argument is bigger than file size.\n");
            goto cleanup;
        }
        /* PARTS: fixed slice size = ceil(total / parts), then plain
         * recursive split. May give fewer files than requested (e.g. 5
         * parts of 7 bytes -> 2,2,2,1): slices are never rebalanced. */
        if (parts) {
            nparts = size;
            size = (total + nparts - 1) / nparts;
        }
        rewind(in);
    }

    /* Main loop */
    while ((n = fread(buf, 1, buflen, in)) > 0) {
        if (got_break) goto brk;
        i = 0;
        seg = 0;
        while (i < n) {
            if (out == NULL) {
                /* Start a new part */
                out = open_next();
                if (out == NULL) goto cleanup;
                seg = i;

                if ((!recursive && partidx >= 1) ||
                    (maxfiles && partidx + 1 >= maxfiles) ||
                    (parts && partidx + 1 >= nparts)) {
                    phase = PH_REST;
                } else if (arg_set[A_SIZE]) {
                    phase = PH_UNITS;
                    counter = size;
                } else {
                    phase = PH_SEARCH;
                    matcher_reset();
                }
            }

            switch (phase) {
            case PH_UNITS:
                if (bytemode) {
                    size_t take = n - i;
                    if ((u64)take > counter) take = (size_t)counter;
                    i += take;
                    counter -= take;
                } else {
                    const unsigned char *p = (const unsigned char *)memchr(buf + i, '\n', n - i);
                    if (p == NULL) {
                        i = n;
                    } else {
                        i = (size_t)(p - buf) + 1;
                        linesdone++;
                        counter--;
                    }
                }
                last = buf[i - 1];
                if (counter == 0) {
                    if (hassearch) { phase = PH_SEARCH; matcher_reset(); }
                    else phase = nocut ? PH_NOCUT : PH_DONE;
                    if (phase == PH_NOCUT && last == '\n') phase = PH_DONE;
                }
                break;

            case PH_SEARCH:
                while (i < n) {
                    int c = buf[i++];
                    if (c == '\n') linesdone++;
                    if (matcher_feed(c, nocase)) {
                        phase = (nocut && c != '\n') ? PH_NOCUT : PH_DONE;
                        break;
                    }
                }
                last = buf[i - 1];
                break;

            case PH_NOCUT: {
                const unsigned char *p = (const unsigned char *)memchr(buf + i, '\n', n - i);
                if (p == NULL) {
                    i = n;
                } else {
                    i = (size_t)(p - buf) + 1;
                    linesdone++;
                    phase = PH_DONE;
                }
                last = buf[i - 1];
                break;
            }

            default: /* PH_REST */
                i = n;
                break;
            }

            if (phase == PH_DONE) {
                if (fwrite(buf + seg, 1, i - seg, out) != i - seg) goto werr;
                written += i - seg;
                if (fclose(out) != 0) { out = NULL; goto werr; }
                out = NULL;
                partidx++;
                seg = i;
            }
        }
        if (out != NULL && i > seg) {
            if (fwrite(buf + seg, 1, i - seg, out) != i - seg) goto werr;
            written += i - seg;
        }
    }
    if (ferror(in)) {
        fprintf(stderr, "Splat: read error on \"%s\"\n", arg_str[A_FILE]);
        goto cleanup;
    }
    if (nfiles == 0) {
        fprintf(stderr, "Splat: \"%s\" is empty\n", arg_str[A_FILE]);
        goto cleanup;
    }
    if (out != NULL) {
        if (fclose(out) != 0) { out = NULL; goto werr; }
        out = NULL;
    }

    if (!quiet) {
        if (quick) printf("%lu", nfiles);
        else printf("%lu files created.\n", nfiles);
    }
    ret = EXIT_SUCCESS;
    goto cleanup;

werr:
    fprintf(stderr, "Splat: write error on \"%s\": %s\n", outname, strerror(errno));
    goto cleanup;

brk:
    fprintf(stderr, "***Break\n");

cleanup:
    if (out) fclose(out);
    if (in) fclose(in);
    free(buf);
    free(outbase);
    free(outname);
    return ret;
}
