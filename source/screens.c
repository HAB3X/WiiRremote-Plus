/*
   screens.c - remote, button finder, code editor, controller map,
   controller mode, calibration, code scanner and controller jobs.
   WiiRremote Plus by Habex. Based on WiiRremote by Abdelali221. GPL-3.0.
*/
#include "app.h"
#include "patterns.h"
#include "presets.h"
#include "calkb.h"
#include <dirent.h>
#include <sys/stat.h>
#include <strings.h>

Pad pads_now[MAX_PADS];

static void title(int ic, const char *t, const char *sub)
{
	icon(ic, 58, 50, 30, C_ACCENT);
	text_fit(F_TITLE, 86, 28, 300, t, C_TEXT, 255);
	if (sub) text_fit(F_SMALL, 88, 64, 360, sub, C_DIM, 255);
}

/* ================================================================== */
/* remote                                                             */
/* ================================================================== */

static int sel_order[MAX_BTNS], sel_n, rpage, edit_mode;
static int more_open, more_add, more_manage, key_open, del_open, key_idx, switch_open;
static int qa_open, qa_slot, qa_pick, qa_page, show_old, show_dead, ex_page;   /* on-screen remote popups */
static int key_more;   /* edit menu: second page */
static char sw_files[MAX_REMOTES][40], sw_names[MAX_REMOTES][NAME_LEN];
static int sw_n;

void remote_screen_reset(void)
{
	memset(sel_order, 0, sizeof sel_order);
	sel_n = 0; rpage = 0; edit_mode = 0;
	more_open = more_add = more_manage = key_open = del_open = switch_open = 0;
	qa_open = qa_pick = key_more = 0;
}

static void toggle_sel(int i)
{
	if (sel_order[i]) {
		int o = sel_order[i];
		sel_order[i] = 0;
		for (int k = 0; k < cur.nbtn; k++) if (sel_order[k] > o) sel_order[k]--;
		sel_n--;
	} else {
		sel_order[i] = ++sel_n;
	}
}

static int selection_codes(Code *out, int max)
{
	int n = 0;
	for (int o = 1; o <= sel_n; o++)
		for (int i = 0; i < cur.nbtn; i++)
			if (sel_order[i] == o) n += expand_btn(i, out + n, max - n);
	return n;
}

static void clear_sel(void) { memset(sel_order, 0, sizeof sel_order); sel_n = 0; }

/* ---- the on-screen remote: buttons in the spots a real TV remote has them ---- */
enum { SL_NONE, SL_POWER, SL_INPUT, SL_MUTE, SL_MENU, SL_UP, SL_BACK, SL_LEFT, SL_OK, SL_RIGHT, SL_INFO, SL_DOWN, SL_SERVICE,
       SL_VOLUP, SL_PICTURE, SL_CHUP, SL_VOLDN, SL_ASPECT, SL_CHDN, SL_COLOUR, SL_HALFMUTE, SL_SLEEP, SL_COUNT };
static const char *SL_NAME[SL_COUNT] = { "", "POWER", "INPUT", "MUTE", "MENU", "UP", "BACK", "LEFT", "OK", "RIGHT", "INFO", "DOWN", "SERVICE",
	"VOL+", "PICTURE", "P+", "VOL-", "ASPECT", "P-", "COLOUR", "HALF MUTE", "SLEEP" };
static const char *SL_ALIAS[SL_COUNT] = { "", "POWER|PWR|STANDBY|ON/OFF", "INPUT|TV/AV|AV|SOURCE|TV AV|AV/TV", "MUTE", "MENU|SETUP",
	"UP|CURSOR UP", "BACK|EXIT|RETURN", "LEFT", "OK|ENTER|SELECT", "RIGHT", "INFO|DISPLAY|RECALL|OSD", "DOWN", "SERVICE|FACTORY|SERVICE MENU",
	"VOL+|VOL UP|VOLUME UP", "PICTURE|P.MODE|PMODE|PICTURE MODE", "P+|CH+|CH UP|PR+|PR +", "VOL-|VOL DN|VOL DOWN|VOLUME DOWN", "ASPECT|ZOOM|16:9|WIDE|FORMAT",
	"P-|CH-|CH DN|CH DOWN|PR-|PR -", "COLOUR SYS|COLOR SYS|COLOUR SYSTEM|SYSTEM|COLOUR|COLOR", "HALF MUTE|HALF", "SLEEP|TIMER" };
static const u8 SL_GRID[7][3] = {
	{ SL_POWER, SL_INPUT, SL_MUTE }, { SL_MENU, SL_UP, SL_BACK }, { SL_LEFT, SL_OK, SL_RIGHT }, { SL_INFO, SL_DOWN, SL_SERVICE },
	{ SL_VOLUP, SL_PICTURE, SL_CHUP }, { SL_VOLDN, SL_ASPECT, SL_CHDN }, { SL_COLOUR, SL_HALFMUTE, SL_SLEEP },
};

static int name_matches(const char *name, const char *alias)
{
	char a[96]; snprintf(a, sizeof a, "%s", alias);
	for (char *t = strtok(a, "|"); t; t = strtok(NULL, "|")) if (!strcasecmp(name, t)) return 1;
	return 0;
}
/* which button sits in a spot: one you put there, else one with a matching name */
static int slot_btn(int sl)
{
	for (int i = 0; i < cur.nbtn; i++) if (cur.btn[i].slot == sl && !cur.btn[i].dead) return i;
	for (int i = 0; i < cur.nbtn; i++) if (!cur.btn[i].slot && !cur.btn[i].dead && name_matches(cur.btn[i].name, SL_ALIAS[sl])) return i;
	return -1;
}
/* colour keys, numbers, tape/disc keys: kept out of the way */
static int is_old_button(const char *n)
{
	static const char *OLD = "RED|GREEN|YELLOW|BLUE|0|1|2|3|4|5|6|7|8|9|PLAY|PAUSE|STOP|REW|FF|REC|RECORD|EJECT|REWIND|FORWARD|TEXT|TELETEXT|HOLD|REVEAL|-/--|--";
	return name_matches(n, OLD);
}


static void visual_remote(void)
{
	int used[MAX_BTNS] = { 0 };
	/* the remote body */
	gfx_round(40, 92, 222, 328, 26, C_INSET, 255);
	gfx_round_line(40, 92, 222, 328, 26, 1, C_LINE, 255);
	for (int r = 0; r < 7; r++)
		for (int c = 0; c < 3; c++) {
			int sl = SL_GRID[r][c];
			int x = 50 + c * 68, y = 100 + r * 45, w = 64, h = 40;
			int b = slot_btn(sl);
			if (b >= 0) used[b] = 1;
			int id = 5300 + sl;
			int f;
			if (b >= 0) {
				int ic = icon_for_name(SL_NAME[sl]);
				f = ui_tile(id, x, y, w, h, -1, cur.btn[b].name, ST_KEY, sel_order[b]);
				(void)ic;
				if (edit_mode) { gfx_circle(x + w - 8, y + 8, 8, C_ACCENT, 255); icon(IC_PENCIL, x + w - 8, y + 8, 10, C_WHITE); }
				if (f & UI_CLICK) { if (edit_mode) { key_open = 1; key_more = 0; key_idx = b; } else press_btn(b, BTN_A); }
				if ((f & UI_ALT) && !edit_mode) toggle_sel(b);
			} else {
				/* empty spot: dashed look, pick it to fill it */
				f = ui_item(id, x, y, w, h);
				gfx_round_line(x, y, w, h, 12, 1, (f & UI_FOCUS) ? C_ACCENT : C_LINE, 255);
				char sn[12]; snprintf(sn, sizeof sn, "%s", sl == SL_HALFMUTE ? "HALF" : sl == SL_SERVICE ? "SERVICE" : SL_NAME[sl]);
				text_c(F_SMALL, x + w / 2, y + 11, sn, C_DIM, 160);
				if (f & UI_FOCUS) ui_focus_ring(x, y, w, h, 12);
				if (f & UI_CLICK) { qa_open = 1; qa_slot = sl; ui_set_focus(9000); }
			}
		}
	/* everything else, in a short list */
	int list[MAX_BTNS], n = 0, nold = 0;
	for (int i = 0; i < cur.nbtn; i++) {
		if (used[i]) continue;
		if (cur.btn[i].dead && !show_dead) continue;
		int old = is_old_button(cur.btn[i].name);
		if (old) nold++;
		if (old && !show_old) continue;
		list[n++] = i;
	}
	text(F_SMALL, 274, 94, "Other buttons", C_DIM, 255);
	int per = 6, pages = (n + per - 1) / per; if (pages < 1) pages = 1;
	int fcs = ui_get_focus();
	for (int k = 0; k < n; k++) if (fcs == 5400 + list[k]) ex_page = k / per;
	if (ex_page >= pages) ex_page = pages - 1;
	for (int k = ex_page * per; k < n && k < (ex_page + 1) * per; k++) {
		int i = list[k], y = 116 + (k - ex_page * per) * 42;
		RBtn *b = &cur.btn[i];
		int ic = icon_for_name(b->name); if (ic < 0 && b->nsteps) ic = IC_SEQ;
		int f = ui_ibutton(5400 + i, 274, y, 150, 36, ic, b->name, b->dead ? ST_GHOST : ST_KEY);
		if (sel_order[i]) gfx_round_line(272, y - 2, 154, 40, 12, 2, C_SEL, 255);
		if (f & UI_CLICK) { if (edit_mode || b->dead) { key_open = 1; key_more = 0; key_idx = i; } else press_btn(i, BTN_A); }
		if ((f & UI_ALT) && !edit_mode) toggle_sel(i);
	}
	if (!n) text_fit(F_SMALL, 274, 124, 150, cur.nbtn ? "All in their spots" : "No buttons yet", C_DIM, 255);
	int by = 370;
	if (pages > 1) {
		if (ui_tile(5470, 274, by, 46, 32, IC_UP, "", ST_NORMAL, 0) & UI_CLICK) ex_page = (ex_page + pages - 1) % pages;
		if (ui_tile(5471, 326, by, 46, 32, IC_DOWN, "", ST_NORMAL, 0) & UI_CLICK) ex_page = (ex_page + 1) % pages;
	}
	if (nold) {
		char ol[24]; snprintf(ol, sizeof ol, show_old ? "Hide old" : "Old (%d)", nold);
		if (ui_button(5472, pages > 1 ? 378 : 274, by, pages > 1 ? 46 : 150, 32, pages > 1 ? (show_old ? "-" : "+") : ol, ST_GHOST) & UI_CLICK) show_old = !show_old;
	}
}

