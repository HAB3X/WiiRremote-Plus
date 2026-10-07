/*
   app.c - app core: navigation, sending, text entry, home, settings.
   WiiRremote Plus by Habex.
   Based on WiiRremote by Abdelali221 (https://github.com/abdelali221/WiiRremote).
   GPL-3.0-or-later.
*/
#include "app.h"
#include "patterns.h"
#include "presets.h"

int screen = S_HOME;
Remote cur;
int have_cur = 0;
int edit_btn = -1, edit_note = -1;
u8 pend_proto, pend_cmd;
u16 pend_addr;
Code pend_macro[MAX_STEPS];
int pend_macro_n;
char last_sent[NAME_LEN + 8];
u64 last_sent_ms;
u64 sleep_at;
int sleep_btn = -1;

/* where HOME / Back leads: the screens you came through (typing screens are skipped) */
static int nav_stack[16], nav_n;
void go(int s)
{
	if (s != screen && screen != S_KEYBOARD && screen != S_NAME && screen != S_QUIT) {
		/* going back to a screen already on the path = trim the path there */
		int k = 0; while (k < nav_n && nav_stack[k] != s) k++;
		if (k < nav_n) nav_n = k;
		else {
			if (nav_n == 16) { memmove(nav_stack, nav_stack + 1, 15 * sizeof(int)); nav_n--; }
			nav_stack[nav_n++] = screen;
		}
	}
	if (s == S_HOME) nav_n = 0;   /* Home (your TVs) is the root */
	screen = s;
	ui_set_focus(-1);
	if (s == S_HOME) home_refresh();
}
void go_back(void)
{
	int s = S_HOME;
	while (nav_n > 0) {
		s = nav_stack[--nav_n];
		if (s == S_REMOTE && !have_cur) continue;   /* that TV is gone */
		break;
	}
	if (nav_n == 0 && s != S_HOME && s == S_REMOTE && !have_cur) s = S_HOME;
	screen = s;
	ui_set_focus(-1);
	if (s == S_HOME) home_refresh();
}

/* ---- undo: one step back for deletes and overwrites ---- */
static Remote undo_r;
static int undo_ok;
static u64 undo_until;
static char undo_what[32];

void undo_save(const char *what)
{
	undo_r = cur;
	undo_ok = 1;
	undo_until = plat_ms() + 10000;
	snprintf(undo_what, sizeof undo_what, "%s", what);
}

int undo_available(void) { return undo_ok && plat_ms() < undo_until; }

int undo_do(void)
{
	if (!undo_available()) return 0;
	cur = undo_r;
	undo_ok = 0;
	save_cur();
	char m[48]; snprintf(m, sizeof m, "Undone: %s", undo_what);
	ui_toast(m);
	return 1;
}

void draw_undo_chip(void)
{
	if (!undo_available()) return;
	char m[48]; snprintf(m, sizeof m, "%s - press - to undo", undo_what);
	int w = text_w(F_BODYB, m) + 30;
	gfx_round((SCR_W - w) / 2, SCR_H - 160, w, 38, 19, C_SEL, 240);
	text_c(F_BODYB, SCR_W / 2, SCR_H - 153, m, C_WHITE, 255);
}

void save_cur(void)
{
	if (remote_save(&cur)) {
		strcpy(cfg.last, cur.file);
		config_save();
	} else {
		ui_toast("Can't save - is the SD card in?");
	}
}

void open_remote_file(const char *file)
{
	if (remote_load(&cur, file)) {
		have_cur = 1;
		sleep_at = 0;
		strcpy(cfg.last, file);
		config_save();
		remote_screen_reset();
		go(S_REMOTE);
	} else {
		ui_toast("Can't open that remote");
	}
}

/* ------------------------------------------------------------------ */
/* sending: single press, hold-to-repeat, queued sequences              */
/* ------------------------------------------------------------------ */

Input menu_in, tv_in;
u32 flash_bit;
u64 flash_ms;
static struct { int active; u8 p, c; u16 a; u32 bit; u64 next; Input *src; } hold;
static Code q[64];
static int qn, qi;
static u64 q_next;

static void set_last(const char *name)
{
	snprintf(last_sent, sizeof last_sent, "%s", name);
	last_sent_ms = plat_ms();
}

static void buzz(void) { if (cfg.rumble) plat_rumble(18); plat_click(); }   /* a tiny tap: the code went out */

static Input *press_src;

void send_code_press(u8 proto, u16 addr, u8 cmd, u32 hold_bit)
{
	ir_send(proto, addr, cmd);
	buzz();
	hold.active = hold_bit != 0; hold.p = proto; hold.a = addr; hold.c = cmd; hold.bit = hold_bit;
	hold.src = press_src ? press_src : &menu_in;
	press_src = NULL;
	hold.next = plat_ms() + 450;
}

void queue_codes(const Code *c, int n)
{
	qn = 0; qi = 0;
	for (int i = 0; i < n && qn < 64; i++) q[qn++] = c[i];
	q_next = 0;
}

int expand_btn(int i, Code *out, int max)
{
	if (i < 0 || i >= cur.nbtn || max <= 0) return 0;
	RBtn *b = &cur.btn[i];
	if (!b->nsteps) { out[0].proto = b->proto; out[0].addr = b->addr; out[0].cmd = b->cmd; return 1; }
	int n = b->nsteps < max ? b->nsteps : max;
	memcpy(out, b->steps, n * sizeof(Code));
	return n;
}

void press_btn(int i, u32 bit)
{
	if (i < 0 || i >= cur.nbtn) return;
	RBtn *b = &cur.btn[i];
	if (b->nsteps) {
		queue_codes(b->steps, b->nsteps);
	} else {
		send_code_press(b->proto, b->addr, b->cmd, bit);
	}
	set_last(b->name);
}

/* Mapped physical buttons from one input source.
   wm_mask selects which Wii Remote buttons are live. */
/* Shift layer: B held (or tapped on/off) switches the Wii Remote to map_wm2 */
int shift_on_menu, shift_on_tv;

int shift_active(Input *src)
{
	if (!cfg.shift_mode) return 0;
	if (cfg.shift_mode == 1) return (src->held & BTN_B) != 0;
	return src == &tv_in ? shift_on_tv : shift_on_menu;
}

int handle_mapped(Input *src, u32 wm_mask)
{
	if (!have_cur || !src->down) return 0;
	int use_shift = cfg.shift_mode && (wm_mask & BTN_B);
	if (use_shift) {
		wm_mask &= ~BTN_B;   /* B is the shift button, not a TV button */
		if (cfg.shift_mode == 2 && (src->down & BTN_B)) {
			int *t = src == &tv_in ? &shift_on_tv : &shift_on_menu;
			*t = !*t;
			ui_toast(*t ? "Shift ON" : "Shift off");
		}
	}
	int layer2 = use_shift && shift_active(src);
	for (int k = 0; k < WM_COUNT; k++) {
		int idx = (layer2 && cur.map_wm2[k] >= 0) ? cur.map_wm2[k] : cur.map_wm[k];
		if (layer2 && cur.map_wm2[k] < 0) idx = -1;   /* shift layer only uses its own buttons */
		if ((src->down & WM_BITS[k]) && (WM_BITS[k] & wm_mask) && idx >= 0) {
			press_src = src;
			press_btn(idx, WM_BITS[k]);
			flash_bit = WM_BITS[k]; flash_ms = plat_ms();
			return 1;
		}
	}
	for (int k = 0; k < CC_COUNT; k++)
		if ((src->down & CC_BITS[k]) && cur.map_cc[k] >= 0) {
			press_src = src;
			press_btn(cur.map_cc[k], CC_BITS[k]);
			flash_bit = CC_BITS[k]; flash_ms = plat_ms();
			return 1;
		}
	return 0;
}

