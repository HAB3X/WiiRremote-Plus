/*
   calui.c - the Calibration Suite's guided modes (v1.9.1).

   Three ways through each area (Colour, Grey levels, Geometry, Sharpness, or everything):
   1 Guided          you do it; each step says what it is, what to change, what "right" looks like
   2 Intense guided  it asks questions (with test pictures) until it knows the fault, double-checks,
                     then tells you exactly what to change - and what to try next if that fails
   3 Ultra handhold  same, but with YOUR numbers: it keeps your originals as a backup, says which of
                     your settings to move, by how much, and narrows it down from your answers

   Everywhere: + = your TV remote on screen, - = hide everything to see the picture,
   Left = problems & fixes, Right = the settings for this step. WiiRremote Plus by Habex. GPL-3.0.
*/
#include "app.h"
#include "calkb.h"
#include "patterns.h"
#include <strings.h>

extern u32 *app_native_buf;
void pat_set_field(int col, int lvl);
void pat_set_chan(int c);
void health_add(const char *what);

enum { V_MODE, V_WARN, V_ENTRY, V_PICK, V_FLOW, V_DONE };
enum { MD_SOLO, MD_INTENSE, MD_ULTRA };
enum { FL_GUIDE, FL_QUESTION, FL_RESULT, FL_FIX, FL_ANYMORE, FL_STUCK, FL_SAFETY };

static int cu_view, cu_area, cu_mode, cu_warned, cu_after_warn;
static int fl_kind, fl_sel, fl_hidden, fl_tab, fl_tab_top, fl_q, fl_pdraw = -1, fl_hint;
static u64 fl_hint_until;
static int fl_first;   /* the "what kind of problem?" question at the start of a full check-up */
static int g_list[96], g_n, g_pos;
static int fx_fault, fx_step, fx_dir, fx_size, fx_moves, fx_nochg, fx_rev, fx_sv, fx_net, fx_orig;
static int fixed_list[16], nfixed, ruled[32], nruled;
static int ov_open, ov_sel;   /* TV remote on screen */
static int ent_sel, ent_top, pick_top, pick_sel, val_open, val_k, val_v;
static char msgbuf[200];

static const CalFault *FX(void) { return &CAL_FAULT[fx_fault]; }
static const CalSetting *SET(int s) { return &CAL_SET[s]; }
static int fix_is_check(const CalStep *st) { return st->set < 0 || (st->dir == 0 && SET(st->set)->menu != MN_SERVICE); }

/* ================================================================== */
/* small helpers                                                      */
/* ================================================================== */
/* wrap text into lines that fit */
static int wrap(const Font *f, const char *s, int w, char out[][128], int max)
{
	int n = 0; char line[128] = ""; const char *p = s;
	while (*p && n < max) {
		char word[64]; int k = 0;
		while (*p == ' ') p++;
		while (*p && *p != ' ' && k < 63) word[k++] = *p++;
		word[k] = 0;
		if (!k) break;
		char t[192]; snprintf(t, sizeof t, line[0] ? "%s %s" : "%s%s", line, word);
		if (text_w(f, t) > w && line[0]) { snprintf(out[n++], 128, "%s", line); snprintf(line, sizeof line, "%s", word); }
		else snprintf(line, sizeof line, "%s", t);
	}
#ifdef HOST
	{ void gfx_audit_note(const char *, const char *); while (*p == ' ') p++; if (*p || (line[0] && n >= max)) gfx_audit_note("too many lines", s); }
#endif
	if (line[0] && n < max) snprintf(out[n++], 128, "%s", line);
	return n;
}
static int text_block(const Font *f, int x, int y, int w, const char *s, u32 c, int max, int lh)
{
	char L[8][128]; int n = wrap(f, s, w, L, max < 8 ? max : 8);
	for (int i = 0; i < n; i++) text(f, x, y + i * lh, L[i], c, 255);
	return n;
}
static int find_sv(int s)
{
	for (int k = 0; k < cur.nsv; k++) if (cal_setting_find(cur.sv_name[k]) == s) return k;
	return -1;
}
static int sv_now(int k) { return atoi(cur.sv_after[k][0] ? cur.sv_after[k] : cur.sv_before[k]); }

/* ================================================================== */
/* the TV remote, anywhere in the suite                               */
/* ================================================================== */
static int ov_list(int *out, int max)
{
	int n = 0;
	if (!have_cur) return 0;
	static const char *FIRST[] = { "SERVICE", "MENU", "UP", "DOWN", "LEFT", "RIGHT", "OK", "BACK", "EXIT", "VOL+", "VOL-", "P+", "P-", "CH+", "CH-", "INFO", "PICTURE", "POWER" };
	for (size_t k = 0; k < sizeof FIRST / sizeof FIRST[0] && n < max; k++) {
		int i = remote_find_btn(&cur, FIRST[k]);
		if (i >= 0) { int dup = 0; for (int j = 0; j < n; j++) dup |= out[j] == i; if (!dup) out[n++] = i; }
	}
	for (int i = 0; i < cur.nbtn && n < max; i++) {
		int dup = 0; for (int j = 0; j < n; j++) dup |= out[j] == i;
		if (!dup && !cur.btn[i].dead) out[n++] = i;
	}
	return n;
}
/* returns 1 while it's open (and takes the buttons). W x H = where to draw, pointer in those units */
int calui_remote_overlay(int W, int H, int pv, int px, int py)
{
	u32 d = in->down;
	if (!ov_open) {
		if (d & BTN_PLUS) { if (!have_cur) ui_toast("Pick your TV remote first"); else { ov_open = 1; ov_sel = 0; } in->down &= ~BTN_PLUS; }
		if (!ov_open) return 0;
		d = 0;   /* the press that opened it doesn't also close or send */
	}
	int L[24], n = ov_list(L, 18);
	int cols = 5, bw = (W - 48) / cols, rows = (n + cols - 1) / cols; if (rows < 1) rows = 1;
	int ph = 40 + rows * 40, y0 = H - ph - 12;
	gfx_round(12, y0, W - 24, ph, 16, C_HUD, 235);
	text(F_SMALL, 26, y0 + 6, "Your TV remote", RGB(190, 196, 210), 255);
	text_r(F_SMALL, W - 26, y0 + 6, "A send   + / B close", RGB(190, 196, 210), 255);
	if (d & BTN_LEFT) ov_sel = n ? (ov_sel + n - 1) % n : 0;
	if (d & BTN_RIGHT) ov_sel = n ? (ov_sel + 1) % n : 0;
	if (d & BTN_UP) ov_sel = ov_sel >= cols ? ov_sel - cols : ov_sel;
	if (d & BTN_DOWN) ov_sel = ov_sel + cols < n ? ov_sel + cols : ov_sel;
	int hit = -1;
	for (int k = 0; k < n; k++) {
		int x = 24 + (k % cols) * bw, y = y0 + 30 + (k / cols) * 40;
		if (pv && px >= x && px < x + bw - 6 && py >= y && py < y + 34) { hit = k; ov_sel = k; }
		gfx_round(x, y, bw - 6, 34, 10, k == ov_sel ? C_ACCENT : RGB(58, 64, 82), 255);
		text_fit(F_SMALL, x + 8, y + 7, bw - 20, cur.btn[L[k]].name, C_WHITE, 255);
	}
	if (!n) text(F_BODYB, 26, y0 + 36, "This remote has no buttons yet", C_WHITE, 255);
	if ((d & BTN_A) && n && (!pv || hit >= 0)) press_btn(L[ov_sel], BTN_A);
	if (d & (BTN_PLUS | BTN_B | BTN_HOME)) ov_open = 0;
	in->down = 0;
	return 1;
}