void scr_remote(void)
{
	int popup = more_open || more_add || more_manage || key_open || del_open || switch_open || qa_open || qa_pick;
	int blocked = popup || help_is_open();
	if (blocked) ui_suspend(1);
	if (ui_get_focus() < 0) { int f0 = -1; for (int i = 0; i < cur.nbtn && f0 < 0; i++) if (!cur.btn[i].dead) f0 = i; ui_set_focus(f0 >= 0 ? 5000 + f0 : 5104); }
	gfx_bg();

	/* header */
	icon(IC_TV, 58, 50, 30, C_ACCENT);
	text_fit(F_TITLE, 86, 28, 234, cur.name, C_TEXT, 255);
	int nlive = 0; for (int i = 0; i < cur.nbtn; i++) nlive += !cur.btn[i].dead;
	char sub[48]; snprintf(sub, sizeof sub, "%s  %02X   -   %d buttons", PROTO_NAME[cur.proto], cur.addr, nlive);
	text(F_SMALL, 88, 64, sub, C_DIM, 255);
	if (ui_ibutton(5091, 330, 32, 96, 38, edit_mode ? IC_CHECK : IC_PENCIL, edit_mode ? "Done" : "Edit", edit_mode ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) {
		edit_mode = !edit_mode; clear_sel();
	}
	if (ui_ibutton(5090, 432, 32, 110, 38, IC_STAR, "Smart", ST_NORMAL) & UI_CLICK) { smart_start(); return; }   /* fill the gaps any time */
	draw_sleep_chip(250, 60);

	if (screen == S_VREMOTE) visual_remote();   /* More > Remote picture */
	else {
	/* favourites */
	int favs[8], nf = 0;
	for (int i = 0; i < cur.nbtn && nf < 5; i++) if (cur.btn[i].fav) favs[nf++] = i;
	int gt = 100, rows = 4;
	if (nf) {
		gfx_round(40, 96, 382, 52, 14, C_SOFT_WARN, 255);
		icon(IC_STAR, 58, 122, 20, C_SEL);
		for (int k = 0; k < nf; k++) {
			RBtn *b = &cur.btn[favs[k]];
			int x = 74 + k * 69;
			int ic = icon_for_name(b->name); if (ic < 0 && b->nsteps) ic = IC_SEQ;
			int f = ui_tile(5050 + k, x, 100, 64, 44, ic, b->name, ST_KEY, 0);
			if ((f & UI_CLICK) && !edit_mode) press_btn(favs[k], BTN_A);
		}
		gt = 156; rows = 3;
	}

	/* key grid */
	int cols = 4, kw = 88, kh = 50, g = 10;
	int per = cols * rows;
	/* dead codes stay out of sight (More > Show dead codes brings them back) */
	int vis[MAX_BTNS], nv = 0;
	for (int i = 0; i < cur.nbtn; i++) if (!cur.btn[i].dead || show_dead) vis[nv++] = i;
	int pages = (nv + per - 1) / per; if (pages < 1) pages = 1;
	int fcs = ui_get_focus();
	for (int k = 0; k < nv; k++) if (fcs == 5000 + vis[k]) rpage = k / per;
	if (rpage >= pages) rpage = pages - 1;
	if (!nv) {
		gfx_round(40, gt, 382, 180, 18, C_CARD_HI, 255);
		icon(IC_SEARCH, 231, gt + 50, 44, C_DIM);
		text_c(F_BODYB, 231, gt + 86, "No buttons yet", C_TEXT, 255);
		if (ui_ibutton(5080, 111, gt + 122, 240, 46, IC_SEARCH, "Find buttons", ST_NORMAL) & UI_CLICK) { finder_start(); return; }
	}
	for (int v = rpage * per; v < nv && v < (rpage + 1) * per; v++) {
		int i = vis[v], k = v - rpage * per;
		int x = 40 + (k % cols) * (kw + g), y = gt + (k / cols) * (kh + g);
		RBtn *b = &cur.btn[i];
		int ic = icon_for_name(b->name);
		if (ic < 0 && b->nsteps) ic = IC_SEQ;
		int f = ui_tile(5000 + i, x, y, kw, kh, ic, b->name, b->dead ? ST_GHOST : b->nsteps ? ST_NORMAL : ST_KEY, sel_order[i]);
		if (b->fav) icon(IC_STAR, x + 12, y + 12, 14, C_SEL);
		if (edit_mode) { gfx_circle(x + kw - 12, y + 12, 11, C_ACCENT, 255); icon(IC_PENCIL, x + kw - 12, y + 12, 14, C_WHITE); }
		if (f & UI_CLICK) {
			if (edit_mode) { key_open = 1; key_more = 0; key_idx = i; }
			else press_btn(i, BTN_A);
		}
		if ((f & UI_ALT) && !edit_mode) toggle_sel(i);
	}
	if (pages > 1) {
		int py = gt + rows * (kh + g);
		if (ui_ibutton(5070, 40, py, 120, 36, IC_LEFT, "Page", ST_NORMAL) & UI_CLICK) { if (rpage > 0) rpage--; ui_set_focus(5000 + vis[rpage * per]); }
		char pg[16]; snprintf(pg, sizeof pg, "%d / %d", rpage + 1, pages);
		text_c(F_BODYB, 231, py + 6, pg, C_TEXT, 255);
		if (ui_ibutton(5071, 302, py, 120, 36, IC_RIGHT, "Page", ST_NORMAL) & UI_CLICK) { if (rpage < pages - 1) rpage++; ui_set_focus(5000 + vis[rpage * per]); }
	}

	}

	/* sidebar */
	int sx = 436, sw = 164, sy = 100, sh = 42, sg = 8;
	if (edit_mode) {
		if (ui_ibutton(5100, sx, sy, sw, sh, IC_CHECK, "Done", ST_NORMAL) & UI_CLICK) edit_mode = 0;
		sy += sh + sg;
		text(F_SMALL, sx, sy + 4, "Choose a button", C_DIM, 255);
		text(F_SMALL, sx, sy + 24, "to change it.", C_DIM, 255);
	} else if (sel_n) {
		char l[24]; snprintf(l, sizeof l, "Send %d", sel_n);
		if (ui_ibutton(5101, sx, sy, sw, sh, IC_PLAY, l, ST_NORMAL) & UI_CLICK) {
			Code c[64]; int n = selection_codes(c, 64);
			queue_codes(c, n);
			snprintf(last_sent, sizeof last_sent, "%d buttons", sel_n); last_sent_ms = plat_ms();
		}
		sy += sh + sg;
		if (sel_n >= 2) {
			if (ui_ibutton(5102, sx, sy, sw, sh, IC_SEQ, "Macro", ST_NORMAL) & UI_CLICK) {
				pend_macro_n = selection_codes(pend_macro, MAX_STEPS);
				clear_sel();
				macro_edit_start(-2);
				return;
			}
			sy += sh + sg;
		}
		if (ui_ibutton(5103, sx, sy, sw, sh, IC_CLOSE, "Clear", ST_NORMAL) & UI_CLICK) clear_sel();
	} else {
		if (ui_ibutton(5104, sx, sy, sw, sh, IC_SEARCH, "Find", ST_NORMAL) & UI_CLICK) { finder_start(); return; }
		sy += sh + sg;
		if (ui_ibutton(5105, sx, sy, sw, sh, IC_GAMEPAD, "Controller", ST_NORMAL) & UI_CLICK) { direct_start(); return; }
		sy += sh + sg;
		if (ui_ibutton(5106, sx, sy, sw, sh, IC_WRENCH, "Service", ST_NORMAL) & UI_CLICK) { cal_service_start(); return; }
		sy += sh + sg;
		if (ui_ibutton(5107, sx, sy, sw, sh, IC_MORE, "More", ST_NORMAL) & UI_CLICK) more_open = 1;
		sy += sh + sg + 6;
		/* volume rocker */
		int vu = remote_find_btn(&cur, "VOL+"), vd = remote_find_btn(&cur, "VOL-");
		if (vu >= 0 && vd >= 0) {
			gfx_round(sx, sy, sw, 64, 32, C_INSET, 255);
			int f1 = ui_tile(5108, sx + 4, sy + 4, 60, 56, IC_VOLDN, "", ST_KEY, 0);
			int f2 = ui_tile(5109, sx + sw - 64, sy + 4, 60, 56, IC_VOLUP, "", ST_KEY, 0);
			text_c(F_SMALL, sx + sw / 2, sy + 20, "VOL", C_DIM, 255);
			if (f1 & UI_CLICK) press_btn(vd, BTN_A);
			if (f2 & UI_CLICK) press_btn(vu, BTN_A);
		}
	}

	draw_last_sent(SCR_W - 40, 384);

	ui_footer_begin();
	if (edit_mode) { ui_footer_hint("A", "Change"); ui_footer_hint("HOME", "Done"); }
	else { ui_footer_hint("A", "Send"); ui_footer_hint("B", "Select"); ui_footer_hint("HOME", "All TVs"); }

	static const char *help[] = {
		"A = send the button. Hold A = keeps sending.",
		"B = select. Pick a few, then Send does them in order.",
		"Make macro = save the selection as one button.",
		"Find = test codes and name the ones that work.",
		"Controller = every controller button works the TV.",
		"Edit (or press 1) = Quick Edit: pick a button to change it. 1 again = done.",
		"In the edit menu, hold 2 to keep testing that button.",
		"Empty spots on the remote: pick one to fill it.",
		"More = remote picture, add a button, sleep timer, notes, manage.",
		"Shortcuts: 1, 2, +, - and Classic buttons (see map).",
		"Smart = fill in missing buttons. HOME = back to your TVs.",
	};
	if (!popup) ui_help("Remote", help, 11);

	/* popups */
	/* More: 5 things, with the rarer ones one step further in */
	const char *more_items[] = { screen == S_VREMOTE ? "Button pages" : "Remote picture", "Add a button", "Sleep timer", "Notes", "Manage this remote" };
	static const int more_icons[] = { IC_REMOTE, IC_PLUS, IC_MOON, IC_NOTE, IC_GEAR };
	int c = menu_popup(&more_open, more_items, more_icons, 5, "More");
	if (c == 0) { if (screen == S_VREMOTE) go_back(); else go(S_VREMOTE); return; }
	int just = 0;   /* a sub-menu opened this frame: draw it from the next frame, so the same press doesn't pick in it */
	if (c == 1) { more_add = 1; ui_set_focus(9000); just = 1; }
	if (c == 2) { go(S_SLEEP); return; }
	if (c == 3) { go(S_NOTES); return; }
	if (c == 4) { more_manage = 1; ui_set_focus(9000); just = 1; }
	static const char *add_items[] = { "Add by code", "New macro", "Input shortcut" };
	static const int add_icons[] = { IC_PLUS, IC_SEQ, IC_INPUT };
	c = just ? -1 : menu_popup(&more_add, add_items, add_icons, 3, "Add a button");
	if (c == 0) { code_start(-1); return; }
	if (c == 1) { macro_edit_start(-1); return; }
	if (c == 2) { input_macro_start(); return; }
	int ndead = 0; for (int i = 0; i < cur.nbtn; i++) ndead += cur.btn[i].dead;
	char dl[32]; snprintf(dl, sizeof dl, show_dead ? "Hide dead codes" : "Show dead codes (%d)", ndead);
	const char *man_items[] = { "Rename remote", "Controller map", "Export to SD", dl, "Delete remote" };
	static const int man_icons[] = { IC_PENCIL, IC_GAMEPAD, IC_SAVE, IC_CLOSE, IC_TRASH };
	c = just ? -1 : menu_popup(&more_manage, man_items, man_icons, 5, "Manage this remote");
	if (c == 0) { kb_open(K_RENAME_REMOTE, cur.name, "Rename remote", NAME_LEN - 1); return; }
	if (c == 1) { map_start(); return; }
	if (c == 2) { char m[64]; if (remote_export(&cur, m, sizeof m)) ui_toast(m); else ui_toast("Export failed - is the SD card in?"); }
	if (c == 3) show_dead = !show_dead;
	if (c == 4) del_open = 1;
	if (key_open && (key_idx < 0 || key_idx >= cur.nbtn)) key_open = 0;   /* button gone */
	if (key_open) {
		RBtn *b = &cur.btn[key_idx];
		char tt[48]; snprintf(tt, sizeof tt, "%s   (hold 2 = test)", b->name);
		/* testing while editing: hold 2 keeps sending this button */
		if (in->held & BTN_2) { if (in->down & BTN_2) press_btn(key_idx, BTN_2); }
		if (!key_more) {
			/* the short menu: the things you do most */
			const char *items[5] = { "Name it", b->nsteps ? "Edit macro steps" : "Change code", b->fav ? "Unfavourite" : "Favourite", "Delete button", "More options" };
			static const int icons[] = { IC_PENCIL, IC_REMOTE, IC_STAR, IC_TRASH, IC_MORE };
			c = menu_popup(&key_open, items, icons, 5, tt);
			switch (c) {
				/* the same quick name tiles as when you first find a button, plus Type my own */
				case 0: edit_btn = key_idx; name_open(K_RENAME_BTN, "What does it do?"); return;
				case 1: if (!b->nsteps) { code_start(key_idx); return; } macro_edit_start(key_idx); return;
				case 2: b->fav = !b->fav; save_cur(); break;
				case 3: undo_save("Button deleted"); remote_del_btn(&cur, key_idx); save_cur(); ui_toast("Deleted. - to undo"); break;
				case 4: key_open = 1; key_more = 1; ui_set_focus(9000); break;
			}
		} else {
			const char *items[6] = { "Move earlier", "Move later", "Put in a spot", "Map to a Wii button", "Smart match (fill gaps)", "Bring back (not dead)" };
			static const int icons[] = { IC_LEFT, IC_RIGHT, IC_REMOTE, IC_GAMEPAD, IC_STAR, IC_CHECK };
			c = menu_popup(&key_open, items, icons, b->dead ? 6 : 5, tt);
			if (c == 3) c = 4; else if (c == 4) c = 5; else if (c == 5) c = 3;
			if (c >= 0 || !key_open) key_more = 0;
			switch (c) {
				case 0: remote_move_btn(&cur, key_idx, -1); save_cur(); break;
				case 1: remote_move_btn(&cur, key_idx, 1); save_cur(); break;
				case 2: key_open = 0; qa_open = 0; qa_pick = 2; qa_page = 0; ui_set_focus(-1); break;
				case 3: b->dead = 0; save_cur(); ui_toast("Back on the remote"); break;
				case 4: key_open = 0; map_start(); return;
				case 5: key_open = 0; smart_start(); return;
			}
		}
	}
	/* an empty spot on the on-screen remote was picked */
	if (qa_open) {
		static const char *qi[] = { "Pick one of my buttons", "Find it (Find buttons)", "Smart match", "Cancel" };
		static const int qic[] = { IC_REMOTE, IC_SEARCH, IC_STAR, IC_CLOSE };
		char t[40]; snprintf(t, sizeof t, "Fill the %s spot", SL_NAME[qa_slot]);
		c = menu_popup(&qa_open, qi, qic, 4, t);
		if (c == 0) { qa_pick = 1; qa_page = 0; ui_set_focus(-1); }
		if (c == 1) { finder_start(); return; }
		if (c == 2) { smart_start(); return; }
	}
	/* pick a button for a spot (qa_pick 1), or a spot for a button (qa_pick 2) */
	if (qa_pick) {
		ui_suspend(0); ui_dim(); ui_panel(30, 30, 580, 390);
		if (qa_pick == 1) {
			char t[48]; snprintf(t, sizeof t, "Which button is %s?", SL_NAME[qa_slot]);
			text(F_TITLE, 56, 44, t, C_TEXT, 255);
			int per = 16, pages = (cur.nbtn + per - 1) / per; if (pages < 1) pages = 1;
			for (int i = qa_page * per; i < cur.nbtn && i < (qa_page + 1) * per; i++) {
				int k = i - qa_page * per;
				RBtn *b = &cur.btn[i];
				if (ui_tile(5500 + i, 50 + (k % 4) * 136, 94 + (k / 4) * 62, 128, 54, icon_for_name(b->name), b->name, ST_KEY, 0) & UI_CLICK) {
					for (int j = 0; j < cur.nbtn; j++) if (cur.btn[j].slot == qa_slot) cur.btn[j].slot = 0;
					b->slot = qa_slot; b->dead = 0; save_cur(); qa_pick = 0; ui_toast("Done. Rename it any time in Edit");
				}
			}
			if (pages > 1) {
				if (ui_tile(5590, 250, 360, 60, 46, IC_LEFT, "", ST_NORMAL, 0) & UI_CLICK) qa_page = (qa_page + pages - 1) % pages;
				if (ui_tile(5591, 330, 360, 60, 46, IC_RIGHT, "", ST_NORMAL, 0) & UI_CLICK) qa_page = (qa_page + 1) % pages;
			}
		} else {
			char t[48]; snprintf(t, sizeof t, "Where does %s go?", cur.btn[key_idx].name);
			text(F_TITLE, 56, 44, t, C_TEXT, 255);
			for (int sl = 1; sl < SL_COUNT; sl++) {
				int k = sl - 1;
				if (ui_tile(5500 + sl, 50 + (k % 5) * 110, 90 + (k / 5) * 52, 104, 46, -1, SL_NAME[sl], ST_KEY, 0) & UI_CLICK) {
					for (int j = 0; j < cur.nbtn; j++) if (cur.btn[j].slot == sl) cur.btn[j].slot = 0;
					cur.btn[key_idx].slot = sl; save_cur(); qa_pick = 0; ui_toast("Moved");
				}
			}
		}
		if (ui_ibutton(5592, 460, 360, 130, 46, IC_BACK, "Cancel", ST_NORMAL) & UI_CLICK) qa_pick = 0;
		if (in->down & (BTN_HOME | BTN_B)) { qa_pick = 0; in->down = 0; }
		return;
	}
	static const char *del_items[] = { "Delete this remote", "Cancel" };
	static const int del_icons[] = { IC_TRASH, IC_CLOSE };
	c = menu_popup(&del_open, del_items, del_icons, 2, "Delete remote?");
	if (c == 0) {
		undo_save("Remote deleted");
		remote_delete(cur.file);
		have_cur = 0; cfg.last[0] = 0; config_save();
		go(S_HOME);
		return;
	}
	if (switch_open) {
		const char *items[8]; int icons[8];
		for (int i = 0; i < sw_n; i++) { items[i] = sw_names[i]; icons[i] = IC_TV; }
		items[sw_n] = "All TVs"; icons[sw_n] = IC_MENU;
		c = menu_popup(&switch_open, items, icons, sw_n + 1, "Switch TV");
		if (c >= 0 && c < sw_n) { open_remote_file(sw_files[c]); return; }
		if (c == sw_n) { go(S_HOME); return; }
	}

	if (!blocked) {
		/* 1 = Quick Edit on / off */
		int tilted = tilt_frame();   /* hold 1 + tilt = volume / channel (Settings > Extras) */
		if (cfg.tilt ? ((in->up & BTN_1) && !tilted) : (in->down & BTN_1)) { edit_mode = !edit_mode; clear_sel(); ui_toast(edit_mode ? "Quick Edit: pick a button. 1 = done" : "Quick Edit off"); }
		else if (!edit_mode && !undo_available()) handle_mapped(&menu_in, BTN_2 | BTN_PLUS | BTN_MINUS);
		else if (!edit_mode) handle_mapped(&menu_in, BTN_2 | BTN_PLUS);
		if (in->down & BTN_HOME) { if (edit_mode) edit_mode = 0; else if (screen == S_VREMOTE) go_back(); else go(S_HOME); }
	}
}

/* ================================================================== */
/* button finder                                                      */
/* ================================================================== */

static u8 f_cmd, f_proto;
static u16 f_addr;
static int f_auto, f_skip = 1;
static u64 f_next;
static const int AUTO_STEPS[] = { 500, 750, 1000, 1500, 2000, 3000, 5000 };

static int saved_as(u8 cmd)
{
	for (int i = 0; i < cur.nbtn; i++) {
		RBtn *b = &cur.btn[i];
		if (!b->nsteps && b->proto == f_proto && b->addr == f_addr && b->cmd == cmd) return i;
	}
	return -1;
}

static void f_send(void) { send_code_press(f_proto, f_addr, f_cmd, 0); smart_note(f_proto, f_addr, f_cmd, R_NONE, 0.2f); }

static void f_step(int dir)
{
	for (int k = 0; k < 256; k++) {
		f_cmd += dir;
		if (!f_skip || saved_as(f_cmd) < 0) break;
	}
	f_send();
}

static int f_quick_n;

/* ---- Group test (switch in Find buttons): send a few codes, ask once. Nothing? They're all
   ruled out together. Something? Split the group in half until it's one code. ---- */
static u8 gt_code[8], gt_tested[256];
static int gt_n, gt_lo, gt_hi, gt_plo, gt_phi, gt_known, gt_si, gt_state;   /* state: 0 idle, 1 sending, 2 asking */
static u64 gt_next;
static u8 gt_p; static u16 gt_a;
static int gt_gap(void) { int g = cfg.auto_ms / 2; return g < 350 ? 350 : g > 1500 ? 1500 : g; }
static void gt_reset(void) { memset(gt_tested, 0, sizeof gt_tested); gt_n = 0; gt_state = 0; gt_p = f_proto; gt_a = f_addr; }
static void gt_send_window(void) { gt_si = gt_lo; gt_state = 1; gt_next = 0; }
static void gt_start(void)
{
	if (gt_p != f_proto || gt_a != f_addr) gt_reset();
	int lim = (f_proto == P_RC5 || f_proto == P_SONY || f_proto == P_SONY15) ? 128 : 256;
	gt_n = 0;
	for (int k = 0; k < lim && gt_n < cfg.group_size; k++) {
		int c = (f_cmd + k) % lim;
		if (gt_tested[c] || saved_as((u8)c) >= 0) continue;
		gt_code[gt_n++] = (u8)c;
	}
	if (!gt_n) { gt_state = 0; ui_toast("Every code here is tested or saved"); return; }
	gt_lo = 0; gt_hi = gt_n; gt_known = 0;
	gt_send_window();
}
static void gt_found(int i)
{
	f_cmd = gt_code[i];
	send_code_press(f_proto, f_addr, f_cmd, 0);
	gt_state = 0; gt_n = 0;
	pend_proto = f_proto; pend_addr = f_addr; pend_cmd = f_cmd;
	name_open(K_NEW_BTN, "Found it! What did it do?");
}
static void gt_answer(int yes)
{
	if (yes) {
		if (gt_hi - gt_lo == 1) { gt_found(gt_lo); return; }
		gt_plo = gt_lo; gt_phi = gt_hi; gt_known = 1;
		gt_hi = gt_lo + (gt_hi - gt_lo) / 2;   /* try the first half */
		gt_send_window();
		return;
	}
	for (int i = gt_lo; i < gt_hi; i++) { gt_tested[gt_code[i]] = 1; smart_note(f_proto, f_addr, gt_code[i], R_NONE, 0.5f); }
	if (!gt_known) { f_cmd = gt_code[gt_n - 1]; f_cmd++; gt_start(); return; }   /* whole group: nothing. Next group */
	/* the first half did nothing, so it's in the other half */
	gt_plo = gt_hi;
	if (gt_phi - gt_plo == 1) { gt_found(gt_plo); return; }
	gt_lo = gt_plo; gt_hi = gt_plo + (gt_phi - gt_plo) / 2;
	gt_send_window();
}

void finder_start(void)
{
	gt_reset(); gt_n = 0;
	f_proto = cur.proto; f_addr = cur.addr; f_cmd = 0; f_auto = 0;
	if (f_skip) while (saved_as(f_cmd) >= 0 && f_cmd < 255) f_cmd++;
	go(S_FINDER);
	ui_set_focus(5213);
}

/* this code does nothing useful: keep it (hidden) so the search skips it from now on */
static void f_dead(void)
{
	if (saved_as(f_cmd) >= 0) { ui_toast("Already saved"); return; }
	char nm[NAME_LEN]; char base[16]; snprintf(base, sizeof base, "DEAD %02X", f_cmd); unique_name(&cur, base, nm);
	int b = remote_add_btn(&cur, nm, f_proto, f_addr, f_cmd);
	if (b >= 0) { cur.btn[b].dead = 1; save_cur(); smart_note(f_proto, f_addr, f_cmd, R_NONE, 1); ui_toast("Marked dead: it will be skipped"); f_auto = 0; f_step(1); }
	else ui_toast("This remote is full");
}

static void f_quick_save(void)
{
	char nm[NAME_LEN];
	unique_name(&cur, "UNKNOWN", nm);
	if (remote_add_btn(&cur, nm, f_proto, f_addr, f_cmd) < 0) { ui_toast("This remote is full"); return; }
	smart_note(f_proto, f_addr, f_cmd, R_OTHER, 1);
	save_cur();
	char m[48]; snprintf(m, sizeof m, "Quick saved as %s", nm);
	ui_toast(m);
	f_quick_n++;
}

void scr_finder(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	char sub[64]; snprintf(sub, sizeof sub, "%s  -  %s  -  address %02X", cur.name, PROTO_NAME[f_proto], f_addr);
	title(IC_SEARCH, "Find buttons", sub);
	if (smart_answers() >= 2) {
		static SGroup g[SMART_MAXG];
		if (smart_groups(g, 1) && g[0].pct >= 50) {
			char m[32]; snprintf(m, sizeof m, "Smart match %d%%", (int)(g[0].pct + 0.5f));
			if (ui_ibutton(5198, 364, 26, 176, 34, IC_STAR, m, ST_NORMAL) & UI_CLICK) { go(S_SMART); return; }
		}
	}

	/* the code card: focus it and use Left/Right (hold to scroll fast) */
	int fc = ui_item(5199, 40, 92, 560, 150);
	gfx_shadow(40, 92, 560, 150, 20);
	gfx_round_grad(40, 92, 560, 150, 20, (fc & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, (fc & UI_FOCUS) ? C_FOCUS_BOT : C_CARD, 255);
	text(F_SMALL, 64, 102, "CODE", C_DIM, 255);
	char big[8]; snprintf(big, sizeof big, "%02X", f_cmd);
	text(F_BIG, 62, 120, big, C_TEXT, 255);
	if (fc & UI_FOCUS) {
		icon(IC_LEFT, 52, 145, 16, C_ACCENT);
		icon(IC_RIGHT, 150, 145, 16, C_ACCENT);
	}
	char dec[24]; snprintf(dec, sizeof dec, "%d of 255", f_cmd);
	text(F_SMALL, 66, 192, dec, C_DIM, 255);
	int s = saved_as(f_cmd);
	if (s >= 0) {
		gfx_round(230, 108, 350, 64, 16, C_SOFT_GOOD, 255);
		icon(IC_CHECK, 262, 140, 30, C_GOOD);
		text(F_SMALL, 290, 114, "Already saved as", C_TEXT, 255);
		text_fit(F_BTN, 290, 134, 280, cur.btn[s].name, C_TEXT, 255);
	} else {
		gfx_round(230, 108, 350, 64, 16, C_INSET, 255);
		icon(IC_TV, 262, 140, 30, C_ACCENT);
		text(F_BODYB, 290, 116, "Watch the TV...", C_TEXT, 255);
		text(F_SMALL, 290, 142, "Something happened? Save it!", C_DIM, 255);
	}
	/* address -/+ inside the card */
	char ad[16]; snprintf(ad, sizeof ad, "Address %02X", f_addr);
	text(F_SMALL, 232, 188, ad, C_DIM, 255);
	int fam = ui_tile(5208, 318, 182, 38, 32, -1, "-", ST_KEY, 0);
	int fap = ui_tile(5209, 360, 182, 38, 32, -1, "+", ST_KEY, 0);
	/* is someone set up as a test remote? */
	int tester = 0;
	for (int c = 0; c < MAX_PADS; c++)
		if ((pads_now[c].connected && cfg.role_wm[c] == ROLE_TV) || (pads_now[c].gc_connected && cfg.role_gc[c] == ROLE_TV)) tester = 1;
	/* Frankenstein remotes: any code type, not just this TV's */
	char ty[24]; snprintf(ty, sizeof ty, "Type: %s", PROTO_NAME[f_proto]);
	int fty = ui_button(5211, 408, 182, 176, 32, ty, f_proto != cur.proto ? ST_PRIMARY : ST_KEY);
	if (fty & UI_CLICK) { f_proto = (f_proto + 1) % P_COUNT; f_addr = addr_wrap(f_proto, f_addr); f_cmd = 0; ui_toast(f_proto == cur.proto ? "Back to this TV's code type" : "Other device: codes save onto this remote too"); }
	(void)tester;

	/* the code strip: point and click to jump, or highlight it and use the D-pad.
	   A sends, 1 quick saves (hold 1 = pick what it did), 2 = dead */
	int fst = ui_item(5213, 34, 245, 572, 32);
	if (fst & UI_FOCUS) { gfx_round(34, 245, 572, 32, 10, C_CARD_HI, 255); ui_focus_ring(34, 245, 572, 32, 10); }
	for (int c = 0; c < 256; c++) {
		int x = 40 + c * 560 / 256, w = 560 / 256 + 1, sv = saved_as(c);
		gfx_rect(x, 252, w, 12, sv < 0 ? C_LINE : cur.btn[sv].dead ? C_BAD : C_GOOD, 255);
	}
	gfx_rect(40 + f_cmd * 560 / 256 - 1, 246, 4, 24, C_ACCENT, 255);
	if ((fst & UI_CLICK) && ui_pointer_active() && in->ptr_valid) {
		int c = (in->px - 40) * 256 / 560; if (c < 0) c = 0; if (c > 255) c = 255;
		f_cmd = (u8)c;
	}

	/* group test switch (under the row), and the group test itself */
	int gy = 282;
	if (cfg.group_test) {
		if (gt_p != f_proto || gt_a != f_addr) gt_reset();
		/* strip: codes ruled out this time in blue */
		for (int c = 0; c < 256; c++) if (gt_tested[c] && saved_as(c) < 0) gfx_rect(40 + c * 560 / 256, 252, 560 / 256 + 1, 12, RGB(70, 100, 170), 255);
		if (gt_state == 1 && plat_ms() >= gt_next) {   /* send the group one code at a time */
			if (gt_si < gt_hi) { f_cmd = gt_code[gt_si]; send_code_press(f_proto, f_addr, f_cmd, 0); gt_si++; gt_next = plat_ms() + gt_gap(); }
			else gt_state = 2;
		}
		gfx_round(40, gy, 560, 62, 16, C_CARD_HI, 255);
		char gl[96] = "", q[64];
		for (int i = gt_lo; i < gt_hi && gt_n; i++) { char t[6]; snprintf(t, sizeof t, "%02X ", gt_code[i]); strcat(gl, t); }
		if (!gt_n) snprintf(q, sizeof q, "Group test: %d codes at a time", cfg.group_size);
		else if (gt_state == 1) snprintf(q, sizeof q, "Sending %d of %d... watch the TV", gt_si - gt_lo + 1 > gt_hi - gt_lo ? gt_hi - gt_lo : gt_si - gt_lo + 1, gt_hi - gt_lo);
		else snprintf(q, sizeof q, gt_known ? "Did THESE do anything? (narrowing down)" : "Did the TV do anything?");
		text_fit(F_BODYB, 56, gy + 6, 528, q, C_TEXT, 255);
		text_fit(F_SMALL, 56, gy + 34, 528, gt_n ? gl : "No: all ruled out at once. Yes: it splits them until it finds the one.", C_DIM, 255);
		int by = gy + 68, ask = gt_state == 2;
		int f_st = ui_ibutton(5230, 40, by, 136, 36, IC_PLAY, gt_n ? "Resend" : "Start", gt_n ? ST_NORMAL : ST_PRIMARY);
		int f_ye = ui_ibutton(5231, 184, by, 136, 36, IC_CHECK, "Yes", ask ? ST_PRIMARY : ST_NORMAL);
		int f_no = ui_ibutton(5232, 328, by, 136, 36, IC_CLOSE, "No", ST_NORMAL);
		char gs[24]; snprintf(gs, sizeof gs, "Size: %d", cfg.group_size);
		int f_sz = ui_button(5233, 472, by, 128, 36, gs, ST_NORMAL);
		char sk[32]; snprintf(sk, sizeof sk, "Group test: On");
		int f_gt = ui_ibutton(5234, 40, by + 42, 200, 30, IC_CHECK, sk, ST_PRIMARY);
		text_fit(F_SMALL, 250, by + 47, 350, "Arrows / OK only react with a TV menu open", C_DIM, 255);
		if (!blocked) {
			if (f_st & UI_CLICK) { if (gt_n) gt_send_window(); else gt_start(); }
			if ((f_ye & UI_CLICK) && gt_n && gt_state != 1) { gt_answer(1); if (screen != S_FINDER) return; }
			if ((f_no & UI_CLICK) && gt_n && gt_state != 1) { gt_answer(0); if (screen != S_FINDER) return; }
			if (f_sz & UI_CLICK) { cfg.group_size = cfg.group_size >= 8 ? 4 : cfg.group_size + 2; config_save(); gt_n = 0; gt_state = 0; }
			if (f_gt & UI_CLICK) { cfg.group_test = 0; config_save(); gt_n = 0; gt_state = 0; ui_toast("Group test off: one code at a time"); }
		}
		ui_footer_begin();
		ui_footer_hint("A", "Choose");
		ui_footer_hint("B", "Done");
		static const char *ghelp[] = {
			"Group test sends a few codes, then asks once.",
			"No = every code in the group is ruled out in one go.",
			"Yes = it splits the group in half and asks again, until it's one code. Then you name it.",
			"Size changes how many codes per group (4, 6 or 8). Speed (Auto) sets the gap between codes.",
			"Blue on the strip = ruled out this time. Green = saved, red = dead.",
		};
		ui_help("Group test", ghelp, 5);
		if (!blocked && (in->down & (BTN_HOME | BTN_B))) { gt_state = 0; go_back(); }
		return;
	}

	/* one row: Back, Next, Send, Dead, Active, Auto, Speed */
	int y = 282, tileh = 84, tw = 74, tg = 7;
	int fb = ui_tile(5200, 40 + 0 * (tw + tg), y, tw, tileh, IC_LEFT, "Back", ST_NORMAL, 0);
	int fn = ui_tile(5202, 40 + 1 * (tw + tg), y, tw, tileh, IC_RIGHT, "Next", ST_NORMAL, 0);
	int fs = ui_tile(5201, 40 + 2 * (tw + tg), y, tw, tileh, IC_RADAR, "Send", ST_NORMAL, 0);
	int fdd = ui_tile(5212, 40 + 3 * (tw + tg), y, tw, tileh, IC_CLOSE, "Dead", ST_NORMAL, 0);
	int fv = ui_tile(5203, 40 + 4 * (tw + tg), y, tw, tileh, IC_CHECK, "Active", ST_NORMAL, 0);
	int fa = ui_tile(5204, 40 + 5 * (tw + tg), y, tw, tileh, f_auto ? IC_PAUSE : IC_PLAY, f_auto ? "Stop" : "Auto", f_auto ? ST_PRIMARY : ST_NORMAL, 0);
	int sxp = 40 + 6 * (tw + tg);
	int fsp = ui_tile(5205, sxp, y, 600 - sxp, tileh, -1, "", ST_NORMAL, 0);
	{
		int pr = (fsp & UI_HELD) ? 2 : 0;
		char sp[16]; snprintf(sp, sizeof sp, "%.1f s", cfg.auto_ms / 1000.0);
		text_c(F_SMALL, sxp + (600 - sxp) / 2, y + 16 + pr, "Speed", C_DIM, 255);
		text_c(F_BODYB, sxp + (600 - sxp) / 2, y + 40 + pr, sp, C_TEXT, 255);
	}
	/* a code that does nothing (or something useless): hide it so the search skips it from now on */
	if ((fdd & UI_CLICK) || (!blocked && (in->down & BTN_2))) f_dead();
	/* skip codes already marked Active or Dead (- also toggles it) */
	char skl[32]; snprintf(skl, sizeof skl, "Skip saved codes: %s", f_skip ? "On" : "Off");
	int fk = ui_ibutton(5207, 40, y + tileh + 10, 250, 38, f_skip ? IC_CHECK : IC_CLOSE, skl, f_skip ? ST_PRIMARY : ST_NORMAL);
	int fgt = ui_ibutton(5234, 298, y + tileh + 10, 190, 38, IC_CLOSE, "Group test: Off", ST_NORMAL);
	text_r(F_SMALL, 600, y + tileh + 19, "1 save  2 dead", C_DIM, 255);
	if (fgt & UI_CLICK) { cfg.group_test = 1; config_save(); f_auto = 0; gt_reset(); ui_toast("Group test: a few codes at a time"); }
	int ai = 0; for (int k = 0; k < 7; k++) if (AUTO_STEPS[k] == cfg.auto_ms) ai = k;
	u32 d = blocked ? 0 : in->down;
	int user = 0;
	int on_card = (ui_get_focus() == 5199 || ui_get_focus() == 5213) && !ui_pointer_active() && !blocked;
	if ((fb & UI_CLICK) || (on_card && ui_repeat(BTN_LEFT))) { f_step(-1); user = 1; }
	if ((fn & UI_CLICK) || (on_card && ui_repeat(BTN_RIGHT))) { f_step(1); user = 1; }
	if (on_card) in->down &= ~(BTN_LEFT | BTN_RIGHT);   /* don't move focus while scrolling codes */
	if ((fs & UI_CLICK) || (fc & UI_CLICK) || (fst & UI_CLICK)) { send_code_press(f_proto, f_addr, f_cmd, cfg.safe_hold ? 0 : BTN_A); user = 1; }
	if (fam & UI_CLICK) { f_addr = addr_wrap(f_proto, f_addr - 1); user = 1; }
	if (fap & UI_CLICK) { f_addr = addr_wrap(f_proto, f_addr + 1); user = 1; }
	if (fa & UI_CLICK) {
		f_auto = !f_auto;
		if (f_auto) { f_step(1); f_next = plat_ms() + cfg.auto_ms; ui_toast("Auto: sending the next code by itself"); }   /* start straight away */
	}
	else if (user) f_auto = 0;
	if (fsp & UI_CLICK) { cfg.auto_ms = AUTO_STEPS[(ai + 1) % 7]; config_save(); }
	if ((fk & UI_CLICK) || (d & BTN_MINUS)) { f_skip = !f_skip; ui_toast(f_skip ? "Skipping codes you marked Active or Dead" : "Showing every code"); }

	/* 1 (or +): tap = quick save (it did something), hold = pick what it did */
	static u64 one_at; static int one_long;
	u32 SAVEK = BTN_1 | BTN_PLUS;
	if (d & SAVEK) { one_at = plat_ms(); one_long = 0; }
	if (one_at && (in->held & SAVEK) && !one_long && plat_ms() - one_at > 600) {
		one_long = 1; one_at = 0; f_auto = 0;
		pend_proto = f_proto; pend_addr = f_addr; pend_cmd = f_cmd;
		name_open(K_NEW_BTN, "What did it do?");
		return;
	}
	if (one_at && (in->up & SAVEK)) { one_at = 0; f_auto = 0; f_quick_save(); if (f_skip) f_step(1); }
	if (fv & UI_CLICK) {
		f_auto = 0;
		pend_proto = f_proto; pend_addr = f_addr; pend_cmd = f_cmd;
		name_open(K_NEW_BTN, "What did it do?");
		return;
	}
	if (f_auto && plat_ms() >= f_next) { f_step(1); f_next = plat_ms() + cfg.auto_ms; }

	ui_footer_begin();
	ui_footer_hint("<>", "Code");
	ui_footer_hint("A", "Send");
	ui_footer_hint("1", "Save");
	ui_footer_hint("2", "Dead");
	ui_footer_hint("B", "Done");
	static const char *help[] = {
		"Highlight the code strip (or the big code) and press Left / Right. Hold to scroll fast. Point and click the strip to jump.",
		"A sends the code. 1 = it did something (quick save). Hold 1 = pick what it did. 2 = dead.",
		"Each code is sent to the TV. Watch the TV each time.",
		"Active = it did something. Pick what it did and it's saved.",
		"Dead = it does nothing useful: it's hidden and skipped from now on.",
		"Auto goes through codes by itself. Tap Speed to change how fast.",
		"- = hide codes you already have. Green on the strip = saved, red = dead.",
		"Test a saved button with a 2nd controller: Settings > Controllers, set it to TV.",
		"Address - / + is only needed for other devices.",
		"Tip: a hidden code may open the service menu. Name it SERVICE.",
	};
	ui_help("Find buttons", help, (int)(sizeof help / sizeof help[0]));
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) { f_auto = 0; go_back(); }
}

/* ================================================================== */
/* add by code / change code                                          */
/* ================================================================== */

static u8 c_p, c_c;
static u16 c_a;
static int c_idx;

void code_start(int idx)
{
	c_idx = idx;
	if (idx >= 0) { c_p = cur.btn[idx].proto; c_a = cur.btn[idx].addr; c_c = cur.btn[idx].cmd; }
	else { c_p = cur.proto; c_a = cur.addr; c_c = 0; }
	go(S_CODE);
}

void scr_code(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_REMOTE, c_idx >= 0 ? "Change code" : "Add by code", c_idx >= 0 ? cur.btn[c_idx].name : cur.name);
	char v[16]; int d, y = 104;
	snprintf(v, sizeof v, "%s", PROTO_NAME[c_p]);
	if (value_row(5300, y, "Type", v, &d)) { c_p = (c_p + P_COUNT + d) % P_COUNT; c_a = addr_wrap(c_p, c_a); }
	y += 60;
	snprintf(v, sizeof v, c_a > 255 ? "0x%04X" : "0x%02X", c_a);
	if (value_row(5302, y, "Address", v, &d)) c_a = addr_wrap(c_p, c_a + d);
	y += 60;
	snprintf(v, sizeof v, "0x%02X", c_c);
	if (value_row(5304, y, "Command", v, &d)) c_c += d;
	y += 76;
	if (ui_ibutton(5310, 60, y, 160, 54, IC_PLAY, "Test", ST_NORMAL) & UI_CLICK) send_code_press(c_p, c_a, c_c, BTN_A);
	if (ui_ibutton(5311, 232, y, 150, 54, IC_CLOSE, "Cancel", ST_NORMAL) & UI_CLICK) { go(S_REMOTE); return; }
	if (ui_ibutton(5312, 394, y, 188, 54, IC_CHECK, "Save", ST_NORMAL) & UI_CLICK) {
		if (c_idx >= 0) {
			RBtn *b = &cur.btn[c_idx];
			undo_save("Code changed");
			b->proto = c_p; b->addr = c_a; b->cmd = c_c;
			save_cur(); go(S_REMOTE);
		} else {
			pend_proto = c_p; pend_addr = c_a; pend_cmd = c_c;
			name_open(K_CODE_BTN, "Name this button");
		}
		return;
	}
	ui_footer_begin();
	ui_footer_hint("A", "Change");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Use this if you already know a button's code.",
		"Type and Address are usually the same for the whole TV.",
		"Command is the button. Test sends it right now.",
		"Hold A on - or + to count fast.",
	};
	ui_help("Codes", help, 4);
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ================================================================== */
/* controller pictures                                                */
/* ================================================================== */

