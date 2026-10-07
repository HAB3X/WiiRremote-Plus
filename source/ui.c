/*
   ui.c - immediate-mode widgets with pointer and D-pad navigation.
   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "ui.h"
#include "icons.h"
static void btn_colors(int style, int f, u32 *top, u32 *bot, u32 *tc);

Input *in;

typedef struct { int id, x, y, w, h; } Item;
static Item items[160];
static int nitems;
static int focus = -1;
static int nav = 1;
static int focus_seen;
static int hover_id = -1;
static int suspended;

static char toast_msg[80];
static u64 toast_until;

void ui_begin(Input *i, int nav_enabled)
{
	in = i;
	nitems = 0;
	nav = nav_enabled;
	focus_seen = 0;
	hover_id = -1;
	suspended = 0;
}

int ui_pointer_active(void) { return in && in->ptr_valid; }

void ui_suspend(int s) { suspended = s; }

int ui_item(int id, int x, int y, int w, int h)
{
	if (suspended) return 0;
	if (nitems < (int)(sizeof items / sizeof items[0])) items[nitems++] = (Item){ id, x, y, w, h };
	int f = 0;
	if (ui_pointer_active()) {
		if (in->px >= x && in->px < x + w && in->py >= y && in->py < y + h) {
			focus = id; hover_id = id;
			f |= UI_FOCUS | UI_HOVER;
		}
	} else if (focus == id && nav) {
		f |= UI_FOCUS;
	}
	if (focus == id) focus_seen = 1;
	if (f & UI_FOCUS) {
		if (in->down & BTN_A) f |= UI_CLICK;
		if (in->held & BTN_A) f |= UI_HELD;
		if (in->down & BTN_B) f |= UI_ALT;
	}
	return f;
}

void ui_set_focus(int id) { focus = id; }
int  ui_get_focus(void) { return focus; }

int ui_focus_rect(int *x, int *y, int *w, int *h)
{
	for (int i = 0; i < nitems; i++)
		if (items[i].id == focus) { *x = items[i].x; *y = items[i].y; *w = items[i].w; *h = items[i].h; return 1; }
	return 0;
}

void ui_end(void)
{
	if (!nitems) return;
	if (!focus_seen && !ui_pointer_active() && nav) { focus = items[0].id; return; }
	if (!nav || ui_pointer_active()) return;

	int dx = 0, dy = 0;
	/* holding a direction keeps moving (ui_repeat: first press, then every 70 ms after 350 ms) */
	if (ui_repeat(BTN_UP)) dy = -1;
	else if (ui_repeat(BTN_DOWN)) dy = 1;
	else if (ui_repeat(BTN_LEFT)) dx = -1;
	else if (ui_repeat(BTN_RIGHT)) dx = 1;
	if (!dx && !dy) return;

	Item *cur = NULL;
	for (int i = 0; i < nitems; i++) if (items[i].id == focus) cur = &items[i];
	if (!cur) { focus = items[0].id; return; }
	int cx = cur->x + cur->w / 2, cy = cur->y + cur->h / 2;
	long best = -1; int best_id = focus;
	for (int i = 0; i < nitems; i++) {
		Item *it = &items[i];
		if (it->id == focus) continue;
		int ix = it->x + it->w / 2, iy = it->y + it->h / 2;
		int along = dx ? (ix - cx) * dx : (iy - cy) * dy;
		/* must be clearly in that direction */
		if (dx) { if (along <= cur->w / 4) continue; }
		else    { if (along <= cur->h / 4) continue; }
		int across = dx ? abs(iy - cy) : abs(ix - cx);
		/* prefer items that overlap on the other axis */
		int overlap = dx ? (it->y < cur->y + cur->h && it->y + it->h > cur->y)
		                 : (it->x < cur->x + cur->w && it->x + it->w > cur->x);
		long score = (long)along + (long)across * (overlap ? 1 : 4) + (overlap ? 0 : 200);
		if (best < 0 || score < best) { best = score; best_id = it->id; }
	}
	focus = best_id;
}

