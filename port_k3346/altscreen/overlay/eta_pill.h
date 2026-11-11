/*
 * eta_pill — our own arrival-time box on the VC map.
 *
 * The phone's ETA box (showETA=yes) is laid out by the phone: with the maneuver
 * card it is cut down to the arrival time or left out, with Google Maps on the
 * small gauges it drops the distance, and asking for it in a second showUI made
 * the map blink (car, rc28, run-99 and its three films).  So the phone is no
 * longer asked for the box and this pill is drawn in the map picture instead,
 * with the look of the call card (call_card.h) and by the gauges:
 *
 *     06:43 · 15 min · 12 km
 *
 *   /tmp/vc-eta-pill     Java -> overlay   "<arrival>\n<rest>\nEND\n"
 *   /tmp/vc-eta-pill-ok  overlay -> Java   exists while the pill can be drawn
 *
 * The format and its rule are those of capsule_text.h (no rename() on /tmp, a
 * file without the marker is "not ready yet"); two empty lines mean no pill.
 * Java (BAPBridge.publishBar) writes the text only while the pill can be drawn,
 * and puts the times on the VC bar otherwise.
 *
 * No GL in this file, so overlay/tests/eta_pill_test.c runs all of it on the host.
 */

#ifndef K3346_ETA_PILL_H
#define K3346_ETA_PILL_H

#include "call_card.h"

#define ETA_PILL_PATH    "/tmp/vc-eta-pill"
#define ETA_PILL_OK_PATH "/tmp/vc-eta-pill-ok"

/* Where the pill sits in its strip: by the rev counter on the small gauges (and on
 * the Sport skin), in the middle between the two big gauges. */
#define ETA_PILL_ALIGN_LEFT   0
#define ETA_PILL_ALIGN_CENTRE 1

#define ETA_PILL_PAD 20   /* glass before and after the text */

/* The look follows the call card option; with the card off the pill is smoked. */
static int eta_pill_style(int callcard_mode)
{
    return callcard_mode == CALL_CARD_STYLE_LIGHT ? CALL_CARD_STYLE_LIGHT : CALL_CARD_STYLE_SMOKED;
}

/* Left by the rev counter on the small gauges and on the Sport skin (its map is left
 * of the rev counter), the middle between the two big gauges of Classic. */
static int eta_pill_align(int layout_small, int sport)
{
    return !layout_small && !sport ? ETA_PILL_ALIGN_CENTRE : ETA_PILL_ALIGN_LEFT;
}

/* With the big gauges the call card stands where the pill does, so during a call it
 * takes the place; the times are on the VC bar then (K3346NavBarText.timesOnBar).  On
 * the small gauges the two stand side by side. */
static int eta_pill_hidden_by_call(int layout_small, int call_card_visible)
{
    return !layout_small && call_card_visible;
}

static int eta_pill_empty(const capsule_lines_t *lines)
{
    return !lines->left[0] && !lines->right[0];
}

/* The second part as drawn: after the arrival time it starts with the separator. */
static void eta_pill_tail(const capsule_lines_t *lines, char *out, size_t cap)
{
    if (!lines->right[0]) out[0] = '\0';
    else if (lines->left[0]) snprintf(out, cap, " \xC2\xB7 %s", lines->right);
    else snprintf(out, cap, "%s", lines->right);
}

/* How wide the pill is for this text, never wider than the strip. */
static int eta_pill_width(const capsule_font_t *bold, const capsule_font_t *normal,
                          const capsule_lines_t *lines, int w)
{
    char tail[CAPSULE_LINE_MAX + 8];
    int pw;

    if (eta_pill_empty(lines)) return 0;
    eta_pill_tail(lines, tail, sizeof(tail));
    pw = 2 * ETA_PILL_PAD + capsule_text_width(bold, lines->left) + capsule_text_width(normal, tail);
    return pw > w ? w : pw;
}

/* The first column of the pill in its strip. */
static int eta_pill_x0(int pill_w, int w, int align)
{
    return align == ETA_PILL_ALIGN_CENTRE ? (w - pill_w) / 2 : 0;
}

/* rgba top row first, premultiplied; scratch is w*h bytes.  No text: the strip is
 * cleared.  The arrival time is bold, the rest follows in the lighter face; what
 * does not fit the strip ends with the mark, like a long name on the call card. */
static void eta_pill_render(const capsule_font_t *bold, const capsule_font_t *normal,
                            const capsule_lines_t *lines, int style, int align,
                            unsigned char *scratch, unsigned char *rgba, int w, int h)
{
    char tail[CAPSULE_LINE_MAX + 8], shown[CAPSULE_LINE_MAX + 8];
    int pw, x0, baseline, left_w, room;

    memset(rgba, 0, (size_t)w * (size_t)h * 4u);
    if (eta_pill_empty(lines)) return;

    pw = eta_pill_width(bold, normal, lines, w);
    x0 = eta_pill_x0(pw, w, align);
    call_card_glass(rgba, w, h, x0, pw, eta_pill_style(style));

    baseline = (h + (int)(bold->ascent * bold->scale) + (int)(bold->descent * bold->scale)) / 2;
    left_w = capsule_text_width(bold, lines->left);
    eta_pill_tail(lines, tail, sizeof(tail));
    room = pw - 2 * ETA_PILL_PAD - left_w;
    if (room < 0) room = 0;
    call_card_fit(normal, tail, room, shown, sizeof(shown));
    call_card_text(bold, lines->left, (float)(x0 + ETA_PILL_PAD), baseline, 255, 255, 255,
                   scratch, rgba, w, h);
    if (shown[0] && tail[0])
        call_card_text(normal, shown, (float)(x0 + ETA_PILL_PAD + left_w), baseline, 255, 255, 255,
                       scratch, rgba, w, h);
}

/* The overlay says it can draw the pill; removed again when it cannot. */
static int eta_pill_ok_write(const char *path, int ok)
{
    FILE *f;

    if (!ok) return remove(path) == 0 || errno == ENOENT ? 0 : -1;
    f = fopen(path, "w");
    if (!f) return -1;
    if (fputs("1\n", f) < 0) { fclose(f); return -1; }
    return fclose(f) == 0 ? 0 : -1;
}

#endif /* K3346_ETA_PILL_H */
