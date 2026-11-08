/*
 * Non-regression tests for splat - portable ANSI C (C89).
 *
 * Usage:   test_splat <path-to-splat-executable>
 *   Unix:  ./test_splat ./splat            (or: make test)
 *   MSVC:  test_splat.exe splat.exe
 *
 * Run it from a writable directory: it creates its input files there
 * (prefix "tsp_") and deletes them, together with all outputs, at the end.
 *
 * For each case it:
 *   - runs splat through system() (sh on Unix, cmd.exe on Windows),
 *   - checks the exit status (zero / non-zero),
 *   - checks the exact size of every output file .0, .1, ...,
 *   - checks that concatenating the parts gives back the original file
 *     byte for byte,
 *   - optionally checks what splat printed on stdout (QUICK / QUIET).
 *
 * Adding a case: append a line to the 'cases' table below.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXPARTS  1000
#define STDOUT_F  "tsp_stdout.txt"
#define STDERR_F  "tsp_stderr.txt"

struct testcase {
    const char *input;           /* input file name (generated below) */
    const char *args;            /* arguments after the input name */
    const char *out_base;        /* output base name, NULL = same as input */
    const char *expect_stdout;   /* exact stdout, NULL = don't check */
    const char *sizes;           /* expected part sizes: "8192*3,1696" means
                                    8192,8192,8192,1696; "FAIL" means splat
                                    must exit with an error and create nothing */
};

/* Search strings are written with splat's own escapes (\n, \t, \?) inside
 * double quotes: this is interpreted identically by sh and cmd.exe. */
static const struct testcase cases[] = {
    /* --- binary file, byte sizes --- */
    { "tsp_bin", "3 parts", NULL, "3 files created.\n", "33334,33333*2" },
    { "tsp_bin", "8 k", NULL, NULL, "8192,91808" },
    { "tsp_bin", "8 K R", NULL, NULL, "8192*12,1696" },
    { "tsp_bin", "8 k recursive max 4", NULL, NULL, "8192*3,75424" },
    { "tsp_bin", "size=8 k r max=4 buf=1", NULL, NULL, "8192*3,75424" },
    { "tsp_bin", "4 parts", NULL, NULL, "25000*4" },
    { "tsp_bin", "1 k r quiet", NULL, "", "1024*97,672" },
    { "tsp_bin", "100 parts quick", NULL, "100", "1000*100" },

    /* --- text file "1\n2\n...1000\n" (3893 bytes) --- */
    { "tsp_txt", "1 k r", NULL, NULL, "1024*3,821" },
    { "tsp_txt", "500 r max 3", NULL, NULL, "500*2,2893" },
    { "tsp_txt", "3 lines", NULL, NULL, "6,3887" },
    { "tsp_txt", "100 lines r", NULL, "10 files created.\n", "292,400*8,401" },
    { "tsp_txt", "7 parts lines", NULL, NULL, "464,572*5,569" },
    { "tsp_txt", "1000 r nocut", NULL, NULL, "1000*3,893" },
    { "tsp_txt", "4 parts nocut", NULL, NULL, "976*2,972,969" },
    { "tsp_txt", "S \"\\n50\\n\"", NULL, NULL, "141,3752" },
    { "tsp_txt", "SEARCH=99 r", NULL, NULL, "287,400*8,363,4*9,7" },
    { "tsp_txt", "s 99 r nocut", NULL, NULL, "288,400*8,364,4*9,5" },
    { "tsp_txt", "S \"9?0\" dowild r quick", NULL, "11", "3491,40*9,42" },
    { "tsp_txt", "S \"9\\?0\" dowild r", NULL, NULL, "3893" },   /* escaped ?: literal */
    { "tsp_txt", "S \"9?0\" r", NULL, NULL, "3893" },            /* no DOWILD: literal */
    { "tsp_txt", "1 k S \"5?0\" dowild r", NULL, NULL, "1891,2002" },
    { "tsp_txt", "2000 search \"\\n1?0\\n\" dowild r nocut", NULL, NULL, "3893" },

    /* --- search details --- */
    { "tsp_case", "S world nocase r", NULL, NULL, "11,10,14" },
    { "tsp_case", "S world r", NULL, NULL, "35" },
    { "tsp_case", "S WORLD r nocut", NULL, NULL, "26,9" },
    { "tsp_aab", "S aab r", NULL, NULL, "4,6" },                  /* backtracking */
    { "tsp_tab", "1 search \"\\t?1\\n\" nocase r nocut dowild", NULL, NULL, "5,7,2" },
    { "tsp_edge", "S MARKER r buf=1", NULL, NULL, "1026,100" },  /* across buffers */
    { "tsp_crlf", "1 lines r", NULL, NULL, "4*2,3" },          /* binary mode */
    { "tsp_crlf", "S \"\\r\" r", NULL, NULL, "3,4*2" },

    /* --- TO keyword --- */
    { "tsp_txt", "2 parts TO tsp_dest", "tsp_dest", NULL, "1947,1946" },
    { "tsp_txt", "2 parts to=tsp_dest", "tsp_dest", NULL, "1947,1946" },

    /* --- errors: non-zero exit status, no file created --- */
    { "tsp_txt", "3893", NULL, NULL, "FAIL" },          /* SIZE >= file size */
    { "tsp_txt", "1 k lines r", NULL, NULL, "FAIL" },   /* 1024 lines > 1000 */
    { "tsp_txt", "3894 parts", NULL, NULL, "FAIL" },
    { "tsp_txt", "", NULL, NULL, "FAIL" },              /* no SIZE nor SEARCH */
    { "tsp_txt", "0", NULL, NULL, "FAIL" },
    { "tsp_txt", "abc", NULL, NULL, "FAIL" },           /* bad number */
    { "tsp_txt", "3 4", NULL, NULL, "FAIL" },           /* too many arguments */
    { "tsp_txt", "3 r max 1", NULL, NULL, "FAIL" },
    { "tsp_txt", "parts", NULL, NULL, "FAIL" },
    { "tsp_txt", "S \"\"", NULL, NULL, "FAIL" },        /* empty search string */
    { "tsp_txt", "3 TO", NULL, NULL, "FAIL" },          /* keyword without value */
    { "tsp_empty", "3", NULL, NULL, "FAIL" },
    { "tsp_empty", "S x", NULL, NULL, "FAIL" },
    { "tsp_missing", "3", NULL, NULL, "FAIL" }
};