void ui_focus_ring(int x, int y, int w, int h, int r)
{
	gfx_round_line(x - 3, y - 3, w + 6, h + 6, r + 3, 3, C_ACCENT, 255);
	gfx_round_line(x - 7, y - 7, w + 14, h + 14, r + 7, 4, C_ACCENT, 70);
}

int ui_button2(int id, int x, int y, int w, int h, const char *label, const char *sub, int style, int selected)
{
	int f = ui_item(id, x, y, w, h);
	int r = h < 40 ? h / 2 : 14;
	u32 top, bot, tc;
	btn_colors(style, f, &top, &bot, &tc);
	int press = (f & UI_HELD) ? 2 : 0;
	if (style != ST_GHOST) gfx_shadow(x, y + press, w, h, r);
	gfx_round_grad(x, y + press, w, h, r, top, bot, 255);
	if (style == ST_KEY || style == ST_NORMAL) gfx_round_line(x, y + press, w, h, r, 1, C_LINE, 255);
	if (style == ST_GHOST) gfx_round_line(x, y, w, h, r, 2, C_LINE, 255);
	if (selected) {
		gfx_round_line(x - 3, y - 3 + press, w + 6, h + 6, r + 3, 3, C_SEL, 255);
		char n[8];
		snprintf(n, sizeof n, "%d", selected);
		gfx_circle(x + w - 6, y + 6 + press, 11, C_SEL, 255);
		text_c(F_SMALL, x + w - 6, y - 5 + press, n, C_WHITE, 255);
	}
	if (f & UI_FOCUS) ui_focus_ring(x, y + press, w, h, r);

	const Font *fnt = h >= 44 ? F_BTN : F_BODYB;
	if (text_w(fnt, label) > w - 16) fnt = F_BODYB;
	if (text_w(fnt, label) > w - 16) fnt = F_SMALL;
	if (sub) {
		int ty = y + press + h / 2 - (fnt->height + F_SMALL->height) / 2;
		text_c(fnt, x + w / 2, ty, label, tc, 255);
		text_c(F_SMALL, x + w / 2, ty + fnt->height - 2, sub, style == ST_PRIMARY ? RGB(225, 245, 255) : C_DIM, 255);
	} else {
		char buf[64];
		strncpy(buf, label, 63); buf[63] = 0;
		while (text_w(fnt, buf) > w - 12 && strlen(buf) > 1) buf[strlen(buf) - 1] = 0;
		text_c(fnt, x + w / 2, y + press + (h - fnt->height) / 2 + 1, buf, tc, 255);
	}
	return f;
}

int ui_button(int id, int x, int y, int w, int h, const char *label, int style)
{
	return ui_button2(id, x, y, w, h, label, NULL, style, 0);
}

void ui_header(const char *title, const char *sub)
{
	text(F_TITLE, 40, 26, title, C_TEXT, 255);
	if (sub) text(F_SMALL, 42, 64, sub, C_DIM, 255);
	gfx_rect(40, 90, SCR_W - 80, 2, C_LINE, 160);
}

static int fx, fx_limit;
void ui_footer_begin(void)
{
	/* Wii Menu style bottom bar with a clock */
	gfx_rect(0, SCR_H - 52, SCR_W, 52, th.footer, th.footer_a);
	gfx_rect(0, SCR_H - 52, SCR_W, 2, C_LINE, 255);
	if (th.stripes) gfx_rect(0, SCR_H - 50, SCR_W, 1, C_WHITE, 255);
	char clk[16];
	plat_clock(clk, sizeof clk);
	int cx = SCR_W - 40;
	if (clk[0]) { text_r(F_BODYB, cx, SCR_H - 40, clk, C_DIM, 255); cx -= text_w(F_BODYB, clk) + 14; }
	/* Wii Remote batteries */
	extern int ui_battery[MAX_PADS];
	for (int c = MAX_PADS - 1; c >= 0; c--) {
		int b = ui_battery[c];
		if (b < -1) continue;
		int x = cx - 34, y = SCR_H - 34;
		gfx_round(x, y, 26, 14, 3, C_DIM, 255);
		gfx_rect(x + 26, y + 4, 3, 6, C_DIM, 255);
		gfx_round(x + 2, y + 2, 22, 10, 2, th.footer_a < 200 ? RGB(20, 24, 36) : th.footer, 255);
		if (b >= 0) {
			int bars = b >= 75 ? 4 : b >= 50 ? 3 : b >= 25 ? 2 : 1;
			for (int k = 0; k < bars; k++) gfx_rect(x + 4 + k * 5, y + 4, 4, 6, bars <= 1 ? C_BAD : C_GOOD, 255);
		}
		char n[4]; snprintf(n, sizeof n, "%d", c + 1);
		text_r(F_SMALL, x - 3, y - 3, n, C_DIM, 255);
		cx -= 52;
	}
	fx = 40;
	fx_limit = cx - 10;
}