static void send_update(void)
{
	u64 now = plat_ms();
	if (hold.active) {
		if (hold.src->held & hold.bit) {
			if (now >= hold.next) {
				ir_send_repeat(hold.p, hold.a, hold.c);
				hold.next = plat_ms() + cfg.repeat_ms;
			}
		} else hold.active = 0;
	}
	if (qi < qn && now >= q_next) {
		ir_send(q[qi].proto, q[qi].addr, q[qi].cmd);
		q_next = plat_ms() + (q[qi].wait ? q[qi].wait : cfg.seq_gap);
		qi++;
		buzz();
	}
	/* sleep timer */
	if (sleep_at && now >= sleep_at) {
		sleep_at = 0;
		if (have_cur && sleep_btn >= 0 && sleep_btn < cur.nbtn) {
			press_btn(sleep_btn, 0);
			ui_toast("Sleep timer: TV off");
		}
	}
}

void draw_last_sent(int x, int y)
{
	u64 age = plat_ms() - last_sent_ms;
	if (!last_sent_ms || age > 1600) return;
	int a = age > 1200 ? (int)(255 - (age - 1200) * 255 / 400) : 255;
	char m[40]; snprintf(m, sizeof m, "Sent  %s", last_sent);
	int w = text_w(F_BODYB, m) + 48;
	gfx_round(x - w, y, w, 36, 18, C_GOOD, a);
	icon(IC_CHECK, x - w + 20, y + 18, 18, C_WHITE);
	text(F_BODYB, x - w + 36, y + 6, m, C_WHITE, a);
}

void draw_sleep_chip(int x, int y)
{
	if (!sleep_at) return;
	u64 left = (sleep_at - plat_ms()) / 1000;
	char m[24]; snprintf(m, sizeof m, "%d:%02d", (int)(left / 60), (int)(left % 60));
	int w = text_w(F_BODYB, m) + 44;
	gfx_round(x, y, w, 32, 16, C_ACCENT2, 255);
	icon(IC_MOON, x + 18, y + 16, 18, C_WHITE);
	text(F_BODYB, x + 32, y + 4, m, C_WHITE, 255);
}

/* the remote's button that opens the service menu (named SERVICE / FACTORY) */
int service_btn(void)
{
	if (!have_cur) return -1;
	int i = remote_find_btn(&cur, "SERVICE");
	if (i < 0) i = remote_find_btn(&cur, "FACTORY");
	if (i < 0) i = remote_find_btn(&cur, "SERVICE MENU");
	return i;
}

/* ------------------------------------------------------------------ */
/* popup menu                                                           */
/* ------------------------------------------------------------------ */

int menu_popup(int *open, const char **items, const int *icons, int n, const char *title)
{
	if (!*open) return -1;
	ui_suspend(0);
	ui_dim();
	/* always fits above the bottom bar; long menus use two columns */
	int two = n > 8, rows = two ? (n + 1) / 2 : n;
	int compact = 72 + rows * 54 + 8 > 404;
	int w = two ? 540 : 320, bh = compact ? (rows > 7 ? 34 : 38) : 46, gap = compact ? 5 : 8, top = compact ? 46 : 62;
	int h = top + 6 + rows * (bh + gap) + 2;
	int x = (SCR_W - w) / 2, y = (SCR_H - 52 - h) / 2;
	int cw = two ? (w - 50) / 2 : w - 40;
	ui_panel(x, y, w, h);
	text_c(F_BODYB, SCR_W / 2, y + 20, title, C_TEXT, 255);
	int chosen = -1;
	for (int i = 0; i < n; i++) {
		int st = ST_NORMAL;
		if (!strncmp(items[i], "Delete", 6) && !strncmp(title, "Delete", 6)) st = ST_DANGER;   /* red only on the final "are you sure" */
		int col = two ? i / rows : 0, row = two ? i % rows : i;
		if (ui_ibutton(9000 + i, x + 20 + col * (cw + 10), y + top + row * (bh + gap), cw, bh, icons ? icons[i] : -1, items[i], st) & UI_CLICK) chosen = i;
	}
	if (in->down & (BTN_HOME | BTN_B)) { *open = 0; in->down = 0; return -1; }
	if (chosen >= 0) *open = 0;
	return chosen;
}

/* ------------------------------------------------------------------ */
/* keyboard and name picker                                             */
/* ------------------------------------------------------------------ */

char kb_text[NOTE_LEN];
static int kb_purpose, kb_return, kb_max = NAME_LEN - 1;
static char kb_title[48];
static int kb_lower;

static int kb_ph;   /* the text is a suggestion: the first key typed replaces it */
void kb_open(int purpose, const char *initial, const char *title, int maxlen)
{
	kb_purpose = purpose;
	if (screen != S_NAME) kb_return = screen;
	kb_max = maxlen;
	snprintf(kb_text, sizeof kb_text, "%s", initial ? initial : "");
	kb_text[kb_max] = 0;
	snprintf(kb_title, sizeof kb_title, "%s", title);
	kb_ph = kb_text[0] != 0;
	go(S_KEYBOARD);
}

static int name_purpose;
static char name_title[48];
void name_open(int purpose, const char *title)
{
	name_purpose = purpose;
	kb_return = screen;
	snprintf(name_title, sizeof name_title, "%s", title);
	go(S_NAME);
}

static void kb_cancel(void) { go(kb_return); }

void kb_type_char(char ch)
{
	if (kb_ph) { kb_text[0] = 0; kb_ph = 0; }
	int len = strlen(kb_text);
	if (ch == '|') ch = '/';
	if (len < kb_max) { kb_text[len] = ch; kb_text[len + 1] = 0; }
}

void scr_keyboard(void)
{
	gfx_bg();
	icon(IC_PENCIL, 58, 50, 30, C_ACCENT);
	text(F_TITLE, 84, 30, kb_title, C_TEXT, 255);
	gfx_shadow(40, 96, SCR_W - 80, 56, 14);
	gfx_round(40, 96, SCR_W - 80, 56, 14, C_FIELD, 255);
	gfx_round_line(40, 96, SCR_W - 80, 56, 14, 2, C_ACCENT, 255);
	if (kb_ph && kb_text[0]) gfx_round(54, 106, text_w(F_BTN, kb_text) + 10, 36, 8, C_ACCENT, 90);   /* selected: typing replaces it */
	text_fit(F_BTN, 58, 108, SCR_W - 180, kb_text, C_TEXT, 255);
	int cx = 58 + text_w(F_BTN, kb_text) + 2;
	if (cx > SCR_W - 120) cx = SCR_W - 120;
	if ((plat_ms() / 500) % 2) gfx_rect(cx, 110, 3, 30, C_ACCENT, 255);
	char cnt[16]; snprintf(cnt, sizeof cnt, "%d/%d", (int)strlen(kb_text), kb_max);
	text_r(F_SMALL, SCR_W - 56, 114, cnt, C_DIM, 255);

	static const char *rows[4] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM+./" };
	int kw = 50, kh = 44, g = 6;
	int y0 = 166;
	int len = strlen(kb_text);
	for (int r = 0; r < 4; r++) {
		int n = strlen(rows[r]);
		int x0 = (SCR_W - (n * kw + (n - 1) * g)) / 2;
		for (int i = 0; i < n; i++) {
			char k[2] = { rows[r][i], 0 };
			if (kb_lower && k[0] >= 'A' && k[0] <= 'Z') k[0] += 32;
			int f = ui_tile(100 + r * 16 + i, x0 + i * (kw + g), y0 + r * (kh + g), kw, kh, -1, k, ST_KEY, 0);
			if ((f & UI_CLICK) || ((f & UI_HELD) && !(in->down & BTN_A) && ui_repeat(BTN_A))) { kb_type_char(k[0]); len = strlen(kb_text); }
		}
	}
	int y4 = y0 + 4 * (kh + g) + 2;
	int x = 43;
	if (ui_tile(200, x, y4, 90, kh, -1, kb_lower ? "ABC" : "abc", ST_NORMAL, 0) & UI_CLICK) kb_lower = !kb_lower;
	x += 96;
	int fsp = ui_tile(201, x, y4, 164, kh, -1, "Space", ST_KEY, 0);
	if ((fsp & UI_CLICK) || ((fsp & UI_HELD) && !(in->down & BTN_A) && ui_repeat(BTN_A))) { if (kb_ph) { kb_text[0] = 0; kb_ph = 0; } len = strlen(kb_text); if (len && len < kb_max) { kb_text[len] = ' '; kb_text[len + 1] = 0; } }
	x += 170;
	int fdl = ui_ibutton(202, x, y4, 120, kh, IC_BACK, "Delete", ST_NORMAL);
	int del = (fdl & UI_CLICK) || ((fdl & UI_HELD) && !(in->down & BTN_A) && ui_repeat(BTN_A)) || ui_repeat(BTN_B);
	if (del) { if (kb_ph) { kb_text[0] = 0; kb_ph = 0; } else { len = strlen(kb_text); if (len) kb_text[len - 1] = 0; } }
	x += 126;
	if (ui_ibutton(203, x, y4, 128, kh, IC_CHECK, "Done", ST_NORMAL) & UI_CLICK) {
		int n = strlen(kb_text);
		while (n && kb_text[n - 1] == ' ') kb_text[--n] = 0;
		if (n) text_done(kb_purpose, kb_text); else ui_toast("Type something first");
		return;
	}
	if ((in->down & BTN_PLUS) && strlen(kb_text)) { text_done(kb_purpose, kb_text); return; }

	ui_footer_begin();
	ui_footer_hint("A", "Type");
	if (usb.kb_connected) ui_footer_hint("KEYS", "Type on your keyboard");
	ui_footer_hint("B", "Delete");
	ui_footer_hint("+", "Done");
	ui_footer_hint("HOME", "Cancel");
	if (in->down & BTN_HOME) kb_cancel();
}

