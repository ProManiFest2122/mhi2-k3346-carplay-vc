/*
 * Host test for overlay_ready.h - the renderer's "my window is up" report.
 *
 * WHY THIS IS WORTH A TEST.  This one file is what mib2high-lvds-control.sh
 * believes before it switches the cluster off the context that currently works.
 * If it can report readiness that is not true, or hand the shell a half-written
 * line, the cost is the cluster composing a displayable nobody is drawing.  So
 * the cases below are mostly about the ways it must REFUSE to answer.
 *
 * Built with the container's native gcc:  bash overlay/tests/run_host_tests.sh
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../overlay_ready.h"

static int g_pass, g_fail;

static void check(int cond, const char *what)
{
    if (cond) g_pass++;
    else { g_fail++; printf("  FAIL: %s\n", what); }
}

static void check_int(int got, int want, const char *what)
{
    if (got == want) g_pass++;
    else { g_fail++; printf("  FAIL: %s (got %d, want %d)\n", what, got, want); }
}

static const char *P     = "/tmp/ovl_ready_test";
static const char *P_TMP = "/tmp/ovl_ready_test.tmp";

static void put(const char *path, const char *body)
{
    FILE *f = fopen(path, "w");
    if (!f) { printf("  FAIL: cannot write %s\n", path); g_fail++; return; }
    fputs(body, f);
    fclose(f);
}

/* Exactly the field the shell's awk takes: the first blank-separated token. */
static int first_token(const char *path, char *out, size_t n)
{
    FILE *f = fopen(path, "r");
    int got;
    if (!f) return 0;
    got = fscanf(f, "%31s", out);
    fclose(f);
    (void)n;
    return got == 1;
}

static void t_roundtrip(void)
{
    int id = 0, w = 0, h = 0;
    char tok[32];

    printf("write -> read round trip\n");
    overlay_ready_clear(P);
    check_int(overlay_ready_write(P, 98, 210, 153), 0, "write succeeded");
    check_int(overlay_ready_read(P, &id, &w, &h), 1, "read succeeded");
    check_int(id, 98, "id");
    check_int(w, 210, "width");
    check_int(h, 153, "height");

    /* The shell does not parse the file, it takes field 1 with awk.  That has
     * to be the number, not a label, or the gate compares the wrong thing. */
    check(first_token(P, tok, sizeof(tok)) && strcmp(tok, "98") == 0,
          "first token is the bare displayable number");

    check(access(P_TMP, F_OK) != 0, "no .tmp left behind after a write");
}

static void t_clear(void)
{
    int id = 0;
    printf("clear removes the report and its temporary\n");
    check_int(overlay_ready_write(P, 98, 210, 153), 0, "write");
    put(P_TMP, "leftover");
    overlay_ready_clear(P);
    check(access(P, F_OK) != 0, "report gone");
    check(access(P_TMP, F_OK) != 0, "temporary gone");
    check_int(overlay_ready_read(P, &id, NULL, NULL), 0, "read of a missing file says no");
}

static void t_refuses_nonsense(void)
{
    int id = 0, w = 0, h = 0;

    printf("a report that cannot be trusted is not a report\n");

    overlay_ready_clear(P);
    check_int(overlay_ready_read(P, &id, &w, &h), 0, "missing file");

    put(P, "");
    check_int(overlay_ready_read(P, &id, &w, &h), 0, "empty file");

    put(P, "98\n");
    check_int(overlay_ready_read(P, &id, &w, &h), 0, "id only, no geometry");

    put(P, "98 210\n");
    check_int(overlay_ready_read(P, &id, &w, &h), 0, "missing height");

    put(P, "notanumber 210 153\n");
    check_int(overlay_ready_read(P, &id, &w, &h), 0, "non-numeric id");

    /* 0 and -1 are the two caption values this DisplayManager mishandles, so a
     * report carrying them is a bug upstream, not something to act on. */
    put(P, "0 210 153\n");
    check_int(overlay_ready_read(P, &id, &w, &h), 0, "id 0 refused");

    put(P, "-1 210 153\n");
    check_int(overlay_ready_read(P, &id, &w, &h), 0, "id -1 refused");

    put(P, "110 210 153\n");
    check_int(overlay_ready_read(P, &id, &w, &h), 0, "id above MAX_DISPLAYABLES refused");

    put(P, "98 0 0\n");
    check_int(overlay_ready_read(P, &id, &w, &h), 0, "degenerate geometry refused");

    overlay_ready_clear(P);
}