void ui_footer_hint(const char *key, const char *label)
{
	int need = text_w(F_SMALL, key) + 14 + 6 + text_w(F_SMALL, label);
	if (fx + need > fx_limit) return;   /* no room: leave it out rather than overlap */
	fx += hint(fx, SCR_H - 38, key, label);
}

void ui_toast(const char *msg)
{
	strncpy(toast_msg, msg, sizeof toast_msg - 1);
	toast_until = plat_ms() + 1800;
}

void ui_draw_toast(void)
{
	/* lives in the bottom bar (over the button hints) so it never covers the screen */
	if (plat_ms() > toast_until || !toast_msg[0]) return;
	int lim = fx_limit > 200 ? fx_limit : SCR_W - 200;
	gfx_rect(0, SCR_H - 50, lim, 50, th.footer, 255);
	int w = text_w(F_BODYB, toast_msg) + 40;
	if (w > lim - 30) w = lim - 30;
	int x = 24, y = SCR_H - 45;
	gfx_round(x, y, w, 38, 19, C_HUD, 245);
	gfx_round(x + 12, y + 14, 10, 10, 5, C_GOOD, 255);
	text_fit(F_BODYB, x + 30, y + 7, w - 40, toast_msg, C_WHITE, 255);
}

void ui_panel(int x, int y, int w, int h)
{
	gfx_shadow(x, y, w, h, 20);
	gfx_round_grad(x, y, w, h, 20, th.panel_top, th.panel_bot, 255);
	gfx_round_line(x, y, w, h, 20, 1, C_LINE, 255);
}

void ui_dim(void)
{
	gfx_rect(0, 0, SCR_W, SCR_H, th.dim_ov, th.dim_a);
}

/* ---------------- icon buttons ---------------- */
#include "icons.h"

static void btn_colors(int style, int f, u32 *top, u32 *bot, u32 *tc)
{
	*top = C_CARD_HI; *bot = C_CARD; *tc = C_TEXT;
	switch (style) {
		case ST_PRIMARY: *top = RGB(86, 200, 245); *bot = RGB(30, 158, 222); *tc = C_WHITE; break;
		case ST_DANGER:  *top = RGB(255, 96, 86); *bot = RGB(222, 52, 48); *tc = C_WHITE; break;
		case ST_KEY:     *top = th.key_top; *bot = th.key_bot; break;
		case ST_GHOST:   *top = *bot = C_BG1; break;
	}
	if ((f & UI_FOCUS) && style != ST_PRIMARY && style != ST_DANGER) { *top = C_FOCUS_TOP; *bot = C_FOCUS_BOT; }
}

