#ifndef PATTERNS_H
#define PATTERNS_H
#include "common.h"

/* built-in test pictures (240p Test Suite style, drawn at the Wii's real output size) */
enum {
	PAT_SCENE, PAT_CHECKER, PAT_RAMPS, PAT_EBU, PAT_SMPTE, PAT_REFBARS, PAT_BARSGREY,
	PAT_BLEED, PAT_PLUGE, PAT_STEPS, PAT_RAMP, PAT_FIELD, PAT_IRE, PAT_MONO, PAT_GRID,
	PAT_SHARP, PAT_OVERSCAN, PAT_CONVERGE, PAT_STRIPES, PAT_SKIN, PAT_BLOOM, PAT_AVSYNC, PAT_LAG,
	PAT_RED, PAT_GREEN, PAT_BLUE, PAT_GREY50, PAT_CHANNEL, PAT_EDGE, PAT_WASHER, PAT_SWEEP, PAT_SPEAKERS,
	PAT_SCROLL, PAT_GRIDSCROLL, PAT_SPRITE, PAT_DIAG, PAT_WHITECLIP, PAT_GAMMA, PAT_LINEARITY, PAT_WARMUP, PAT_BUILTIN
};

void pat_scan_user(void);              /* look for pictures on the SD card */
int  pat_count(void);                   /* built-in + your pictures */
int  pat_user_count(void);
const char *pat_name(int i);
const char *pat_tip(int i);
const char *pat_option(int i);          /* current setting text, "" if none */
const char *pat_controls(int i);        /* what the D-pad / A / B do in picture mode, "" if nothing */
void pat_draw(int i);                   /* into the current gfx target */
int  pat_input(int i, u32 down);        /* picture-mode buttons; returns 1 if it must be redrawn */
const char *pat_user_folder(void);
int  pat_animated(int i);               /* must be redrawn every frame */
int  pat_grabs_input(int i);            /* test that always gets the buttons (lag test) */
void pat_leave(void);
void pat_pointer(int valid, int x, int y);   /* Wii Remote pointer in picture pixels */                   /* stop sounds when a test picture is closed or changed */

/* the Calibration Suite's groups */
enum { CAT_COLOUR, CAT_GREY, CAT_GEOM, CAT_SHARP, CAT_MOTION, CAT_AUDIO, CAT_CARE, CAT_USER, CAT_COUNT };
extern const char *CAT_NAME[CAT_COUNT], *CAT_TIP[CAT_COUNT];
int pat_in_cat(int cat, int *out, int max);   /* test picture numbers in a group */

#endif