typedef struct { int x, y, w, h, round; const char *lbl; int idx; } PB;

/* Wii Remote, standing up. idx = index in WM_BITS, -1 = not mappable */
static const PB WM_PIC[] = {
	{ 58, 22, 22, 22, 4, "", 6 }, { 58, 66, 22, 22, 4, "", 7 }, { 36, 44, 22, 22, 4, "", 8 }, { 80, 44, 22, 22, 4, "", 9 },
	{ 49, 104, 40, 40, 20, "A", 0 },
	{ 118, 96, 26, 56, 10, "B", 1 },
	{ 30, 170, 24, 24, 12, "-", 5 }, { 57, 170, 24, 24, 12, "H", -1 }, { 84, 170, 24, 24, 12, "+", 4 },
	{ 53, 206, 32, 32, 16, "1", 2 }, { 53, 244, 32, 32, 16, "2", 3 },
};
/* Classic Controller, lying down */
static const PB CC_PIC[] = {
	{ 50, 78, 20, 20, 4, "", 10 }, { 50, 118, 20, 20, 4, "", 11 }, { 30, 98, 20, 20, 4, "", 12 }, { 70, 98, 20, 20, 4, "", 13 },
	{ 206, 76, 26, 26, 13, "x", 2 }, { 230, 100, 26, 26, 13, "a", 0 }, { 206, 124, 26, 26, 13, "b", 1 }, { 182, 100, 26, 26, 13, "y", 3 },
	{ 104, 90, 22, 22, 11, "-", 9 }, { 130, 90, 22, 22, 11, "H", -1 }, { 156, 90, 22, 22, 11, "+", 8 },
	{ 20, 30, 60, 20, 8, "L", 4 }, { 198, 30, 60, 20, 8, "R", 5 }, { 84, 30, 46, 20, 8, "ZL", 6 }, { 148, 30, 46, 20, 8, "ZR", 7 },
};

/* returns the picture's height */
void controller_picture(int which, int x, int y, int interactive, int idbase, int *clicked, u32 fbit)
{
	const PB *pb = which ? CC_PIC : WM_PIC;
	int n = which ? (int)(sizeof CC_PIC / sizeof CC_PIC[0]) : (int)(sizeof WM_PIC / sizeof WM_PIC[0]);
	u32 body = C_WHITE, btn = RGB(70, 76, 92);
	if (!which) {
		gfx_shadow(x + 14, y, 110, 286, 40);
		gfx_round(x + 14, y, 110, 286, 44, body, 255);
		gfx_round_line(x + 14, y, 110, 286, 44, 2, C_LINE, 255);
		gfx_round(x + 118, y + 90, 32, 68, 12, RGB(200, 206, 218), 255);   /* B trigger on the back */
		for (int k = 0; k < 4; k++) gfx_circle(x + 43 + k * 17, y + 268, 3, RGB(90, 150, 255), 255);
	} else {
		gfx_shadow(x, y + 40, 280, 140, 50);
		gfx_round(x + 28, y + 148, 74, 64, 32, C_LINE, 255);
		gfx_round(x + 178, y + 148, 74, 64, 32, C_LINE, 255);
		gfx_round(x - 2, y + 38, 284, 144, 58, C_LINE, 255);
		gfx_round(x, y + 40, 280, 140, 56, body, 255);
		gfx_round(x + 30, y + 150, 70, 60, 30, body, 255);
		gfx_round(x + 180, y + 150, 70, 60, 30, body, 255);
	}
	u64 now = plat_ms();
	for (int i = 0; i < n; i++) {
		const PB *b = &pb[i];
		int bx = x + b->x, by = y + b->y;
		if (which && b->y < 40) { bx = x + b->x; by = y + b->y; }
		u32 col = b->idx < 0 ? RGB(150, 156, 170) : btn;
		u32 bit = b->idx < 0 ? 0 : (which ? CC_BITS[b->idx] : WM_BITS[b->idx]);
		if (bit && bit == fbit && now - flash_ms < 350) col = C_GOOD;
		int f = 0;
		if (interactive && b->idx >= 0) f = ui_item(idbase + b->idx, bx - 3, by - 3, b->w + 6, b->h + 6);
		if (f & UI_FOCUS) col = C_ACCENT;
		gfx_round(bx, by, b->w, b->h, b->round, col, 255);
		if (b->lbl[0]) text_c(F_SMALL, bx + b->w / 2, by + b->h / 2 - 11, b->lbl, RGB(255, 255, 255), 255);
		else if (b->idx >= 6 && !which) icon(IC_UP + (b->idx - 6), bx + b->w / 2, by + b->h / 2, 14, RGB(255, 255, 255));
		else if (which && b->idx >= 10) icon(IC_UP + (b->idx - 10), bx + b->w / 2, by + b->h / 2, 14, RGB(255, 255, 255));
		if (f & UI_FOCUS) ui_focus_ring(bx, by, b->w, b->h, b->round);
		if ((f & UI_CLICK) && clicked) *clicked = b->idx;
	}
}

static const char *mapped_name(int which, int k)
{
	int idx = which == 1 ? cur.map_cc[k] : which == 2 ? cur.map_wm2[k] : cur.map_wm[k];
	if (idx < 0 || idx >= cur.nbtn) return "-";
	return cur.btn[idx].name;
}

/* ================================================================== */
/* controller map                                                     */
/* ================================================================== */

static int m_tab, pick_open, pick_k, pick_page;   /* tab: 0 Wii Remote, 1 Shift layer, 2 Classic / GameCube */

void map_start(void) { m_tab = 0; pick_open = 0; go(S_MAP); }

static s8 *map_slot(int tab, int k) { return tab == 2 ? &cur.map_cc[k] : tab == 1 ? &cur.map_wm2[k] : &cur.map_wm[k]; }