/* big square-ish tile: icon on top, 1-2 words under it */
int ui_tile(int id, int x, int y, int w, int h, int ic, const char *label, int style, int selected)
{
	int f = ui_item(id, x, y, w, h);
	u32 top, bot, tc;
	btn_colors(style, f, &top, &bot, &tc);
	int press = (f & UI_HELD) ? 2 : 0;
	int r = 16;
	gfx_shadow(x, y + press, w, h, r);
	gfx_round_grad(x, y + press, w, h, r, top, bot, 255);
	gfx_round_line(x, y + press, w, h, r, 1, C_LINE, 255);
	int isz = h >= 80 ? 34 : (h >= 60 ? 26 : 20);
	const Font *fnt = h >= 70 ? F_BODYB : F_SMALL;
	char buf[40]; snprintf(buf, sizeof buf, "%s", label);
	while (text_w(fnt, buf) > w - 10 && strlen(buf) > 1) buf[strlen(buf) - 1] = 0;
	if (ic >= 0 && !buf[0]) {
		icon(ic, x + w / 2, y + press + h / 2, isz, tc);   /* icon only: centred */
	} else if (ic >= 0) {
		int total = isz + 6 + fnt->height;
		int ty = y + press + (h - total) / 2;
		icon(ic, x + w / 2, ty + isz / 2, isz, tc);
		text_c(fnt, x + w / 2, ty + isz + 4, buf, tc, 255);
	} else {
		const Font *bf = h >= 44 ? F_BTN : F_BODYB;
		if (text_w(bf, buf) > w - 10) bf = F_BODYB;
		if (text_w(bf, buf) > w - 10) bf = F_SMALL;
		text_c(bf, x + w / 2, y + press + (h - bf->height) / 2 + 1, buf, tc, 255);
	}
	if (selected) {
		char n[8]; snprintf(n, sizeof n, "%d", selected);
		gfx_round_line(x - 3, y - 3 + press, w + 6, h + 6, r + 3, 3, C_SEL, 255);
		gfx_circle(x + w - 8, y + 8 + press, 12, C_SEL, 255);
		text_c(F_SMALL, x + w - 8, y - 3 + press, n, C_WHITE, 255);
	}
	if (f & UI_FOCUS) ui_focus_ring(x, y + press, w, h, r);
	return f;
}

/* wide button: icon on the left, label */
int ui_ibutton(int id, int x, int y, int w, int h, int ic, const char *label, int style)
{
	int f = ui_item(id, x, y, w, h);
	u32 top, bot, tc;
	btn_colors(style, f, &top, &bot, &tc);
	int press = (f & UI_HELD) ? 2 : 0;
	int r = h / 2 < 14 ? h / 2 : 14;
	gfx_shadow(x, y + press, w, h, r);
	gfx_round_grad(x, y + press, w, h, r, top, bot, 255);
	gfx_round_line(x, y + press, w, h, r, 1, C_LINE, 255);
	int isz = h - 20; if (isz > 24) isz = 24; if (isz < 14) isz = 14;
	const Font *fnt = F_BODYB;
	int lx = x + 14;
	if (ic >= 0) { icon(ic, x + 14 + isz / 2, y + press + h / 2, isz, tc); lx += isz + 10; }
	text_fit(fnt, lx, y + press + (h - fnt->height) / 2 + 1, x + w - lx - 8, label, tc, 255);
	if (f & UI_FOCUS) ui_focus_ring(x, y + press, w, h, r);
	return f;
}

/* ---------------- help overlay ---------------- */
static int help_open, help_page;
int help_is_open(void) { return help_open; }
u32 *help_seen_bits;   /* set by the app so first visits show help once */
void (*help_seen_save)(void);

static u32 help_bit(const char *t)
{
	u32 h = 5381;
	while (*t) h = h * 33 + (unsigned char)*t++;
	return 1u << (h % 32);
}

/* word-wrap s into at most 2 lines that fit maxw */
int wrap2(const Font *f, const char *s, int maxw, char *l1, char *l2)
{
	l2[0] = 0;
	if (text_w(f, s) <= maxw) { strcpy(l1, s); return 1; }
	int n = strlen(s), cut = -1;
	for (int i = 0; i < n; i++) {
		if (s[i] != ' ') continue;
		char t[128]; memcpy(t, s, i); t[i] = 0;
		if (text_w(f, t) <= maxw) cut = i; else break;
	}
	if (cut < 0) { strcpy(l1, s); return 1; }
	memcpy(l1, s, cut); l1[cut] = 0;
	strcpy(l2, s + cut + 1);
	return 2;
}