/* ================================================================== */
/* starting                                                           */
/* ================================================================== */
void calui_suite_enter(void) { cu_warned = 0; ov_open = 0; }
int calui_remote_is_open(void) { return ov_open; }

void calui_start(int area)
{
	cu_area = area; cu_view = V_MODE; ov_open = 0;
	ui_set_focus(7700);
	go(S_CALFLOW);
}

static void flow_begin(void);
static void choose_mode(int m)
{
	cu_mode = m;
	if (!cu_warned) { cu_view = V_WARN; cu_after_warn = m; ui_set_focus(7720); return; }
	if (m == MD_ULTRA) { cu_view = V_ENTRY; ent_sel = 0; ent_top = 0; ui_set_focus(7740); return; }
	flow_begin();
}

/* ---- the flow's first screen ---- */
static void next_question(void);
static void flow_begin(void)
{
	fl_hidden = 0; fl_tab = 0; fl_sel = 0; fl_pdraw = -1; nfixed = 0; nruled = 0; fl_first = 0; fl_hint_until = plat_ms() + 5000;
	if (cu_mode == MD_SOLO) {
		g_n = 0;
		for (int a = 0; a < AR_COUNT; a++) for (int i = 0; i < CAL_NGUIDE && g_n < 96; i++)
			if (CAL_GUIDE[i].area == a && (cu_area == AR_ALL || cu_area == a)) g_list[g_n++] = i;
		g_pos = 0; fl_kind = FL_GUIDE;
	} else {
		cal_dx_start(cu_area);
		fl_kind = FL_SAFETY; fl_sel = 0;   /* first: anything dangerous? */
	}
	cu_view = V_FLOW;
}

static void dx_restart_keep(void)
{
	cal_dx_start(cu_area);
	for (int i = 0; i < nfixed; i++) cal_dx_exclude(fixed_list[i]);
	for (int i = 0; i < nruled; i++) cal_dx_exclude(ruled[i]);
}

static void next_question(void)
{
	fl_sel = 0; fl_pdraw = -1;
	fl_q = cal_dx_next();
	if (fl_q >= 0) { fl_kind = FL_QUESTION; return; }
	int f[3]; float p[3];
	int n = cal_dx_top(f, p, 3);
	if (!n || p[0] < 0.25f) { fl_kind = FL_STUCK; return; }
	fx_fault = f[0]; fl_kind = FL_RESULT;
}

/* ================================================================== */
/* fixing: one step at a time, reading your answers                   */
/* ================================================================== */
static void fix_step_begin(void)
{
	const CalStep *st = &FX()->fix[fx_step];
	fx_dir = st->dir; fx_size = 2; fx_moves = 0; fx_nochg = 0; fx_rev = 0; fx_net = 0; fx_sv = -1; fx_orig = 0;
	if (st->set >= 0 && SET(st->set)->menu == MN_SERVICE && cu_mode == MD_ULTRA) {
		fx_sv = find_sv(st->set);
		if (fx_sv >= 0) fx_orig = sv_now(fx_sv);
	}
	fl_sel = 0; fl_pdraw = -1;
}
static void fix_start(int f) { fx_fault = f; fx_step = 0; fl_kind = FL_FIX; fix_step_begin(); }

static void fix_next_step(void)
{
	/* put this setting back if it didn't help (Ultra knows the number) */
	(void)fx_orig;
	fx_step++;
	/* Ultra: skip service settings your TV doesn't have */
	while (fx_step < FX()->nfix && cu_mode == MD_ULTRA) {
		const CalStep *st = &FX()->fix[fx_step];
		if (st->set >= 0 && SET(st->set)->menu == MN_SERVICE && find_sv(st->set) < 0 && cur.nsv > 0) { fx_step++; continue; }
		break;
	}
	if (fx_step >= FX()->nfix) {
		/* nothing here fixed it: this cause is ruled out, look for the next one */
		if (nruled < 32) ruled[nruled++] = fx_fault;
		snprintf(msgbuf, sizeof msgbuf, "%s: the settings didn't fix it.%s Looking for another cause.", FX()->name, FX()->hw ? " It may be a part wearing out (see the last step)." : "");
		ui_toast("Trying the next most likely cause");
		dx_restart_keep();
		next_question();
		return;
	}
	fix_step_begin();
}

static void fix_fixed(void)
{
	char h[80]; snprintf(h, sizeof h, "Fixed: %s", FX()->name); health_add(h);
	if (nfixed < 16) fixed_list[nfixed++] = fx_fault;
	fl_kind = FL_ANYMORE; fl_sel = 0; fl_pdraw = -1;
}

/* your answer after turning a setting: 0 fixed, 1 better but not right, 2 nothing changes, 3 worse */
static void fix_react(int r)
{
	const CalStep *st = &FX()->fix[fx_step];
	if (r == 0) { fix_fixed(); return; }
	if (fix_is_check(st)) { fix_next_step(); return; }   /* a check that didn't help */
	if (r == 1) { fx_net = 0; ui_toast("Leave it there. Next setting"); fix_next_step(); return; }
	if (r == 2 || fx_rev) { ui_toast("Put it back where it was. Next setting"); fix_next_step(); return; }
	/* worse: the other way, once */
	fx_rev = 1; fx_dir = fx_dir ? -fx_dir : -1;
	fl_pdraw = -1;
}

/* ================================================================== */
/* the picture view (questions, steps, fixes)                         */
/* ================================================================== */
static u32 *pbuf, *obuf, *vbuf;
static CalPic cur_pic(void)
{
	switch (fl_kind) {
	case FL_GUIDE: return CAL_GUIDE[g_list[g_pos]].pic;
	case FL_QUESTION: return cal_q_pic(fl_q);
	case FL_FIX: { const CalStep *st = &FX()->fix[fx_step]; if (st->set >= 0) { CalPic p = cal_setting_pic(st->set); if (p.pat != 255) return p; } return FX()->pic; }
	case FL_RESULT: return FX()->pic;
	default: { CalPic p = { PAT_SMPTE, 0, 0 }; return p; }
	}
}
static void set_pic_options(CalPic p)
{
	if (p.pat == PAT_FIELD) pat_set_field(p.a, p.b ? p.b : 255);
	if (p.pat == PAT_CHANNEL) pat_set_chan(p.a);
}