void scr_map(void)
{
	int blocked = pick_open || help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_GAMEPAD, "Controller map", cur.name);
	if (ui_button(5400, 40, 90, 118, 38, "Wii Remote", m_tab == 0 ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) m_tab = 0;
	if (ui_button(5404, 164, 90, 100, 38, "Shift", m_tab == 1 ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) m_tab = 1;
	if (ui_button(5401, 270, 90, 112, 38, "Classic", m_tab == 2 ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) m_tab = 2;
	if (ui_button(5405, 388, 90, 92, 38, "Keys", ST_NORMAL) & UI_CLICK) { go(S_KEYMAP); return; }
	if (ui_tile(5402, 486, 90, 54, 38, IC_STAR, "", ST_NORMAL, 0) & UI_CLICK) { remote_automap(&cur); save_cur(); ui_toast("Auto map filled in"); }
	if (ui_tile(5403, 546, 90, 54, 38, IC_TRASH, "", ST_NORMAL, 0) & UI_CLICK) {
		undo_save("Map cleared");
		if (m_tab == 2) memset(cur.map_cc, -1, sizeof cur.map_cc); else if (m_tab == 1) memset(cur.map_wm2, -1, sizeof cur.map_wm2); else memset(cur.map_wm, -1, sizeof cur.map_wm);
		save_cur();
	}
	int clicked = -1;
	int cc = m_tab == 2;
	if (!cc) controller_picture(0, 60, 134, 1, 5500, &clicked, flash_bit);
	else controller_picture(1, 30, 130, 1, 5500, &clicked, flash_bit);
	if (cc) text(F_SMALL, 40, 356, "GameCube controllers use this too.", C_DIM, 255);
	if (m_tab == 1 && !cfg.shift_mode) {
		gfx_round(40, 386, 210, 34, 10, C_SOFT_WARN, 255);
		text(F_SMALL, 50, 392, "Shift is off: Settings > Shift", C_TEXT, 255);
	}

	/* list */
	int cnt = cc ? CC_COUNT : WM_COUNT;
	int lx = cc ? 330 : 250, colw = cc ? 132 : 170, rh = cc ? 34 : 40, per_col = cc ? 7 : 6;
	int which = m_tab == 2 ? 1 : m_tab == 1 ? 2 : 0;
	for (int k = 0; k < cnt; k++) {
		int x = lx + (k / per_col) * (colw + 8), y = 146 + (k % per_col) * (rh + 4);
		int f = ui_item(5600 + k, x, y, colw, rh);
		int foc = (f & UI_FOCUS) || ui_get_focus() == 5500 + k;
		gfx_round(x, y, colw, rh, 10, foc ? C_FOCUS_TOP : C_CARD_HI, 255);
		gfx_round_line(x, y, colw, rh, 10, 1, C_LINE, 255);
		int bw = badge(x + 6, y + (rh - 24) / 2, cc ? CC_NAMES[k] : WM_NAMES[k], C_TEXT);
		const char *nm = mapped_name(which, k);
		if (m_tab == 1 && k == 1) nm = "(shift)";
		text_fit(F_BODYB, x + bw + 12, y + (rh - F_BODYB->height) / 2 + 1, colw - bw - 18, nm, nm[0] == '-' || nm[0] == '(' ? C_DIM : C_TEXT, 255);
		if (f & UI_FOCUS) ui_focus_ring(x, y, colw, rh, 10);
		if (f & UI_CLICK) clicked = k;
	}
	if (m_tab == 1 && clicked == 1) { ui_toast("B is the shift button"); clicked = -1; }
	if (clicked >= 0 && !blocked) { pick_open = 1; pick_k = clicked; pick_page = 0; ui_set_focus(-1); }

	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("-", "Undo");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Choose a controller button, then pick what it sends.",
		"Star = auto fill (OK, MENU, UP, VOL+...). Bin = clear this tab.",
		"Keys = map USB keyboard keys to TV buttons.",
		"C and Z are the Nunchuk buttons.",
		"Shift = a 2nd set of Wii Remote buttons, used while B is held (or tapped). Turn it on in Settings.",
		"On the remote screen: 1, 2, +, - and Classic/GameCube work.",
		"In Controller mode and Calibrate: every button works.",
		"HOME is never mapped, so you can always get back.",
	};
	if (!pick_open) ui_help("Controller map", help, 8);

	if (pick_open) {
		ui_suspend(0);
		ui_dim();
		ui_panel(30, 30, 580, 390);
		char t[48]; snprintf(t, sizeof t, "%s%s  sends...", m_tab == 1 ? "Shift + " : "", cc ? CC_NAMES[pick_k] : WM_NAMES[pick_k]);
		text(F_TITLE, 56, 54, t, C_TEXT, 255);
		int per = 16, pages = (cur.nbtn + per - 1) / per; if (pages < 1) pages = 1;
		for (int i = pick_page * per; i < cur.nbtn && i < (pick_page + 1) * per; i++) {
			int k = i - pick_page * per;
			RBtn *b = &cur.btn[i];
			int ic = icon_for_name(b->name); if (ic < 0 && b->nsteps) ic = IC_SEQ;
			if (ui_tile(5700 + i, 50 + (k % 4) * 136, 104 + (k / 4) * 62, 128, 54, ic, b->name, ST_KEY, 0) & UI_CLICK) {
				*map_slot(m_tab, pick_k) = i;
				save_cur(); pick_open = 0;
			}
		}
		int by = 360;
		if (ui_ibutton(5790, 50, by, 130, 46, IC_CLOSE, "Nothing", ST_NORMAL) & UI_CLICK) {
			*map_slot(m_tab, pick_k) = -1;
			save_cur(); pick_open = 0;
		}
		if (pages > 1) {
			if (ui_tile(5791, 250, by, 60, 46, IC_LEFT, "", ST_NORMAL, 0) & UI_CLICK) { if (pick_page) pick_page--; }
			char pg[12]; snprintf(pg, sizeof pg, "%d/%d", pick_page + 1, pages);
			text_c(F_BODYB, 340, by + 10, pg, C_TEXT, 255);
			if (ui_tile(5792, 370, by, 60, 46, IC_RIGHT, "", ST_NORMAL, 0) & UI_CLICK) { if (pick_page < pages - 1) pick_page++; }
		}
		if (ui_ibutton(5793, 460, by, 130, 46, IC_BACK, "Cancel", ST_NORMAL) & UI_CLICK) pick_open = 0;
		if (in->down & (BTN_HOME | BTN_B)) { pick_open = 0; in->down = 0; }
		return;
	}
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ================================================================== */
/* controller mode                                                    */
/* ================================================================== */

void direct_start(void) { go(S_DIRECT); }

static void wm_labels(int x, int y, int which)
{
	/* name tags beside the Wii Remote picture */
	struct { int k, side, dy; } L[] = {
		{ 6, 0, 22 }, { 8, 0, 44 }, { 7, 0, 66 }, { 9, 1, 44 }, { 0, 0, 112 }, { 1, 1, 112 },
		{ 5, 0, 170 }, { 4, 1, 170 }, { 2, 0, 210 }, { 3, 0, 248 },
	};
	for (size_t i = 0; i < sizeof L / sizeof L[0]; i++) {
		const char *nm = mapped_name(which, L[i].k);
		if (which == 2 && L[i].k == 1) nm = "SHIFT";
		u32 c = nm[0] == '-' ? C_DIM : C_TEXT;
		if (flash_bit == WM_BITS[L[i].k] && plat_ms() - flash_ms < 350) c = C_GOOD;
		if (L[i].side == 0) text_r(F_BODYB, x + 4, y + L[i].dy, nm, c, 255);
		else text(F_BODYB, x + 164, y + L[i].dy, nm, c, 255);
	}
}

void scr_direct(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_GAMEPAD, "Controller mode", cur.name);
	controller_picture(0, 150, 110, 0, 0, NULL, flash_bit);
	int sh = shift_active(&menu_in);
	wm_labels(150, 110, sh ? 2 : 0);
	if (cfg.shift_mode) {
		gfx_round(164, 84, 110, 22, 11, sh ? C_SEL : C_INSET, 255);
		text_c(F_SMALL, 219, 85, sh ? "SHIFT ON" : "Shift off", sh ? C_WHITE : C_DIM, 255);
	}
	/* classic list */
	int x = 400;
	text(F_BODYB, x, 104, "Classic", C_DIM, 255);
	for (int k = 0; k < CC_COUNT; k++) {
		int yy = 132 + k * 21;
		const char *nm = mapped_name(1, k);
		u32 c = nm[0] == '-' ? C_DIM : C_TEXT;
		if (flash_bit == CC_BITS[k] && plat_ms() - flash_ms < 350) c = C_GOOD;
		text(F_SMALL, x, yy, CC_NAMES[k], C_ACCENT, 255);
		text_fit(F_SMALL, x + 50, yy, 150, nm, c, 255);
	}
	draw_last_sent(530, 36);
	ui_footer_begin();
	ui_footer_hint("ALL", "Send to TV");
	ui_footer_hint("HOME", "Leave");
	static const char *help[] = {
		"Every controller button now sends its TV button.",
		"The names show what each button does.",
		"Hold a button to keep sending (volume).",
		"Change them in More > Controller map.",
		"Press HOME to leave.",
	};
	ui_help("Controller mode", help, 5);
	if (!blocked) {
		handle_mapped(&menu_in, 0xFFFF & ~BTN_HOME);
		if (in->down & BTN_HOME) go_back();
	}
}

/* ================================================================== */
/* calibration                                                        */
/* ================================================================== */

static int cal_pics, pat, pat_drawn = -1, pic_mode, cal_svc_open;
static int cal_group_back;               /* the group was opened from a guided area: HOME goes back there */
static int cal_cat = -1;                 /* open group in the suite, -1 = the suite's front page */
static int cal_list[96], cal_n, cal_pos; /* the pictures 1 / 2 step through */
static int cal_back;                     /* where HOME goes from the pictures: 0 suite/group, 1 the remote */
static int ov_open, ov_sel;              /* service remote over the picture */
static u64 hud_until, mode_until;
static u32 *patbuf, *outbuf;
u32 *app_native_buf;

static int script;   /* Tint Doctor / Guided calibration running (see below) */
static int sc_hidden;        /* the script panel is tucked away so the whole picture shows */
static u64 sc_hint_until;    /* how long to show the small "panel hidden" reminder */
static void cal_remote_pick_open(void);
void cal_start(void) { cal_group_back = 0; script = 0; ov_open = 0; cal_pics = 0; cal_cat = -1; cal_svc_open = 0; pat_scan_user(); ui_set_focus(5820); calui_suite_enter(); go(S_CAL); cal_remote_pick_open(); }
int cal_in_pictures(void) { return screen == S_CAL && cal_pics; }
void cal_open_group(int cat) { cal_group_back = 1; cal_pics = 0; cal_cat = cat; pat_scan_user(); ui_set_focus(5860); go(S_CAL); }

static void hud_text(const Font *f, int x, int y, const char *t, u32 c, int a) { text(f, x, y, t, c, a); }

/* open the pictures on a list; overlay = start with the service remote showing */
static void pics_open(const int *list, int n, int pos, int overlay, int back)
{
	if (n <= 0) { ui_toast("No pictures here yet"); return; }
	if (n > 96) n = 96;
	memcpy(cal_list, list, n * sizeof(int)); cal_n = n; cal_pos = pos < n ? pos : 0;
	pat = cal_list[cal_pos]; pat_drawn = -1; cal_pics = 1; cal_back = back;
	ov_open = overlay && have_cur; ov_sel = 0; pic_mode = 0; script = 0;
	hud_until = plat_ms() + 5000;
}

/* the remote's buttons in the order a service menu needs them */
static int ov_order(int *out, int max)
{
	int n = 0;
	if (!have_cur) return 0;
	static const char *FIRST[] = { "SERVICE", "MENU", "UP", "DOWN", "LEFT", "RIGHT", "OK", "BACK", "EXIT", "VOL+", "VOL-", "P+", "P-", "CH+", "CH-", "INFO", "PICTURE" };
	for (size_t k = 0; k < sizeof FIRST / sizeof FIRST[0] && n < max; k++) {
		int i = remote_find_btn(&cur, FIRST[k]);
		if (i >= 0) { int dup = 0; for (int j = 0; j < n; j++) if (out[j] == i) dup = 1; if (!dup) out[n++] = i; }
	}
	for (int i = 0; i < cur.nbtn && n < max; i++) {
		int dup = 0; for (int j = 0; j < n; j++) if (out[j] == i) dup = 1;
		if (!dup && (cur.btn[i].fav || strstr(cur.btn[i].name, "SERVICE"))) out[n++] = i;
	}
	for (int i = 0; i < cur.nbtn && n < max; i++) {
		int dup = 0; for (int j = 0; j < n; j++) if (out[j] == i) dup = 1;
		if (!dup && !cur.btn[i].dead) out[n++] = i;
	}
	return n;
}

/* small battery row, for screens with no bottom bar */
static void batteries(int x, int y, int warn_only)
{
	for (int c = MAX_PADS - 1; c >= 0; c--) {   /* right to left, so 1 ends up on the left */
		int b = ui_battery[c];
		if (b < -1 || (warn_only && (b < 0 || b >= 25))) continue;
		gfx_round(x - 4, y - 3, 58, 22, 11, C_HUD, 200);
		char n[4]; snprintf(n, sizeof n, "%d", c + 1);
		text(F_SMALL, x + 2, y - 2, n, C_WHITE, 255);
		gfx_round(x + 14, y + 2, 26, 12, 3, RGB(200, 204, 214), 255);
		gfx_rect(x + 40, y + 5, 3, 6, RGB(200, 204, 214), 255);
		gfx_round(x + 16, y + 4, 22, 8, 2, RGB(20, 24, 36), 255);
		if (b >= 0) { int bars = b >= 75 ? 4 : b >= 50 ? 3 : b >= 25 ? 2 : 1; for (int k = 0; k < bars; k++) gfx_rect(x + 17 + k * 5, y + 5, 4, 6, bars <= 1 ? C_BAD : C_GOOD, 255); }
		x -= 64;
	}
}

/* ---- Tint Doctor and Guided calibration: a test picture with a question or a step ---- */
typedef struct { int pat, chan; const char *title, *text; int yes, no; } Step;
enum { SCRIPT_NONE, SCRIPT_DOCTOR, SCRIPT_GUIDE };
static int sc_step, sc_result = -1, sc_sel, sc_tint;   /* sc_tint: 0 = green, 1 = pink */
/* Tint Doctor: each question has its own answers. next: step number, -1-r = result r, 101 = green, 102 = pink */
typedef struct { int pat; const char *title, *text; int n; const char *lab[3]; int next[3]; } DStep;
static const DStep DOCTOR[] = {
	{ PAT_GREY50, "Tint Doctor  1/4", "What colour does this grey look?", 3, { "Grey", "Green", "Pink" }, { -1, 101, 102 } },
	{ PAT_SMPTE, "Tint Doctor  2/4", "Look past the tint. Is yellow yellow, and magenta pink-purple?", 2, { "Yes", "No, swapped" }, { 2, -2 } },
	{ PAT_STEPS, "Tint Doctor  3/4", "Look at the DARK steps on the left. Are they %s too?", 2, { "Yes, %s", "No, grey" }, { -3, 3 } },
	{ PAT_FIELD, "Tint Doctor  4/4", "Is plain white a little %s?", 2, { "Yes, %s", "No, white" }, { -4, -5 } },
};
static const char *DOC_RES[8][2] = {
	{ "No tint found", "Grey looks right. Games still off? Check the TV's own Colour and Tint first." },
	{ "Colours are swapped, not tinted", "Component: check the red and blue plugs aren't swapped, and the TV input is YPbPr (not RGB). AV: try another cable." },
	{ "Green in the dark parts", "The TV's green CUT-OFF (bias) is too high. Service menu: lower G CUT, or raise R CUT and B CUT, 1-2 steps at a time." },
	{ "Green in the bright parts", "Green DRIVE (gain) is too high. Service menu: lower G DRIVE, or raise R DRIVE and B DRIVE, 1-2 steps at a time." },
	{ "Even green everywhere", "Try the normal Tint / Hue setting first. Still green? Lower G DRIVE and G CUT together, 1 step at a time." },
	{ "Pink in the dark parts", "The TV's green CUT-OFF (bias) is too low. Service menu: raise G CUT, or lower R CUT and B CUT, 1-2 steps at a time." },
	{ "Pink in the bright parts", "Green DRIVE (gain) is too low. Service menu: raise G DRIVE, or lower R DRIVE and B DRIVE, 1-2 steps at a time." },
	{ "Even pink everywhere", "Try the normal Tint / Hue setting first. Still pink? Raise G DRIVE and G CUT together, 1 step at a time." },
};
static const char *tint_word(void) { return sc_tint ? "pink" : "green"; }
static const Step GUIDE[] = {
	{ PAT_PLUGE, 0, "1 Brightness", "Turn BRIGHTNESS up until the darkest bar shows, then down until it just disappears.", 0, 0 },
	{ PAT_WHITECLIP, 0, "2 Contrast", "Raise CONTRAST until the brightest blocks merge into white, then go back until they all show.", 0, 0 },
	{ PAT_CHANNEL, 2, "3 Colour", "Blue only: set COLOUR so the outer bars match the strip underneath.", 0, 0 },
	{ PAT_CHANNEL, 2, "4 Tint / Hue", "Same picture: set TINT so the middle bars match the strip underneath.", 0, 0 },
	{ PAT_SHARP, 0, "5 Sharpness", "Lower SHARPNESS until the white halos around the lines are gone.", 0, 0 },
	{ PAT_GREY50, 0, "6 Grey check", "Should be plain grey. Green or pink? Run the Tint Doctor after this.", 0, 0 },
	{ PAT_GRID, 0, "7 Geometry", "Lines straight and squares square. Fix in the service menu (H/V size, pincushion).", 0, 0 },
	{ PAT_EDGE, 0, "8 Overscan", "Point at each edge you can see and press A. Aim for a little cut off, even on all sides.", 0, 0 },
};
#define N_GUIDE ((int)(sizeof GUIDE / sizeof GUIDE[0]))
void pat_set_chan(int c);

void doctor_start(void) { sc_hidden = 0; script = SCRIPT_DOCTOR; sc_step = 0; sc_result = -1; sc_sel = 0; sc_tint = 0; cal_n = 0; cal_pics = 1; cal_back = 0; ov_open = 0; pat_drawn = -1; go(S_CAL); }
void guide_start(void) { sc_hidden = 0; script = SCRIPT_GUIDE; sc_step = 0; sc_result = -1; sc_sel = 0; cal_n = 0; cal_pics = 1; cal_back = 0; ov_open = 0; pat_drawn = -1; go(S_CAL); }

static void cal_pictures(void)
{
	int NW = plat_native_w(), NH = plat_native_h();
	if (!patbuf) { patbuf = malloc(640 * 576 * 4); outbuf = malloc(640 * 576 * 4); }
	u64 now = plat_ms();
	if (cal_n <= 0) { cal_list[0] = pat; cal_n = 1; cal_pos = 0; }
	u32 d = in->down;
	int prev = pat;
	/* 240p / 288p: the menus are drawn at double height, then squashed, so they look the same on the TV */
	int LH = NH < 400 ? NH * 2 : NH;
	/* on-screen buttons for the script panel: point and press A, or use the D-pad */
	int hy0 = LH - 176, pv = menu_in.ptr_valid, ppx = menu_in.px * NW / SCR_W, ppy = menu_in.py * LH / SCR_H;
	int sbx[3], sbw = 124, sby = hy0 + 112, sbh = 38, sbn = 0, hit = -1;
	if (script) {
		sbn = script == SCRIPT_GUIDE ? 3 : sc_result < 0 ? DOCTOR[sc_step].n : 2;
		for (int k = 0; k < sbn; k++) sbx[k] = NW - 38 - (sbn - k) * (sbw + 8) + 8;
		if (pv && !sc_hidden) for (int k = 0; k < sbn; k++) if (ppx >= sbx[k] && ppx < sbx[k] + sbw && ppy >= sby && ppy < sby + sbh) hit = k;
		/* D-pad Down hides the panel so the whole picture can be seen; Down, Up or A brings it back */
		if (!ov_open) {
			if (d & BTN_DOWN) { sc_hidden ^= 1; sc_hint_until = sc_hidden ? now + 2500 : 0; d &= ~BTN_DOWN; }
			else if (sc_hidden && (d & BTN_UP)) { sc_hidden = 0; d &= ~BTN_UP; }
			else if (sc_hidden && (d & BTN_A) && !pat_grabs_input(script == SCRIPT_DOCTOR ? DOCTOR[sc_step].pat : GUIDE[sc_step].pat)) { sc_hidden = 0; d &= ~BTN_A; }
		}
		/* guide: a click on a button becomes that button's key */
		if (script == SCRIPT_GUIDE && pv && (d & BTN_A) && !ov_open) {
			d &= ~BTN_A;
			d |= hit == 0 ? BTN_1 : hit == 1 ? BTN_MINUS : hit == 2 ? BTN_2 : (pat_grabs_input(pat) ? BTN_A : 0);
		}
	}
	int was_step = sc_step, was_res = sc_result;
	if (script) {
		if (d & BTN_HOME) { if (ov_open) ov_open = 0; else { script = 0; cal_pics = 0; pat_leave(); ui_set_focus(5800); return; } d = 0; }
		if (script == SCRIPT_DOCTOR && !ov_open) {
			/* D-pad or pointer picks an answer, A chooses it, B goes back a step */
			if (d & BTN_LEFT) sc_sel = (sc_sel + sbn - 1) % sbn;
			if (d & BTN_RIGHT) sc_sel = (sc_sel + 1) % sbn;
			if (hit >= 0) sc_sel = hit;
			if (sc_sel >= sbn) sc_sel = 0;
			int act = -1;
			if ((d & BTN_A) && (!pv || hit >= 0)) act = sc_sel;
			if ((d & BTN_2) && sc_result >= 0) act = 0;
			if (d & BTN_B) {
				if (sc_result >= 0) sc_result = -1; else if (sc_step > 0) sc_step--;
				sc_sel = 0; pat_drawn = -1;
			}
			d &= ~(BTN_A | BTN_B | BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN);
			if (act >= 0 && sc_result < 0) {
				int nx = DOCTOR[sc_step].next[act];
				if (nx >= 100) { sc_tint = nx - 101; sc_step = 1; }
				else if (nx < 0) {
					int r = -nx - 1; if (r >= 2 && sc_tint) r += 3;
					sc_result = r;
					char m[64]; snprintf(m, sizeof m, "Tint Doctor: %s", DOC_RES[r][0]); health_add(m);
				} else sc_step = nx;
				sc_sel = 0; pat_drawn = -1;
			} else if (act >= 0) {
				if (act == 0) { sc_step = 0; sc_result = -1; sc_sel = 0; pat_drawn = -1; }
				else { script = 0; cal_pics = 0; pat_leave(); ui_set_focus(5800); return; }
			}
		}
		if (script == SCRIPT_GUIDE) {
			if (d & BTN_1) { if (sc_step > 0) sc_step--; d &= ~BTN_1; pat_drawn = -1; }
			if (d & BTN_2) { if (sc_step < N_GUIDE - 1) sc_step++; else { script = 0; cal_pics = 0; ui_toast("Done! Your values are in Notes"); return; } d &= ~BTN_2; pat_drawn = -1; }
			if ((d & BTN_MINUS) && !ov_open && have_cur) {
				char pre[NOTE_LEN]; snprintf(pre, sizeof pre, "%s: ", GUIDE[sc_step].title + 2);
				for (char *q = pre; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
				kb_open(K_GUIDE_NOTE, pre, "Write down the value", NOTE_LEN - 1);
				return;
			}
			d &= ~BTN_MINUS;
		}
		if (sc_step != was_step || sc_result != was_res) sc_hidden = 0;   /* a new step: show its instructions */
		if (script == SCRIPT_DOCTOR) pat = DOCTOR[sc_step].pat;
		else { pat = GUIDE[sc_step].pat; if (pat == PAT_CHANNEL) pat_set_chan(GUIDE[sc_step].chan); }
		d &= ~(BTN_1 | BTN_2);
	} else {
	if (d & BTN_1) { cal_pos = (cal_pos + cal_n - 1) % cal_n; hud_until = now + 4000; d = 0; }
	if (d & BTN_2) { cal_pos = (cal_pos + 1) % cal_n; hud_until = now + 4000; d = 0; }
	pat = cal_list[cal_pos];
	}
	if (pat != prev) pat_leave();
	if (d & BTN_PLUS) {
		if (!have_cur) ui_toast("Pick a TV on Home first");
		else { ov_open ^= 1; hud_until = now + 3000; }
		d = 0;
	}
	if ((d & BTN_HOME) && !script) {
		pat_leave();
		if (ov_open) { ov_open = 0; d = 0; }
		else { cal_pics = 0; pat_drawn = -1; if (cal_back) { go(S_REMOTE); return; } ui_set_focus(cal_cat < 0 ? 5800 : 5860); return; }
	}
	int ovl[MAX_BTNS], ovn = ov_open ? ov_order(ovl, 12) : 0;
	if (ov_open) {
		/* the service remote: D-pad picks a button, A sends it (hold A to keep sending) */
		if (ovn) {
			if (d & BTN_LEFT) ov_sel = (ov_sel + ovn - 1) % ovn;
			if (d & BTN_RIGHT) ov_sel = (ov_sel + 1) % ovn;
			if (d & BTN_UP) ov_sel = ov_sel >= 6 ? ov_sel - 6 : ov_sel;
			if (d & BTN_DOWN) ov_sel = ov_sel + 6 < ovn ? ov_sel + 6 : ov_sel;
			if (ov_sel >= ovn) ov_sel = ovn - 1;
			if (d & BTN_A) press_btn(ovl[ov_sel], BTN_A);
		}
		if (d & BTN_MINUS) ov_open = 0;
	} else if (script) {
		pat_pointer(menu_in.ptr_valid, menu_in.px * NW / SCR_W, menu_in.py * NH / SCR_H);
		if (pat_grabs_input(pat) && pat_input(pat, d)) pat_drawn = -1;
	} else {
		if (d & BTN_MINUS) { pic_mode ^= 1; mode_until = now + 2500; hud_until = now + 4000; d = 0; }
		pat_pointer(menu_in.ptr_valid, menu_in.px * NW / SCR_W, menu_in.py * NH / SCR_H);
		if (pic_mode || pat_grabs_input(pat)) { if (pat_input(pat, d)) { pat_drawn = -1; hud_until = now + 4000; } }
		else if (have_cur) handle_mapped(&menu_in, 0xFFFF & ~(BTN_1 | BTN_2 | BTN_MINUS | BTN_HOME | BTN_PLUS));
	}

	static int drawn_h;
	if (pat_drawn != pat || drawn_h != NH || pat_animated(pat)) {
		gfx_target(patbuf, NW, NH);
		pat_draw(pat);
		pat_drawn = pat; drawn_h = NH;
	}
	memcpy(outbuf, patbuf, NW * NH * 4);
	static u32 *vbuf;
	if (LH != NH) {
		if (!vbuf) vbuf = malloc(640 * 576 * 4);
		for (int y = 0; y < NH; y++) { memcpy(vbuf + (2 * y) * NW, patbuf + y * NW, NW * 4); memcpy(vbuf + (2 * y + 1) * NW, patbuf + y * NW, NW * 4); }
		gfx_target(vbuf, NW, LH);
	} else gfx_target(outbuf, NW, NH);
	app_native_buf = outbuf;

	const u32 SOFT = RGB(190, 196, 210);
	if (ov_open) {
		/* service remote panel along the bottom: 2 rows of 6 */
		int ph = 112, py = LH - ph - 12;
		gfx_round(12, py, NW - 24, ph, 16, C_HUD, 225);
		text(F_SMALL, 26, py + 4, "Service remote", SOFT, 255);
		text_r(F_SMALL, NW - 26, py + 4, "A send   + / HOME close   1 2 picture", SOFT, 255);
		int bw = (NW - 48) / 6;
		for (int k = 0; k < ovn; k++) {
			int x = 24 + (k % 6) * bw, y = py + 28 + (k / 6) * 40;
			int sel = k == ov_sel;
			gfx_round(x, y, bw - 6, 34, 10, sel ? C_ACCENT : RGB(58, 64, 82), 255);
			text_fit(F_SMALL, x + 8, y + 7, bw - 20, cur.btn[ovl[k]].name, C_WHITE, 255);
		}
		if (!ovn) text(F_BODYB, 26, py + 40, "This remote has no buttons yet", C_WHITE, 255);
	} else if (script && sc_hidden) {
		if (now < sc_hint_until) {   /* a small reminder at the top, then nothing over the picture */
			int a = sc_hint_until - now < 400 ? (int)((sc_hint_until - now) * 220 / 400) : 220;
			const char *m = "Panel hidden. Down, Up or A shows it";
			int w = text_w(F_SMALL, m) + 28;
			gfx_round(NW / 2 - w / 2, 16, w, 28, 14, C_HUD, a);
			text_c(F_SMALL, NW / 2, 19, m, C_WHITE, a);
		}
	} else if (script) {
		int hy = hy0;
		/* the answer may have moved the step on: lay the buttons out again */
		sbn = script == SCRIPT_GUIDE ? 3 : sc_result < 0 ? DOCTOR[sc_step].n : 2;
		for (int k = 0; k < sbn; k++) sbx[k] = NW - 38 - (sbn - k) * (sbw + 8) + 8;
		if (sc_sel >= sbn) sc_sel = 0;
		gfx_round(20, hy, NW - 40, 164, 18, C_HUD, 235);
		text_r(F_SMALL, NW - 38, hy + 10, "Down: hide this panel", SOFT, 255);
		if (script == SCRIPT_DOCTOR && sc_result >= 0) {
			text(F_BODYB, 38, hy + 8, DOC_RES[sc_result][0], C_SEL, 255);
			char l1[96], l2[96]; wrap2(F_SMALL, DOC_RES[sc_result][1], NW - 80, l1, l2);
			text(F_SMALL, 38, hy + 40, l1, C_WHITE, 255); text(F_SMALL, 38, hy + 64, l2, C_WHITE, 255);
			text_fit(F_SMALL, 38, hy + 122, sbx[0] - 48, "B back   Saved in Health log", SOFT, 255);
			static const char *RB[2] = { "Again", "Done" };
			for (int k = 0; k < 2; k++) {
				gfx_round(sbx[k], sby, sbw, sbh, 12, k == sc_sel ? C_ACCENT : RGB(58, 64, 82), 255);
				text_c(F_BODYB, sbx[k] + sbw / 2, sby + 8, RB[k], C_WHITE, 255);
			}
		} else {
			const char *title = script == SCRIPT_DOCTOR ? DOCTOR[sc_step].title : GUIDE[sc_step].title;
			char q[128];
			if (script == SCRIPT_DOCTOR) snprintf(q, sizeof q, DOCTOR[sc_step].text, tint_word());
			else snprintf(q, sizeof q, "%s", GUIDE[sc_step].text);
			text(F_BODYB, 38, hy + 8, title, C_ACCENT, 255);
			char l1[96], l2[96]; wrap2(F_BODYB, q, NW - 80, l1, l2);
			text(F_BODYB, 38, hy + 40, l1, C_WHITE, 255); text(F_BODYB, 38, hy + 66, l2, C_WHITE, 255);
			const char *keys = script == SCRIPT_DOCTOR ? (sc_step ? "B back   + remote   HOME stop" : "+ remote   HOME stop") : "+ remote   HOME stop";
			text_fit(F_SMALL, 38, hy + 122, sbx[0] - 48, keys, SOFT, 255);
			static const char *GB[3] = { "Back (1)", "Write (-)", "Next (2)" };
			for (int k = 0; k < sbn; k++) {
				char lb[32];
				if (script == SCRIPT_DOCTOR) snprintf(lb, sizeof lb, DOCTOR[sc_step].lab[k], tint_word()); else snprintf(lb, sizeof lb, "%s", GB[k]);
				int on = script == SCRIPT_DOCTOR ? k == sc_sel : k == hit;
				gfx_round(sbx[k], sby, sbw, sbh, 12, on ? C_ACCENT : RGB(58, 64, 82), 255);
				text_c(F_BODYB, sbx[k] + sbw / 2, sby + 8, lb, C_WHITE, 255);
			}
		}
	} else if (now < hud_until) {
		int a = hud_until - now < 400 ? (int)((hud_until - now) * 235 / 400) : 235;
		int hy = LH - 150;
		gfx_round(20, hy, NW - 40, 128, 18, C_HUD, a);
		char nm[64]; snprintf(nm, sizeof nm, "%d/%d  %s", cal_pos + 1, cal_n, pat_name(pat));
		hud_text(F_BODYB, 38, hy + 8, nm, C_WHITE, 255);
		const char *opt = pat_option(pat);
		if (opt[0]) text_r(F_BODYB, NW - 38, hy + 8, opt, C_SEL, 255);
		text_fit(F_SMALL, 38, hy + 34, NW - 80, pat_tip(pat), SOFT, 255);
		const char *ctl = pat_controls(pat);
		char line[96];
		int grab = pat_grabs_input(pat);
		if (grab) snprintf(line, sizeof line, "%s", ctl);
		else if (pic_mode) snprintf(line, sizeof line, "PICTURE controls:  %s", ctl[0] ? ctl : "nothing to change here");
		else snprintf(line, sizeof line, "TV controls: your buttons go to the TV   (- = picture controls)");
		text_fit(F_SMALL, 38, hy + 56, NW - 80, line, (pic_mode || grab) ? C_SEL : C_ACCENT, 255);
		int x = 38, y = hy + 88;
		x += badge(x, y, "1", 0) + 4; text(F_SMALL, x, y + 1, "back", SOFT, 255); x += 42;
		x += badge(x, y, "2", 0) + 4; text(F_SMALL, x, y + 1, "next", SOFT, 255); x += 44;
		if (!grab) { x += badge(x, y, "-", 0) + 4; text(F_SMALL, x, y + 1, pic_mode ? "TV" : "picture", SOFT, 255); x += pic_mode ? 30 : 58; }
		if (have_cur) { x += badge(x, y, "+", 0) + 4; text(F_SMALL, x, y + 1, "remote", SOFT, 255); x += 62; }
		x += badge(x, y, "HOME", 0) + 4; text(F_SMALL, x, y + 1, "stop", SOFT, 255);
		text_r(F_SMALL, NW - 38, y + 1, plat_video_mode(), RGB(150, 156, 170), 255);
	}
	if (script && pv && !ov_open && (!sc_hidden || pat_grabs_input(pat))) { gfx_circle(ppx, ppy, 7, C_WHITE, 255); gfx_circle(ppx, ppy, 4, C_ACCENT, 255); }   /* pointer */
	/* a Wii Remote about to run flat: always show it */
	if (!ov_open) batteries(NW - 96, 14, 1);
	if (now < mode_until && !ov_open) {
		const char *m = pic_mode ? "PICTURE controls" : "TV controls";
		int w = text_w(F_BODYB, m) + 30;
		gfx_round(NW / 2 - w / 2, 20, w, 36, 18, pic_mode ? C_SEL : C_ACCENT, 240);
		text_c(F_BODYB, NW / 2, 27, m, C_WHITE, 255);
	}
	if (last_sent_ms && now - last_sent_ms < 1200 && (!pic_mode || ov_open)) {
		char m[32]; snprintf(m, sizeof m, "> %s", last_sent);
		int w = text_w(F_SMALL, m) + 20;
		gfx_round(16, 16, w, 26, 13, C_HUD, 210);
		text(F_SMALL, 26, 18, m, C_WHITE, 255);
	}
	if (LH != NH) {   /* squash the double-height menus back to the real lines */
		for (int y = 0; y < NH; y++) {
			u32 *a0 = vbuf + (2 * y) * NW, *a1 = a0 + NW, *o = outbuf + y * NW;
			for (int x = 0; x < NW; x++) { u32 p0 = a0[x], p1 = a1[x]; o[x] = p0 == p1 ? p0 : (((p0 >> 1) & 0x7F7F7F7F) + ((p1 >> 1) & 0x7F7F7F7F)); }
		}
	}
	gfx_target(NULL, 0, 0);
}

/* service mode from the remote: a test picture with the service remote on top */
void cal_service_start(void)
{
	pat_scan_user();
	int list[16], n = pat_in_cat(CAT_COLOUR, list, 16);
	int g[16], gn = pat_in_cat(CAT_GREY, g, 16);
	for (int k = 0; k < gn && n < 16; k++) list[n++] = g[k];
	cal_cat = -1;
	go(S_CAL);
	pics_open(list, n, 0, 1, 1);
}

static const int CAT_IC[CAT_COUNT] = { IC_PALETTE, IC_STAR, IC_MENU, IC_SEARCH, IC_PLAY, IC_VOLUP, IC_MOON, IC_SAVE };

static void cal_group(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	int all = cal_cat >= CAT_COUNT;
	icon(all ? IC_MENU : CAT_IC[cal_cat], 58, 50, 30, C_ACCENT);
	text_fit(F_TITLE, 86, 28, 440, all ? "All test pictures" : CAT_NAME[cal_cat], C_TEXT, 255);
	text_fit(F_SMALL, 88, 64, 440, all ? "Every built-in picture, plus yours" : CAT_TIP[cal_cat], C_DIM, 255);
	int list[96], n = pat_in_cat(cal_cat, list, 96);
	if (!n) {
		gfx_round(40, 96, 560, 110, 16, C_CARD_HI, 255);
		text(F_BODYB, 60, 110, "No pictures yet", C_TEXT, 255);
		text(F_SMALL, 60, 140, "Put PNG or JPG files in this folder on the SD card:", C_DIM, 255);
		text(F_BODYB, 60, 164, "apps/WiiRremotePlus/pictures", C_ACCENT, 255);
	}
	/* one card per picture: name + what to look for */
	int per = 6;
	static int page;
	int pages = (n + per - 1) / per; if (pages < 1) pages = 1;
	if (page >= pages) page = 0;
	for (int k = page * per; k < n && k < (page + 1) * per; k++) {
		int j = k - page * per;
		int x = 40 + (j % 2) * 284, y = 90 + (j / 2) * 94;
		int f = ui_item(5860 + j, x, y, 276, 88);
		gfx_round(x, y, 276, 88, 14, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, 255);
		gfx_round_line(x, y, 276, 88, 14, 1, C_LINE, 255);
		text_fit(F_BODYB, x + 14, y + 6, 250, pat_name(list[k]), C_TEXT, 255);
		/* the tip, wrapped (up to three lines) */
		{ const char *t = pat_tip(list[k]); int ly = y + 30;
		  for (int ln = 0; ln < 3 && *t; ln++) {
			char l1[128]; int cut = 0, n = strlen(t);
			for (int e = 0; e <= n && e < 127; e++) if (t[e] == ' ' || !t[e]) { memcpy(l1, t, e); l1[e] = 0; if (text_w(F_SMALL, l1) <= 250) cut = e; else break; }
			if (!cut) cut = n < 127 ? n : 127;
			memcpy(l1, t, cut); l1[cut] = 0;
			if (ln == 2 && t[cut]) text_fit(F_SMALL, x + 14, ly, 250, t, C_DIM, 255); else text(F_SMALL, x + 14, ly, l1, C_DIM, 255);
			t += cut; while (*t == ' ') t++; ly += 18;
		  } }
		if (f & UI_FOCUS) ui_focus_ring(x, y, 276, 88, 14);
		if (f & UI_CLICK) { pics_open(list, n, k, 0, 0); return; }
	}
	if (pages > 1) {
		if (ui_ibutton(5890, 40, 374, 120, 40, IC_LEFT, "Page", ST_NORMAL) & UI_CLICK) page = (page + pages - 1) % pages;
		char pg[16]; snprintf(pg, sizeof pg, "%d / %d", page + 1, pages);
		text_c(F_BODYB, 320, 382, pg, C_TEXT, 255);
		if (ui_ibutton(5891, 480, 374, 120, 40, IC_RIGHT, "Page", ST_NORMAL) & UI_CLICK) page = (page + 1) % pages;
	}
	ui_footer_begin();
	ui_footer_hint("A", "Show");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Pick a picture. 1 and 2 step through this group.",
		"+ shows your TV's service remote on top of the picture.",
		"- swaps between TV controls and picture controls.",
	};
	ui_help(all ? "All test pictures" : CAT_NAME[cal_cat], help, 3);
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) { cal_cat = -1; ui_set_focus(5820); if (cal_group_back) { cal_group_back = 0; in->down = 0; go(S_CALFLOW); } }
}

