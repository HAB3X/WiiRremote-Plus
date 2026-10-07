#ifndef GFX_H
#define GFX_H
#include "common.h"
#include "font.h"

#define RGB(r,g,b) (((u32)(r)<<16)|((u32)(g)<<8)|(u32)(b))

/* ---- themes: dark (default) and light (Wii Menu / Apple style) ---- */
typedef struct {
	u32 bg0, bg1, card, card_hi, line, text, dim, inset, field;
	u32 key_top, key_bot, focus_top, focus_bot;
	u32 soft_good, soft_bad, soft_warn;
	u32 footer, panel_top, panel_bot, dim_ov, shadow;
	u32 badge_bg, badge_fg, help_btn;
	int footer_a, dim_a, shadow_a, stripes;
} Theme;
extern Theme th;
void theme_set(int light);

#define C_BG0       th.bg0
#define C_BG1       th.bg1
#define C_CARD      th.card
#define C_CARD_HI   th.card_hi
#define C_LINE      th.line
#define C_TEXT      th.text
#define C_DIM       th.dim
#define C_INSET     th.inset
#define C_FIELD     th.field
#define C_FOCUS_TOP th.focus_top
#define C_FOCUS_BOT th.focus_bot
#define C_SOFT_GOOD th.soft_good
#define C_SOFT_BAD  th.soft_bad
#define C_SOFT_WARN th.soft_warn
#define C_WHITE    RGB(255,255,255)
#define C_ACCENT   RGB(40,176,232)    /* Wii blue */
#define C_ACCENT2  RGB(100,110,245)
#define C_SEL      RGB(255,149,0)     /* selected (orange) */
#define C_GOOD     RGB(40,180,90)
#define C_BAD      RGB(230,60,60)
#define C_HUD      RGB(24,28,36)      /* dark panels over test pictures */

extern u32 canvas[SCR_W * SCR_H];
void gfx_target(u32 *buf, int w, int h);   /* NULL = back to the UI canvas */
int  gfx_w(void);
int  gfx_h(void);
u32 *gfx_buf(void);

void gfx_clear(u32 c);
void gfx_bg(void);                                   /* themed gradient background */
void gfx_rect(int x, int y, int w, int h, u32 c, int a);
void gfx_vgrad(int x, int y, int w, int h, u32 top, u32 bot, int a);
void gfx_round(int x, int y, int w, int h, int r, u32 c, int a);
void gfx_round_grad(int x, int y, int w, int h, int r, u32 top, u32 bot, int a);
void gfx_round_line(int x, int y, int w, int h, int r, int t, u32 c, int a);
void gfx_circle(int cx, int cy, int r, u32 c, int a);
void gfx_ring(int cx, int cy, int r, int t, u32 c, int a);
void gfx_shadow(int x, int y, int w, int h, int r);
void gfx_tri(int x0, int y0, int x1, int y1, int x2, int y2, u32 c, int a);
void gfx_line(int x0, int y0, int x1, int y1, int t, u32 c, int a);

int  text_w(const Font *f, const char *s);
void text(const Font *f, int x, int y, const char *s, u32 c, int a);      /* y = top */
void text_c(const Font *f, int cx, int y, const char *s, u32 c, int a);   /* centred */
void text_r(const Font *f, int rx, int y, const char *s, u32 c, int a);   /* right aligned */
void text_fit(const Font *f, int x, int y, int maxw, const char *s, u32 c, int a);

/* little controller button badge, e.g. "A", "B", "+", "HOME" */
int  badge(int x, int y, const char *label, u32 c);
int  hint(int x, int y, const char *key, const char *label);

void gfx_cursor(int x, int y);
#endif