static const char *inputs[] = {
    "tsp_bin", "tsp_txt", "tsp_case", "tsp_aab", "tsp_tab", "tsp_edge",
    "tsp_crlf", "tsp_empty"
};

#define NCASES  (sizeof(cases) / sizeof(cases[0]))
#define NINPUTS (sizeof(inputs) / sizeof(inputs[0]))

/* ------------------------------------------------------------------ */

static void fatal(const char *msg, const char *arg)
{
    fprintf(stderr, "test_splat: %s %s\n", msg, arg ? arg : "");
    exit(EXIT_FAILURE);
}

static void write_str(const char *name, const char *s)
{
    FILE *f = fopen(name, "wb");
    size_t len = strlen(s);
    if (f == NULL) fatal("can't create", name);
    if (len && fwrite(s, 1, len, f) != len) fatal("can't write", name);
    if (fclose(f) != 0) fatal("can't write", name);
}

/* Expands "8192*3,1696" into sizes[]; returns the count */
static int parse_sizes(const char *spec, long *sizes)
{
    int n = 0;
    while (*spec) {
        char *end;
        long v = strtol(spec, &end, 10), rep = 1;
        if (end == spec) fatal("bad sizes spec", spec);
        spec = end;
        if (*spec == '*') {
            rep = strtol(spec + 1, &end, 10);
            spec = end;
        }
        while (rep-- > 0) {
            if (n >= MAXPARTS) fatal("too many parts in", spec);
            sizes[n++] = v;
        }
        if (*spec == ',') spec++;
    }
    return n;
}

static void write_file(const char *name, const unsigned char *data, size_t len)
{
    FILE *f = fopen(name, "wb");
    if (f == NULL) fatal("can't create", name);
    if (len && fwrite(data, 1, len, f) != len) fatal("can't write", name);
    if (fclose(f) != 0) fatal("can't write", name);
}

/* Reads a whole file; returns NULL if it doesn't exist */
static unsigned char *read_file(const char *name, long *len)
{
    FILE *f = fopen(name, "rb");
    unsigned char *p = NULL;
    long cap = 0, n = 0;
    size_t r;
    if (f == NULL) return NULL;
    for (;;) {
        if (n == cap) {
            unsigned char *q;
            cap = cap ? cap * 2 : 4096;
            q = (unsigned char *)realloc(p, (size_t)cap);
            if (q == NULL) fatal("out of memory", NULL);
            p = q;
        }
        r = fread(p + n, 1, (size_t)(cap - n), f);
        if (r == 0) break;
        n += (long)r;
    }
    fclose(f);
    *len = n;
    return p;
}

static void make_inputs(void)
{
    static unsigned char buf[100000];
    unsigned long seed = 12345UL;
    size_t i, n;
    char line[16];

    /* 100000 pseudo-random bytes (fixed LCG: same data on every platform),
     * starting with CR, LF, ^Z and NUL to catch any text-mode translation */
    for (i = 0; i < sizeof(buf); i++) {
        seed = (seed * 1103515245UL + 12345UL) & 0xFFFFFFFFUL;
        buf[i] = (unsigned char)(seed >> 16);
    }
    buf[0] = '\r'; buf[1] = '\n'; buf[2] = 0x1A; buf[3] = 0; buf[4] = '\n';
    write_file("tsp_bin", buf, sizeof(buf));

    n = 0;
    for (i = 1; i <= 1000; i++) {
        sprintf(line, "%lu\n", (unsigned long)i);
        memcpy(buf + n, line, strlen(line));
        n += strlen(line);
    }
    write_file("tsp_txt", buf, n);

    write_str("tsp_case", "hello World\nfoo WORLD bar\nend world");
    write_str("tsp_aab", "aaab xaaab");
    write_str("tsp_tab", "a\tB1\nzzz\tx1\nqq");
    write_str("tsp_crlf", "ab\r\ncd\r\nef\r");
    write_str("tsp_empty", "");

    memset(buf, 'x', 1020);
    memcpy(buf + 1020, "MARKER", 6);
    memset(buf + 1026, 'y', 100);
    write_file("tsp_edge", buf, 1126);
}