#if 0   /* the v1.9 front page (replaced below) */
void scr_cal(void)
{
	if (cal_pics) { cal_pictures(); return; }
	if (cal_cat >= 0) { cal_group(); return; }

	int blocked = help_is_open() || cal_svc_open;
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_PALETTE, "Calibration Suite", "Picture and sound tests for your TV");
	static const char *SHORT[CAT_COUNT] = { "Colour", "Grey levels", "Geometry", "Sharpness", "Motion & lag", "Sound", "Screen care", "Your pictures" };
	/* the eight groups: big and easy */
	int focus_tip = -1;
	for (int k = 0; k < CAT_COUNT; k++) {
		int x = 40 + (k % 4) * 142, y = 90 + (k / 4) * 104;
		int f = ui_tile(5800 + k, x, y, 134, 96, CAT_IC[k], SHORT[k], ST_NORMAL, 0);
		if (f & UI_FOCUS) focus_tip = k;
		if (f & UI_CLICK) { cal_cat = k; ui_set_focus(5860); return; }
	}
	/* tools: smaller */
	int y = 304, tw = 75, g = 6;
	struct { int ic; const char *l; } T[7] = { { IC_PLAY, "Guided" }, { IC_PALETTE, "Tint Dr" }, { IC_WRENCH, "Service" }, { IC_GAMEPAD, "Consoles" },
		{ IC_TV, "Video" }, { IC_NOTE, "Notes" }, { IC_STAR, "Health" } };
	for (int k = 0; k < 7; k++) {
		int f = ui_tile(5810 + k, 40 + k * (tw + g), y, tw, 56, T[k].ic, T[k].l, ST_NORMAL, 0);
		if (f & UI_FOCUS) focus_tip = 100 + k;
		if (!(f & UI_CLICK)) continue;
		int need = k == 2 || k == 3 || k == 5;
		if (need && !have_cur) { ui_toast("Pick a TV on Home first"); continue; }
		switch (k) {
		case 0: guide_start(); return;
		case 1: doctor_start(); return;
		case 2: cal_svc_open = 1; break;
		case 3: go(S_CONSOLES); return;
		case 4: go(S_VIDEO); return;
		case 5: go(S_NOTES); return;
		case 6: health_start(); return;
		}
	}
	/* one line about whatever is picked */
	static const char *TT[7] = { "Step by step: brightness, contrast, colour, tint, geometry", "Finds out why the picture is green (or pink)",
		"Service menu, service remote, safety net", "One tap per games console", "Output mode, 50/60Hz, flicker filter", "Write your settings down", "Dated results for your CRT" };
	const char *tip = focus_tip >= 100 ? TT[focus_tip - 100] : focus_tip >= 0 ? CAT_TIP[focus_tip] : have_cur ? cur.name : "No TV remote open: tests work, the service remote needs one";
	text_fit(F_SMALL, 40, 372, 400, tip, C_DIM, 255);
	text_r(F_SMALL, 600, 372, plat_video_mode(), C_ACCENT, 255);

	ui_footer_begin();
	ui_footer_hint("A", "Open");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Pick a group of tests. Each one says what to look for.",
		"Green tint? Colour > Colour channel check and 50% grey.",
		"Service: opens your TV's service menu, then shows tests with the service remote on top.",
		"In a picture: 1 / 2 = back / next, + = service remote, HOME = back.",
		"Consoles: one tap switches the TV for each games console.",
		"Write down every setting before you change it (Notes).",
	};
	if (!cal_svc_open) ui_help("Calibration Suite", help, 6);
	static const char *svc[] = { "Send SERVICE, then tests", "Tests + service remote", "Service values (safety net)", "Service menu presets", "Cancel" };
	static const int svci[] = { IC_WRENCH, IC_REMOTE, IC_NOTE, IC_MENU, IC_CLOSE };
	int ch = menu_popup(&cal_svc_open, svc, svci, 5, "Service menu");
	if (ch == 2) { cal_svc_open = 0; go(S_SVALS); return; }
	if (ch == 3) ch = 2;
	if (ch == 0) { cal_svc_open = 0; if (service_btn() >= 0) press_btn(service_btn(), 0); else ui_toast("No SERVICE button yet: try the presets"); cal_service_start(); cal_back = 0; return; }
	if (ch == 1) { cal_svc_open = 0; cal_service_start(); cal_back = 0; return; }
	if (ch == 2) { cal_svc_open = 0; go(S_PRESETS); return; }
	if (!blocked && !cal_svc_open && (in->down & (BTN_HOME | BTN_B))) go_back();
}
#endif

/* ---- choosing the TV remote when you come into the suite ---- */
static int cal_rpick, cal_rn;
static char cal_rfiles[14][40], cal_rnames[14][NAME_LEN];
static void cal_remote_pick_open(void)
{
	static char f[MAX_REMOTES][40], nm[MAX_REMOTES][NAME_LEN];
	int n = remotes_list(f, nm, MAX_REMOTES);
	cal_rn = 0;
	for (int i = 0; i < n && cal_rn < 13; i++) { memcpy(cal_rfiles[cal_rn], f[i], 40); memcpy(cal_rnames[cal_rn], nm[i], NAME_LEN); cal_rn++; }
	cal_rpick = cal_rn > 0;
	if (cal_rpick) ui_set_focus(9000);
}
static void cal_remote_pick(void)
{
	if (!cal_rpick) return;
	const char *items[14]; int ic[14];
	for (int i = 0; i < cal_rn; i++) { items[i] = cal_rnames[i]; ic[i] = (have_cur && !strcmp(cfg.last, cal_rfiles[i])) ? IC_CHECK : IC_REMOTE; }
	items[cal_rn] = "No remote"; ic[cal_rn] = IC_CLOSE;
	int c = menu_popup(&cal_rpick, items, ic, cal_rn + 1, "Which TV remote do you have?");
	if (c >= 0 && c < cal_rn) {
		if (remote_load(&cur, cal_rfiles[c])) { have_cur = 1; snprintf(cfg.last, sizeof cfg.last, "%s", cal_rfiles[c]); config_save(); ui_toast("Press + any time for your TV remote"); }
		else ui_toast("Can't open that remote");
		ui_set_focus(5820);
	} else if (c == cal_rn) { have_cur = 0; ui_toast("OK: change settings with your own remote"); ui_set_focus(5820); }
	in->down = 0;
}

void scr_cal(void)
{
	if (cal_pics) { cal_pictures(); return; }
	if (cal_cat >= 0) { cal_group(); return; }

	int blocked = help_is_open() || cal_svc_open || cal_rpick || calui_remote_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_PALETTE, 58, 50, 30, C_ACCENT);
	text_fit(F_TITLE, 86, 28, 440, "Calibration Suite", C_TEXT, 255);
	text_fit(F_SMALL, 88, 64, 440, have_cur ? cur.name : "No TV remote chosen", C_DIM, 255);
	int focus_tip = -1;

	static const int AIC[4] = { IC_PALETTE, IC_STAR, IC_MENU, IC_SEARCH };
	/* the guided hub */
	gfx_round(34, 94, 572, 142, 18, C_CARD_HI, 255);
	gfx_round_line(34, 94, 572, 142, 18, 1, C_LINE, 255);
	text(F_SMALL, 50, 100, "GUIDED CALIBRATION", C_ACCENT, 255);
	{
		int f = ui_tile(5820, 46, 122, 170, 104, IC_PLAY, "Full check-up", ST_PRIMARY, 0);
		if (f & UI_FOCUS) focus_tip = 300;
		if ((f & UI_CLICK) && !blocked) { calui_start(AR_ALL); return; }
	}
	static const char *ANAME[4] = { "Colour", "Grey levels", "Geometry", "Sharpness" };
	for (int a = 0; a < 4; a++) {
		int x = 224 + (a % 2) * 190, y = 122 + (a / 2) * 54;
		int f = ui_ibutton(5821 + a, x, y, 182, 48, AIC[a], ANAME[a], ST_NORMAL);
		if (f & UI_FOCUS) focus_tip = 200 + a;
		if ((f & UI_CLICK) && !blocked) { calui_start(a); return; }
	}

	/* more test pictures (the colour, grey, geometry and sharpness ones are inside each area) */
	text(F_SMALL, 40, 244, "MORE TEST PICTURES", C_DIM, 255);
	static const int MORE[5] = { 4, 5, 6, 7, CAT_COUNT };
	static const char *MNAME[5] = { "Motion", "Sound", "Care", "Mine", "All" };
	for (int k = 0; k < 5; k++) {
		int f = ui_tile(5804 + k, 40 + k * 113, 262, 108, 56, k == 4 ? IC_MENU : CAT_IC[MORE[k]], MNAME[k], ST_NORMAL, 0);
		if (f & UI_FOCUS) focus_tip = k == 4 ? 400 : MORE[k];
		if ((f & UI_CLICK) && !blocked) { cal_cat = MORE[k]; ui_set_focus(5860); return; }
	}

	/* tools */
	struct { int ic; const char *l; } T[4] = { { IC_WRENCH, "Service" }, { IC_REMOTE, "Remote" }, { IC_TV, "Video" }, { IC_STAR, "Health" } };
	for (int k = 0; k < 4; k++) {
		int f = ui_ibutton(5810 + k, 40 + k * 141, 326, 137, 42, T[k].ic, T[k].l, ST_NORMAL);
		if (f & UI_FOCUS) focus_tip = 100 + k;
		if (!(f & UI_CLICK) || blocked) continue;
		if (k == 0 && !have_cur) { ui_toast("Pick your TV remote first"); cal_remote_pick_open(); continue; }
		switch (k) {
		case 0: cal_svc_open = 1; break;
		case 1: cal_remote_pick_open(); if (!cal_rpick) ui_toast("No remotes yet: make one from Home first"); break;
		case 2: go(S_VIDEO); return;
		case 3: health_start(); return;
		}
	}
	static const char *TT[4] = { "Service menu, service remote, service codes", "Choose which TV remote the suite uses",
		"Output mode, 50/60Hz, flicker filter", "Dated results for your CRT" };
	static const char *QT[4] = { "Colour: RGB, colour level, tint, colour faults", "Grey levels: black, white and grey without a tint",
		"Geometry: size, position, straight lines, overscan", "Sharpness: focus, halos, detail, fringes" };
	const char *tip = focus_tip == 400 ? "Every test picture in one list (40 built in, plus yours)" : focus_tip >= 300 ? "All four areas: it finds what's wrong and fixes it with you" : focus_tip >= 200 ? QT[focus_tip - 200] : focus_tip >= 100 ? TT[focus_tip - 100] : focus_tip >= 0 ? CAT_TIP[focus_tip] : have_cur ? "Press + any time for your TV remote" : "No TV remote: use your own to change settings";
	text_fit(F_SMALL, 40, 382, 420, tip, C_DIM, 255);
	text_r(F_SMALL, 600, 382, plat_video_mode(), C_ACCENT, 255);

	ui_footer_begin();
	ui_footer_hint("A", "Open");
	ui_footer_hint("+", "TV remote");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Pick an area (or Full check-up), then Guided, Intense or Ultra handhold.",
		"Each area also has its test pictures to look through on your own.",
		"+ shows your TV remote anywhere in the suite. - hides everything over a test picture.",
		"In a guided step: Left = problems & fixes, Right = the settings for that step.",
		"Service menus can do damage: write every number down first (Ultra handhold saves them).",
		"More test pictures: motion, sound, screen care and your own. 1 / 2 = back / next.",
	};
	if (!cal_svc_open && !cal_rpick && !calui_remote_is_open()) ui_help("Calibration Suite", help, 6);
	static const char *svc[] = { "Send SERVICE, then tests", "Tests + service remote", "Your service numbers", "Service menu codes", "Cancel" };
	static const int svci[] = { IC_WRENCH, IC_REMOTE, IC_NOTE, IC_MENU, IC_CLOSE };
	int ch = menu_popup(&cal_svc_open, svc, svci, 5, "Service: write numbers down first");
	if (ch == 2) { cal_svc_open = 0; go(S_SVALS); return; }
	if (ch == 3) ch = 2;
	if (ch == 0) { cal_svc_open = 0; if (service_btn() >= 0) press_btn(service_btn(), 0); else ui_toast("No SERVICE button yet: try the codes"); cal_service_start(); cal_back = 0; return; }
	if (ch == 1) { cal_svc_open = 0; cal_service_start(); cal_back = 0; return; }
	if (ch == 2) { cal_svc_open = 0; go(S_PRESETS); return; }
	cal_remote_pick();
	if (!cal_svc_open && !cal_rpick && !help_is_open()) {
		ui_suspend(0);
		calui_remote_overlay(SCR_W, SCR_H - 52, menu_in.ptr_valid, menu_in.px, menu_in.py);
	}
	if (!blocked && !cal_svc_open && !cal_rpick && (in->down & (BTN_HOME | BTN_B))) go_back();
}

/* ================================================================== */
/* code scanner                                                     */
/* ================================================================== */

static const char *SCAN_PNAME[P_COUNT] = { "NEC", "Samsung", "RC5", "Sony", "JVC", "Panasonic", "Sharp", "RCA", "Mitsubishi", "NEC ext", "RC6", "Sony 15", "RCA 38k" };
static u8 nec_order[256], jvc_order[256], pana_order[256], rc5_order[32], sony_order[32];
static int orders_built;
static const u8 NEC_PRIORITY[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x18, 0x20, 0x40, 0x50, 0x80, 0x7F, 0x86, 0xFF
};
static int sc_p = P_NEC, sc_speed = 1, sc_run, sc_done, sc_worked_open;
static u32 sc_idx;
static u64 sc_next;
static u32 sc_hist[6]; static int sc_hn;
static const int SC_GAP[5] = { 25, 60, 150, 300, 600 };
static const char *SC_SPEED[5] = { "Very fast", "Fast", "Normal", "Slow", "Very slow" };

int scan_running(void) { return screen == S_SCAN && sc_run; }

static void build_orders(void)
{
	u8 used[256] = { 0 }; int n = 0;
	for (size_t i = 0; i < sizeof NEC_PRIORITY; i++) { nec_order[n++] = NEC_PRIORITY[i]; used[NEC_PRIORITY[i]] = 1; }
	for (int a = 0; a < 256; a++) if (!used[a]) nec_order[n++] = a;
	/* JVC TVs: 3, 15, 31, 67 first.  Panasonic TVs: 0x80 first */
	static const u8 JP[] = { 3, 15, 31, 67 }, PP[] = { 0x80, 0x81, 0x90, 0xA0 };
	memset(used, 0, sizeof used); n = 0;
	for (size_t i = 0; i < sizeof JP; i++) { jvc_order[n++] = JP[i]; used[JP[i]] = 1; }
	for (int a = 0; a < 256; a++) if (!used[a]) jvc_order[n++] = a;
	memset(used, 0, sizeof used); n = 0;
	for (size_t i = 0; i < sizeof PP; i++) { pana_order[n++] = PP[i]; used[PP[i]] = 1; }
	for (int a = 0; a < 256; a++) if (!used[a]) pana_order[n++] = a;
	for (int a = 0; a < 32; a++) rc5_order[a] = a;
	sony_order[0] = 1; n = 1;
	for (int a = 0; a < 32; a++) if (a != 1) sony_order[n++] = a;
	orders_built = 1;
}

#define PASS1 64
/* small protocols: every command of address 0, then address 1... (Sony and RC5 start with TV addresses) */
static int sc_small(void) { return sc_p == P_RC5 || sc_p == P_SONY || sc_p == P_SHARP || sc_p == P_RCA; }
static int sc_ncmd(void) { return (sc_p == P_RC5 || sc_p == P_SONY) ? 128 : 256; }
static int sc_naddr(void) { return sc_p == P_RCA ? 16 : 32; }

/* ---- "Address first" (Settings in Find codes): step 1 sends a few safe, visible buttons (Mute,
   Volume, Menu) at every address, most common TVs first. When the TV reacts, step 2 tries every
   code on just that address: for NEC that's 256 codes instead of 65,536. ---- */
#define AF_PROBES 4
#define AF_MAX 320
static u8 af_addr[AF_MAX], af_probe[AF_MAX][AF_PROBES];
static int af_n, af_built_p = -1, sc_locked, sc_lock_addr;
static void af_build(void)
{
	smart_has_group(0, 0);   /* makes sure the library is loaded */
	af_n = 0; af_built_p = sc_p;
	int amax = sc_small() ? sc_naddr() : 256;
	static const int WANT[AF_PROBES] = { FN_MUTE, FN_VOLUP, FN_VOLDN, FN_MENU };
	/* the codes these buttons most often have, across every TV of this type */
	static u16 cnt[AF_PROBES][256]; memset(cnt, 0, sizeof cnt);
	static float aw[256]; memset(aw, 0, sizeof aw);
	for (int i = 0; i < PRESETS_DB_N; i++) {
		const PSet *ps = &PRESETS_DB[i];
		if (ps->proto != sc_p || ps->addr >= amax) continue;
		aw[ps->addr] += ps->seen ? ps->seen : 1;
		for (int k = 0; k < ps->n; k++) for (int w = 0; w < AF_PROBES; w++)
			if (PRESET_CODES[ps->off + k].fid == WANT[w]) cnt[w][PRESET_CODES[ps->off + k].cmd] += ps->seen ? ps->seen : 1;
	}
	u8 common[AF_PROBES];
	for (int w = 0; w < AF_PROBES; w++) { int b = 0; for (int c = 1; c < 256; c++) if (cnt[w][c] > cnt[w][b]) b = c; common[w] = (u8)b; }
	/* addresses: the ones TVs use most, then every other one */
	int used[256] = { 0 };
	for (;;) {
		int b = -1; for (int a = 0; a < amax; a++) if (!used[a] && aw[a] > 0 && (b < 0 || aw[a] > aw[b])) b = a;
		if (b < 0 || af_n >= AF_MAX) break;
		used[b] = 1; af_addr[af_n++] = (u8)b;
	}
	for (int a = 0; a < amax && af_n < AF_MAX; a++) if (!used[a]) af_addr[af_n++] = (u8)a;
	/* the probes: this address's own Mute / Volume / Menu if the library knows them */
	for (int k = 0; k < af_n; k++) {
		int best = -1;
		for (int i = 0; i < PRESETS_DB_N; i++) if (PRESETS_DB[i].proto == sc_p && PRESETS_DB[i].addr == af_addr[k] && (best < 0 || PRESETS_DB[i].seen > PRESETS_DB[best].seen)) best = i;
		int n = 0;
		for (int w = 0; w < AF_PROBES; w++) {
			int c = -1;
			if (best >= 0) for (int q = 0; q < PRESETS_DB[best].n; q++) if (PRESET_CODES[PRESETS_DB[best].off + q].fid == WANT[w]) c = PRESET_CODES[PRESETS_DB[best].off + q].cmd;
			if (c < 0) c = common[w];
			if (c >= sc_ncmd()) c &= sc_ncmd() - 1;
			int dup = 0; for (int j = 0; j < n; j++) dup |= af_probe[k][j] == c;
			if (!dup) af_probe[k][n++] = (u8)c;
		}
		for (int c = 0; n < AF_PROBES; c++) { int dup = 0; for (int j = 0; j < n; j++) dup |= af_probe[k][j] == c; if (!dup) af_probe[k][n++] = (u8)c; }
	}
}
static int sc_af(void) { return cfg.addr_first; }
static u32 sc_total(void)
{
	if (sc_af()) { if (sc_locked) return sc_ncmd(); if (af_built_p != sc_p) af_build(); return (u32)af_n * AF_PROBES; }
	return sc_small() ? (u32)sc_naddr() * sc_ncmd() : 65536;
}