/* quick names: every button a CRT TV remote has, in 4 pages */
static int name_tab;
void scr_name(void)
{
	static const char *TABS[4] = { "Main", "Picture & sound", "Numbers & inputs", "Teletext & more" };
	static const char *P[4][24] = {
		{ "POWER", "INPUT", "MUTE", "MENU", "OK", "EXIT",
		  "UP", "DOWN", "LEFT", "RIGHT", "BACK", "INFO",
		  "VOL+", "VOL-", "P+", "P-", "RECALL", "SLEEP",
		  "PICTURE", "SOUND", "ASPECT", "SYSTEM", "SERVICE", "UNKNOWN" },
		{ "BRIGHT+", "BRIGHT-", "CONTRAST+", "CONTRAST-", "COLOUR+", "COLOUR-",
		  "TINT+", "TINT-", "SHARP+", "SHARP-", "NORMALISE", "PP",
		  "GAME", "100HZ", "DNR", "SOUND SYS", "STEREO", "I/II",
		  "HALF MUTE", "SURROUND", "BASS", "TREBLE", "EQ", "SPEAKER" },
		{ "1", "2", "3", "4", "5", "6",
		  "7", "8", "9", "0", "-/--", "100+",
		  "TV", "AV", "AV1", "AV2", "AV3", "COMP",
		  "S-VIDEO", "SCART", "HDMI1", "HDMI2", "PWR ON", "PWR OFF" },
		{ "TEXT", "HOLD", "REVEAL", "SIZE", "INDEX", "SUBPAGE",
		  "MIX", "CANCEL", "RED", "GREEN", "YELLOW", "BLUE",
		  "FAV", "CLOCK", "TIMER", "LOCK", "PC", "HDMI",
		  "TUNE", "SEARCH", "PAUSE", "REW", "FF", "FACTORY" },
	};
	gfx_bg();
	icon(IC_PENCIL, 58, 50, 30, C_ACCENT);
	text(F_TITLE, 84, 30, name_title, C_TEXT, 255);
	int cw = 88, ch = 44, g = 6;
	int x0 = (SCR_W - (6 * cw + 5 * g)) / 2;
	/* page tabs */
	int tw = (6 * cw + 5 * g - 3 * g) / 4;
	for (int t = 0; t < 4; t++) {
		int ft = ui_button(380 + t, x0 + t * (tw + g), 84, tw, 34, TABS[t], t == name_tab ? ST_PRIMARY : ST_NORMAL);
		if (ft & UI_CLICK) { name_tab = t; ui_set_focus(300); }
	}
	if ((in->down & BTN_1) && name_tab > 0) name_tab--;
	if ((in->down & BTN_2) && name_tab < 3) name_tab++;
	for (int i = 0; i < 24; i++) {
		int x = x0 + (i % 6) * (cw + g), y = 128 + (i / 6) * (ch + g);
		const char *nm = P[name_tab][i];
		int ic = icon_for_name(nm);
		int fl = ui_tile(300 + i, x, y, cw, ch, -1, nm, ic >= 0 ? ST_NORMAL : ST_KEY, 0);
		if (fl & UI_CLICK) { text_done(name_purpose, nm); return; }
	}
	int y = 128 + 4 * (ch + g) + 8;
	if (ui_ibutton(390, x0, y, 300, 46, IC_PENCIL, "Type my own name", ST_NORMAL) & UI_CLICK) {
		kb_open(name_purpose, "", name_title, NAME_LEN - 1);
		return;
	}
	if (ui_ibutton(391, x0 + 6 * (cw + g) - g - 170, y, 170, 46, IC_CLOSE, "Cancel", ST_NORMAL) & UI_CLICK) { kb_cancel(); return; }
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("1", "Page");
	ui_footer_hint("2", "Page");
	ui_footer_hint("HOME", "Cancel");
	if (in->down & BTN_HOME) kb_cancel();
}

static u8 newr_proto = P_NEC, newr_cmd = 0;
static u16 newr_addr = 0;
static int newr_has_cmd = 0;

void text_done(int purpose, const char *txt)
{
	char nm[NOTE_LEN];
	snprintf(nm, sizeof nm, "%s", txt);
	if (purpose != K_NOTE_ADD && purpose != K_NOTE_EDIT && purpose != K_CONS_NOTE && purpose != K_HEALTH && purpose != K_GUIDE_NOTE) nm[NAME_LEN - 1] = 0;
	switch (purpose) {
	case K_NEW_REMOTE:
	case K_SCAN_REMOTE: {
		int nr = purpose == K_NEW_REMOTE;
		u8 p = nr ? newr_proto : pend_proto, c = nr ? newr_cmd : pend_cmd;
		u16 a = nr ? newr_addr : pend_addr;
		int hc = nr ? newr_has_cmd : 1;
		remote_new(&cur, nm, p, a);
		if (hc) { char bn[NAME_LEN]; snprintf(bn, sizeof bn, "Found %02X", c); remote_add_btn(&cur, bn, p, a, c); }
		have_cur = 1;
		save_cur();
		remote_screen_reset();
		go(S_REMOTE);
		ui_toast("Remote made!");
		break;
	}
	case K_RENAME_REMOTE:
		strcpy(cur.name, nm); save_cur(); go(S_REMOTE); ui_toast("Renamed");
		break;
	case K_NEW_BTN:
	case K_CODE_BTN: {
		char un[NAME_LEN]; unique_name(&cur, nm, un); strcpy(nm, un);
		{ int f = fid_from_name(nm); smart_note(pend_proto, pend_addr, pend_cmd, f >= 0 ? resp_for_fid(f) : R_OTHER, 1); }
		if (remote_add_btn(&cur, nm, pend_proto, pend_addr, pend_cmd) < 0) ui_toast("This remote is full");
		else { remote_automap(&cur); save_cur(); char m[64]; snprintf(m, sizeof m, "Saved %s", nm); ui_toast(m); }
		go(purpose == K_NEW_BTN ? S_FINDER : S_REMOTE);
		break;
	}
	case K_MACRO: {
		char un[NAME_LEN]; unique_name(&cur, nm, un); strcpy(nm, un);
		if (remote_add_macro(&cur, nm, pend_macro, pend_macro_n) < 0) ui_toast("This remote is full");
		else { save_cur(); ui_toast("Macro saved"); }
		remote_screen_reset();
		go(S_REMOTE);
		break;
	}
	case K_RENAME_BTN:
		if (edit_btn >= 0 && edit_btn < cur.nbtn) { char un[NAME_LEN]; if (strcmp(cur.btn[edit_btn].name, nm)) unique_name(&cur, nm, un); else strcpy(un, nm); strcpy(cur.btn[edit_btn].name, un); remote_automap(&cur); save_cur(); }
		go_back();   /* back to the remote (or remote picture) */
		break;
	case K_NOTE_ADD:
		if (cur.nnotes < MAX_NOTES) { snprintf(cur.notes[cur.nnotes++], NOTE_LEN, "%s", nm); save_cur(); }
		go(S_NOTES);
		break;
	case K_PROFILE:
		if (profile_create(nm)) { profile_use(nm); config_save(); have_cur = 0; ui_toast("Room made"); }
		else ui_toast("Can't make that room");
		go(S_HOME);
		break;
	case K_HEALTH: health_add(nm); go(S_HEALTH); break;
	case K_GUIDE_NOTE:
		if (cur.nnotes < MAX_NOTES) { snprintf(cur.notes[cur.nnotes++], NOTE_LEN, "%s", nm); save_cur(); ui_toast("Written in Notes"); }
		else ui_toast("Notes are full");
		go(S_CAL);
		break;
	case K_SV_NAME: case K_SV_BEFORE: case K_SV_AFTER: svals_text_done(purpose, nm); break;
	case K_CONS_NAME: case K_CONS_NOTE:
		cons_text_done(purpose, nm);
		break;
	case K_NOTE_EDIT:
		if (edit_note >= 0 && edit_note < cur.nnotes) { snprintf(cur.notes[edit_note], NOTE_LEN, "%s", nm); save_cur(); }
		go(S_NOTES);
		break;
	}
}