/* the buttons of the current panel */
#define MAXB 10
static const char *btn_lab[MAXB]; static int nbtn, btn_cols;
static void panel_buttons(void)
{
	nbtn = 0; btn_cols = 3;
	static const char *GUIDE_B[] = { "Back", "Done, next", "Finish" };
	static const char *RESULT_B[] = { "Fix it", "Not it", "More info" };
	static const char *STOP_B[] = { "OK, it's off", "Not it", "More info" };
	static const char *NORMAL_B[] = { "OK", "Not it", "More info" };
	static const char *FIX_B[] = { "Fixed", "Closer", "No change", "Worse" };
	static const char *CHECK_B[] = { "Fixed", "Didn't help", "Skip" };
	static const char *ANY_B[] = { "Colours or greys", "Size, shape, position", "Blur or outlines", "Too dark / too bright", "Unsteady, noise, sound", "Something else", "No, all good" };
	static const char *STUCK_B[] = { "Guide me", "Start again", "Finish" };
	static const char *SAFE_B[] = { "None of these", "Only a line, no picture", "Crackling or burning smell", "Flashes, then turns off" };
	switch (fl_kind) {
	case FL_GUIDE: for (int i = 0; i < 3; i++) btn_lab[nbtn++] = GUIDE_B[i]; break;
	case FL_QUESTION: {
		int n = cal_q_nopt(fl_q);
		for (int i = 0; i < n; i++) btn_lab[nbtn++] = cal_q_opt(fl_q, i);
		btn_lab[nbtn++] = "Not sure";
		btn_cols = nbtn <= 5 ? nbtn : (nbtn + 1) / 2;
		break; }
	case FL_RESULT: for (int i = 0; i < 3; i++) btn_lab[nbtn++] = FX()->hw == 2 ? STOP_B[i] : FX()->hw == 3 ? NORMAL_B[i] : RESULT_B[i]; break;
	case FL_FIX: {
		const CalStep *st = &FX()->fix[fx_step];
		if (fix_is_check(st)) for (int i = 0; i < 3; i++) btn_lab[nbtn++] = CHECK_B[i];
		else { for (int i = 0; i < 4; i++) btn_lab[nbtn++] = FIX_B[i]; btn_cols = 4; }
		break; }
	case FL_ANYMORE: for (int i = 0; i < 7; i++) btn_lab[nbtn++] = (i == 6 && fl_first) ? "Not sure, ask me" : ANY_B[i]; btn_cols = 4; break;
	case FL_STUCK: for (int i = 0; i < 3; i++) btn_lab[nbtn++] = STUCK_B[i]; break;
	case FL_SAFETY: for (int i = 0; i < 4; i++) btn_lab[nbtn++] = SAFE_B[i]; btn_cols = 2; break;
	}
}

/* fewer columns until every answer fits in full */
static int btn_cols_fit(int NW)
{
	int gap = 5;
	while (btn_cols > 2) {
		int w = (NW - 56 - (btn_cols - 1) * gap) / btn_cols, ok = 1;
		for (int k = 0; k < nbtn; k++) if (text_w(F_SMALL, btn_lab[k]) + 36 > w) ok = 0;
		if (ok) break;
		btn_cols--;
	}
	return btn_cols;
}
static void on_button(int b);
/* the name your TV's menu uses: yours (Ultra) or the usual label */
static const char *code_for(int set)
{
	int k = find_sv(set);
	return k >= 0 ? cur.sv_name[k] : cal_setting_code(set);
}
/* two lines: "Title: description" and the question (or instruction) */
static void panel_text(char *ttl, int tn, char *body, int bn)
{
	ttl[0] = body[0] = 0;
	switch (fl_kind) {
	case FL_GUIDE: {
		const CalGuide *g = &CAL_GUIDE[g_list[g_pos]];
		snprintf(ttl, tn, "%s %d/%d: %s", AREA_NAME[g->area], g_pos + 1, g_n, g->title);
		snprintf(body, bn, "%s", g->text);
		break; }
	case FL_QUESTION: {
		char b[200];
		snprintf(ttl, tn, fl_q >= CAL_NQ ? "Question %d: checking" : "Question %d", cal_dx_asked() + 1);
		snprintf(body, bn, "%s", cal_q_text(fl_q, b, sizeof b));
		break; }
	case FL_RESULT: {
		int f[1]; float p[1]; cal_dx_top(f, p, 1);
		if (FX()->hw == 2) { snprintf(ttl, tn, "STOP: %s", FX()->name); snprintf(body, bn, "%s", FX()->fix[0].how); }
		else if (FX()->hw == 3) { snprintf(ttl, tn, "Normal, not a fault: %s", FX()->name); snprintf(body, bn, "%s", FX()->why); }
		else {
			int pc = (int)(p[0] * 100 + 0.5f); if (pc > 99) pc = 99;
			snprintf(ttl, tn, "Found it (%d%% sure): %s", pc, FX()->name);
			/* short: the settings and which way, e.g. "Turn G-BIA down and G-DRV up." */
			int n = 0; char parts[2][120]; s16 used[2];
			for (int i = 0; i < FX()->nfix && n < 2; i++) {
				const CalStep *st = &FX()->fix[i];
				if (st->set < 0 || st->dir == 0 || SET(st->set)->menu > MN_SERVICE) continue;
				if (n && used[0] == st->set) continue;
				char cd[32]; snprintf(cd, sizeof cd, "%s", code_for(st->set)); char *br = strstr(cd, " ("); if (br) *br = 0;
				snprintf(parts[n], 120, "%s %s until %s", cd, st->dir > 0 ? "up" : "down", cal_setting_until(st->set)); used[n++] = st->set;
			}
			if (n == 2) snprintf(body, bn, "Turn %s. Then %s.", parts[0], parts[1]);
			else if (n == 1) snprintf(body, bn, "Turn %s.", parts[0]);
			else snprintf(body, bn, "%s.", FX()->fix[FX()->nfix > 1 && FX()->fix[0].set == -2 ? 1 : 0].how);
		}
		break; }
	case FL_FIX: {
		const CalStep *st = &FX()->fix[fx_step];
		snprintf(ttl, tn, "Step %d of %d: %s", fx_step + 1, FX()->nfix, FX()->name);
		if (fix_is_check(st)) { snprintf(body, bn, "%s.", st->how); break; }
		const CalSetting *s = SET(st->set);
		const char *where = s->menu == MN_SERVICE ? "Service menu" : s->menu == MN_USER ? "Menu" : "";
		char now[24] = ""; if (fx_sv >= 0) snprintf(now, sizeof now, " (now %d)", sv_now(fx_sv));
		if (fx_dir == 0) snprintf(body, bn, "%s: %s%s: turn it up or down until %s.", where, code_for(st->set), now, cal_setting_until(st->set));
		else snprintf(body, bn, "%s%s: %s%s: turn %s until %s.", fx_rev ? "Put it back, then " : "", where, code_for(st->set), now, fx_dir > 0 ? "up" : "down", cal_setting_until(st->set));
		break; }
	case FL_ANYMORE:
		if (fl_first) { snprintf(ttl, tn, "Full check-up"); snprintf(body, bn, "What looks wrong? Pick the closest."); break; }
		snprintf(ttl, tn, "Fixed: %s", nfixed ? CAL_FAULT[fixed_list[nfixed - 1]].name : "");
		snprintf(body, bn, "Anything else wrong? Pick what kind.");
		break;
	case FL_SAFETY:
		snprintf(ttl, tn, "Safety check: before we start");
		snprintf(body, bn, "Is any of these happening?");
		break;
	case FL_STUCK:
		snprintf(ttl, tn, "Not sure yet");
		snprintf(body, bn, "The answers don't point at one fault. Try the step-by-step guide, or start again.");
		break;
	}
}

