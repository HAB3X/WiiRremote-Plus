/*
   extras.c - Health log, Service values (safety net), auto TV off, tilt controls.
   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "app.h"
#include "presets.h"
#include <time.h>

/* ================================================================== */
/* CRT health log: dated results, so you can see the set age          */
/* ================================================================== */
#define HL_MAX 40
static char hl_line[HL_MAX][72];
static int hl_n = -1, hl_top;

static void hl_path(char *p, int n) { snprintf(p, n, "%s/health.txt", base_dir); }

static void hl_load(void)
{
	hl_n = 0;
	char p[128]; hl_path(p, sizeof p);
	FILE *f = fopen(p, "r");
	if (!f) return;
	char line[96];
	while (fgets(line, sizeof line, f)) {
		int l = strlen(line); while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
		if (!l) continue;
		if (hl_n == HL_MAX) { memmove(hl_line[0], hl_line[1], (HL_MAX - 1) * sizeof hl_line[0]); hl_n--; }
		snprintf(hl_line[hl_n++], sizeof hl_line[0], "%s", line);
	}
	fclose(f);
}

void health_add(const char *what)
{
	char p[128]; hl_path(p, sizeof p);
	FILE *f = fopen(p, "a");
	char stamp[24] = "";
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	if (tm && tm->tm_year > 100) strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M", tm);
	char tv[NAME_LEN + 4] = ""; if (have_cur) snprintf(tv, sizeof tv, " [%s]", cur.name);
	if (f) { fprintf(f, "%s  %s%s\n", stamp, what, tv); fclose(f); }
	hl_n = -1;
}

void health_start(void) { hl_n = -1; hl_top = 0; go(S_HEALTH); }

void scr_health(void)
{
	if (hl_n < 0) { hl_load(); hl_top = hl_n > 7 ? hl_n - 7 : 0; }
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_STAR, 58, 50, 30, C_ACCENT);
	text(F_TITLE, 86, 30, "CRT health log", C_TEXT, 255);
	text(F_SMALL, 88, 64, "Dated results: see if your TV is changing", C_DIM, 255);
	/* quick results you can log with one tap */
	static const char *Q[8] = { "Bloom OK", "Bloom grows", "Colour OK", "Colour tinted", "Shape OK", "Shape off", "Focus sharp", "Focus soft" };
	for (int k = 0; k < 8; k++) {
		int x = 40 + (k % 4) * 142, y = 90 + (k / 4) * 44;
		if (ui_button(7700 + k, x, y, 134, 38, Q[k], ST_KEY) & UI_CLICK) { health_add(Q[k]); ui_toast("Logged"); }
	}
	/* the log, newest at the bottom */
	gfx_round(40, 184, 560, 196, 14, C_INSET, 255);
	if (!hl_n) text(F_SMALL, 56, 196, "Nothing yet. The lag test and Tint Doctor log here too.", C_DIM, 255);
	int show = 7;
	if (hl_top > hl_n - show) hl_top = hl_n - show; if (hl_top < 0) hl_top = 0;
	for (int k = 0; k < show && hl_top + k < hl_n; k++) text_fit(F_SMALL, 54, 192 + k * 26, 532, hl_line[hl_top + k], k == show - 1 || hl_top + k == hl_n - 1 ? C_TEXT : C_DIM, 255);
	if (ui_ibutton(7710, 40, 388, 120, 32, IC_UP, "Older", ST_NORMAL) & UI_CLICK) { if (hl_top > 0) hl_top--; }
	if (ui_ibutton(7711, 166, 388, 120, 32, IC_DOWN, "Newer", ST_NORMAL) & UI_CLICK) { if (hl_top < hl_n - show) hl_top++; }
	if (ui_ibutton(7712, 440, 388, 160, 32, IC_PENCIL, "Type one", ST_NORMAL) & UI_CLICK) { kb_open(K_HEALTH, "", "What did you notice?", NOTE_LEN - 1); return; }
	ui_footer_begin();
	ui_footer_hint("A", "Log it");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Tap a result to save it with today's date.",
		"Do the same tests every few months and compare.",
		"Saved on the SD card: wiirremote/health.txt",
	};
	ui_help("Health log", help, 3);
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) go_back();
}

/* ================================================================== */
/* Service values: write down every setting before you change it      */
/* ================================================================== */
static int sv_sel = -1, sv_menu, sv_step, sv_top;

void svals_text_done(int purpose, const char *txt)
{
	if (purpose == K_SV_NAME) {
		if (cur.nsv >= MAX_SV) { ui_toast("48 max"); go(S_SVALS); return; }
		sv_sel = cur.nsv++;
		snprintf(cur.sv_name[sv_sel], NAME_LEN, "%s", txt);
		cur.sv_before[sv_sel][0] = cur.sv_after[sv_sel][0] = 0;
		save_cur();
		kb_open(K_SV_BEFORE, "", "Value now (before you change it)", 10);
		return;
	}
	if (sv_sel >= 0 && sv_sel < cur.nsv) {
		if (purpose == K_SV_BEFORE) snprintf(cur.sv_before[sv_sel], 12, "%s", txt);
		else snprintf(cur.sv_after[sv_sel], 12, "%s", txt);
		save_cur();
	}
	go(S_SVALS);
}