/* ------------------------------------------------------------------ */
/* home                                                                 */
/* ------------------------------------------------------------------ */

static char rfiles[MAX_REMOTES][40], rnames[MAX_REMOTES][NAME_LEN];
static Remote rinfo[MAX_REMOTES];
static int nrem = -1, home_scroll = 0;

void home_refresh(void) { nrem = -1; }

static void logo(int x, int y)
{
	gfx_round_grad(x, y, 46, 46, 13, RGB(84, 214, 255), RGB(120, 110, 255), 255);
	for (int k = 0; k < 3; k++) gfx_ring(x + 23, y + 23, 8 + k * 7, 2, RGB(255, 255, 255), 200 - k * 60);
	gfx_circle(x + 23, y + 23, 4, RGB(255, 255, 255), 255);
}

void scr_home(void)
{
	if (nrem < 0) {
		nrem = remotes_list(rfiles, rnames, MAX_REMOTES);
		for (int i = 0; i < nrem; i++) remote_load(&rinfo[i], rfiles[i]);
	}
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	logo(40, 26);
	text(F_TITLE, 98, 24, APP_NAME, C_TEXT, 255);
	{ char rm[48]; snprintf(rm, sizeof rm, "by " APP_AUTHOR "   -   Room: %s", cfg.profile[0] ? cfg.profile : "Main"); text(F_SMALL, 100, 60, rm, C_DIM, 255); }
	if (!plat_storage_ok()) {
		gfx_round(300, 36, 200, 30, 15, C_BAD, 255);
		text_c(F_SMALL, 400, 40, "No SD card!", C_WHITE, 255);
	}
	draw_sleep_chip(410, 36);

	/* small corner buttons: rooms and settings */
	if (ui_tile(2004, 452, 30, 44, 40, IC_TV, "", ST_NORMAL, 0) & UI_CLICK) { go(S_PROFILES); return; }
	if (ui_tile(2003, 500, 30, 44, 40, IC_GEAR, "", ST_NORMAL, 0) & UI_CLICK) { go(S_SETTINGS); return; }

	/* the two big paths */
	int f1 = ui_item(2001, 40, 88, 274, 78), f2 = ui_item(2002, 326, 88, 274, 78);
	for (int k = 0; k < 2; k++) {
		int x = k ? 326 : 40, f = k ? f2 : f1;
		gfx_shadow(x, 88, 274, 78, 16);
		gfx_round_grad(x, 88, 274, 78, 16, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, (f & UI_FOCUS) ? C_FOCUS_BOT : C_CARD, 255);
		gfx_round_line(x, 88, 274, 78, 16, 1, C_LINE, 255);
		icon(k ? IC_REMOTE : IC_PALETTE, x + 36, 127, 30, k ? C_ACCENT : C_WHITE);
		text(F_BTN, x + 62, 100, k ? "Remote Setup" : "Calibration Suite", C_TEXT, 255);
		text(F_SMALL, x + 62, 132, k ? "Find, Smart match, mapping" : "Picture & sound tests", C_DIM, 255);
		if (f & UI_FOCUS) ui_focus_ring(x, 88, 274, 78, 16);
	}
	if (f1 & UI_CLICK) { cal_start(); return; }
	if (f2 & UI_CLICK) { go(S_SETUP); return; }

	/* your TVs */
	text(F_BODYB, 40, 178, "Your TVs", C_TEXT, 255);
	int cw = 274, ch = 72, g = 12;
	int total = nrem + 1, per = 4;
	int focus = ui_get_focus();
	if (focus >= 1000 && focus < 1000 + total) {
		int row = (focus - 1000) / 2;
		if (row < home_scroll) home_scroll = row;
		if (row > home_scroll + 1) home_scroll = row - 1;
	}
	if (home_scroll * 2 >= total) home_scroll = 0;
	for (int i = home_scroll * 2; i < total && i < home_scroll * 2 + per; i++) {
		int k = i - home_scroll * 2;
		int x = 40 + (k % 2) * (cw + g), y = 204 + (k / 2) * (ch + 10);
		int f = ui_item(1000 + i, x, y, cw, ch);
		if (i < nrem) {
			int last = !strcmp(rfiles[i], cfg.last);
			gfx_round_grad(x, y, cw, ch, 16, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, (f & UI_FOCUS) ? C_FOCUS_BOT : C_CARD, 255);
			gfx_round_line(x, y, cw, ch, 16, 1, C_LINE, 255);
			gfx_round(x + 12, y + 14, 44, 44, 12, last ? C_ACCENT : C_ACCENT2, 255);
			icon(IC_TV, x + 34, y + 36, 26, C_WHITE);
			text_fit(F_BODYB, x + 68, y + 12, cw - 80, rinfo[i].name, C_TEXT, 255);
			char sub[40];
			const char *pl = rinfo[i].nbtn == 1 ? "" : "s";
			if (last) snprintf(sub, sizeof sub, "%d button%s  -  last used", rinfo[i].nbtn, pl);
			else snprintf(sub, sizeof sub, "%d button%s", rinfo[i].nbtn, pl);
			text_fit(F_SMALL, x + 68, y + 40, cw - 80, sub, last ? C_GOOD : C_DIM, 255);
			if (f & UI_FOCUS) ui_focus_ring(x, y, cw, ch, 16);
			if (f & UI_CLICK) { open_remote_file(rfiles[i]); return; }
		} else {
			gfx_round_line(x, y, cw, ch, 16, 2, (f & UI_FOCUS) ? C_ACCENT : C_LINE, 255);
			gfx_circle(x + 34, y + ch / 2, 18, C_ACCENT, 255);
			icon(IC_PLUS, x + 34, y + ch / 2, 22, C_WHITE);
			text(F_BODYB, x + 64, y + ch / 2 - 12, "New TV remote", C_TEXT, 255);
			if (f & UI_FOCUS) ui_focus_ring(x, y, cw, ch, 16);
			if (f & UI_CLICK) { wizard_start(); return; }
		}
	}
	if (total > per) {
		char pg[32]; snprintf(pg, sizeof pg, "%d-%d of %d  (down for more)", home_scroll * 2 + 1,
		                      home_scroll * 2 + per < total ? home_scroll * 2 + per : total, total);
		text_r(F_SMALL, SCR_W - 40, 180, pg, C_DIM, 255);
	}

	ui_footer_begin();
	ui_footer_hint("A", "Open");
	ui_footer_hint("HOME", "Quit");
	static const char *help[] = {
		"Calibration Suite: test pictures and sound tests.",
		"Remote Setup: find codes, Smart match, map buttons.",
		"Your TVs: pick one to use its remote.",
		"Top right: Rooms (sets of TVs) and Settings.",
		"HOME on any remote brings you back here.",
	};
	ui_help("Home", help, 5);
	if (!blocked && (in->down & BTN_HOME)) screen = S_QUIT;
}