static void on_button(int b)
{
	switch (fl_kind) {
	case FL_GUIDE:
		if (b == 0 && g_pos > 0) g_pos--;
		else if (b == 1) { if (g_pos < g_n - 1) g_pos++; else { cu_view = V_DONE; } }
		else if (b == 2) cu_view = V_DONE;
		fl_pdraw = -1; break;
	case FL_QUESTION:
		cal_dx_answer(fl_q, b >= cal_q_nopt(fl_q) ? ANS_UNSURE : b);
		next_question(); break;
	case FL_RESULT:
		if (b == 0 && FX()->hw == 2) { char h[80]; snprintf(h, sizeof h, "Switched off: %s", FX()->name); health_add(h); cu_view = V_DONE; }
		else if (b == 0 && FX()->hw == 3) fix_fixed();
		else if (b == 0) fix_start(fx_fault);
		else if (b == 1) { cal_dx_answer(CAL_NQ + fx_fault, ANS_NO); next_question(); }
		else { fl_tab = 1; fl_tab_top = 0; }
		break;
	case FL_FIX: {
		const CalStep *st = &FX()->fix[fx_step];
		if (fix_is_check(st)) { if (b == 0) fix_fixed(); else fix_next_step(); }
		else fix_react(b);
		break; }
	case FL_ANYMORE:
		if (b == 6 && !fl_first) { cu_view = V_DONE; break; }
		fl_first = 0;
		dx_restart_keep();
		if (b < 5) {
			/* you've said which kind: answer the opening questions for you */
			static const char *KEY[5] = { "colours or tints", "size, shape", "blurriness", "how dark or bright", "unsteady" };
			for (int q = 0; q < CAL_NQ; q++) if (CAL_Q[q].gate && !strstr(CAL_Q[q].text, "colourful things"))
				cal_dx_answer(q, strstr(CAL_Q[q].text, KEY[b]) ? ANS_YES : ANS_NO);
		}
		next_question();
		break;
	case FL_SAFETY: {
		static const char *NM[4] = { "", "Just a bright line across the middle", "Crackling, sizzling or a burning smell", "Flash, then it turns itself off" };
		if (b == 0) {
			if (cu_area == AR_ALL) { fl_first = 1; fl_kind = FL_ANYMORE; fl_sel = 0; }   /* ask what kind first: far fewer questions */
			else next_question();
			break;
		}
		for (int f = 0; f < CAL_NFAULT; f++) if (!strcmp(CAL_FAULT[f].name, NM[b])) { fx_fault = f; fl_kind = FL_RESULT; fl_sel = 0; }
		break; }
	case FL_STUCK:
		if (b == 0) { cu_mode = MD_SOLO; flow_begin(); }
		else if (b == 1) { dx_restart_keep(); next_question(); if (fl_kind == FL_STUCK) { cal_dx_start(cu_area); next_question(); } }
		else cu_view = V_DONE;
		break;
	}
}