void scr_svals(void)
{
	int blocked = help_is_open() || sv_menu;
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_WRENCH, 58, 50, 30, C_ACCENT);
	text(F_TITLE, 86, 30, "Service values", C_TEXT, 255);
	text(F_SMALL, 88, 64, "Your safety net: the value before, and what you changed it to", C_DIM, 255);
	/* table */
	gfx_round(40, 90, 560, 30, 10, C_CARD_HI, 255);
	text(F_SMALL, 54, 96, "Setting", C_DIM, 255);
	text(F_SMALL, 330, 96, "Before", C_DIM, 255);
	text(F_SMALL, 450, 96, "Now", C_DIM, 255);
	int show = 6;
	if (sv_top > cur.nsv - show) sv_top = cur.nsv - show; if (sv_top < 0) sv_top = 0;
	for (int k = 0; k < show && sv_top + k < cur.nsv; k++) {
		int i = sv_top + k, y = 126 + k * 40;
		int f = ui_item(7800 + i, 40, y, 560, 36);
		gfx_round(40, y, 560, 36, 10, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD, 255);
		text_fit(F_BODYB, 54, y + 6, 260, cur.sv_name[i], C_TEXT, 255);
		text(F_BODYB, 330, y + 6, cur.sv_before[i][0] ? cur.sv_before[i] : "-", C_TEXT, 255);
		int changed = cur.sv_after[i][0] && strcmp(cur.sv_after[i], cur.sv_before[i]);
		text(F_BODYB, 450, y + 6, cur.sv_after[i][0] ? cur.sv_after[i] : "-", changed ? C_SEL : C_DIM, 255);
		if (f & UI_FOCUS) ui_focus_ring(40, y, 560, 36, 10);
		if (f & UI_CLICK) { sv_sel = i; sv_menu = 1; ui_set_focus(9000); }
	}
	if (!cur.nsv) text(F_SMALL, 54, 134, "Before changing anything in the service menu, add it here.", C_DIM, 255);
	int by = 374;
	if (ui_ibutton(7890, 40, by, 200, 40, IC_PLUS, "Add a setting", ST_NORMAL) & UI_CLICK) { kb_open(K_SV_NAME, "", "Setting name (e.g. G DRIVE)", NAME_LEN - 1); return; }
	if (cur.nsv > show) {
		if (ui_tile(7891, 250, by, 50, 40, IC_UP, "", ST_NORMAL, 0) & UI_CLICK) { if (sv_top) sv_top--; }
		if (ui_tile(7892, 306, by, 50, 40, IC_DOWN, "", ST_NORMAL, 0) & UI_CLICK) sv_top++;
	}
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Before you change a service setting, add it with its value.",
		"Then put in the new value. Changed values show in yellow.",
		"Something went wrong? Set it back to the Before value.",
	};
	if (!sv_menu) ui_help("Service values", help, 3);
	if (sv_menu && sv_sel >= 0 && sv_sel < cur.nsv) {
		static const char *it[] = { "Set the new value", "Fix the before value", "Put it back (now = before)", "Delete", "Close" };
		static const int ic[] = { IC_PENCIL, IC_PENCIL, IC_BACK, IC_TRASH, IC_CLOSE };
		int ch = menu_popup(&sv_menu, it, ic, 5, cur.sv_name[sv_sel]);
		if (ch == 0) { kb_open(K_SV_AFTER, cur.sv_after[sv_sel], "New value", 10); return; }
		if (ch == 1) { kb_open(K_SV_BEFORE, cur.sv_before[sv_sel], "Value before", 10); return; }
		if (ch == 2) { snprintf(cur.sv_after[sv_sel], 12, "%s", cur.sv_before[sv_sel]); save_cur(); ui_toast("Now set it to that on the TV too"); }
		if (ch == 3) {
			int i = sv_sel;
			memmove(cur.sv_name[i], cur.sv_name[i + 1], (cur.nsv - i - 1) * sizeof cur.sv_name[0]);
			memmove(cur.sv_before[i], cur.sv_before[i + 1], (cur.nsv - i - 1) * sizeof cur.sv_before[0]);
			memmove(cur.sv_after[i], cur.sv_after[i + 1], (cur.nsv - i - 1) * sizeof cur.sv_after[0]);
			cur.nsv--; save_cur(); sv_sel = -1;
		}
		return;
	}
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) go_back();
}

/* ================================================================== */
/* Auto TV off: the Wii has been left alone, so switch the CRT off     */
/* ================================================================== */
void auto_off_check(u64 idle_ms)
{
	static int done;
	if (!cfg.auto_off || !have_cur) { done = 0; return; }
	if (idle_ms < (u64)cfg.auto_off * 60000) { done = 0; return; }
	if (done) return;
	done = 1;
	int p = remote_find_btn(&cur, "POWER");
	if (p < 0) p = remote_find_btn(&cur, "POWER OFF");
	if (p < 0) return;
	press_btn(p, 0);
	health_add("Auto TV off (Wii left idle)");
}

/* ================================================================== */
/* Tilt: hold 1 and tilt. Right/left = volume, up/down = channel       */
/* ================================================================== */
int tilt_frame(void)
{
	/* returns 1 if 1 was used for tilting (so the tap-1 shortcut should not fire) */
	static u64 held_at, next;
	static int used, was;
	int held = (menu_in.held & BTN_1) != 0;
	u64 now = plat_ms();
	if (held && !was) { held_at = now; used = 0; next = 0; }
	was = held;
	if (!held || !cfg.tilt || !have_cur || !menu_in.tilt_ok) return 0;
	const char *want = NULL;
	if (menu_in.roll > 30) want = "VOL+";
	else if (menu_in.roll < -30) want = "VOL-";
	else if (menu_in.pitch > 35) want = "CH+";
	else if (menu_in.pitch < -35) want = "CH-";
	if (!want || now < next) return used;
	int b = remote_find_btn(&cur, want);
	if (b < 0 && want[0] == 'C') b = remote_find_btn(&cur, want[2] == '+' ? "P+" : "P-");
	if (b >= 0) { press_btn(b, 0); used = 1; next = now + (want[0] == 'V' ? 180 : 700); }
	(void)held_at;
	return used;
}