static void sc_code(u32 idx, u8 *a, u8 *c, int *pass)
{
	if (sc_af()) {
		if (sc_locked) { *a = (u8)sc_lock_addr; *c = (u8)(idx % sc_ncmd()); *pass = 2; return; }
		if (af_built_p != sc_p) af_build();
		u32 k = idx / AF_PROBES; if (k >= (u32)af_n) k = af_n - 1;
		*a = af_addr[k]; *c = af_probe[k][idx % AF_PROBES]; *pass = 1; return;
	}
	if (sc_small()) {
		u32 ai = idx / sc_ncmd();
		*a = sc_p == P_RC5 ? rc5_order[ai] : sc_p == P_SONY ? sony_order[ai] : (u8)ai;
		*c = idx % sc_ncmd(); *pass = 1; return;
	}
	const u8 *ord = sc_p == P_JVC ? jvc_order : sc_p == P_PANA ? pana_order : nec_order;
	if (idx < 256 * PASS1) { *a = ord[idx / PASS1]; *c = idx % PASS1; *pass = 1; }
	else { u32 j = idx - 256 * PASS1; *a = ord[j / (256 - PASS1)]; *c = PASS1 + j % (256 - PASS1); *pass = 2; }
}

static u32 sc_block(u32 idx)
{
	if (sc_small()) return idx - idx % sc_ncmd();
	if (idx < 256 * PASS1) return idx - idx % PASS1;
	u32 j = idx - 256 * PASS1;
	return 256 * PASS1 + j - j % (256 - PASS1);
}
static u32 sc_bsize(u32 idx) { return sc_small() ? (u32)sc_ncmd() : (idx < 256 * PASS1 ? PASS1 : 256 - PASS1); }

static void sc_push(u32 idx)
{
	if (sc_hn && sc_hist[0] == idx) return;
	for (int i = 5; i > 0; i--) sc_hist[i] = sc_hist[i - 1];
	sc_hist[0] = idx; if (sc_hn < 6) sc_hn++;
}

static void sc_send(u32 idx)
{
	u8 a, c; int ps;
	sc_code(idx, &a, &c, &ps);
	ir_send(sc_p, a, c);
	sc_push(idx);
	{ static u8 gp = 0xFF; static u16 ga; static int has;   /* only codes the library knows are worth remembering */
	  if (gp != sc_p || ga != a) { gp = sc_p; ga = a; has = smart_has_group(sc_p, a); }
	  if (has) smart_note(sc_p, a, c, R_NONE, 0.05f); }
}

/* ---- group test for Find codes: send a few codes, ask once, halve the group if yes ---- */
static u32 sg_idx[16]; static int sg_n, sg_lo, sg_hi, sg_plo, sg_phi, sg_known, sg_si, sg_state;   /* state: 0 idle, 1 sending, 2 asking */
static u64 sg_next;
static int sc_saved(u32 idx);
static void sg_send_window(void) { sg_si = sg_lo; sg_state = 1; sg_next = 0; }
static void sg_start(u32 tot)
{
	sg_n = 0;
	int size = cfg.group_size < 2 ? 2 : cfg.group_size > 16 ? 16 : cfg.group_size;
	while (sc_idx < tot && sg_n < size) { if (!sc_saved(sc_idx)) sg_idx[sg_n++] = sc_idx; sc_idx++; }
	if (!sg_n) { sg_state = 0; sc_done = 1; return; }
	sg_lo = 0; sg_hi = sg_n; sg_known = 0;
	sg_send_window();
}
static void sg_answer(int yes, u32 tot);
static void scan_save(void)
{
	char p[128]; snprintf(p, sizeof p, "%s/scan_progress.cfg", data_dir);
	FILE *f = fopen(p, "w");
	if (!f) return;
	fprintf(f, "proto=%d\nidx=%u\nspeed=%d\nlocked=%d\nlock_addr=%d\n", sc_p, (unsigned)sc_idx, sc_speed, sc_locked, sc_lock_addr);
	fclose(f);
}

static void sg_answer(int yes, u32 tot)
{
	if (yes) {
		if (sg_hi - sg_lo == 1) { sc_send(sg_idx[sg_lo]); sg_state = 99; return; }   /* found: It worked! */
		sg_plo = sg_lo; sg_phi = sg_hi; sg_known = 1;
		sg_hi = sg_lo + (sg_hi - sg_lo) / 2;   /* try the first half */
		sg_send_window(); return;
	}
	if (!sg_known) { scan_save(); sg_start(tot); return; }   /* the whole group did nothing: next group */
	sg_plo = sg_hi;                                          /* it was the other half */
	if (sg_phi - sg_plo == 1) { sc_send(sg_idx[sg_plo]); sg_state = 99; return; }
	sg_lo = sg_plo; sg_hi = sg_plo + (sg_phi - sg_plo) / 2;
	sg_send_window();
}

static void scan_load(void)
{
	char p[128], line[64]; snprintf(p, sizeof p, "%s/scan_progress.cfg", data_dir);
	FILE *f = fopen(p, "r");
	if (!f) return;
	while (fgets(line, sizeof line, f)) {
		if (!strncmp(line, "proto=", 6)) sc_p = atoi(line + 6) % P_SCANNABLE;
		else if (!strncmp(line, "idx=", 4)) sc_idx = strtoul(line + 4, NULL, 10);
		else if (!strncmp(line, "speed=", 6)) sc_speed = atoi(line + 6) % 5;
		else if (!strncmp(line, "locked=", 7)) sc_locked = atoi(line + 7) != 0;
		else if (!strncmp(line, "lock_addr=", 10)) sc_lock_addr = atoi(line + 10) & 0xFF;
	}
	fclose(f);
	if (sc_idx) { char m[48]; snprintf(m, sizeof m, "Resumed at code %u", (unsigned)sc_idx); ui_toast(m); }
}

/* skip codes that are already buttons on the open remote */
static int sc_saved(u32 idx)
{
	if (!have_cur) return 0;
	u8 a, c; int ps; sc_code(idx, &a, &c, &ps);
	for (int i = 0; i < cur.nbtn; i++)
		if (!cur.btn[i].nsteps && cur.btn[i].proto == sc_p && cur.btn[i].addr == a && cur.btn[i].cmd == c) return 1;
	return 0;
}

void scan_start(void)
{
	static int loaded;
	if (!orders_built) build_orders();
	if (!loaded) { loaded = 1; scan_load(); }
	go(S_SCAN);
}

void scr_scan(void)
{
	if (!orders_built) build_orders();
	int blocked = help_is_open() || sc_worked_open;
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_RADAR, "Find codes", "For a TV with no remote");

	u32 tot = sc_total();
	u32 shown = sc_hn ? sc_hist[0] : (sc_idx < tot ? sc_idx : tot - 1);
	u8 a, c; int pass;
	sc_code(shown, &a, &c, &pass);

	/* status pill */
	const char *st = sc_done ? "DONE" : (sc_run ? "SCANNING" : "PAUSED");
	u32 stc = sc_done ? C_ACCENT2 : (sc_run ? C_GOOD : C_SEL);
	gfx_round(390, 34, 140, 36, 18, stc, 255);
	text_c(F_BODYB, 460, 40, st, C_WHITE, 255);

	/* three big boxes: type / address / code */
	int fbt = ui_item(5900, 40, 94, 150, 100);
	gfx_shadow(40, 94, 150, 100, 16);
	gfx_round_grad(40, 94, 150, 100, 16, (fbt & UI_HOVER) ? C_FOCUS_TOP : C_CARD_HI, C_CARD, 255);
	text_c(F_SMALL, 115, 102, "TYPE", C_DIM, 255);
	text_c(F_BTN, 115, 130, SCAN_PNAME[sc_p], C_TEXT, 255);
	if (fbt & UI_FOCUS) ui_focus_ring(40, 94, 150, 100, 16);
	char v[8];
	gfx_shadow(200, 94, 130, 100, 16);
	gfx_round_grad(200, 94, 130, 100, 16, C_CARD_HI, C_CARD, 255);
	text_c(F_SMALL, 265, 102, "ADDRESS", C_DIM, 255);
	snprintf(v, sizeof v, "%02X", a); text_c(F_BIG, 265, 122, v, C_TEXT, 255);
	gfx_shadow(340, 94, 130, 100, 16);
	gfx_round_grad(340, 94, 130, 100, 16, C_CARD_HI, C_CARD, 255);
	text_c(F_SMALL, 405, 102, "CODE", C_DIM, 255);
	snprintf(v, sizeof v, "%02X", c); text_c(F_BIG, 405, 122, v, C_TEXT, 255);
	/* last 5 codes sent: pick one to send it again and go back to it */
	gfx_round(480, 94, 120, 172, 16, C_INSET, 255);
	text_c(F_SMALL, 540, 100, "Last 5 sent", C_DIM, 255);
	int hist_pick = -1;
	for (int i = 0; i < sc_hn && i < 5; i++) {
		u8 ha, hc; int hp; sc_code(sc_hist[i], &ha, &hc, &hp);
		int hy = 122 + i * 28;
		int fh = ui_item(5910 + i, 486, hy, 108, 26);
		if (fh & UI_FOCUS) gfx_round(486, hy, 108, 26, 8, C_FOCUS_TOP, 255);
		char h[16]; snprintf(h, sizeof h, "%02X  %02X", ha, hc);
		text_c(F_BODYB, 540, hy + 2, h, i ? C_DIM : C_TEXT, 255);
		if (fh & UI_CLICK) hist_pick = i;
	}
	/* progress */
	int pw = 430;
	gfx_round(40, 206, pw, 16, 8, C_INSET, 255);
	int fill = (int)((u64)(sc_idx > tot ? tot : sc_idx) * pw / tot);
	if (fill > 0) gfx_round(40, 206, fill < 16 ? 16 : fill, 16, 8, C_ACCENT, 255);
	char pr[64];
	if (sc_af() && !sc_locked) snprintf(pr, sizeof pr, "Address %u of %u  (step 1: Mute / Volume)", (unsigned)(sc_idx / AF_PROBES + 1 > (u32)af_n ? (u32)af_n : sc_idx / AF_PROBES + 1), (unsigned)af_n);
	else if (sc_af()) snprintf(pr, sizeof pr, "%u of %u  (step 2: address %02X)", (unsigned)(sc_idx < tot ? sc_idx : tot), (unsigned)tot, sc_lock_addr);
	else snprintf(pr, sizeof pr, "%u of %u  (step %d)", (unsigned)(sc_idx < tot ? sc_idx : tot), (unsigned)tot, pass);
	text(F_SMALL, 40, 228, pr, C_DIM, 255);
	int fsp = ui_item(5905, 330, 226, 140, 26);
	char spd[24]; snprintf(spd, sizeof spd, "Speed: %s", SC_SPEED[sc_speed]);
	text_r(F_SMALL, 470, 228, spd, (fsp & UI_FOCUS) ? C_ACCENT : C_TEXT, 255);

	/* buttons */
	int y = 272;
	int grp = cfg.group_test, f_go = 0, f_bk = 0, f_nx = 0, f_ok = 0, f_yes = 0, f_no = 0;
	if (grp && sg_state == 2) {
		/* group test: one question for the whole group */
		text_fit(F_BODYB, 40, y + 4, 300, sg_known ? "Did THESE do anything?" : "Did the TV do anything?", C_TEXT, 255);
		char gl[64] = ""; for (int i = sg_lo; i < sg_hi; i++) { u8 ga, gc; int gp; sc_code(sg_idx[i], &ga, &gc, &gp); char t[10]; snprintf(t, sizeof t, "%02X/%02X ", ga, gc); if (strlen(gl) + strlen(t) < sizeof gl) strcat(gl, t); }
		text_fit(F_SMALL, 40, y + 34, 300, gl, C_DIM, 255);
		f_yes = ui_tile(5902, 350, y, 120, 80, IC_CHECK, "Yes", ST_PRIMARY, 0);
		f_no = ui_tile(5903, 480, y, 120, 80, IC_CLOSE, "No", ST_NORMAL, 0);
	} else if (grp) {
		char gs[40]; snprintf(gs, sizeof gs, sg_state == 1 ? "Sending %d of %d..." : "Test the next %d", sg_state == 1 ? (sg_si - sg_lo < sg_hi - sg_lo ? sg_si - sg_lo + 1 : sg_hi - sg_lo) : cfg.group_size, sg_hi - sg_lo);
		f_go = ui_tile(5901, 40, y, 300, 80, IC_PLAY, gs, ST_PRIMARY, 0);
		f_ok = ui_tile(5904, 350, y, 250, 80, IC_STAR, "It worked!", ST_NORMAL, 0);
	} else {
		f_go = ui_tile(5901, 40, y, 150, 80, sc_run ? IC_PAUSE : IC_PLAY, sc_run ? "Pause" : "Start", ST_PRIMARY, 0);
		f_bk = ui_tile(5902, 200, y, 90, 80, IC_LEFT, "Back", ST_NORMAL, 0);
		f_nx = ui_tile(5903, 300, y, 90, 80, IC_RIGHT, "Next", ST_NORMAL, 0);
		f_ok = ui_tile(5904, 400, y, 200, 80, IC_STAR, "It worked!", ST_NORMAL, 0);
	}

	/* the switches: Address first, Group test, and a way out of step 2 */
	int locked_ui = sc_af() && sc_locked;
	int f_af = ui_ibutton(5906, 40, 360, locked_ui ? 180 : 275, 32, sc_af() ? IC_CHECK : IC_CLOSE, sc_af() ? "Address first" : "Address first: Off", sc_af() ? ST_PRIMARY : ST_NORMAL);
	int f_gt = ui_ibutton(5908, locked_ui ? 228 : 325, 360, locked_ui ? 180 : 275, 32, grp ? IC_CHECK : IC_CLOSE, grp ? "Group test" : "Group test: Off", grp ? ST_PRIMARY : ST_NORMAL);
	int f_ul = locked_ui ? ui_ibutton(5907, 416, 360, 184, 32, IC_SEARCH, "New address", ST_NORMAL) : 0;
	if (f_gt & UI_CLICK) { cfg.group_test = !cfg.group_test; config_save(); sc_run = 0; sg_state = 0; ui_toast(cfg.group_test ? "Group test: sends a few codes, then asks once" : "Group test off: codes go one after another"); }
	int force_ok = 0;
	if (grp && (f_yes & UI_CLICK)) { sg_answer(1, tot); if (sg_state == 99) { sg_state = 0; force_ok = 1; } }
	if (grp && (f_no & UI_CLICK)) sg_answer(0, tot);
	if (grp && (f_go & UI_CLICK) && sg_state == 0) { if (sc_done) { sc_idx = 0; sc_done = 0; sc_hn = 0; } sg_start(tot); f_go = 0; }
	if (grp && sg_state == 1 && plat_ms() >= sg_next) {
		if (sg_si < sg_hi) { sc_send(sg_idx[sg_si]); sg_si++; int g = SC_GAP[sc_speed] * 3; sg_next = plat_ms() + (g < 400 ? 400 : g); }
		else { sg_state = 2; ui_set_focus(5902); }
	}
	if (grp) f_go = 0;   /* the group button is handled above */

	u32 d = blocked ? 0 : in->down;
	if (f_af & UI_CLICK) {
		cfg.addr_first = !cfg.addr_first; config_save();
		sc_run = 0; sc_idx = 0; sc_hn = 0; sc_done = 0; sc_locked = 0; af_built_p = -1; scan_save();
		ui_toast(cfg.addr_first ? "Step 1 tries Mute and Volume at each address" : "Trying every code, address by address");
	}
	if (f_ul & UI_CLICK) { sc_run = 0; sc_locked = 0; sc_idx = 0; sc_hn = 0; sc_done = 0; scan_save(); ui_toast("Back to step 1: finding the address"); }
	/* the D-pad moves between buttons; codes only change with Back / Next / the history */
	int toggle = (f_go & UI_CLICK);
	if (sc_run && !ui_pointer_active() && ui_get_focus() != 5901) ui_set_focus(5901);   /* A always pauses while scanning */
	if (hist_pick >= 0) { sc_run = 0; u32 cu = sc_hist[hist_pick]; sc_send(cu); sc_idx = cu + 1; sc_done = 0; ui_toast("Sent again. It worked? Press It worked!"); }
	if (toggle) {
		if (sc_done) { sc_idx = 0; sc_done = 0; sc_hn = 0; }
		sc_run = !sc_run; sc_next = plat_ms();
		if (!sc_run) scan_save();
	}
	if (!sc_run) {
		int held_bk = (f_bk & UI_HELD) && !(in->down & BTN_A) && ui_repeat(BTN_A);   /* hold A on Back/Next = keep stepping */
		int held_nx = (f_nx & UI_HELD) && !(in->down & BTN_A) && ui_repeat(BTN_A);
		if ((f_bk & UI_CLICK) || held_bk) { u32 cu = sc_hn ? sc_hist[0] : sc_idx; if (cu > 0) cu--; sc_send(cu); sc_idx = cu + 1; sc_done = 0; }
		if ((f_nx & UI_CLICK) || held_nx) { u32 cu = sc_hn ? sc_hist[0] + 1 : sc_idx; if (cu < tot) { sc_send(cu); sc_idx = cu + 1; } }
		if (d & BTN_B) { u32 cu = sc_hn ? sc_hist[0] : sc_idx; if (cu < tot) sc_send(cu); }
		if ((fbt & UI_CLICK) || (d & BTN_1)) { sc_p = (sc_p + 1) % P_SCANNABLE; sc_idx = 0; sc_hn = 0; sc_done = 0; sc_locked = 0; scan_save(); }
		if (d & BTN_MINUS) { sc_idx = 0; sc_hn = 0; sc_done = 0; scan_save(); ui_toast("Started again"); }
	}
	if ((fsp & UI_CLICK) || (d & BTN_2)) sc_speed = (sc_speed + 1) % 5;
	if ((f_ok & UI_CLICK) || (d & BTN_PLUS) || force_ok) {
		sc_run = 0;
		u8 fa, fc; int fp; sc_code(sc_hn ? sc_hist[0] : sc_idx, &fa, &fc, &fp);
		char p[96]; snprintf(p, sizeof p, "%s/found.txt", data_dir);
		FILE *f = fopen(p, "a");
		if (f) { fprintf(f, "%s address 0x%02X command 0x%02X\n", PROTO_NAME[sc_p], fa, fc); fclose(f); }
		pend_proto = sc_p; pend_addr = fa; pend_cmd = fc;
		/* the TV reacted to one of the last few codes (most likely the newest) */
		{ static const float W[3] = { 0.8f, 0.35f, 0.2f };
		  for (int h = 0; h < 3 && h < (sc_hn ? sc_hn : 1); h++) { u8 ha, hc; int hp; sc_code(sc_hn ? sc_hist[h] : sc_idx, &ha, &hc, &hp); smart_note(sc_p, ha, hc, R_OTHER, W[h]); } }
		sc_worked_open = 1; ui_set_focus(9000);
	}
	if (sc_run && plat_ms() >= sc_next) {
		if (sc_idx >= tot) { sc_run = 0; sc_done = 1; }
		else {
			int guard = 0;
			while (sc_idx < tot && sc_saved(sc_idx) && guard++ < 4096) sc_idx++;   /* skip known codes */
			if (sc_idx < tot) { sc_send(sc_idx); sc_idx++; }
			if ((sc_idx & 255) == 0) scan_save();
		}
		sc_next = plat_ms() + SC_GAP[sc_speed];
	}

	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("B", "Resend");
	ui_footer_hint("+", "Worked");
	ui_footer_hint("2", "Speed");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Point the sensor bar at the TV's IR window, close up.",
		"Press Start. Watch the TV the whole time.",
		"TV does anything? Press A to pause straight away (Pause is picked for you).",
		"Then Back / Next, or tap a code in Last 5 sent, to find it again.",
		"Found it? Press It worked! to make a remote from it.",
		"Type: tap it (or press 1) to try NEC, Samsung, RC5, Sony.",
		"Speed: tap it (or press 2). Slower = easier to catch.",
		"The D-pad moves between buttons. B sends the code again. - starts over.",
		"Your place is saved: come back later and it carries on.",
		"Codes already saved on your open remote are skipped.",
		"Address first (On): step 1 sends Mute / Volume at each address. TV reacts? It worked! > Use this address.",
	};
	if (!sc_worked_open) ui_help("Find codes", help, 11);

	static const char *wk[] = { "Smart match", "Make a remote from it", "Add to this remote", "Keep scanning" };
	static const int wki[] = { IC_STAR, IC_TV, IC_PLUS, IC_RADAR };
	/* address first, step 1: the best next move is every code on that address */
	int lockable = sc_af() && !sc_locked;
	const char *items[5]; int icons[5], acts[5], n = 0;
	if (lockable) { items[n] = "Use this address"; icons[n] = IC_SEARCH; acts[n++] = 9; }
	items[n] = wk[0]; icons[n] = wki[0]; acts[n++] = 0;
	items[n] = wk[1]; icons[n] = wki[1]; acts[n++] = 1;
	if (have_cur) { items[n] = wk[2]; icons[n] = wki[2]; acts[n++] = 2; }
	items[n] = wk[3]; icons[n] = wki[3]; acts[n++] = 3;
	int ch = menu_popup(&sc_worked_open, items, icons, n, "Saved to found.txt!");
	ch = ch >= 0 ? acts[ch] : -1;
	if (ch == 9) {
		u8 la, lc; int lp; sc_code(sc_hn ? sc_hist[0] : sc_idx, &la, &lc, &lp);
		sc_locked = 1; sc_lock_addr = la; sc_idx = 0; sc_hn = 0; sc_done = 0; scan_save();
		char m[48]; snprintf(m, sizeof m, "Address %02X: now every code on it", la); ui_toast(m);
		return;
	}
	if (ch == 0) { sc_worked_open = 0; go(S_SMART); return; }
	if (ch == 1) { kb_open(K_SCAN_REMOTE, "TV", "Name this remote", NAME_LEN - 1); return; }
	if (ch == 2 && have_cur) { name_open(K_CODE_BTN, "What did it do?"); return; }

	if (!blocked && (in->down & BTN_HOME)) { sc_run = 0; scan_save(); go_back(); }
}

/* ================================================================== */
/* controller jobs                                                    */
/* ================================================================== */