/* call at the END of a screen. Draws the (?) button, or the help pages. */
void ui_help(const char *title, const char **lines, int n)
{
	u32 bit = help_bit(title);
	if (!help_open && help_seen_bits && !(*help_seen_bits & bit)) {
		*help_seen_bits |= bit;
		if (help_seen_save) help_seen_save();
		help_open = 1; help_page = 0;
		ui_set_focus(8998);
	}
	if (!help_open) {
		int f = ui_item(8999, SCR_W - 92, 28, 52, 44);
		gfx_shadow(SCR_W - 92, 28, 52, 44, 22);
		gfx_round(SCR_W - 92, 28, 52, 44, 22, (f & UI_FOCUS) ? C_ACCENT : th.help_btn, 255);
		icon(IC_HELP, SCR_W - 66, 50, 30, (f & UI_FOCUS) ? C_WHITE : C_TEXT);
		if (f & UI_FOCUS) ui_focus_ring(SCR_W - 92, 28, 52, 44, 22);
		if (f & UI_CLICK) { help_open = 1; help_page = 0; ui_set_focus(8998); in->down = 0; }
		return;
	}
	ui_suspend(0);
	ui_dim();
	int per = 3, pages = (n + per - 1) / per;   /* 3 short tips a page: easy to read */
	if (help_page >= pages) help_page = pages - 1;
	int x = 24, y = 16, w = SCR_W - 48, h = SCR_H - 52 - 24;   /* stays above the bottom bar */
	ui_panel(x, y, w, h);
	gfx_circle(x + 40, y + 40, 20, C_ACCENT, 255);
	icon(IC_HELP, x + 40, y + 40, 30, C_WHITE);
	text(F_TITLE, x + 72, y + 20, title, C_TEXT, 255);
	if (pages > 1) {
		char pg[16]; snprintf(pg, sizeof pg, "%d / %d", help_page + 1, pages);
		text_r(F_BODYB, x + w - 24, y + 28, pg, C_DIM, 255);
	}
	int ly = y + 80;
	for (int i = help_page * per; i < n && i < (help_page + 1) * per; i++) {
		char num[4]; snprintf(num, sizeof num, "%d", i + 1);
		gfx_circle(x + 40, ly + 16, 16, C_ACCENT, 255);
		text_c(F_BODYB, x + 40, ly + 4, num, C_WHITE, 255);
		char l1[128], l2[128];
		int nl = wrap2(F_BTN, lines[i], w - 90, l1, l2);
		text(F_BTN, x + 70, ly + 1, l1, C_TEXT, 255);
		if (nl == 2) text_fit(F_BTN, x + 70, ly + 31, w - 90, l2, C_TEXT, 255);
		ly += nl == 2 ? 70 : 48;
	}
	int by = y + h - 64;
	int last = help_page >= pages - 1;
	if (help_page > 0 && (ui_ibutton(8997, x + 24, by, 150, 48, IC_LEFT, "Back", ST_NORMAL) & UI_CLICK)) { help_page--; in->down = 0; return; }
	if (ui_ibutton(8998, x + w - 204, by, 180, 48, last ? IC_CHECK : IC_RIGHT, last ? "Got it" : "Next", ST_PRIMARY) & UI_CLICK) {
		if (last) help_open = 0; else help_page++;
		in->down = 0;
		ui_set_focus(8998);
		return;
	}
	if (in->down & (BTN_HOME | BTN_B)) { help_open = 0; in->down = 0; }
	else if (in->down & BTN_RIGHT) { if (!last) help_page++; in->down = 0; }
	else if (in->down & BTN_LEFT) { if (help_page) help_page--; in->down = 0; }
	else if ((in->down & BTN_A) && !ui_pointer_active()) { if (last) help_open = 0; else help_page++; in->down = 0; }
}


/* ---------------- hold-to-repeat ---------------- */
/* true on press, then again every 70 ms after holding for 350 ms */
int ui_repeat(u32 bit)
{
	static u64 next[32];
	int k = 0; while (k < 32 && !(bit & (1u << k))) k++;
	if (k >= 32) return 0;
	u64 now = plat_ms();
	if (in->down & bit) { next[k] = now + 350; return 1; }
	if ((in->held & bit) && now >= next[k]) { next[k] = now + 70; return 1; }
	return 0;
}