/* ---- the side tabs: Left = problems & fixes, Right = settings ---- */
static int tab_items(char T[][64], char D[][200], int max)
{
	int n = 0;
	int area = cu_area;
	if (fl_kind == FL_GUIDE) area = CAL_GUIDE[g_list[g_pos]].area;
	else if (fl_kind == FL_FIX || fl_kind == FL_RESULT) area = FX()->area;
	if (fl_tab == 1) {
		/* problems: the ones it suspects first, then the notes */
		int f[3]; float p[3]; int k = (fl_kind == FL_QUESTION || fl_kind == FL_RESULT) ? cal_dx_top(f, p, 3) : 0;
		for (int i = 0; i < k && n < max; i++) if (p[i] > 0.05f) {
			snprintf(T[n], 64, "%s", CAL_FAULT[f[i]].name);
			snprintf(D[n], 200, "%d%% likely. %s Fix: %s", (int)(p[i] * 100 + 0.5f), CAL_FAULT[f[i]].looks, CAL_FAULT[f[i]].fix[0].how); n++;
		}
		for (int i = 0; i < CAL_NNOTE && n < max; i++) if (area == AR_ALL || CAL_NOTE[i].area == area || CAL_NOTE[i].area == AR_ALL) {
			snprintf(T[n], 64, "%s", CAL_NOTE[i].seen); snprintf(D[n], 200, "%s", CAL_NOTE[i].try_this); n++;
		}
	} else {
		/* settings: this step's first, then the area's (normal menu, service menu, knobs, Wii) */
		s16 rel[8]; int nr = 0;
		if (fl_kind == FL_GUIDE) for (int i = 0; i < 4; i++) { if (CAL_GUIDE[g_list[g_pos]].set[i] >= 0) rel[nr++] = CAL_GUIDE[g_list[g_pos]].set[i]; }
		if (fl_kind == FL_FIX || fl_kind == FL_RESULT) for (int i = 0; i < FX()->nfix && nr < 8; i++) if (FX()->fix[i].set >= 0) rel[nr++] = FX()->fix[i].set;
		if (fl_kind == FL_QUESTION) { int f[3]; float p[3]; int k = cal_dx_top(f, p, 3); for (int i = 0; i < k && nr < 8; i++) if (CAL_FAULT[f[i]].fix[0].set >= 0) rel[nr++] = CAL_FAULT[f[i]].fix[0].set; }
		s16 L[80]; int nl = cal_settings_for(area, rel, nr, L, 80);
		for (int i = 0; i < nl && n < max; i++) {
			const CalSetting *s = SET(L[i]);
			int k = find_sv(L[i]);
			const char *code = k >= 0 ? cur.sv_name[k] : cal_setting_code(L[i]);
			snprintf(T[n], 64, "%s%s|%s", i < nr ? "> " : "", code, MENU_NAME[s->menu]);
			int same = !strcasecmp(code, s->name);
			char now[24] = ""; if (k >= 0) snprintf(now, sizeof now, "Yours: %s. ", cur.sv_after[k][0] ? cur.sv_after[k] : cur.sv_before[k]);
			snprintf(D[n], 200, "%s%s%s%s%s%s%s%s", now, same ? "" : s->name, same ? "" : ": ", s->what, s->up[0] ? " Up: " : "", s->up, s->up[0] ? "." : "", s->risk == 2 ? " Write the number down first." : "");
			n++;
		}
	}
	return n;
}
static void draw_tab(int W, int H)
{
	static char T[48][64], D[48][200];
	int n = tab_items(T, D, 48);
	int w = W / 2 + 40, x = fl_tab == 1 ? 10 : W - w - 10, y = 10, h = H - 20;
	gfx_round(x, y, w, h, 16, C_HUD, 240);
	text(F_BODYB, x + 14, y + 8, fl_tab == 1 ? "Problems & fixes" : "Settings for this", C_ACCENT, 255);
	text_r(F_SMALL, x + w - 14, y + 12, "Up / Down scroll", RGB(170, 176, 190), 255);
	int yy = y + 40;
	if (fl_tab_top > n - 1) fl_tab_top = n > 0 ? n - 1 : 0;
	for (int i = fl_tab_top; i < n && yy < y + h - 100; i++) {
		/* "title|menu": the menu goes on the right, dimmer */
		char tt[64]; snprintf(tt, sizeof tt, "%s", T[i]); char *bar = strchr(tt, '|');
		if (bar) { *bar = 0; text_r(F_SMALL, x + w - 14, yy, bar + 1, RGB(150, 156, 170), 255); }
		text_fit(F_BODYB, x + 14, yy - 2, bar ? w - 28 - text_w(F_SMALL, bar + 1) - 12 : w - 28, tt, C_WHITE, 255);
		yy += 22;
		int k = text_block(F_SMALL, x + 22, yy, w - 40, D[i], RGB(180, 186, 200), 6, 18);
		yy += k * 18 + 8;
	}
	text(F_SMALL, x + 14, y + h - 28, fl_tab == 1 ? "Right: close" : "Left: close", RGB(170, 176, 190), 255);
}