void scr_pads(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_GAMEPAD, "Controllers", "Give each controller a job");
	struct { char name[32]; char sub[40]; int ic, conn, *role, id; } dev[16];
	int nd = 0;
	for (int c = 0; c < MAX_PADS; c++) {
		Pad *p = &pads_now[c];
		if (p->connected || c == 0) {
			static const char *ext[4] = { "", " + Nunchuk", " + Classic / Wii U Pro", " + Guitar" };
			snprintf(dev[nd].name, 32, "Wii Remote %d", c + 1);
			snprintf(dev[nd].sub, 40, "%s%s", p->connected ? "connected" : "not connected", p->connected ? ext[p->ext & 3] : "");
			dev[nd].ic = IC_REMOTE; dev[nd].conn = p->connected; dev[nd].role = &cfg.role_wm[c]; dev[nd].id = c * 8; nd++;
		}
		if (p->connected && (p->ext == 2 || p->ext == 3)) {
			snprintf(dev[nd].name, 32, p->ext == 3 ? "Guitar %d" : "Classic %d", c + 1);
			snprintf(dev[nd].sub, 40, "plugged into Wii Remote %d", c + 1);
			dev[nd].ic = IC_GAMEPAD; dev[nd].conn = 1; dev[nd].role = &cfg.role_cc[c]; dev[nd].id = c * 8 + 1; nd++;
		}
		if (p->gc_connected) {
			snprintf(dev[nd].name, 32, "GameCube %d", c + 1);
			snprintf(dev[nd].sub, 40, "connected");
			dev[nd].ic = IC_GAMEPAD; dev[nd].conn = 1; dev[nd].role = &cfg.role_gc[c]; dev[nd].id = c * 8 + 2; nd++;
		}
	}
	if (pads_now[0].bb_connected) { snprintf(dev[nd].name, 32, "Balance Board"); snprintf(dev[nd].sub, 40, "lean = arrows"); dev[nd].ic = IC_GAMEPAD; dev[nd].conn = 1; dev[nd].role = &cfg.role_bb; dev[nd].id = 40; nd++; }
	if (usb.kb_connected) { snprintf(dev[nd].name, 32, "USB keyboard"); snprintf(dev[nd].sub, 40, "keys: Controller map > Keys"); dev[nd].ic = IC_MENU; dev[nd].conn = 1; dev[nd].role = &cfg.role_kb; dev[nd].id = 41; nd++; }
	if (usb.mouse_connected) { snprintf(dev[nd].name, 32, "USB mouse"); snprintf(dev[nd].sub, 40, "pointer + clicks"); dev[nd].ic = IC_OK; dev[nd].conn = 1; dev[nd].role = &cfg.role_mouse; dev[nd].id = 42; nd++; }

	static int top;
	int per = 6;
	int fcs = ui_get_focus();
	if (fcs >= 6000 && fcs < 6000 + 43 * 4) {
		int row = -1;
		for (int i = 0; i < nd; i++) if ((fcs - 6000) / 4 == dev[i].id) row = i;
		if (row >= 0 && row < top) top = row;
		if (row >= top + per) top = row - per + 1;
	}
	if (top > nd - per) top = nd - per > 0 ? nd - per : 0;
	int y = 90;
	for (int i = top; i < nd && i < top + per; i++) {
		gfx_shadow(40, y, 560, 44, 12);
		gfx_round(40, y, 560, 44, 12, C_CARD_HI, 255);
		icon(dev[i].ic, 64, y + 22, 24, dev[i].conn ? C_ACCENT : C_DIM);
		text(F_BODYB, 86, y + 2, dev[i].name, C_TEXT, 255);
		text_fit(F_SMALL, 86, y + 23, 170, dev[i].sub, dev[i].conn ? C_GOOD : C_DIM, 255);
		static const int ric[3] = { IC_MENU, IC_TV, IC_CLOSE };
		static const char *short_name[3] = { "Menu", "TV", "Off" };
		for (int r = 0; r < ROLE_COUNT; r++) {
			if (dev[i].role == &cfg.role_mouse && r == ROLE_TV) continue;
			int f = ui_ibutton(6000 + dev[i].id * 4 + r, 262 + r * 112, y + 5, 106, 34, ric[r], short_name[r], *dev[i].role == r ? ST_PRIMARY : ST_NORMAL);
			if (f & UI_CLICK) {
				/* never leave yourself with no controller that works the menus */
				int keep = 0;
				for (int j = 0; j < nd; j++) if (j != i && dev[j].conn && *dev[j].role == ROLE_MENU && dev[j].role != &cfg.role_mouse) keep = 1;
				if (r != ROLE_MENU && *dev[i].role == ROLE_MENU && !keep && dev[i].role != &cfg.role_mouse) ui_toast("One controller must stay on Menu");
				else { *dev[i].role = r; config_save(); }
			}
		}
		y += 48;
	}
	if (nd > per) { char pg[24]; snprintf(pg, sizeof pg, "%d-%d of %d", top + 1, top + per < nd ? top + per : nd, nd); text_r(F_SMALL, 600, 66, pg, C_DIM, 255); }
	y = 378;
	gfx_round(40, y + 4, 560, 40, 12, C_SOFT_WARN, 255);
	icon(IC_HELP, 62, y + 24, 20, C_SEL);
	text(F_SMALL, 82, y + 13, "Stuck? Hold HOME for 3 seconds = all back to Menu.", C_TEXT, 255);
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Menu = moves around this app (and its shortcuts).",
		"TV = every button sends to the TV, on any screen.",
		"Off = the app ignores it. Free for something else.",
		"Classic set to Menu on its own: D-pad moves, a = A, b = B.",
		"GameCube controllers work like a Classic. Start + Z = HOME.",
		"Nunchuk: C and Z are mappable, the stick works as a D-pad.",
		"Guitar, Wii U Pro and Balance Board (lean) work like a Classic.",
		"USB keyboard: arrows / Enter / Esc move around, other keys can be mapped.",
		"USB mouse: moves a pointer, left click = A, right click = B.",
		"Test remote: set a 2nd controller to TV, then test buttons while you find them.",
		"If no controller is set to Menu, all of them work the menu.",
		"Hold HOME 3 seconds on any controller = reset.",
	};
	ui_help("Controllers", help, 12);
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ================================================================== */
/* macro editor                                                       */
/* ================================================================== */

static Code m_steps[MAX_STEPS];
static int m_n, m_idx, m_pick, m_pick_page, m_top;

void macro_edit_start(int idx)
{
	m_idx = idx >= 0 ? idx : -1;
	m_n = 0; m_pick = 0; m_top = 0;
	if (idx >= 0 && idx < cur.nbtn && !cur.btn[idx].nsteps) m_idx = -1;
	if (idx >= 0 && idx < cur.nbtn) { m_n = cur.btn[idx].nsteps; memcpy(m_steps, cur.btn[idx].steps, m_n * sizeof(Code)); }
	else if (idx == -2) { m_n = pend_macro_n; memcpy(m_steps, pend_macro, m_n * sizeof(Code)); }
	go(S_MACRO);
}

static const char *code_label(const Code *c)
{
	for (int i = 0; i < cur.nbtn; i++)
		if (!cur.btn[i].nsteps && cur.btn[i].proto == c->proto && cur.btn[i].addr == c->addr && cur.btn[i].cmd == c->cmd) return cur.btn[i].name;
	static char b[24]; snprintf(b, sizeof b, "%s %02X/%02X", PROTO_NAME[c->proto], c->addr, c->cmd);
	return b;
}

void scr_macro(void)
{
	int blocked = help_is_open() || m_pick;
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_SEQ, m_idx >= 0 ? cur.btn[m_idx].name : "New macro", "Buttons are sent in order, with a wait after each");
	int per = 6;
	int fcs = ui_get_focus();
	if (fcs >= 6100 && fcs < 6100 + MAX_STEPS * 4) {
		int row = (fcs - 6100) / 4;
		if (row < m_top) m_top = row;
		if (row >= m_top + per) m_top = row - per + 1;
	}
	if (m_top > m_n - per) m_top = m_n - per > 0 ? m_n - per : 0;
	int del = -1;
	for (int i = m_top; i < m_n && i < m_top + per; i++) {
		int y = 90 + (i - m_top) * 46;
		gfx_round(40, y, 560, 40, 12, C_CARD_HI, 255);
		gfx_round_line(40, y, 560, 40, 12, 1, C_LINE, 255);
		char n[8]; snprintf(n, sizeof n, "%d", i + 1);
		gfx_circle(62, y + 20, 13, C_ACCENT, 255);
		text_c(F_SMALL, 62, y + 9, n, C_WHITE, 255);
		text_fit(F_BODYB, 84, y + 9, 200, code_label(&m_steps[i]), C_TEXT, 255);
		char w[20];
		if (m_steps[i].wait) snprintf(w, sizeof w, "wait %d.%d s", m_steps[i].wait / 1000, (m_steps[i].wait % 1000) / 100);
		else snprintf(w, sizeof w, "wait normal");
		text_r(F_SMALL, 398, y + 11, w, C_DIM, 255);
		if (ui_tile(6100 + i * 4, 406, y + 4, 40, 32, -1, "-", ST_KEY, 0) & UI_CLICK) { if (m_steps[i].wait >= 100) m_steps[i].wait -= 100; }
		if (ui_tile(6101 + i * 4, 450, y + 4, 40, 32, -1, "+", ST_KEY, 0) & UI_CLICK) { if (m_steps[i].wait < 9900) m_steps[i].wait = m_steps[i].wait ? m_steps[i].wait + 100 : 500; }
		if (ui_tile(6102 + i * 4, 498, y + 4, 44, 32, IC_UP, "", ST_KEY, 0) & UI_CLICK) { if (i > 0) { Code t = m_steps[i]; m_steps[i] = m_steps[i - 1]; m_steps[i - 1] = t; } }
		if (ui_tile(6103 + i * 4, 548, y + 4, 44, 32, IC_TRASH, "", ST_NORMAL, 0) & UI_CLICK) del = i;
	}
	if (del >= 0) { memmove(&m_steps[del], &m_steps[del + 1], (m_n - del - 1) * sizeof(Code)); m_n--; }
	if (!m_n) {
		gfx_round(40, 90, 560, 60, 14, C_CARD_HI, 255);
		text(F_BODYB, 60, 108, "No steps yet - press Add step.", C_DIM, 255);
	}
	if (m_n > per) { char pg[24]; snprintf(pg, sizeof pg, "%d-%d of %d", m_top + 1, m_top + per < m_n ? m_top + per : m_n, m_n); text_r(F_SMALL, 600, 64, pg, C_DIM, 255); }
	int y = 370;
	if (m_n < MAX_STEPS && (ui_ibutton(6200, 40, y, 170, 46, IC_PLUS, "Add step", ST_NORMAL) & UI_CLICK)) { m_pick = 1; m_pick_page = 0; ui_set_focus(-1); }
	if (m_n && (ui_ibutton(6201, 220, y, 150, 46, IC_PLAY, "Test", ST_NORMAL) & UI_CLICK)) queue_codes(m_steps, m_n);
	if (m_n && (ui_ibutton(6202, 380, y, 220, 46, IC_CHECK, "Save macro", ST_NORMAL) & UI_CLICK)) {
		if (m_idx >= 0) {
			undo_save("Macro changed");
			cur.btn[m_idx].nsteps = m_n;
			memcpy(cur.btn[m_idx].steps, m_steps, m_n * sizeof(Code));
			save_cur(); ui_toast("Macro saved"); go(S_REMOTE);
		} else {
			memcpy(pend_macro, m_steps, m_n * sizeof(Code)); pend_macro_n = m_n;
			kb_open(K_MACRO, "MACRO", "Name this macro", NAME_LEN - 1);
		}
		return;
	}
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Cancel");
	static const char *help[] = {
		"Add step = pick a button. Steps are sent top to bottom.",
		"- / + = how long to wait after that step (0.1 s steps).",
		"Example: POWER, wait 1.5 s, then TV/AV.",
		"Up arrow moves a step earlier. Bin removes it.",
		"Name a macro SERVICE to use it from Calibrate.",
	};
	if (!m_pick) ui_help("Macro", help, 5);

	if (m_pick) {
		ui_suspend(0);
		ui_dim();
		ui_panel(30, 30, 580, 390);
		text(F_TITLE, 56, 54, "Add which button?", C_TEXT, 255);
		int idxs[MAX_BTNS], n = 0;
		for (int i = 0; i < cur.nbtn; i++) if (!cur.btn[i].nsteps) idxs[n++] = i;
		int pp = 16, pages = (n + pp - 1) / pp; if (pages < 1) pages = 1;
		for (int k = m_pick_page * pp; k < n && k < (m_pick_page + 1) * pp; k++) {
			int j = k - m_pick_page * pp;
			RBtn *b = &cur.btn[idxs[k]];
			if (ui_tile(6300 + k, 50 + (j % 4) * 136, 104 + (j / 4) * 62, 128, 54, icon_for_name(b->name), b->name, ST_KEY, 0) & UI_CLICK) {
				Code c = { b->proto, b->addr, b->cmd, 0 };
				m_steps[m_n++] = c; m_pick = 0; m_top = m_n > 6 ? m_n - 6 : 0;
			}
		}
		int by = 360;
		if (pages > 1) {
			if (ui_tile(6391, 250, by, 60, 46, IC_LEFT, "", ST_NORMAL, 0) & UI_CLICK) { if (m_pick_page) m_pick_page--; }
			char pg[12]; snprintf(pg, sizeof pg, "%d/%d", m_pick_page + 1, pages);
			text_c(F_BODYB, 340, by + 10, pg, C_TEXT, 255);
			if (ui_tile(6392, 370, by, 60, 46, IC_RIGHT, "", ST_NORMAL, 0) & UI_CLICK) { if (m_pick_page < pages - 1) m_pick_page++; }
		}
		if (ui_ibutton(6393, 460, by, 130, 46, IC_BACK, "Cancel", ST_NORMAL) & UI_CLICK) m_pick = 0;
		if (in->down & (BTN_HOME | BTN_B)) { m_pick = 0; in->down = 0; }
		return;
	}
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ================================================================== */
/* rooms (profiles)                                                   */
/* ================================================================== */

void scr_profiles(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_TV, "Rooms", "Each room keeps its own remotes");
	char names[12][NAME_LEN];
	int n = profiles_list(names, 12);
	for (int i = 0; i < n; i++) {
		int x = 40 + (i % 3) * 190, y = 92 + (i / 3) * 84;
		int now = !strcmp(names[i], cfg.profile);
		if (ui_tile(6400 + i, x, y, 180, 74, IC_TV, names[i][0] ? names[i] : "Main", now ? ST_PRIMARY : ST_NORMAL, 0) & UI_CLICK) {
			if (!now) {
				profile_use(names[i]);
				have_cur = 0; cfg.last[0] = 0;
				config_save();
				char m[48]; snprintf(m, sizeof m, "Room: %s", names[i][0] ? names[i] : "Main"); ui_toast(m);
			}
			go(S_HOME);
			return;
		}
	}
	if (n < 12 && (ui_ibutton(6450, 40, 374, 220, 46, IC_PLUS, "New room", ST_NORMAL) & UI_CLICK)) {
		kb_open(K_PROFILE, "", "Name the room", NAME_LEN - 1);
		return;
	}
	ui_footer_begin();
	ui_footer_hint("A", "Switch");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"A room is a separate set of remotes, e.g. Living Room CRT.",
		"Pick a room to switch to it. Blue = the room you're in.",
		"New room starts empty. Your other rooms stay untouched.",
		"Rooms live in sd:/wiirremote/profiles on the SD card.",
	};
	ui_help("Rooms", help, 4);
	if (!blocked && (in->down & BTN_HOME)) go_back();
}


/* ================================================================== */
/* video & cable                                                      */
/* ================================================================== */

static int v_prev = 0, v_confirm;
static u64 v_until;

void scr_video(void)
{
	int blocked = help_is_open() || v_confirm;
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_TV, "Video & cable", "What the Wii is sending to your TV");
	char L[8][72];
	int n = plat_video_info(L, 8);
	gfx_round(40, 88, 560, 22 + n * 22, 14, C_CARD_HI, 255);
	gfx_round_line(40, 88, 560, 22 + n * 22, 14, 1, C_LINE, 255);
	for (int i = 0; i < n; i++) text_fit(F_SMALL, 58, 98 + i * 22, 530, L[i], i == 0 ? C_ACCENT : C_TEXT, 255);
	int y0 = 88 + 22 + n * 22 + 12;
	text(F_BODYB, 40, y0, "Switch output:", C_TEXT, 255);
	{
		char fl[40]; snprintf(fl, sizeof fl, "Flicker filter: %s", plat_deflicker ? "On" : "Off");
		if (ui_button(6595, 360, y0 - 6, 240, 32, fl, plat_deflicker ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) {
			plat_deflicker = !plat_deflicker;
			ui_toast(plat_deflicker ? "Filter on: soft, like Wii games" : "Filter off: pixel sharp");
		}
	}
	const char *names[12];
	int m = plat_video_modes(names, 12), curm = plat_video_current();
	for (int i = 0; i < m; i++) {
		int x = 40 + (i % 4) * 142, y = y0 + 28 + (i / 4) * 52;
		if (ui_tile(6500 + i, x, y, 134, 44, -1, names[i], i == curm ? ST_PRIMARY : ST_KEY, 0) & UI_CLICK) {
			if (i != curm) {
				int before = curm;
				if (plat_video_set(i)) {
					if (i != 0) { v_prev = before; v_confirm = 1; v_until = plat_ms() + 12000; ui_set_focus(6590); }
				} else ui_toast(i == 2 ? "480p needs the component cable" : "Not possible here");
			}
		}
	}
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"The top card shows exactly what the Wii outputs right now.",
		"Pick a mode to switch straight away - no restart.",
		"240p / 288p are the real retro modes: great for CRT geometry.",
		"50Hz modes may not show on some NTSC-only TVs.",
		"If the picture goes wrong, just wait: it switches back after 12 seconds.",
		"480p only works with the component cable.",
		"Flicker filter: On = soft, the way Wii games look. Off = pixel sharp (best for testing).",
		"The Wii can't tell composite, S-Video and RGB SCART apart - they all show as AV.",
	};
	if (!v_confirm) ui_help("Video & cable", help, 8);
	if (v_confirm) {
		ui_suspend(0);
		ui_dim();
		ui_panel(120, 130, 400, 200);
		int left = (int)((v_until - plat_ms()) / 1000);
		if (plat_ms() >= v_until) { plat_video_set(v_prev); v_confirm = 0; ui_toast("Switched back"); return; }
		text_c(F_TITLE, 320, 146, "Keep this mode?", C_TEXT, 255);
		char m2[48]; snprintf(m2, sizeof m2, "Going back in %d seconds", left + 1);
		text_c(F_BODY, 320, 190, m2, C_DIM, 255);
		if (ui_ibutton(6590, 140, 256, 170, 50, IC_CHECK, "Keep", ST_NORMAL) & UI_CLICK) { v_confirm = 0; ui_toast("Mode kept"); }
		if (ui_ibutton(6591, 330, 256, 170, 50, IC_BACK, "Go back", ST_NORMAL) & UI_CLICK) { plat_video_set(v_prev); v_confirm = 0; }
		if (in->down & (BTN_HOME | BTN_B)) { plat_video_set(v_prev); v_confirm = 0; in->down = 0; }
		return;
	}
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ================================================================== */
/* service menu presets                                               */
/* ================================================================== */

typedef struct { const char *name, *how; int n; Code c[8]; const char *keys; } Preset;

static const Preset PRESETS[] = {
	{ "Samsung", "Standby (red light) first", 4,
	  { { P_SAMSUNG, 0x07, 0x1F, 400 }, { P_SAMSUNG, 0x07, 0x1A, 400 }, { P_SAMSUNG, 0x07, 0x0F, 400 }, { P_SAMSUNG, 0x07, 0x02, 0 } }, NULL },
	{ "Sony", "Standby first", 4,
	  { { P_SONY, 0x01, 0x3A, 400 }, { P_SONY, 0x01, 0x04, 400 }, { P_SONY, 0x01, 0x12, 400 }, { P_SONY, 0x01, 0x15, 0 } }, NULL },
	{ "LG (IN-START)", "TV on", 1, { { P_NEC, 0x04, 0xFB, 0 } }, NULL },
	{ "LG (EZ-ADJUST)", "TV on", 1, { { P_NEC, 0x04, 0xFF, 0 } }, NULL },
	{ "Philips", "TV on: 062596 MENU", 7,
	  { { P_RC5, 0, 0, 300 }, { P_RC5, 0, 6, 300 }, { P_RC5, 0, 2, 300 }, { P_RC5, 0, 5, 300 }, { P_RC5, 0, 9, 300 }, { P_RC5, 0, 6, 300 }, { P_RC5, 0, 0x52, 0 } }, NULL },
	{ "Hitachi (A)", "MENU, MENU, 8, OK", 4,
	  { { P_NEC, 0x50, 0x53, 400 }, { P_NEC, 0x50, 0x53, 400 }, { P_NEC, 0x50, 0x04, 400 }, { P_NEC, 0x50, 0x2E, 0 } }, NULL },
	{ "Hitachi/Vestel (B)", "TV on: MENU 4725", 5,
	  { { P_NEC, 0x50, 0x53, 500 }, { P_NEC, 0x50, 0x1C, 300 }, { P_NEC, 0x50, 0x1F, 300 }, { P_NEC, 0x50, 0x0E, 300 }, { P_NEC, 0x50, 0x1D, 0 } }, NULL },
	{ "JVC (CRT)", "Might need its remote", 3,
	  { { P_JVC, 0x03, 0x03, 500 }, { P_JVC, 0x03, 0x04, 60 }, { P_JVC, 0x03, 0x0D, 0 } }, NULL },
	{ "Panasonic (CRT)", "Hold VOL- on the TV", 4,
	  { { P_PANA, 0x80, 0x0F, 800 }, { P_PANA, 0x80, 0x39, 400 }, { P_PANA, 0x80, 0x39, 400 }, { P_PANA, 0x80, 0x39, 0 } }, NULL },
	/* built from YOUR remote's buttons */
	{ "MENU 8893", "Yours: Chinese TVs", 0, { { 0 } }, "MENU|8|8|9|3" },
	{ "MENU 1147", "Yours: Chinese TVs", 0, { { 0 } }, "MENU|1|1|4|7" },
	{ "MENU 4725", "Yours: Vestel-type TVs", 0, { { 0 } }, "MENU|4|7|2|5" },
	{ "MENU 9735", "Yours: Chinese TVs", 0, { { 0 } }, "MENU|9|7|3|5" },
	{ "MENU 1999", "Yours: Chinese TVs", 0, { { 0 } }, "MENU|1|9|9|9" },
};
#define NPRESET ((int)(sizeof PRESETS / sizeof PRESETS[0]))

/* fill codes for a preset; 0 = your remote is missing a needed button */
static int preset_codes(int k, Code *out, char *missing)
{
	const Preset *p = &PRESETS[k];
	if (!p->keys) { memcpy(out, p->c, p->n * sizeof(Code)); return p->n; }
	char buf[48]; snprintf(buf, sizeof buf, "%s", p->keys);
	int n = 0;
	for (char *t = strtok(buf, "|"); t; t = strtok(NULL, "|")) {
		int i = remote_find_btn(&cur, t);
		if (i < 0 || cur.btn[i].nsteps) { if (missing) snprintf(missing, 24, "%s", t); return 0; }
		out[n].proto = cur.btn[i].proto; out[n].addr = cur.btn[i].addr; out[n].cmd = cur.btn[i].cmd;
		out[n].wait = n == 0 ? 600 : 300;
		n++;
	}
	return n;
}

static int pr_sel = -1;

void scr_presets(void)
{
	int blocked = help_is_open() || pr_sel >= 0;
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_WRENCH, "Service menus", "Known key sequences that open hidden TV menus");
	for (int k = 0; k < NPRESET; k++) {
		int x = 40 + (k % 3) * 188, y = 88 + (k / 3) * 62;
		Code c[8]; char miss[24] = "";
		int ok = preset_codes(k, c, miss) > 0;
		int f = ui_item(6600 + k, x, y, 180, 56);
		gfx_round(x, y, 180, 56, 12, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, ok ? 255 : 140);
		gfx_round_line(x, y, 180, 56, 12, 1, C_LINE, 255);
		text_fit(F_BODYB, x + 10, y + 4, 164, PRESETS[k].name, ok ? C_TEXT : C_DIM, 255);
		char sub[40];
		if (ok) snprintf(sub, sizeof sub, "%s", PRESETS[k].how); else snprintf(sub, sizeof sub, "Needs a %s button", miss);
		text_fit(F_SMALL, x + 10, y + 31, 164, sub, ok ? C_DIM : C_BAD, 255);
		if (f & UI_FOCUS) ui_focus_ring(x, y, 180, 56, 12);
		if ((f & UI_CLICK) && ok) { pr_sel = k; ui_set_focus(6690); }
	}
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Each preset sends a known sequence that opens a hidden service menu.",
		"Brand presets use that brand's own remote codes.",
		"'Yours' presets use the buttons you saved (MENU and numbers).",
		"Some need the TV in standby (red light) first: it says on the card.",
		"Try now sends it once. Save adds it to your remote as SERVICE.",
		"Nothing happens? Try another one. Write down settings before changing them!",
	};
	if (pr_sel < 0) ui_help("Presets", help, 6);
	if (pr_sel >= 0) {
		ui_suspend(0);
		ui_dim();
		ui_panel(110, 110, 420, 240);
		text_c(F_TITLE, 320, 124, PRESETS[pr_sel].name, C_TEXT, 255);
		text_c(F_BODY, 320, 166, PRESETS[pr_sel].how, C_DIM, 255);
		Code c[8]; int n = preset_codes(pr_sel, c, NULL);
		if (ui_ibutton(6690, 130, 214, 180, 50, IC_PLAY, "Try now", ST_NORMAL) & UI_CLICK) { queue_codes(c, n); ui_toast("Sending... watch the TV"); }
		if (ui_ibutton(6691, 330, 214, 180, 50, IC_SAVE, "Save", ST_NORMAL) & UI_CLICK) {
			char nm[NAME_LEN]; unique_name(&cur, "SERVICE", nm);
			if (remote_add_macro(&cur, nm, c, n) >= 0) { save_cur(); ui_toast("Saved as SERVICE"); }
			pr_sel = -1;
		}
		if (ui_ibutton(6692, 230, 280, 180, 44, IC_CLOSE, "Close", ST_NORMAL) & UI_CLICK) pr_sel = -1;
		if (in->down & (BTN_HOME | BTN_B)) { pr_sel = -1; in->down = 0; }
		return;
	}
	if (!blocked && (in->down & BTN_HOME)) go_back();
}