/* ------------------------------------------------------------------ */
/* new remote                                                           */
/* ------------------------------------------------------------------ */

static int found_loaded = 0, found_ok = 0;
static u8 found_p, found_c;
static u16 found_a;

static void load_found(void)
{
	found_loaded = 1;
	char mine[96]; snprintf(mine, sizeof mine, "%s/found.txt", data_dir);
	const char *paths[] = { mine, "sd:/apps/WiiRScan/found.txt", "sd:/found.txt" };
	for (int k = 0; k < 3; k++) {
		FILE *f = fopen(paths[k], "r");
		if (!f) continue;
		char line[128], pn[32]; unsigned a, c;
		while (fgets(line, sizeof line, f)) {
			if (sscanf(line, "%31s address 0x%x command 0x%x", pn, &a, &c) == 3) {
				int p = proto_from_name(pn);
				if (!strncmp(pn, "RC5", 3)) p = P_RC5;
				if (p >= 0) { found_ok = 1; found_p = p; found_a = a; found_c = c; }
			}
		}
		fclose(f);
		if (found_ok) return;
	}
}

int value_row(int id, int y, const char *label, const char *val, int *delta)
{
	*delta = 0;
	text(F_BODYB, 60, y + 9, label, C_TEXT, 255);
	int f1 = ui_tile(id, 330, y, 56, 40, -1, "-", ST_KEY, 0);
	gfx_round(394, y, 124, 40, 12, C_INSET, 255);
	if (text_w(F_BTN, val) > 116) text_c(F_BODYB, 456, y + 10, val, C_TEXT, 255);
	else text_c(F_BTN, 456, y + 7, val, C_TEXT, 255);
	int f2 = ui_tile(id + 1, 526, y, 56, 40, -1, "+", ST_KEY, 0);
	static u64 rep;
	u64 now = plat_ms();
	if (f1 & UI_CLICK) { *delta = -1; rep = now + 400; }
	else if (f2 & UI_CLICK) { *delta = 1; rep = now + 400; }
	else if ((f1 & UI_HELD) && now > rep) { *delta = -1; rep = now + 60; }
	else if ((f2 & UI_HELD) && now > rep) { *delta = 1; rep = now + 60; }
	return *delta;
}

void scr_newremote(void)
{
	if (!found_loaded) {
		load_found();
		if (found_ok) { newr_proto = found_p; newr_addr = found_a; newr_cmd = found_c; newr_has_cmd = 1; }
	}
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_TV, 58, 50, 32, C_ACCENT);
	text(F_TITLE, 86, 30, "New remote", C_TEXT, 255);
	int y = 96;
	if (found_ok) {
		gfx_round(40, y, SCR_W - 80, 44, 14, C_SOFT_GOOD, 255);
		icon(IC_CHECK, 64, y + 22, 22, C_GOOD);
		char m[80]; snprintf(m, sizeof m, "Scanner result filled in:  %s %02X / %02X", PROTO_NAME[found_p], found_a, found_c);
		text(F_BODYB, 86, y + 10, m, C_TEXT, 255);
	}
	y += 58;
	char v[16]; int d;
	snprintf(v, sizeof v, "%s", PROTO_NAME[newr_proto]);
	if (value_row(3000, y, "Type", v, &d)) { newr_proto = (newr_proto + P_COUNT + d) % P_COUNT; newr_addr = addr_wrap(newr_proto, newr_addr); }
	y += 56;
	snprintf(v, sizeof v, newr_addr > 255 ? "0x%04X" : "0x%02X", newr_addr);
	if (value_row(3002, y, "Address", v, &d)) newr_addr = addr_wrap(newr_proto, newr_addr + d);
	y += 56;
	snprintf(v, sizeof v, newr_has_cmd ? "0x%02X" : "none", newr_cmd);
	if (value_row(3004, y, "First button", v, &d)) { if (!newr_has_cmd) newr_has_cmd = 1; else newr_cmd += d; }
	y += 64;
	if (ui_ibutton(3011, 60, y, 200, 50, IC_CLOSE, "Cancel", ST_NORMAL) & UI_CLICK) { go_back(); return; }
	if (ui_ibutton(3010, 330, y, 252, 50, IC_RIGHT, "Next: name it", ST_NORMAL) & UI_CLICK) { kb_open(K_NEW_REMOTE, "TV", "Name this remote", NAME_LEN - 1); return; }
	ui_footer_begin();
	ui_footer_hint("A", "Change");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Type: the code family. Most TVs use NEC.",
		"Address: the TV's number. The scanner finds it.",
		"First button: the code the scanner found.",
		"Not sure? Leave it as it is and press Next.",
		"You can add more buttons later with Find buttons.",
	};
	ui_help("New remote", help, 5);
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ------------------------------------------------------------------ */
/* settings                                                             */
/* ------------------------------------------------------------------ */

static void settings_extras(int blocked)
{
	char v[24]; int d, y = 96;
	snprintf(v, sizeof v, "%s", cfg.speaker ? "On" : "Off");
	if (value_row(4040, y, "Wii Remote click", v, &d)) { cfg.speaker = !cfg.speaker; plat_speaker(cfg.speaker); config_save(); }
	y += 44;
	snprintf(v, sizeof v, "%s", cfg.tilt ? "Hold 1 + tilt" : "Off");
	if (value_row(4050, y, "Tilt for vol / ch", v, &d)) { cfg.tilt = !cfg.tilt; config_save(); }
	y += 44;
	static const int OFFS[5] = { 0, 15, 30, 60, 90 };
	int oi = 0; for (int k = 0; k < 5; k++) if (OFFS[k] == cfg.auto_off) oi = k;
	if (cfg.auto_off) snprintf(v, sizeof v, "%d min idle", cfg.auto_off); else snprintf(v, sizeof v, "Never");
	if (value_row(4052, y, "Auto TV off", v, &d)) { oi = (oi + 5 + d) % 5; cfg.auto_off = OFFS[oi]; config_save(); }
	y += 44;
	int has = plat_has_cios249();
	if (has) {
		snprintf(v, sizeof v, "%s", cfg.fakemote ? "On" : "Off");
		if (value_row(4042, y, "Switch / PS pads", v, &d)) { cfg.fakemote = !cfg.fakemote; config_save(); ui_toast("Restart the app to apply"); }
	} else {
		text(F_BODYB, 60, y + 9, "Switch / PS pads", C_DIM, 255);
		gfx_round(330, y, 252, 40, 12, C_INSET, 255);
		text_c(F_SMALL, 456, y + 10, "Needs Fakemote (cIOS 249)", C_DIM, 255);
	}
	y += 44;
	snprintf(v, sizeof v, "%s", cfg.safe_hold ? "On" : "Off");
	if (value_row(4046, y, "Safe volume lock", v, &d)) { cfg.safe_hold = !cfg.safe_hold; config_save(); }
	y += 44;
	snprintf(v, sizeof v, "%s", cfg.corner ? "Always" : "Normal");
	if (value_row(4048, y, "Screensaver corners", v, &d)) { cfg.corner = !cfg.corner; config_save(); if (cfg.corner) ui_toast("It will hit the corner. Every. Time."); }
	y += 44;
	text(F_BODYB, 60, y + 9, "Just for fun", C_TEXT, 255);
	if (ui_ibutton(4044, 330, y, 118, 40, IC_VOLUP, "Beep", ST_NORMAL) & UI_CLICK) plat_beep(0);
	if (ui_ibutton(4045, 456, y, 126, 40, IC_STAR, "Casino", ST_NORMAL) & UI_CLICK) { casino_start(); return; }
	y += 52;
	ui_footer_begin();
	ui_footer_hint("A", "Change");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Wii Remote click: a tiny click from the remote when a code is sent.",
		"Switch / PS pads: uses Fakemote inside cIOS 249.",
		"Safe volume lock: no fast repeats while finding codes (no surprise max volume).",
		"Only shows On/Off if you have it. Restart the app after changing.",
		"Beep: checks the TV sound for the Audio sync test.",
		"Casino: a little game of blackjack. Chips are just for fun.",
	};
	ui_help("Extras", help, 6);
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