static void flow_picture(void)
{
	int NW = plat_native_w(), NH = plat_native_h();
	int LH = NH < 400 ? NH * 2 : NH;
	if (!pbuf) { pbuf = malloc(640 * 576 * 4); obuf = malloc(640 * 576 * 4); vbuf = malloc(640 * 576 * 4); }
	int pv = menu_in.ptr_valid, px = menu_in.px * NW / SCR_W, py = menu_in.py * LH / SCR_H;

	/* the picture */
	CalPic pic = cur_pic();
	int key = pic.pat * 65536 + pic.a * 256 + pic.b;
	static int drawn_h;
	if (fl_pdraw != key || drawn_h != NH || pat_animated(pic.pat)) {
		set_pic_options(pic);
		gfx_target(pbuf, NW, NH); pat_draw(pic.pat);
		fl_pdraw = key; drawn_h = NH;
	}
	memcpy(obuf, pbuf, NW * NH * 4);
	if (LH != NH) { for (int y = 0; y < NH; y++) { memcpy(vbuf + 2 * y * NW, pbuf + y * NW, NW * 4); memcpy(vbuf + (2 * y + 1) * NW, pbuf + y * NW, NW * 4); } gfx_target(vbuf, NW, LH); }
	else gfx_target(obuf, NW, NH);
	app_native_buf = obuf;

	u32 d = in->down;
	int remote = calui_remote_overlay(NW, LH, pv, px, py);
	if (!remote) {
		/* - hides everything; any button brings it back */
		if (d & BTN_MINUS) { fl_hidden ^= 1; fl_hint = 150; d = 0; }
		else if (fl_hidden && (d & (BTN_A | BTN_B | BTN_UP))) { fl_hidden = 0; d = 0; }
		/* tabs */
		if (!fl_hidden && fl_tab) {
			if ((fl_tab == 1 && (d & BTN_RIGHT)) || (fl_tab == 2 && (d & BTN_LEFT)) || (d & BTN_B)) fl_tab = 0;
			if (d & BTN_UP) { if (fl_tab_top > 0) fl_tab_top--; }
			if (d & BTN_DOWN) fl_tab_top++;
			d = 0;
		} else if (!fl_hidden) {
			/* the D-pad walks the answers; off the left edge = problems tab, off the right = settings,
			   Down from the bottom row hides the panel (Up brings it back) */
			panel_buttons();
			int cols = btn_cols_fit(plat_native_w());
			int col = fl_sel % cols, row = fl_sel / cols, rows = (nbtn + cols - 1) / cols;
			if (d & BTN_LEFT) { if (col == 0) { fl_tab = 1; fl_tab_top = 0; } else fl_sel--; d = 0; }
			else if (d & BTN_RIGHT) { if (col == cols - 1 || fl_sel == nbtn - 1) { fl_tab = 2; fl_tab_top = 0; } else fl_sel++; d = 0; }
			else if (d & BTN_DOWN) { if (row < rows - 1) { fl_sel += cols; if (fl_sel >= nbtn) fl_sel = nbtn - 1; } else { fl_hidden = 1; fl_hint = 120; } d = 0; }
			else if (d & BTN_UP) { if (row > 0) fl_sel -= cols; d = 0; }
		}
		if (d & BTN_HOME) { cu_view = V_DONE; fl_tab = 0; in->down = 0; gfx_target(NULL, 0, 0); return; }
		if (!fl_hidden && !fl_tab && (d & BTN_B) && fl_kind == FL_QUESTION && cal_dx_asked()) { cal_dx_undo(); next_question(); ui_toast("Answer taken back"); d = 0; }
	}

	if (!fl_hidden && !fl_tab && !remote) {
		/* the panel along the bottom: "Title: description", the question, the answers */
		panel_buttons();
		char ttl[160], body[260];
		panel_text(ttl, sizeof ttl, body, sizeof body);
		char BL[4][128]; int nb = wrap(F_BODYB, body, NW - 56, BL, 3);
		int bh = 30, gap = 5;
		btn_cols = btn_cols_fit(NW);
		int rows = (nbtn + btn_cols - 1) / btn_cols;
		int ph = 8 + 20 + nb * 22 + 6 + rows * (bh + gap) + 4;
		int y0 = LH - ph - 8;
		gfx_round(12, y0, NW - 24, ph, 14, C_HUD, 232);
		int y = y0 + 7;
		text_fit(F_SMALL, 28, y, NW - 56, ttl, C_ACCENT, 255); y += 20;
		for (int i = 0; i < nb; i++) { text(F_BODYB, 28, y, BL[i], C_WHITE, 255); y += 22; }
		y += 6;
		/* answers: pointer, or Up / Down (or Left / Right in one row) and A */
		if (fl_sel >= nbtn) fl_sel = 0;
		int bw = (NW - 56 - (btn_cols - 1) * gap) / btn_cols, hit = -1;
		for (int k = 0; k < nbtn; k++) {
			int x = 28 + (k % btn_cols) * (bw + gap), by = y + (k / btn_cols) * (bh + gap);
			if (pv && px >= x && px < x + bw && py >= by && py < by + bh) { hit = k; fl_sel = k; }
			gfx_round(x, by, bw, bh, 9, k == fl_sel ? C_ACCENT : RGB(58, 64, 82), 255);
			int tx = x + 8;
			/* colour chips next to the tint answers */
			if (fl_kind == FL_QUESTION && fl_q < CAL_NQ && cal_q_nopt(fl_q) == 7 && !strcmp(cal_q_opt(fl_q, 1), "Green") && k >= 1 && k <= 6) {
				static const u32 CH[7] = { 0, RGB(40, 170, 60), RGB(210, 80, 190), RGB(60, 90, 230), RGB(210, 170, 40), RGB(220, 60, 40), RGB(40, 190, 200) };
				gfx_round(x + 6, by + 8, 14, 14, 4, CH[k], 255); tx = x + 24;
			}
			text_fit(F_SMALL, tx, by + 6, bw - (tx - x) - 6, btn_lab[k], C_WHITE, 255);
		}
		/* the controls, shown for a few seconds at the start */
		if (plat_ms() < fl_hint_until) {
			const char *m = "Down: hide    Left / Right: answers, then tabs    + TV remote";
			int w = text_w(F_SMALL, m) + 24;
			gfx_round(NW / 2 - w / 2, 10, w, 26, 13, C_HUD, 210);
			text_c(F_SMALL, NW / 2, 13, m, C_WHITE, 255);
		}
		if ((d & BTN_A) && (!pv || hit >= 0)) on_button(fl_sel);
	} else if (fl_tab && !remote) draw_tab(NW, LH);
	else if (fl_hidden && fl_hint > 0) {
		fl_hint--;
		const char *m = "Hidden. Up (or A) brings it back";
		int w = text_w(F_SMALL, m) + 28;
		gfx_round(NW / 2 - w / 2, 14, w, 28, 14, C_HUD, 220);
		text_c(F_SMALL, NW / 2, 17, m, C_WHITE, 255);
	}
	if (LH != NH) {
		for (int y = 0; y < NH; y++) {
			u32 *a0 = vbuf + (2 * y) * NW, *a1 = a0 + NW, *o = obuf + y * NW;
			for (int x = 0; x < NW; x++) { u32 p0 = a0[x], p1 = a1[x]; o[x] = p0 == p1 ? p0 : (((p0 >> 1) & 0x7F7F7F7F) + ((p1 >> 1) & 0x7F7F7F7F)); }
		}
	}
	gfx_target(NULL, 0, 0);
	in->down = 0;
}
int calui_in_picture(void) { return screen == S_CALFLOW && cu_view == V_FLOW; }
#ifdef HOST
/* layout audit: show one question / result / fix step / guide step */
void calui_test_show(int kind, int a, int b)
{
	cu_view = V_FLOW; fl_hidden = 0; fl_tab = 0; fl_sel = 0; fl_pdraw = -1; fl_first = 0;
	fl_kind = kind;
	if (kind == FL_QUESTION) fl_q = a;
	if (kind == FL_RESULT || kind == FL_FIX) { fx_fault = a; if (kind == FL_FIX) { fx_step = b; fix_step_begin(); } }
	if (kind == FL_GUIDE) { g_n = CAL_NGUIDE; for (int i = 0; i < g_n; i++) g_list[i] = i; g_pos = a; }
	if (kind == 7) { fl_kind = FL_ANYMORE; fl_first = 1; }
	if (screen != S_CALFLOW) go(S_CALFLOW);
}
void calui_test_tab(int t) { fl_tab = t; fl_tab_top = 0; }
int calui_test_sel(void) { return fl_sel; }
void calui_test_setsel(int k) { fl_sel = k; }
void calui_test_state(int *view, int *kind, int *q, int *fault, int *step) { *view = cu_view; *kind = fl_kind; *q = fl_q; *fault = fx_fault; *step = fx_step; }
#endif

/* ================================================================== */
/* the menu screens                                                   */
/* ================================================================== */
static void heading(int ic, const char *t, const char *sub)
{
	icon(ic, 58, 50, 30, C_ACCENT);
	text_fit(F_TITLE, 86, 28, 440, t, C_TEXT, 255);
	if (sub) text_fit(F_SMALL, 88, 64, 460, sub, C_DIM, 255);
}

static void view_mode(int blocked)
{
	char t[48]; snprintf(t, sizeof t, cu_area == AR_ALL ? "Full check-up" : "%s calibration", cu_area == AR_ALL ? "" : AREA_NAME[cu_area]);
	heading(IC_PALETTE, t, have_cur ? cur.name : "No TV remote: you'll change settings with your own remote");
	static const char *NAME[4] = { "Guided", "Intense guided", "Ultra handhold", "Just the test pictures" };
	static const char *DESC[4] = {
		"Step by step: what to change, and when to stop.",
		"Answer a few questions. It finds the fault and says what to change.",
		"Like Intense, with your own numbers. Backs up your originals.",
		"Browse this area's test pictures on your own.",
	};
	static const int IC[4] = { IC_PLAY, IC_SEARCH, IC_STAR, IC_MENU };
	int nm = cu_area == AR_ALL ? 3 : 4;
	for (int k = 0; k < nm; k++) {
		int h = nm == 4 ? 68 : 80, y = 96 + k * (h + 10);
		int f = ui_item(7700 + k, 40, y, 560, h);
		gfx_round(40, y, 560, h, 16, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, 255);
		gfx_round_line(40, y, 560, h, 16, 1, C_LINE, 255);
		icon(IC[k], 80, y + h / 2, 28, C_ACCENT);
		int ty = y + h / 2 - 25;   /* title and one line under it, centred in the card */
		text(F_BODYB, 114, ty, NAME[k], C_TEXT, 255);
		text_fit(F_SMALL, 114, ty + 29, 470, DESC[k], C_DIM, 255);
		if (f & UI_FOCUS) ui_focus_ring(40, y, 560, h, 16);
		if ((f & UI_CLICK) && !blocked) { if (k == 3) { cal_open_group(cu_area); return; } choose_mode(k); return; }
	}
	ui_footer_begin();
	ui_footer_hint("A", "Start");
	ui_footer_hint("+", "TV remote");
	ui_footer_hint("HOME", "Back");
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) go(S_CAL);
}