static void t_overwrite(void)
{
    int id = 0, w = 0, h = 0;
    char tok[32];

    printf("a second write replaces the first\n");
    check_int(overlay_ready_write(P, 98, 210, 153), 0, "first write");
    check_int(overlay_ready_write(P, 77, 328, 181), 0, "second write");
    check_int(overlay_ready_read(P, &id, &w, &h), 1, "read");
    check_int(id, 77, "id is the newer one");
    check_int(w, 328, "width is the newer one");
    check_int(h, 181, "height is the newer one");

    /* A SHORTER line over a longer one.  Without O_TRUNC the tail of the old
     * line would survive past the new one and the file would read as
     * "98 210 153\n42" - the gate takes field 1 and would not notice, but
     * anything reading the geometry would. */
    check_int(overlay_ready_write(P, 98, 1440, 542), 0, "long line");
    check_int(overlay_ready_write(P, 9, 16, 16), 0, "shorter line over it");
    {
        FILE *f = fopen(P, "rb");
        char buf[64];
        size_t n = 0;
        if (f) { n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); }
        buf[n] = '\0';
        check(strcmp(buf, "9 16 16\n") == 0, "file holds only the newer line");
    }
    check_int(overlay_ready_read(P, &id, &w, &h), 1, "re-read");
    check_int(w, 16, "no stale geometry from the longer line");

    overlay_ready_clear(P);
    (void)tok;
}

/* The point of the 2026-10-27 rewrite: no rename(), no sibling temporary.
 *
 * On the car the old write-then-rename() version failed every time with
 * errno 18 (EXDEV), because QNX maps /tmp onto /dev/shmem and that filesystem
 * has no rename().  The gate therefore never saw a report and the overlay was
 * never switched on.  This test pins the new behaviour down by making the
 * ".tmp" sibling impossible to create: it is a DIRECTORY here, so any code
 * that still tried to go through it would fail, and a successful write proves
 * the path is not used at all. */
static void t_no_tmp_dependency(void)
{
    int id = 0, w = 0, h = 0;

    printf("the write does not go through a .tmp sibling\n");
    overlay_ready_clear(P);
    rmdir(P_TMP);
    if (mkdir(P_TMP, 0755) != 0) {
        printf("  SKIP: cannot create %s as a directory\n", P_TMP);
        return;
    }
    check_int(overlay_ready_write(P, 98, 1440, 542), 0,
              "write succeeds although .tmp cannot be a file");
    check_int(overlay_ready_read(P, &id, &w, &h), 1, "and the report parses");
    check_int(id, 98, "id");
    check_int(w, 1440, "canvas width reported");

    /* clear() must survive the sibling being a directory, too. */
    overlay_ready_clear(P);
    check(access(P, F_OK) != 0, "report gone");
    rmdir(P_TMP);
    check(access(P_TMP, F_OK) != 0, "test tidied up after itself");
}

static void t_bad_path(void)
{
    printf("an unwritable path fails loudly, not silently\n");
    check(overlay_ready_write("/definitely/not/a/dir/x", 98, 210, 153) != 0,
          "write to a missing directory returns non-zero");
    check(overlay_ready_write(NULL, 98, 210, 153) != 0, "NULL path refused");
    check(overlay_ready_write("", 98, 210, 153) != 0, "empty path refused");
    overlay_ready_clear(NULL);   /* must not crash */
    overlay_ready_clear("");
    g_pass++;                    /* reaching here is the assertion */
}

int main(void)
{
    t_roundtrip();
    t_clear();
    t_refuses_nonsense();
    t_overwrite();
    t_no_tmp_dependency();
    t_bad_path();

    printf("\noverlay_ready: %d OK, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