void scr_settings(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_GEAR, 58, 50, 32, C_ACCENT);
	text(F_TITLE, 86, 30, "Settings", C_TEXT, 255);
	char v[24]; int d, y = 84;
	static int spage;
	if (ui_button(4030, 300, 30, 110, 40, "Main", spage == 0 ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) spage = 0;
	if (ui_button(4031, 416, 30, 110, 40, "Extras", spage == 1 ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) spage = 1;
	if (spage == 1) { settings_extras(blocked); return; }

	snprintf(v, sizeof v, "%s", cfg.pointer ? "On" : "Off");
	if (value_row(4000, y, "Pointer", v, &d)) { cfg.pointer = !cfg.pointer; plat_sensorbar_idle(cfg.pointer); config_save(); }
	y += 40;
	snprintf(v, sizeof v, "%s", cfg.rumble ? "On" : "Off");
	if (value_row(4002, y, "Rumble", v, &d)) { cfg.rumble = !cfg.rumble; config_save(); if (cfg.rumble) plat_rumble(80); }
	y += 40;
	snprintf(v, sizeof v, "%s", cfg.open_last ? "On" : "Off");
	if (value_row(4004, y, "Boot to last TV", v, &d)) { cfg.open_last = !cfg.open_last; config_save(); }
	y += 40;
	static const int dims[] = { 0, 60, 120, 300, 600, 900 };
	int di = 0; for (int k = 0; k < 6; k++) if (dims[k] == cfg.dim_secs) di = k;
	if (cfg.dim_secs) snprintf(v, sizeof v, "%d s", cfg.dim_secs); else snprintf(v, sizeof v, "Never");
	if (value_row(4006, y, "Dim screen after", v, &d)) { di = (di + 6 + d) % 6; cfg.dim_secs = dims[di]; config_save(); }
	y += 40;
	snprintf(v, sizeof v, "%d ms", cfg.seq_gap);
	if (value_row(4008, y, "Gap between codes", v, &d)) { cfg.seq_gap += d * 50; if (cfg.seq_gap < 100) cfg.seq_gap = 100; if (cfg.seq_gap > 2000) cfg.seq_gap = 2000; config_save(); }
	y += 40;
	static const char *SH[3] = { "Off", "Hold B", "Tap B" };
	snprintf(v, sizeof v, "%s", SH[cfg.shift_mode]);
	if (value_row(4012, y, "Shift button", v, &d)) { cfg.shift_mode = (cfg.shift_mode + 3 + d) % 3; config_save(); }
	y += 40;
	snprintf(v, sizeof v, "%d ms", cfg.repeat_ms);
	if (value_row(4010, y, "Hold repeat", v, &d)) { cfg.repeat_ms += d * 10; if (cfg.repeat_ms < 60) cfg.repeat_ms = 60; if (cfg.repeat_ms > 500) cfg.repeat_ms = 500; config_save(); }
	y += 46;
	if (ui_ibutton(4022, 40, y, 162, 44, IC_GAMEPAD, "Controllers", ST_NORMAL) & UI_CLICK) { go(S_PADS); return; }
	if (ui_ibutton(4023, 208, y, 124, 44, cfg.light ? IC_MOON : IC_STAR, cfg.light ? "Dark" : "Light", ST_NORMAL) & UI_CLICK) { cfg.light = !cfg.light; theme_set(cfg.light); config_save(); }
	if (ui_ibutton(4020, 338, y, 128, 44, IC_SAVE, "Backup", ST_NORMAL) & UI_CLICK) {
		int n = backup_make();
		char m[48]; if (n) snprintf(m, sizeof m, "Backup %d saved", n); else snprintf(m, sizeof m, "Backup failed");
		ui_toast(m);
	}
	if (ui_ibutton(4021, 472, y, 128, 44, IC_BACK, "Restore", ST_NORMAL) & UI_CLICK) {
		int n = backup_restore();
		char m[48]; if (n) snprintf(m, sizeof m, "Backup %d restored", n); else snprintf(m, sizeof m, "No backup yet");
		ui_toast(m);
		if (n && have_cur && cur.file[0]) remote_load(&cur, cur.file);
		home_refresh();
	}

	ui_footer_begin();
	ui_footer_hint("A", "Change");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Pointer: point at the screen. Needs the bar facing you.",
		"Sending needs the bar to reach the TV's IR window too.",
		"Can't do both? Turn Pointer off and use the D-pad.",
		"Rumble: the Wii Remote buzzes when a code is sent.",
		"Shift button: B gives the Wii Remote a 2nd set of TV buttons (Controller map > Shift).",
		"Dim: darkens the screen when you're not using it.",
		"Gap: wait between codes in macros and selections.",
		"Backup: copies all remotes to sd:/wiirremote/backup.",
		"Controllers: choose which controller does what.",
		"Dark / Light: switches the look of the app.",
		APP_NAME " " APP_VER " by " APP_AUTHOR ". Based on WiiRremote by Abdelali221.",
	};
	ui_help("Settings", help, 11);
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ------------------------------------------------------------------ */
/* sleep timer                                                          */
/* ------------------------------------------------------------------ */

void scr_sleep(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_MOON, 58, 50, 32, C_ACCENT);
	text(F_TITLE, 86, 30, "Sleep timer", C_TEXT, 255);
	int pw = remote_find_btn(&cur, "POWER");
	if (pw < 0) {
		gfx_round(40, 100, SCR_W - 80, 60, 14, C_SOFT_BAD, 255);
		text(F_BODYB, 60, 108, "This remote has no POWER button yet.", C_TEXT, 255);
		text(F_SMALL, 60, 132, "Add one with Find buttons, then come back.", C_TEXT, 255);
	} else {
		static const int mins[] = { 15, 30, 45, 60, 90, 120 };
		for (int i = 0; i < 6; i++) {
			char l[16]; snprintf(l, sizeof l, "%d min", mins[i]);
			if (ui_tile(4100 + i, 40 + (i % 3) * 190, 100 + (i / 3) * 104, 178, 92, IC_MOON, l, ST_NORMAL, 0) & UI_CLICK) {
				sleep_at = plat_ms() + (u64)mins[i] * 60000;
				sleep_btn = pw;
				ui_toast("Sleep timer on");
				go(S_REMOTE);
				return;
			}
		}
		if (sleep_at) {
			draw_sleep_chip(40, 318);
			if (ui_ibutton(4110, 330, 312, 252, 46, IC_CLOSE, "Turn timer off", ST_NORMAL) & UI_CLICK) { sleep_at = 0; ui_toast("Timer off"); }
		}
	}
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Pick how long until the TV turns off.",
		"It sends your POWER button when time is up.",
		"Keep the Wii on and this app open.",
		"The time left shows at the top of the remote.",
	};
	ui_help("Sleep timer", help, 4);
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ------------------------------------------------------------------ */
/* notes (e.g. colour settings)                                         */
/* ------------------------------------------------------------------ */

