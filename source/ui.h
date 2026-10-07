#ifndef UI_H
#define UI_H
#include "common.h"
#include "gfx.h"

extern Input *in;

enum { ST_NORMAL, ST_PRIMARY, ST_DANGER, ST_KEY, ST_GHOST };

#define UI_FOCUS   1
#define UI_CLICK   2
#define UI_HELD    4
#define UI_ALT     8
#define UI_HOVER   16

void ui_begin(Input *i, int nav_enabled);
int  ui_item(int id, int x, int y, int w, int h);
void ui_end(void);
void ui_suspend(int s);   /* draw widgets without making them interactive (behind a popup) */
void ui_set_focus(int id);
int  ui_get_focus(void);
int  ui_pointer_active(void);
int  ui_focus_rect(int *x, int *y, int *w, int *h);

int  ui_button(int id, int x, int y, int w, int h, const char *label, int style);
int  ui_button2(int id, int x, int y, int w, int h, const char *label, const char *sub, int style, int selected);
void ui_focus_ring(int x, int y, int w, int h, int r);

void ui_header(const char *title, const char *sub);
void ui_footer_begin(void);
void ui_footer_hint(const char *key, const char *label);
void ui_toast(const char *msg);
void ui_draw_toast(void);
void ui_panel(int x, int y, int w, int h);
void ui_dim(void);
int  ui_tile(int id, int x, int y, int w, int h, int ic, const char *label, int style, int selected);
int  ui_ibutton(int id, int x, int y, int w, int h, int ic, const char *label, int style);
int  help_is_open(void);
int  ui_repeat(u32 bit);
void ui_help(const char *title, const char **lines, int n);
int  wrap2(const Font *f, const char *s, int maxw, char *l1, char *l2);   /* split text over 2 lines */

#endif