static void remove_outputs(const char *base)
{
    char name[64];
    int i;
    for (i = 0; i < 1000; i++) {
        sprintf(name, "%s.%d", base, i);
        remove(name);
    }
}

/* Prints the failing command line once, then lets the caller print details */
static int failed_here(int *ok, int num, const struct testcase *tc)
{
    if (*ok)
        printf("FAIL #%d: splat %s %s\n", num, tc->input, tc->args);
    *ok = 0;
    return printf("        ");
}

static int run_case(const char *splat, const struct testcase *tc, int num)
{
    char cmd[1024], name[64];
    const char *base = tc->out_base ? tc->out_base : tc->input;
    unsigned char *orig, *part, *out;
    long origlen = 0, partlen, outlen, pos = 0;
    static long sizes[MAXPARTS];
    int rc, i, nsizes = 0, ok = 1, expect_fail = (strcmp(tc->sizes, "FAIL") == 0);

    remove_outputs(base);
    remove(STDOUT_F);

#ifdef _WIN32
    /* cmd.exe strips the first and last quote of the line: add an outer pair */
    sprintf(cmd, "\"\"%s\" %s %s > %s 2> %s\"", splat, tc->input, tc->args, STDOUT_F, STDERR_F);
#else
    sprintf(cmd, "\"%s\" %s %s > %s 2> %s", splat, tc->input, tc->args, STDOUT_F, STDERR_F);
#endif
    rc = system(cmd);



    if (expect_fail) {
        if (rc == 0) failed_here(&ok, num, tc), printf("expected an error, got exit status %d\n", rc);
        sprintf(name, "%s.0", base);
        part = read_file(name, &partlen);
        if (part) { failed_here(&ok, num, tc), printf("%s should not exist\n", name); free(part); }
        remove_outputs(base);
        return ok;
    }

    if (rc != 0) failed_here(&ok, num, tc), printf("exit status %d\n", rc);

    orig = read_file(tc->input, &origlen);
    if (orig == NULL) fatal("can't read", tc->input);

    nsizes = parse_sizes(tc->sizes, sizes);
    for (i = 0; i < nsizes; i++) {
        sprintf(name, "%s.%d", base, i);
        part = read_file(name, &partlen);
        if (part == NULL) {
            failed_here(&ok, num, tc), printf("%s missing\n", name);
            break;
        }
        if (partlen != sizes[i])
            failed_here(&ok, num, tc), printf("%s: %ld bytes, expected %ld\n", name, partlen, sizes[i]);
        if (pos + partlen > origlen || memcmp(orig + pos, part, (size_t)partlen) != 0)
            failed_here(&ok, num, tc), printf("%s: content differs from the original\n", name);
        pos += partlen;
        free(part);
    }
    if (ok && pos != origlen)
        failed_here(&ok, num, tc), printf("parts total %ld bytes, original is %ld\n", pos, origlen);

    sprintf(name, "%s.%d", base, i);
    part = read_file(name, &partlen);
    if (part) { failed_here(&ok, num, tc), printf("unexpected extra file %s\n", name); free(part); }

    if (tc->expect_stdout) {
        out = read_file(STDOUT_F, &outlen);
        if (out == NULL) fatal("can't read", STDOUT_F);
        /* tolerate CRLF on Windows: drop CRs before comparing */
        {
            long r, w = 0;
            for (r = 0; r < outlen; r++)
                if (out[r] != '\r') out[w++] = out[r];
            outlen = w;
        }
        if (outlen != (long)strlen(tc->expect_stdout) ||
            memcmp(out, tc->expect_stdout, (size_t)outlen) != 0)
            failed_here(&ok, num, tc), printf("stdout was \"%.*s\"\n", (int)outlen, (const char *)out);
        free(out);
    }

    free(orig);
    remove_outputs(base);
    return ok;
}

int main(int argc, char **argv)
{
    size_t i;
    int failed = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: test_splat <path-to-splat>\n");
        return EXIT_FAILURE;
    }

    make_inputs();
    remove("tsp_missing");

    for (i = 0; i < NCASES; i++)
        if (!run_case(argv[1], &cases[i], (int)i + 1))
            failed++;

    for (i = 0; i < NINPUTS; i++)
        remove(inputs[i]);
    remove(STDOUT_F);
    remove(STDERR_F);

    printf("%d/%d tests passed.\n", (int)(NCASES - (size_t)failed), (int)NCASES);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