/* ================================================================== */
/* Flipper Zero .ir import                                            */
/* ================================================================== */

#define MAX_IRF 24
static char irf_path[MAX_IRF][128], irf_name[MAX_IRF][40];
static u8 irf_lirc[MAX_IRF];
static int irf_n = -1;
static char irf_result[96];

static void irf_scan(void)
{
	irf_n = 0;
	char d1[96]; snprintf(d1, sizeof d1, "%s/import", base_dir);
	mkdir(d1, 0777);
	const char *dirs[2] = { d1, "sd:/apps/WiiRremotePlus/import" };
	for (int k = 0; k < 2; k++) {
		DIR *d = opendir(dirs[k]);
		if (!d) continue;
		struct dirent *e;
		while ((e = readdir(d)) && irf_n < MAX_IRF) {
			int l = strlen(e->d_name);
			int cut = 0;
			if (l > 3 && !strcasecmp(e->d_name + l - 3, ".ir")) cut = 3;
			else if (l > 11 && !strcasecmp(e->d_name + l - 11, ".lircd.conf")) cut = 11;
			else if (l > 5 && !strcasecmp(e->d_name + l - 5, ".conf")) cut = 5;
			else if (l > 5 && !strcasecmp(e->d_name + l - 5, ".lirc")) cut = 5;
			else if (l > 4 && !strcasecmp(e->d_name + l - 4, ".txt") && strncasecmp(e->d_name, "READ", 4)) cut = 4;   /* a shared WiiRremote remote */
			if (!cut || e->d_name[0] == '.') continue;
			irf_lirc[irf_n] = cut == 4 ? 2 : cut != 3;
			snprintf(irf_path[irf_n], 128, "%s/%s", dirs[k], e->d_name);
			snprintf(irf_name[irf_n], 40, "%s", e->d_name);
			irf_name[irf_n][l - cut < 39 ? l - cut : 39] = 0;
			for (char *q = irf_name[irf_n]; *q; q++) if (*q == '_') *q = ' ';
			irf_n++;
		}
		closedir(d);
	}
}

static int hexbyte(const char *s, int idx)
{
	/* "18 00 00 00" -> byte idx */
	unsigned v[4] = { 0 };
	sscanf(s, "%x %x %x %x", &v[0], &v[1], &v[2], &v[3]);
	return v[idx & 3] & 0xFF;
}

/* ---- LIRC .conf import ---- */
static u8 rev8(u8 b) { u8 r = 0; for (int i = 0; i < 8; i++) if (b & (1 << i)) r |= 0x80 >> i; return r; }
static int near(int v, int want) { return v > want * 75 / 100 && v < want * 125 / 100; }

typedef struct { int hm, hs, om, os, zm, zs, bits, pbits, rc5, done; unsigned long long pre; } Lirc;

/* turn one LIRC code into our protocol; -1 if we can't send it */
static int lirc_decode(const Lirc *L, unsigned long long code, u16 *a, u8 *c)
{
	int n = L->bits + L->pbits;
	unsigned long long v = (L->pbits ? (L->pre << L->bits) : 0) | code;
	if (L->rc5) {
		*a = (v >> 6) & 0x1F; *c = v & 0x3F;
		if (n >= 13 && !((v >> 12) & 1)) *c |= 0x40;
		return P_RC5;
	}
	if (near(L->hm, 9000) && near(L->hs, 4500) && n == 32) {
		u8 b0 = rev8(v >> 24), b1 = rev8(v >> 16), b2 = rev8(v >> 8), b3 = rev8(v);
		if ((u8)~b2 != b3) return -1;
		if ((u8)~b0 != b1) { *a = b0 | (b1 << 8); *c = b2; return P_NECX; }   /* extended NEC */
		*a = b0; *c = b2; return P_NEC;
	}
	if (near(L->hm, 4500) && near(L->hs, 4500) && n == 32) {
		*a = rev8(v >> 24); *c = rev8(v >> 8); return P_SAMSUNG;
	}
	if (near(L->hm, 8400) && near(L->hs, 4200) && n == 16) {
		*a = rev8(v >> 8); *c = rev8(v); return P_JVC;
	}
	if (near(L->hm, 3500) && near(L->hs, 1750) && n == 48) {
		if (rev8(v >> 40) != 0x02 || rev8(v >> 32) != 0x20) return -1;
		*a = rev8(v >> 24) | (rev8(v >> 16) << 8); *c = rev8(v >> 8); return P_PANA;
	}
	if ((near(L->hm, 2400) || near(L->om, 1200)) && n == 12) {
		/* Sony: LIRC writes the bits in send order; sent order is command then address, LSB first */
		unsigned r = 0; for (int i = 0; i < n; i++) if (v & (1ULL << i)) r |= 1u << (n - 1 - i);
		*c = r & 0x7F; *a = (r >> 7) & (n == 15 ? 0xFF : 0x1F);
		return P_SONY;
	}
	return -1;
}

static void lirc_import(Remote *r, FILE *f, int *ok, int *skipped)
{
	Lirc L; memset(&L, 0, sizeof L);
	char line[200];
	int in_codes = 0, in_remote = 0, got_one = 0;
	while (fgets(line, sizeof line, f)) {
		char *p = line; while (*p == ' ' || *p == '\t') p++;
		char w1[48] = "", w2[48] = "", w3[48] = "";
		int nw = sscanf(p, "%47s %47s %47s", w1, w2, w3);
		if (nw < 1 || w1[0] == '#') continue;
		if (!strcasecmp(w1, "begin") && nw >= 2 && !strcasecmp(w2, "remote")) { if (got_one) break; memset(&L, 0, sizeof L); in_remote = 1; continue; }
		if (!strcasecmp(w1, "begin") && nw >= 2 && !strcasecmp(w2, "codes")) { in_codes = 1; continue; }
		if (!strcasecmp(w1, "begin") && nw >= 2 && !strcasecmp(w2, "raw_codes")) { in_codes = 2; continue; }
		if (!strcasecmp(w1, "end")) { if (nw >= 2 && !strcasecmp(w2, "remote")) { in_remote = 0; if (got_one) break; } in_codes = 0; continue; }
		if (!in_remote) continue;
		if (in_codes == 2) { if (!strcasecmp(w1, "name")) (*skipped)++; continue; }
		if (in_codes == 1) {
			if (nw < 2) continue;
			u16 a; u8 c;
			int pr = lirc_decode(&L, strtoull(w2, NULL, 0), &a, &c);
			if (pr < 0 || r->nbtn >= MAX_BTNS) { (*skipped)++; continue; }
			char nm[NAME_LEN]; const char *src = w1;
			if (!strncasecmp(src, "KEY_", 4)) src += 4;
			snprintf(nm, sizeof nm, "%s", src);
			for (char *q = nm; *q; q++) { if (*q == '_') *q = ' '; if (*q >= 'a' && *q <= 'z') *q -= 32; }
			static const char *AL[][2] = { { "VOLUMEUP", "VOL+" }, { "VOLUMEDOWN", "VOL-" }, { "CHANNELUP", "CH+" }, { "CHANNELDOWN", "CH-" }, { "ENTER", "OK" } };
			for (size_t q = 0; q < sizeof AL / sizeof AL[0]; q++) if (!strcmp(nm, AL[q][0])) snprintf(nm, sizeof nm, "%s", AL[q][1]);
			char un[NAME_LEN]; unique_name(r, nm, un);
			remote_add_btn(r, un, pr, a, c);
			if (*ok == 0) { r->proto = pr; r->addr = a; }
			(*ok)++; got_one = 1;
			continue;
		}
		if (!strcasecmp(w1, "header") && nw >= 3) { L.hm = atoi(w2); L.hs = atoi(w3); }
		else if (!strcasecmp(w1, "one") && nw >= 3) { L.om = atoi(w2); L.os = atoi(w3); }
		else if (!strcasecmp(w1, "zero") && nw >= 3) { L.zm = atoi(w2); L.zs = atoi(w3); }
		else if (!strcasecmp(w1, "bits")) L.bits = atoi(w2);
		else if (!strcasecmp(w1, "pre_data_bits")) L.pbits = atoi(w2);
		else if (!strcasecmp(w1, "pre_data")) L.pre = strtoull(w2, NULL, 0);
		else if (!strcasecmp(w1, "flags") && (strstr(p, "RC5") || strstr(p, "rc5"))) L.rc5 = 1;
	}
}

/* read the file line by line (no big buffers) and build a remote */
/* a remote shared from another Wii (Export to SD): copy it in under a free name */
static void pack_import(int k)
{
	FILE *in = fopen(irf_path[k], "rb");
	char tmp[160]; snprintf(tmp, sizeof tmp, "%s/remotes/_import.txt", data_dir);
	FILE *out = in ? fopen(tmp, "wb") : NULL;
	if (!in || !out) { if (in) fclose(in); snprintf(irf_result, sizeof irf_result, "Can't open that file"); return; }
	char buf[512]; size_t n;
	while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
	fclose(in); fclose(out);
	if (!remote_load(&cur, "_import.txt") || !cur.nbtn) { remove(tmp); snprintf(irf_result, sizeof irf_result, "That isn't a WiiRremote remote file"); if (cfg.last[0]) remote_load(&cur, cfg.last); return; }
	remove(tmp);
	cur.file[0] = 0;
	char nm[NAME_LEN]; unique_remote_name(cur.name, nm); snprintf(cur.name, NAME_LEN, "%s", nm);
	have_cur = 1;
	save_cur();
	remote_screen_reset();
	char m[48]; snprintf(m, sizeof m, "Added %s: %d buttons", cur.name, cur.nbtn);
	go(S_REMOTE);
	ui_toast(m);
}

/* Flipper .ir: parsed signals -> buttons on r */
static int flipper_parse(Remote *r, FILE *f, int *skipped)
{
	char line[160], name[NAME_LEN] = "", proto[24] = "", addr[40] = "", cmd[40] = "", type[16] = "";
	int ok = 0, more = 1;
	while (more) {
		more = fgets(line, sizeof line, f) != NULL;
		int n = more ? strlen(line) : 0;
		while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
		int flush = !more || !strncmp(line, "name:", 5) || !strncmp(line, "#", 1);
		if (flush && name[0]) {
			int pr = -1, a = 0, c = 0;
			if (!strcmp(type, "parsed")) {
				int a0 = hexbyte(addr, 0), a1 = hexbyte(addr, 1);
				c = hexbyte(cmd, 0);
				if (!strcmp(proto, "NEC")) { pr = P_NEC; a = a0; }
				else if (!strcmp(proto, "NECext") && hexbyte(cmd, 1) == (~c & 0xFF)) {
					if (a1 == (~a0 & 0xFF)) { pr = P_NEC; a = a0; } else { pr = P_NECX; a = a0 | (a1 << 8); }
				}
				else if (!strcmp(proto, "RC5X")) { pr = P_RC5; a = a0; }
				else if (!strcmp(proto, "RC6")) { pr = P_RC6; a = a0; }
				else if (!strcmp(proto, "SIRC15")) { pr = P_SONY15; a = a0; }
				else if (!strcmp(proto, "Samsung32")) { pr = P_SAMSUNG; a = a0; }
				else if (!strcmp(proto, "RC5")) { pr = P_RC5; a = a0; }
				else if (!strcmp(proto, "SIRC")) { pr = P_SONY; a = a0; }
				else if (!strcmp(proto, "Kaseikyo") && hexbyte(addr, 1) == 0x02 && hexbyte(addr, 2) == 0x20) {
					/* Panasonic: address 80 02 20 00, command D0 03 = 0x3D0 -> 0x3D */
					int cw = hexbyte(cmd, 0) | (hexbyte(cmd, 1) << 8);
					if (!(cw & 0x0F)) { pr = P_PANA; a = a0; c = (cw >> 4) & 0xFF; }
				}
			}
			if (pr >= 0 && r->nbtn < MAX_BTNS) {
				char un[NAME_LEN]; unique_name(r, name, un);
				remote_add_btn(r, un, pr, a, c);
				if (ok == 0) { r->proto = pr; r->addr = a; }
				ok++;
			} else (*skipped)++;
			name[0] = proto[0] = addr[0] = cmd[0] = type[0] = 0;
		}
		if (!more) break;
		if (!strncmp(line, "name:", 5)) { char *v = line + 5; while (*v == ' ') v++; snprintf(name, sizeof name, "%s", v); for (char *q = name; *q; q++) { if (*q == '|') *q = '/'; if (*q == '_') *q = ' '; } }
		else if (!strncmp(line, "type:", 5)) sscanf(line + 5, "%15s", type);
		else if (!strncmp(line, "protocol:", 9)) sscanf(line + 9, "%23s", proto);
		else if (!strncmp(line, "address:", 8)) snprintf(addr, sizeof addr, "%s", line + 8);
		else if (!strncmp(line, "command:", 8)) snprintf(cmd, sizeof cmd, "%s", line + 8);
	}
	return ok;
}

/* Smart match's library: any shared file -> a remote in memory (nothing is saved) */
int ir_file_to_remote(const char *path, Remote *r, int *skipped)
{
	int l = strlen(path), ok = 0;
	*skipped = 0;
	if (l > 4 && !strcasecmp(path + l - 4, ".txt")) return remote_load_path(r, path) ? r->nbtn : 0;
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	const char *b = strrchr(path, '/'); b = b ? b + 1 : path;
	char nm[NAME_LEN]; snprintf(nm, sizeof nm, "%s", b);
	for (char *q = nm; *q; q++) { if (*q == '_') *q = ' '; if (*q == '.') { *q = 0; break; } }
	remote_new(r, nm, P_NEC, 0);
	int lirc = l > 3 && strcasecmp(path + l - 3, ".ir");
	if (lirc) lirc_import(r, f, &ok, skipped); else ok = flipper_parse(r, f, skipped);
	fclose(f);
	return ok;
}

static void irf_import(int k)
{
	if (irf_lirc[k] == 2) { pack_import(k); return; }
	FILE *f = fopen(irf_path[k], "r");
	if (!f) { snprintf(irf_result, sizeof irf_result, "Can't open that file"); return; }
	/* nice name: underscores -> spaces, and never the same name as a remote you already have */
	char nm[NAME_LEN]; snprintf(nm, sizeof nm, "%s", irf_name[k]);
	for (char *q = nm; *q; q++) if (*q == '_') *q = ' ';
	{ char b2[NAME_LEN]; strcpy(b2, nm); unique_remote_name(b2, nm); }
	remote_new(&cur, nm, P_NEC, 0);
	int ok = 0, skipped = 0;
	if (irf_lirc[k]) lirc_import(&cur, f, &ok, &skipped); else ok = flipper_parse(&cur, f, &skipped);
	fclose(f);
	if (!ok) { snprintf(irf_result, sizeof irf_result, "Nothing usable in that file (%d skipped)", skipped); if (cfg.last[0]) remote_load(&cur, cfg.last); else have_cur = 0; return; }
	remote_automap(&cur);
	have_cur = 1;
	save_cur();
	snprintf(irf_result, sizeof irf_result, "Made remote: %d buttons, %d skipped", ok, skipped);
	remote_screen_reset();
	go(S_REMOTE);
	ui_toast(irf_result);
}

void scr_import(void)
{
	if (irf_n < 0) irf_scan();
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_SAVE, "Import a file", "Flipper .ir, LIRC .conf, or a shared remote .txt");
	if (!irf_n) {
		gfx_round(40, 92, 560, 120, 16, C_CARD_HI, 255);
		text(F_BODYB, 60, 104, "No .ir files found.", C_TEXT, 255);
		text(F_SMALL, 60, 134, "Put .ir or .conf files in this folder on the SD card:", C_DIM, 255);
		text(F_BODYB, 60, 158, "wiirremote/import", C_ACCENT, 255);
		text(F_SMALL, 60, 184, "Free TV files: github.com/Lucaslhm/Flipper-IRDB", C_DIM, 255);
	}
	for (int i = 0; i < irf_n; i++) {
		int x = 40 + (i % 3) * 188, y = 92 + (i / 3) * 56;
		if (ui_tile(6700 + i, x, y, 180, 48, -1, irf_name[i], ST_KEY, 0) & UI_CLICK) { irf_import(i); return; }
	}
	if (irf_result[0]) text_fit(F_SMALL, 40, 392, 360, irf_result, C_DIM, 255);
	if (ui_ibutton(6790, 420, 380, 180, 40, IC_SEARCH, "Look again", ST_NORMAL) & UI_CLICK) irf_n = -1;
	ui_footer_begin();
	ui_footer_hint("A", "Import");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Copy Flipper .ir or LIRC .conf files into wiirremote/import on the SD card.",
		"Or put them in wiirremote/library: Smart match learns them, no new remote.",
		"Pick a file and it becomes a new remote with named buttons.",
		"Works with NEC, Samsung, RC5, RC6, Sony, JVC, Panasonic, Sharp and more.",
		"A friend's remote (.txt from Export to SD) works too: drop it in the same folder.",
		"Raw recordings and other codes are skipped (it tells you how many).",
		"Thousands of TV files: the Flipper-IRDB collection on GitHub.",
	};
	ui_help("Import", help, 7);
	if (!blocked && (in->down & BTN_HOME)) { irf_n = -1; go_back(); }
}

/* ================================================================== */
/* keyboard keys map                                                  */
/* ================================================================== */

extern int kb_capture, kb_waiting;
static int km_step;      /* 0 list, 1 waiting for key, 2 picking button */
static u8 km_code;
static int km_page;

void scr_keymap(void)
{
	int blocked = help_is_open() || km_step;
	if (blocked) ui_suspend(1);
	gfx_bg();
	title(IC_MENU, "Keyboard keys", usb.kb_connected ? "USB keyboard connected" : "Plug in a USB keyboard");
	int del = -1;
	for (int i = 0; i < cur.nkeys; i++) {
		int x = 40 + (i % 2) * 284, y = 90 + (i / 2) * 44;
		if (y > 330) break;
		int f = ui_item(6800 + i, x, y, 272, 38);
		gfx_round(x, y, 272, 38, 10, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, 255);
		int bw = badge(x + 8, y + 7, key_name(cur.key_code[i]), 0);
		const char *nm = cur.key_btn[i] >= 0 && cur.key_btn[i] < cur.nbtn ? cur.btn[cur.key_btn[i]].name : "-";
		text_fit(F_BODYB, x + bw + 16, y + 8, 200 - bw, nm, C_TEXT, 255);
		icon(IC_TRASH, x + 252, y + 19, 16, C_DIM);
		if (f & UI_FOCUS) ui_focus_ring(x, y, 272, 38, 10);
		if (f & UI_CLICK) del = i;
	}
	if (del >= 0) {
		undo_save("Key removed");
		memmove(&cur.key_code[del], &cur.key_code[del + 1], cur.nkeys - del - 1);
		memmove(&cur.key_btn[del], &cur.key_btn[del + 1], cur.nkeys - del - 1);
		cur.nkeys--; save_cur();
	}
	if (!cur.nkeys) {
		gfx_round(40, 90, 560, 70, 14, C_CARD_HI, 255);
		text(F_BODYB, 60, 100, "No keys mapped yet.", C_TEXT, 255);
		text(F_SMALL, 60, 128, "Arrows, Enter, Esc and Backspace always move around the app.", C_DIM, 255);
	}
	if (cur.nkeys < 32 && (ui_ibutton(6890, 40, 376, 220, 46, IC_PLUS, "Add a key", ST_NORMAL) & UI_CLICK)) {
		if (!usb.kb_connected) ui_toast("Plug in a USB keyboard first");
		else { km_step = 1; kb_waiting = 1; kb_capture = -1; }
	}
	ui_footer_begin();
	ui_footer_hint("A", "Remove");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Add a key, press it on the keyboard, then pick the TV button.",
		"Choose a key in the list to remove it (- undoes).",
		"If a key is mapped, it sends even if it's an arrow key.",
		"Keyboard job (Menu / TV / Off) is in Settings > Controllers.",
	};
	if (!km_step) ui_help("Keyboard keys", help, 4);
	if (km_step == 1) {
		ui_suspend(0); ui_dim(); ui_panel(120, 150, 400, 160);
		text_c(F_TITLE, 320, 170, "Press a key...", C_TEXT, 255);
		text_c(F_BODY, 320, 214, "on the USB keyboard", C_DIM, 255);
		if (kb_capture >= 0) { km_code = kb_capture; km_step = 2; km_page = 0; ui_set_focus(-1); }
		if (ui_ibutton(6891, 230, 250, 180, 44, IC_CLOSE, "Cancel", ST_NORMAL) & UI_CLICK) { km_step = 0; kb_waiting = 0; }
		if (in->down & (BTN_HOME | BTN_B)) { km_step = 0; kb_waiting = 0; in->down = 0; }
		return;
	}
	if (km_step == 2) {
		ui_suspend(0); ui_dim(); ui_panel(30, 30, 580, 390);
		char t[48]; snprintf(t, sizeof t, "Key %s sends...", key_name(km_code));
		text(F_TITLE, 56, 54, t, C_TEXT, 255);
		int per = 16, pages = (cur.nbtn + per - 1) / per; if (pages < 1) pages = 1;
		for (int i = km_page * per; i < cur.nbtn && i < (km_page + 1) * per; i++) {
			int k = i - km_page * per;
			RBtn *b = &cur.btn[i];
			if (ui_tile(6900 + i, 50 + (k % 4) * 136, 104 + (k / 4) * 62, 128, 54, icon_for_name(b->name), b->name, ST_KEY, 0) & UI_CLICK) {
				int slot = -1;
				for (int m = 0; m < cur.nkeys; m++) if (cur.key_code[m] == km_code) slot = m;
				if (slot < 0) slot = cur.nkeys++;
				cur.key_code[slot] = km_code; cur.key_btn[slot] = i;
				save_cur(); km_step = 0;
			}
		}
		int by = 360;
		if (pages > 1) {
			if (ui_tile(6991, 250, by, 60, 46, IC_LEFT, "", ST_NORMAL, 0) & UI_CLICK) { if (km_page) km_page--; }
			if (ui_tile(6992, 370, by, 60, 46, IC_RIGHT, "", ST_NORMAL, 0) & UI_CLICK) { if (km_page < pages - 1) km_page++; }
		}
		if (ui_ibutton(6993, 460, by, 130, 46, IC_BACK, "Cancel", ST_NORMAL) & UI_CLICK) km_step = 0;
		if (in->down & (BTN_HOME | BTN_B)) { km_step = 0; in->down = 0; }
		return;
	}
	if (!blocked && (in->down & BTN_HOME)) go_back();
}
#ifdef HOST
void host_pics(int p, int ov) { int l[PAT_BUILTIN]; for (int k = 0; k < PAT_BUILTIN; k++) l[k] = k; go(S_CAL); pics_open(l, PAT_BUILTIN, p, ov, 0); }
#endif

#ifdef HOST
/* tests only: the codes in the group being asked about (0 if not asking) */
int gt_test_window(u8 *out) { if (gt_state != 2) return 0; int n = 0; for (int i = gt_lo; i < gt_hi; i++) out[n++] = gt_code[i]; return n; }
int gt_test_state(void) { return gt_state; }
void sc_test_code(u32 idx, u8 *a, u8 *c) { int p; sc_code(idx, a, c, &p); }
u32 sc_test_total(void) { return sc_total(); }
#endif
#ifdef HOST
void sg_debug(void) { printf("sg: state %d lo %d hi %d si %d n %d next %llu now %llu grp %d\n", sg_state, sg_lo, sg_hi, sg_si, sg_n, (unsigned long long)sg_next, (unsigned long long)plat_ms(), cfg.group_test); }
#endif