void scr_notes(void)
{
	static int popup, which;
	int blocked = help_is_open() || popup;
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_NOTE, 58, 50, 30, C_ACCENT);
	text(F_TITLE, 86, 30, "Notes", C_TEXT, 255);
	text(F_SMALL, 88, 66, cur.name, C_DIM, 255);
	for (int i = 0; i < cur.nnotes; i++) {
		int x = 40 + (i % 2) * 284, y = 96 + (i / 2) * 42;
		int f = ui_item(4200 + i, x, y, 272, 36);
		gfx_round(x, y, 272, 36, 10, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, 255);
		gfx_round_line(x, y, 272, 36, 10, 1, C_LINE, 255);
		text_fit(F_BODY, x + 12, y + 6, 250, cur.notes[i], C_TEXT, 255);
		if (f & UI_FOCUS) ui_focus_ring(x, y, 272, 36, 10);
		if (f & UI_CLICK) { popup = 1; which = i; }
	}
	if (!cur.nnotes) {
		gfx_round(40, 100, SCR_W - 80, 80, 16, C_CARD_HI, 255);
		text(F_BODYB, 60, 112, "No notes yet.", C_TEXT, 255);
		text(F_SMALL, 60, 140, "Write down settings before you change them, e.g. G DRIVE 32", C_DIM, 255);
	}
	if (cur.nnotes < MAX_NOTES && ui_ibutton(4250, 40, 376, 240, 46, IC_PLUS, "Add a note", ST_NORMAL) & UI_CLICK) {
		kb_open(K_NOTE_ADD, "", "New note", NOTE_LEN - 1); return;
	}
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Notes are saved with this remote.",
		"Write the old value before changing a setting.",
		"Example: G DRIVE 32 -> 28",
		"Choose a note to change or delete it.",
	};
	if (!popup) ui_help("Notes", help, 4);
	static const char *items[] = { "Change note", "Delete note", "Cancel" };
	static const int icons[] = { IC_PENCIL, IC_TRASH, IC_CLOSE };
	int c = menu_popup(&popup, items, icons, 3, "Note");
	if (c == 0) { edit_note = which; kb_open(K_NOTE_EDIT, cur.notes[which], "Change note", NOTE_LEN - 1); return; }
	if (c == 1) {
		memmove(cur.notes[which], cur.notes[which + 1], (cur.nnotes - which - 1) * NOTE_LEN);
		cur.nnotes--; save_cur(); ui_toast("Deleted");
	}
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ------------------------------------------------------------------ */
/* main loop glue                                                       */
/* ------------------------------------------------------------------ */

static u64 last_input;

extern u32 *help_seen_bits;
extern void (*help_seen_save)(void);

void app_init(void)
{
	storage_init();
	config_load();
	profile_use(cfg.profile);
	help_seen_bits = &cfg.help_seen;
	help_seen_save = config_save;
	theme_set(cfg.light);
	plat_speaker(cfg.speaker);
	if (plat_fakemote_failed() && cfg.fakemote) { cfg.fakemote = 0; config_save(); ui_toast("Fakemote didn't start, so it's off"); }
	plat_sensorbar_idle(cfg.pointer);
	screen = S_HOME;
	if (cfg.open_last && cfg.last[0] && remote_load(&cur, cfg.last)) {
		have_cur = 1;
		screen = S_REMOTE;
	}
	last_input = plat_ms();
}

int scan_running(void);

/* Split the controllers into the "menu" input and the "TV remote" input,
   following the job set for each one in Settings > Controllers. */
/* A Classic Controller or GameCube controller (both use Classic bits) */
static void add_classic(u32 d, u32 h, int role, int wiimote_is_menu)
{
	if (role == ROLE_MENU) {
		menu_in.has_classic = 1;
		if (d & CC_HOME) menu_in.down |= BTN_HOME;
		if (h & CC_HOME) menu_in.held |= BTN_HOME;
		if (wiimote_is_menu) {
			/* Wii Remote drives the menu; these buttons act as mapped shortcuts */
			menu_in.down |= d & ~CC_HOME; menu_in.held |= h & ~CC_HOME;
		} else {
			/* this controller is the menu device: D-pad moves, a = A, b = B */
			static const u32 from[8] = { CC_UP, CC_DOWN, CC_LEFT, CC_RIGHT, CC_A, CC_B, CC_PLUS, CC_MINUS };
			static const u32 to[8]   = { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_PLUS, BTN_MINUS };
			for (int k = 0; k < 8; k++) {
				if (d & from[k]) menu_in.down |= to[k];
				if (h & from[k]) menu_in.held |= to[k];
			}
			if (d & CC_X) menu_in.down |= BTN_1;
			if (d & CC_Y) menu_in.down |= BTN_2;
			if (h & CC_X) menu_in.held |= BTN_1;
			if (h & CC_Y) menu_in.held |= BTN_2;
		}
	} else if (role == ROLE_TV) {
		tv_in.down |= d & ~CC_HOME; tv_in.held |= h & ~CC_HOME;
	}
}

int ui_battery[MAX_PADS];

static void build_inputs(Pad *pads)
{
	static Input prev_menu, prev_tv;
	memset(&menu_in, 0, sizeof menu_in);
	memset(&tv_in, 0, sizeof tv_in);
	int any_menu = 0;
	for (int c = 0; c < MAX_PADS; c++) {
		if (pads[c].connected && cfg.role_wm[c] == ROLE_MENU) any_menu = 1;
		if (pads[c].connected && pads[c].has_classic && cfg.role_cc[c] == ROLE_MENU) any_menu = 1;
		if (pads[c].gc_connected && cfg.role_gc[c] == ROLE_MENU) any_menu = 1;
		if (c == 0 && pads[c].bb_connected && cfg.role_bb == ROLE_MENU) any_menu = 1;
		ui_battery[c] = pads[c].connected ? pads[c].battery : -2;
	}
	for (int c = 0; c < MAX_PADS; c++) {
		Pad *p = &pads[c];
		int rw = any_menu ? cfg.role_wm[c] : ROLE_MENU;
		int rc = any_menu ? cfg.role_cc[c] : ROLE_MENU;
		int rg = any_menu ? cfg.role_gc[c] : ROLE_MENU;
		if (c == 0 && p->bb_connected) add_classic(p->bb_down, p->bb_held, any_menu ? cfg.role_bb : ROLE_MENU, 0);
		if (p->gc_connected) {
			add_classic(p->gc_down, p->gc_held, rg, 0);
			if (p->gc_down && rg != ROLE_OFF) plat_rumble_chan = c;
		}
		if (!p->connected) continue;
		if (rw == ROLE_MENU) {
			menu_in.down |= p->wm_down; menu_in.held |= p->wm_held;
			if (p->ptr_valid && !menu_in.ptr_valid) { menu_in.ptr_valid = 1; menu_in.px = p->px; menu_in.py = p->py; }
			if (!menu_in.tilt_ok) { menu_in.tilt_ok = 1; menu_in.roll = p->roll; menu_in.pitch = p->pitch; }
			if (p->shake > menu_in.shake) menu_in.shake = p->shake;
			menu_in.connected = 1;
		} else if (rw == ROLE_TV) {
			tv_in.down |= p->wm_down & ~BTN_HOME; tv_in.held |= p->wm_held & ~BTN_HOME;
		}
		if (p->has_classic) add_classic(p->cc_down, p->cc_held, rc, rw == ROLE_MENU);
		if ((p->wm_down | p->cc_down) && (rw != ROLE_OFF || rc != ROLE_OFF)) plat_rumble_chan = c;
	}
	/* USB mouse: a pointer when the Wii Remote isn't pointing */
	if (usb.mouse_connected && cfg.role_mouse == ROLE_MENU) {
		if (!menu_in.ptr_valid && plat_ms() - usb.mouse_moved_ms < 8000) { menu_in.ptr_valid = 1; menu_in.px = usb.mx; menu_in.py = usb.my; }
		if (usb.mouse_down & 1) menu_in.down |= BTN_A;
		if (usb.mouse_held & 1) menu_in.held |= BTN_A;
		if (usb.mouse_down & 2) menu_in.down |= BTN_B;
		if (usb.mouse_down & 4) menu_in.down |= BTN_HOME;
		if (usb.wheel < 0) menu_in.down |= BTN_DOWN;
		if (usb.wheel > 0) menu_in.down |= BTN_UP;
	}
	menu_in.up = prev_menu.held & ~menu_in.held;
	tv_in.up = prev_tv.held & ~tv_in.held;
	prev_menu = menu_in;
	prev_tv = tv_in;

	/* safety: hold HOME on ANY controller for 3 seconds = every controller back to Menu */
	static u64 home_since;
	int home = 0;
	for (int c = 0; c < MAX_PADS; c++)
		if ((pads[c].connected && ((pads[c].wm_held & BTN_HOME) || (pads[c].cc_held & CC_HOME))) ||
		    (pads[c].gc_connected && (pads[c].gc_held & CC_HOME))) home = 1;
	if (home) {
		if (!home_since) home_since = plat_ms();
		else if (plat_ms() - home_since > 3000) {
			for (int c = 0; c < MAX_PADS; c++) { cfg.role_wm[c] = ROLE_MENU; cfg.role_cc[c] = ROLE_MENU; cfg.role_gc[c] = ROLE_MENU; }
			config_save();
			ui_toast("All controllers set to Menu");
			home_since = 0;
		}
	} else home_since = 0;
}

