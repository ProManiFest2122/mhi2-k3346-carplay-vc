/* Which app guides the route, as far as the ETA box on the VC map cares.
 *
 * Apple Maps puts its ETA box in the middle of the safe area when the map goes
 * without the card (/map URL) and ignores maneuverLayout (car, rc21 run-87, rc22
 * run-90).  Google Maps keeps its card and box at the left edge.  So on the small
 * gauges Apple gets an area of its own, wide and further left (view 5 in
 * mib2high-lvds-viewareas.conf).
 *
 * The phone names the app in every route guidance update (iAP2 0x5201, TLV 0x0013
 * SourceName): "Apple Maps" and "Google Maps" in run-90.  The name is the app's
 * display name, so it may be translated; the list covers an iPhone in English and
 * in Polish.
 *
 * mm-ipod (rgd_hook.c) writes the answer to a file in /tmp, the CarPlay session
 * (altscreen_session.c) reads it, the same way as /tmp/k3346-no-rgi.  A missing
 * file or one caught half written means "not Apple": that is the area of rc22. */
#ifndef K3346_NAV_APP_H
#define K3346_NAV_APP_H
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define K3346_NAV_APPLE_PATH "/tmp/k3346-nav-apple"

static int k3346_nav_is_apple(const char *name)
{
    static const char *const apple[] = { "Apple Maps", "Maps", "Mapy" };
    unsigned int i;

    if (name == NULL)
        return 0;
    for (i = 0; i < sizeof(apple) / sizeof(apple[0]); ++i)
        if (strcmp(name, apple[i]) == 0)
            return 1;
    return 0;
}

/* 0 = written.  /tmp on QNX has no rename, see overlay_ready.h: the reader
 * ignores a file that is not exactly "0\n" or "1\n". */
static int k3346_nav_apple_write(const char *path, int apple)
{
    FILE *f = fopen(path, "w");
    int rc;

    if (f == NULL)
        return -1;
    rc = fputs(apple ? "1\n" : "0\n", f) < 0;
    if (fclose(f) != 0)
        rc = 1;
    return rc ? -1 : 0;
}

static int k3346_nav_apple_read(const char *path, int previous)
{
    char buf[8];
    size_t got;
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return errno == ENOENT ? 0 : previous;
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (got != 2 || buf[1] != '\n' || (buf[0] != '0' && buf[0] != '1'))
        return previous;
    return buf[0] - '0';
}

#endif