static void view_warn(int blocked)
{
	heading(IC_WRENCH, "Before you start", "Please read: service menus can damage the picture");
	gfx_round(40, 92, 560, 214, 16, C_SOFT_BAD, 255);
	gfx_round_line(40, 92, 560, 214, 16, 2, C_BAD, 255);
	icon(IC_CLOSE, 70, 124, 26, C_BAD);
	text(F_BODYB, 96, 106, "The service menu is for technicians", C_TEXT, 255);
	int y = 140;
	static const char *L[] = {
		"Flipping through it can change things with no undo: geometry, the tube's voltages, factory options.",
		"WRITE DOWN every number before you change it (Ultra handhold saves them for you).",
		"Change one thing at a time, 1-2 steps. If it gets worse, put the number back.",
		"Never change items you don't know (OPTION, AGC, HV, FACTORY...). The Right tab explains each one.",
	};
	for (int i = 0; i < 4; i++) { int k = text_block(F_SMALL, 62, y, 520, L[i], C_TEXT, 2, 19); y += k * 19 + 6; }
	int f1 = ui_ibutton(7720, 40, 318, 272, 48, IC_CHECK, "I've written them down", ST_PRIMARY);
	int f2 = ui_ibutton(7721, 324, 318, 276, 48, IC_NOTE, "Save them for me (Ultra)", ST_NORMAL);
	if (blocked) return;
	if (f1 & UI_CLICK) { cu_warned = 1; choose_mode(cu_after_warn); return; }
	if (f2 & UI_CLICK) { cu_warned = 1; choose_mode(MD_ULTRA); return; }
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	if (in->down & (BTN_HOME | BTN_B)) cu_view = V_MODE;
}

/* ---- Ultra: your numbers ---- */
static void view_entry(int blocked)
{
	heading(IC_NOTE, "Your settings", "Type in your TV's numbers. The originals are kept.");
	int per = 6;
	if (ent_top > cur.nsv - per) ent_top = cur.nsv - per; if (ent_top < 0) ent_top = 0;
	for (int k = 0; k < per && ent_top + k < cur.nsv; k++) {
		int i = ent_top + k, y = 92 + k * 42;
		int f = ui_item(7750 + k, 40, y, 560, 38);
		gfx_round(40, y, 560, 38, 10, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, 255);
		int s = cal_setting_find(cur.sv_name[i]);
		text_fit(F_BODYB, 54, y + 7, 180, cur.sv_name[i], C_TEXT, 255);
		text_fit(F_SMALL, 240, y + 10, 200, s >= 0 ? SET(s)->name : "not in the guide", s >= 0 ? C_DIM : C_BAD, 255);
		char v[40]; snprintf(v, sizeof v, "was %s  now %s", cur.sv_before[i][0] ? cur.sv_before[i] : "-", cur.sv_after[i][0] ? cur.sv_after[i] : "-");
		text_r(F_SMALL, 588, y + 10, v, C_TEXT, 255);
		if (f & UI_FOCUS) ui_focus_ring(40, y, 560, 38, 10);
		if ((f & UI_CLICK) && !blocked) { val_open = 1; val_k = i; val_v = sv_now(i); ui_set_focus(7790); }
	}
	if (!cur.nsv) text_block(F_SMALL, 60, 110, 520, "Nothing yet. Add the settings your TV's menus have (normal and service), then type in each number. They're saved on your SD card.", C_DIM, 3, 20);
	int by = 350;
	int fa = ui_ibutton(7740, 40, by, 170, 44, IC_PLUS, "Add settings", ST_NORMAL);
	int fs = ui_ibutton(7741, 218, by, 140, 44, IC_PLAY, "Start", cur.nsv ? ST_PRIMARY : ST_NORMAL);
	int fr = ui_ibutton(7742, 366, by, 234, 44, IC_BACK, "Put originals back", ST_NORMAL);
	if (blocked) return;
	if (fa & UI_CLICK) { cu_view = V_PICK; pick_top = 0; pick_sel = -1; ui_set_focus(7800); return; }
	if (fs & UI_CLICK) { flow_begin(); return; }
	if (fr & UI_CLICK) {
		/* the backup: show what to type back in */
		int n = 0; for (int i = 0; i < cur.nsv; i++) if (cur.sv_after[i][0] && strcmp(cur.sv_after[i], cur.sv_before[i])) { snprintf(cur.sv_after[i], 12, "%s", cur.sv_before[i]); n++; }
		save_cur();
		char m[64]; snprintf(m, sizeof m, n ? "Set these %d back on the TV: was = original" : "Nothing changed yet", n); ui_toast(m);
	}
	if (in->down & BTN_UP && ent_top > 0) ent_top--;
	ui_footer_begin();
	ui_footer_hint("A", "Edit");
	ui_footer_hint("+", "TV remote");
	ui_footer_hint("HOME", "Back");
	if (in->down & (BTN_HOME | BTN_B)) cu_view = V_MODE;
}

/* the value stepper (Left / Right 1, Up / Down 10) */
static int value_popup(void)
{
	if (!val_open) return 0;
	ui_suspend(0);
	ui_dim();
	ui_panel(120, 120, 400, 220);
	text_c(F_BODYB, 320, 134, cur.sv_name[val_k], C_TEXT, 255);
	char v[16]; snprintf(v, sizeof v, "%d", val_v);
	text_c(F_BIG, 320, 166, v, C_ACCENT, 255);
	text_c(F_SMALL, 320, 236, "Left / Right: 1    Up / Down: 10", C_DIM, 255);
	int fm = ui_button(7790, 140, 280, 110, 44, "OK", ST_PRIMARY);
	int fc = ui_button(7791, 390, 280, 110, 44, "Cancel", ST_NORMAL);
	int fd = ui_button(7792, 265, 280, 110, 44, "Delete", ST_DANGER);
	u32 d = in->down;
	if (ui_repeat(BTN_RIGHT)) val_v++;
	if (ui_repeat(BTN_LEFT)) val_v--;
	if (d & BTN_UP) val_v += 10;
	if (d & BTN_DOWN) val_v -= 10;
	in->down &= ~(BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN);
	if (fm & UI_CLICK) {
		char s[12]; snprintf(s, sizeof s, "%d", val_v);
		if (!cur.sv_before[val_k][0]) snprintf(cur.sv_before[val_k], 12, "%s", s);   /* the first number = your original */
		snprintf(cur.sv_after[val_k], 12, "%s", s);
		save_cur(); val_open = 0;
	}
	if (fd & UI_CLICK) {
		for (int i = val_k; i < cur.nsv - 1; i++) { memcpy(cur.sv_name[i], cur.sv_name[i + 1], NAME_LEN); memcpy(cur.sv_before[i], cur.sv_before[i + 1], 12); memcpy(cur.sv_after[i], cur.sv_after[i + 1], 12); }
		cur.nsv--; save_cur(); val_open = 0;
	}
	if ((fc & UI_CLICK) || (d & (BTN_B | BTN_HOME))) { val_open = 0; in->down = 0; }
	return 1;
}