/* USB keyboard: arrows / Enter / Esc drive the menu, other keys are mapped TV buttons,
   and on the keyboard screen it types. */
int kb_capture = -1;      /* keymap screen waiting for a key: last key pressed, -1 = not waiting */
int kb_waiting;
void kb_type_char(char ch);

static void keyboard_frame(void)
{
	if (cfg.role_kb == ROLE_OFF) return;
	for (int i = 0; i < usb.nkeys; i++) {
		u8 k = usb.keys[i];
		u16 sym = usb.syms[i];
		if (kb_waiting) { kb_capture = k; kb_waiting = 0; continue; }
		if (screen == S_KEYBOARD) {
			if (k == 42) menu_in.down |= BTN_B;           /* backspace = delete */
			else if (k == 40) menu_in.down |= BTN_PLUS;   /* enter = done */
			else if (k == 41) menu_in.down |= BTN_HOME;   /* esc = cancel */
			else if (sym >= 32 && sym < 127) kb_type_char((char)sym);
			continue;
		}
		int mapped = -1;
		if (have_cur) for (int m = 0; m < cur.nkeys; m++) if (cur.key_code[m] == k && cur.key_btn[m] >= 0) mapped = cur.key_btn[m];
		if (cfg.role_kb == ROLE_MENU) {
			u32 nav = k == 82 ? BTN_UP : k == 81 ? BTN_DOWN : k == 80 ? BTN_LEFT : k == 79 ? BTN_RIGHT :
			          k == 40 ? BTN_A : k == 42 ? BTN_B : k == 41 ? BTN_HOME : 0;
			if (nav && mapped < 0) { menu_in.down |= nav; continue; }
		}
		if (mapped >= 0) { press_btn(mapped, 0); flash_ms = plat_ms(); }
	}
}

extern u32 *app_native_buf;

int app_frame(Pad *pads)
{
	app_native_buf = NULL;
	memcpy(pads_now, pads, sizeof pads_now);
	build_inputs(pads);
	keyboard_frame();
	Input *i = &menu_in;
	u64 now = plat_ms();
	{ static u64 boot_ms; static int boot_done; if (!boot_ms) boot_ms = now; if (!boot_done && now - boot_ms > 4000) { boot_done = 1; plat_boot_ok(); } }
	static int last_px, last_py;
	int moved = i->ptr_valid && (abs(i->px - last_px) > 3 || abs(i->py - last_py) > 3);
	last_px = i->px; last_py = i->py;
	if (i->down || i->held || moved || tv_in.down) {
		if (cfg.dim_secs && now - last_input > (u64)cfg.dim_secs * 1000 && i->down) i->down = 0; /* wake only */
		last_input = now;
	}

	int nav = !(screen == S_DIRECT || cal_in_pictures() || calui_in_picture() || screen == S_SCAN);
	ui_begin(i, nav || help_is_open());
	if (undo_available() && (i->down & BTN_MINUS) && !help_is_open() &&
	    (screen == S_REMOTE || screen == S_VREMOTE || screen == S_MAP || screen == S_HOME || screen == S_NOTES)) {
		int was_deleted = !have_cur;
		undo_do();
		if (was_deleted) { have_cur = 1; go(S_REMOTE); }
		i->down &= ~BTN_MINUS;
	}
	/* never leave a tone playing: stop it the moment you leave the test pictures or the casino */
	{ static int was_pics, was_casino;
	  if (was_pics && !cal_in_pictures() && !calui_in_picture()) pat_leave();
	  if (was_casino && screen != S_CASINO) casino_leave();
	  was_pics = cal_in_pictures() || calui_in_picture(); was_casino = screen == S_CASINO; }
	switch (screen) {
		case S_HOME: scr_home(); break;
		case S_REMOTE: case S_VREMOTE: if (have_cur) scr_remote(); else go(S_HOME); break;
		case S_FINDER: scr_finder(); break;
		case S_NAME: scr_name(); break;
		case S_KEYBOARD: scr_keyboard(); break;
		case S_MAP: scr_map(); break;
		case S_DIRECT: scr_direct(); break;
		case S_CAL: scr_cal(); break;
		case S_SCAN: scr_scan(); break;
		case S_SETTINGS: scr_settings(); break;
		case S_CODE: scr_code(); break;
		case S_NEWREMOTE: scr_newremote(); break;
		case S_NOTES: scr_notes(); break;
		case S_SLEEP: scr_sleep(); break;
		case S_PADS: scr_pads(); break;
		case S_MACRO: scr_macro(); break;
		case S_PROFILES: scr_profiles(); break;
		case S_VIDEO: scr_video(); break;
		case S_PRESETS: scr_presets(); break;
		case S_IMPORT: scr_import(); break;
		case S_KEYMAP: scr_keymap(); break;
		case S_WIZARD: scr_wizard(); break;
		case S_SMART: scr_smart(); break;
		case S_CONSOLES: scr_consoles(); break;
		case S_SETUP: scr_setup(); break;
		case S_INPUTMAC: scr_inputmac(); break;
		case S_HEALTH: scr_health(); break;
		case S_SVALS: scr_svals(); break;
		case S_CALFLOW: scr_calflow(); break;
		case S_CASINO: scr_casino(); break;
	}
	ui_end();
	/* controllers set to "TV remote" work on every screen */
	handle_mapped(&tv_in, 0xFFFF);
	send_update();
	draw_undo_chip();
	ui_draw_toast();
	if (ui_pointer_active() && screen != S_DIRECT && screen != S_CAL) gfx_cursor(in->px, in->py);

	auto_off_check(now - last_input);

	/* screensaver: dim 80% and bounce the logo, to protect CRTs from burn-in
	   (never during calibration pictures or a running scan) */
	if (cfg.dim_secs && now - last_input > (u64)cfg.dim_secs * 1000 && screen != S_CAL && !scan_running()) {
		gfx_rect(0, 0, SCR_W, SCR_H, RGB(0, 0, 0), 205);
		static int bx = 100, by = 100, vx = 2, vy = 1, corners, hue;
		static u32 t;
		static u64 flash_until;
		int w = 250, h = 60;
		if (cfg.corner) {
			/* the secret setting: a path that lands in a corner every single time */
			const int T = 300;   /* frames from corner to corner */
			t++;
			int ph = t % (2 * T), f = ph < T ? ph : 2 * T - ph;
			int gx = (t / (2 * T)) & 1;   /* swap diagonals each lap so it visits all four corners */
			bx = (SCR_W - w) * f / T; by = (SCR_H - h) * f / T;
			if (gx) bx = (SCR_W - w) - bx;
			if (ph == 0 || ph == T) { corners++; hue++; flash_until = now + 600; }
		} else {
			bx += vx; by += vy;
			if (bx < 0 || bx + w > SCR_W) { vx = -vx; bx += vx * 2; }
			if (by < 0 || by + h > SCR_H) { vy = -vy; by += vy * 2; }
		}
		static const u32 HUES[4][2] = { { RGB(84, 214, 255), RGB(120, 110, 255) }, { RGB(255, 120, 150), RGB(255, 190, 80) },
		                               { RGB(90, 230, 140), RGB(40, 170, 230) }, { RGB(240, 110, 255), RGB(110, 90, 255) } };
		gfx_round_grad(bx, by, 50, 50, 14, HUES[hue & 3][0], HUES[hue & 3][1], 160);
		if (cfg.corner && now < flash_until) {
			char cc[32]; snprintf(cc, sizeof cc, "CORNER!  x%d", corners);
			text_c(F_TITLE, SCR_W / 2, SCR_H / 2 - 20, cc, HUES[hue & 3][0], 255);
		}
		for (int k = 0; k < 3; k++) gfx_ring(bx + 25, by + 25, 8 + k * 7, 2, RGB(255, 255, 255), 150 - k * 40);
		text(F_BODYB, bx + 60, by + 4, APP_NAME, RGB(170, 176, 190), 255);
		text(F_SMALL, bx + 60, by + 28, "Press any button", RGB(110, 116, 130), 255);
	}
	return screen != S_QUIT;
}