/* pick settings from the guide's list (grouped by area; tap = add) */
static void view_pick(int blocked)
{
	heading(IC_PLUS, "Which does your TV have?", "Tap each setting your TV has. Small text: other names.");
	static s16 L[128]; int n = 0;
	for (int m = 0; m < 2; m++) for (int a = 0; a < AR_COUNT; a++) for (int i = 0; i < CAL_NSET; i++)
		if (CAL_SET[i].menu == m && CAL_SET[i].area == a && n < 128) L[n++] = (s16)i;
	int per = 10;
	if (pick_top > n - per) pick_top = n - per; if (pick_top < 0) pick_top = 0;
	for (int k = 0; k < per && pick_top + k < n; k++) {
		int i = L[pick_top + k], x = 40 + (k % 2) * 284, y = 90 + (k / 2) * 52;
		int have = find_sv(i) >= 0;
		int f = ui_item(7800 + k, x, y, 276, 46);
		gfx_round(x, y, 276, 46, 12, have ? C_SOFT_GOOD : (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, 255);
		text_fit(F_BODYB, x + 12, y + 3, 250, SET(i)->name, C_TEXT, 255);
		char al[64]; snprintf(al, sizeof al, "%s: %s", MENU_NAME[SET(i)->menu], SET(i)->aliases);
		{ int bars = 0; for (char *q = al; *q; q++) if (*q == '|') { if (++bars == 1) { *q = 0; break; } *q = ' '; } }
		text_fit(F_SMALL, x + 12, y + 25, 250, al, C_DIM, 255);
		if (f & UI_FOCUS) ui_focus_ring(x, y, 276, 46, 12);
		if ((f & UI_CLICK) && !blocked) {
			if (have) ui_toast("Already on your list");
			else if (cur.nsv >= MAX_SV) ui_toast("The list is full");
			else { int k2 = cur.nsv++; snprintf(cur.sv_name[k2], NAME_LEN, "%s", cal_setting_code(i)); { char *br = strstr(cur.sv_name[k2], " ("); if (br) *br = 0; } cur.sv_before[k2][0] = cur.sv_after[k2][0] = 0; save_cur(); val_open = 1; val_k = k2; val_v = 0; ui_set_focus(7790); }
		}
	}
	if (ui_ibutton(7830, 40, 358, 130, 40, IC_UP, "Page", ST_NORMAL) & UI_CLICK) pick_top -= per;
	if (ui_ibutton(7831, 178, 358, 130, 40, IC_DOWN, "Page", ST_NORMAL) & UI_CLICK) pick_top += per;
	if ((ui_ibutton(7832, 470, 358, 130, 40, IC_CHECK, "Done", ST_PRIMARY) & UI_CLICK) && !blocked) { cu_view = V_ENTRY; return; }
	char pg[32]; snprintf(pg, sizeof pg, "%d-%d of %d", pick_top + 1, pick_top + per < n ? pick_top + per : n, n);
	text_c(F_SMALL, 390, 370, pg, C_DIM, 255);
	ui_footer_begin();
	ui_footer_hint("A", "Add");
	ui_footer_hint("HOME", "Done");
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) cu_view = V_ENTRY;
}

static void view_done(int blocked)
{
	heading(IC_CHECK, "Calibration done", nfixed ? "Nice work. What was fixed is in your Health log." : "Come back any time.");
	int y = 96;
	for (int i = 0; i < nfixed && i < 5; i++) { char m[96]; snprintf(m, sizeof m, "Fixed: %s", CAL_FAULT[fixed_list[i]].name); text_fit(F_BODYB, 60, y, 520, m, C_GOOD, 255); y += 28; }
	int ch = 0;
	for (int i = 0; i < cur.nsv; i++) if (cur.sv_after[i][0] && cur.sv_before[i][0] && strcmp(cur.sv_after[i], cur.sv_before[i])) {
		if (ch < 6) { char m[96]; snprintf(m, sizeof m, "%s: %s -> %s", cur.sv_name[i], cur.sv_before[i], cur.sv_after[i]); text_fit(F_SMALL, 60, y + 4, 520, m, C_TEXT, 255); y += 22; }
		ch++;
	}
	if (ch) text_fit(F_SMALL, 60, y + 8, 520, "Your originals are saved: Your settings > Put originals back.", C_DIM, 255);
	int f1 = ui_ibutton(7860, 40, 350, 270, 46, IC_CHECK, "Finish", ST_PRIMARY);
	int f2 = ui_ibutton(7861, 330, 350, 270, 46, IC_PLAY, "Another area", ST_NORMAL);
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	if (blocked) return;
	if (f1 & UI_CLICK) { go(S_CAL); return; }
	if (f2 & UI_CLICK) { go(S_CAL); return; }
	if (in->down & (BTN_HOME | BTN_B)) go(S_CAL);
}

void scr_calflow(void)
{
	if (cu_view == V_FLOW) { flow_picture(); return; }
	int blocked = help_is_open() || val_open || ov_open;
	if (blocked) ui_suspend(1);
	gfx_bg();
	switch (cu_view) {
	case V_MODE: view_mode(blocked); break;
	case V_WARN: view_warn(blocked); break;
	case V_ENTRY: view_entry(blocked); break;
	case V_PICK: view_pick(blocked); break;
	case V_DONE: view_done(blocked); break;
	}
	if (screen != S_CALFLOW) return;
	if (value_popup()) return;
	ui_suspend(0);
	calui_remote_overlay(SCR_W, SCR_H - 52, menu_in.ptr_valid, menu_in.px, menu_in.py);
	static const char *help[] = {
		"Guided: you go step by step. Intense: it asks and finds the fault. Ultra: it uses your numbers.",
		"In the pictures: - hides everything. Left = problems & fixes, Right = the settings.",
		"+ shows your TV remote anywhere in the Calibration Suite.",
		"Service menus: write every number down first. Ultra keeps them for you.",
	};
	if (!val_open && !ov_open) ui_help("Calibration", help, 4);
}
#ifdef HOST
void calui_debug(void) { printf("calui: view %d kind %d val_open %d ov_open %d help %d fl_tab %d hidden %d\n", cu_view, fl_kind, val_open, ov_open, help_is_open(), fl_tab, fl_hidden); }
#endif
