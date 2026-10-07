/*
   wizard.c - "New remote" hub and the TV brand setup wizard.
   Brand codes come from the public IRDB (github.com/probonopd/irdb).
   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "app.h"
#include <strings.h>

typedef struct { const char *name; u8 cmd; } BKey;
typedef struct { const char *brand, *note; u8 proto, addr; int n; BKey k[26]; } Brand;

#define DIGITS(a1,a2,a3,a4,a5,a6,a7,a8,a9,a0) \
	{ "1", a1 }, { "2", a2 }, { "3", a3 }, { "4", a4 }, { "5", a5 }, { "6", a6 }, { "7", a7 }, { "8", a8 }, { "9", a9 }, { "0", a0 }

static const Brand BRANDS[] = {
	{ "Samsung", "", P_SAMSUNG, 0x07, 24, {
		{ "POWER", 0x02 }, { "MUTE", 0x0F }, { "VOL+", 0x07 }, { "VOL-", 0x0B }, { "P+", 0x12 }, { "P-", 0x10 },
		{ "MENU", 0x1A }, { "OK", 0x68 }, { "UP", 0x60 }, { "DOWN", 0x61 }, { "LEFT", 0x65 }, { "RIGHT", 0x62 },
		{ "EXIT", 0x2D }, { "INPUT", 0x01 }, DIGITS(0x04, 0x05, 0x06, 0x08, 0x09, 0x0A, 0x0C, 0x0D, 0x0E, 0x11) } },
	{ "LG", "", P_NEC, 0x04, 24, {
		{ "POWER", 0x08 }, { "MUTE", 0x09 }, { "VOL+", 0x02 }, { "VOL-", 0x03 }, { "P+", 0x00 }, { "P-", 0x01 },
		{ "MENU", 0x43 }, { "OK", 0x44 }, { "UP", 0x40 }, { "DOWN", 0x41 }, { "LEFT", 0x07 }, { "RIGHT", 0x06 },
		{ "BACK", 0x28 }, { "INPUT", 0x0B }, DIGITS(0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x10) } },
	{ "Sony", "", P_SONY, 0x01, 24, {
		{ "POWER", 0x15 }, { "MUTE", 0x14 }, { "VOL+", 0x12 }, { "VOL-", 0x13 }, { "P+", 0x10 }, { "P-", 0x11 },
		{ "MENU", 0x60 }, { "OK", 0x65 }, { "UP", 0x74 }, { "DOWN", 0x75 }, { "LEFT", 0x34 }, { "RIGHT", 0x33 },
		{ "BACK", 0x63 }, { "INPUT", 0x25 }, DIGITS(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09) } },
	{ "Panasonic", "", P_PANA, 0x80, 24, {
		{ "POWER", 0x3D }, { "MUTE", 0x32 }, { "VOL+", 0x20 }, { "VOL-", 0x21 }, { "P+", 0x34 }, { "P-", 0x35 },
		{ "MENU", 0x52 }, { "OK", 0x49 }, { "UP", 0x4A }, { "DOWN", 0x4B }, { "LEFT", 0x4E }, { "RIGHT", 0x4F },
		{ "EXIT", 0xD3 }, { "INPUT", 0x05 }, DIGITS(0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19) } },
	{ "Philips", "Also Alba, Akura, Loewe, Transonic", P_RC5, 0x00, 18, {
		{ "POWER", 0x0C }, { "MUTE", 0x0D }, { "VOL+", 0x10 }, { "VOL-", 0x11 }, { "P+", 0x20 }, { "P-", 0x21 },
		{ "MENU", 0x2E }, { "INPUT", 0x38 }, DIGITS(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00) } },
	{ "JVC", "", P_JVC, 0x03, 20, {
		{ "POWER", 0x17 }, { "MUTE", 0x1C }, { "VOL+", 0x1E }, { "VOL-", 0x1F }, { "P+", 0x19 }, { "P-", 0x18 },
		{ "MENU", 0x7A }, { "OK", 0x0A }, { "DISPLAY", 0x04 }, { "INPUT", 0x13 },
		DIGITS(0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x20) } },
	{ "Toshiba", "", P_NEC, 0x40, 17, {
		{ "POWER", 0x12 }, { "MUTE", 0x10 }, { "VOL+", 0x1A }, { "VOL-", 0x1E }, { "P+", 0x1B }, { "P-", 0x1F },
		{ "INPUT", 0x0F }, DIGITS(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00) } },
	{ "Hitachi", "", P_NEC, 0x50, 18, {
		{ "POWER", 0x17 }, { "MUTE", 0x0B }, { "VOL+", 0x12 }, { "VOL-", 0x15 }, { "P+", 0x19 }, { "P-", 0x18 },
		{ "MENU", 0x53 }, { "INPUT", 0x09 }, DIGITS(0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F, 0x04, 0x05, 0x0C) } },
	{ "Sanyo", "", P_NEC, 0x38, 18, {
		{ "POWER", 0x12 }, { "MUTE", 0x18 }, { "VOL+", 0x0E }, { "VOL-", 0x0F }, { "P+", 0x0A }, { "P-", 0x0B },
		{ "MENU", 0x17 }, { "INPUT", 0x13 }, DIGITS(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00) } },
	{ "Transonic", "Nothing? Try Philips (RC5)", P_NEC, 0x18, 5, {
		{ "POWER", 0x08 }, { "MUTE", 0x09 }, { "VOL+", 0x02 }, { "VOL-", 0x03 }, { "INPUT", 0x0A } } },
};
#define NBRAND ((int)(sizeof BRANDS / sizeof BRANDS[0]))

/* a remote name nobody else is using: "Sony TV", "Sony TV 2"... */
void unique_remote_name(const char *base, char *out)
{
	static char fl[MAX_REMOTES][40], nl[MAX_REMOTES][NAME_LEN];
	int nr = remotes_list(fl, nl, MAX_REMOTES);
	snprintf(out, NAME_LEN, "%s", base);
	for (int t = 2; t < 100; t++) {
		int clash = 0;
		for (int j = 0; j < nr; j++) if (!strcasecmp(nl[j], out)) clash = 1;
		if (!clash) return;
		char tmp[NAME_LEN + 8]; snprintf(tmp, sizeof tmp, "%.*s %d", NAME_LEN - 5, base, t);
		snprintf(out, NAME_LEN, "%s", tmp);
	}
}

/* OSSC line doubler / scaler: its remote codes, straight from the OSSC firmware (marqs85/ossc,
   controls.c rc_keymap_default). The OSSC's receiver packs NEC as {address, command} bit-reversed. */
static const struct { const char *name; u8 addr, cmd; } OSSC_KEYS[] = {
	{ "OSSC 1", 0x7C, 0x94 },
	{ "OSSC 2", 0x7C, 0x95 },
	{ "OSSC 3", 0x7C, 0x96 },
	{ "OSSC 4", 0x7C, 0x97 },
	{ "OSSC 5", 0x7C, 0x98 },
	{ "OSSC 6", 0x7C, 0x99 },
	{ "OSSC 7", 0x7C, 0x9A },
	{ "OSSC 8", 0x7C, 0x9B },
	{ "OSSC 9", 0x7C, 0x9C },
	{ "OSSC 0", 0x7C, 0x93 },
	{ "OSSC MENU", 0x7C, 0xB2 },
	{ "OSSC OK", 0x7C, 0xB8 },
	{ "OSSC BACK", 0x7C, 0xB7 },
	{ "OSSC UP", 0x7C, 0xB4 },
	{ "OSSC DOWN", 0x7C, 0xB3 },
	{ "OSSC LEFT", 0x7C, 0xB5 },
	{ "OSSC RIGHT", 0x7C, 0xB6 },
	{ "OSSC INFO", 0x7C, 0xA6 },
	{ "OSSC LCD", 0x7C, 0x80 },
	{ "SCANLINES", 0x38, 0x12 },
	{ "SL TYPE", 0x38, 0x18 },
	{ "SL +", 0x38, 0x0A },
	{ "SL -", 0x38, 0x0B },
	{ "MASK MODE+", 0x7C, 0xBA },
	{ "MASK MODE-", 0x7C, 0xB9 },
	{ "MASK +", 0x38, 0x0E },
	{ "MASK -", 0x38, 0x0F },
	{ "LINE MULT", 0x38, 0x13 },
	{ "PHASE +", 0x7A, 0x1A },
	{ "PHASE -", 0x7A, 0x1B },
	{ "PROFILE", 0x7C, 0x9D }
};

/* add the OSSC buttons to the open remote (one remote for TV + scaler), or make an OSSC remote */
static void add_ossc(void)
{
	if (!have_cur) { char nm[NAME_LEN]; unique_remote_name("OSSC", nm); remote_new(&cur, nm, P_NEC, OSSC_KEYS[0].addr); have_cur = 1; }
	int added = 0;
	for (size_t k = 0; k < sizeof OSSC_KEYS / sizeof OSSC_KEYS[0]; k++) {
		int dup = 0;
		for (int i = 0; i < cur.nbtn; i++) if (cur.btn[i].proto == P_NEC && cur.btn[i].addr == OSSC_KEYS[k].addr && cur.btn[i].cmd == OSSC_KEYS[k].cmd) dup = 1;
		if (dup) continue;
		char nm[NAME_LEN]; unique_name(&cur, OSSC_KEYS[k].name, nm);
		if (remote_add_btn(&cur, nm, P_NEC, OSSC_KEYS[k].addr, OSSC_KEYS[k].cmd) >= 0) added++;
	}
	save_cur(); remote_screen_reset(); home_refresh();
	go(S_REMOTE);
	char m[48]; snprintf(m, sizeof m, "Added %d OSSC buttons", added); ui_toast(m);
}

static int wz_brand = -1, wz_tested;

void wizard_start(void) { wz_brand = -1; wz_tested = 0; ui_set_focus(-1); go(S_WIZARD); }

static int key_of(const Brand *b, const char *name)
{
	for (int i = 0; i < b->n; i++) if (!strcmp(b->k[i].name, name)) return b->k[i].cmd;
	return -1;
}

static void make_remote(const Brand *b)
{
	char base[NAME_LEN], nm[NAME_LEN];
	snprintf(base, sizeof base, "%s TV", b->brand);
	unique_remote_name(base, nm);
	remote_new(&cur, nm, b->proto, b->addr);
	for (int i = 0; i < b->n; i++) remote_add_btn(&cur, b->k[i].name, b->proto, b->addr, b->k[i].cmd);
	remote_automap(&cur);
	have_cur = 1;
	save_cur();
	remote_screen_reset();
	home_refresh();
	go(S_REMOTE);
	char m[48]; snprintf(m, sizeof m, "%s ready: %d buttons", nm, b->n);
	ui_toast(m);
}

static void hub(int blocked)
{
	icon(IC_PLUS, 58, 50, 30, C_ACCENT);
	text(F_TITLE, 86, 30, "New remote", C_TEXT, 255);
	text(F_SMALL, 88, 64, "Pick the easiest way", C_DIM, 255);
	/* brand grid */
	text(F_BODYB, 40, 92, "1. Pick your TV brand", C_TEXT, 255);
	for (int k = 0; k < NBRAND; k++) {
		int x = 40 + (k % 5) * 112, y = 120 + (k / 5) * 56;
		if (ui_tile(7000 + k, x, y, 104, 48, -1, BRANDS[k].brand, ST_KEY, 0) & UI_CLICK) { wz_brand = k; wz_tested = 0; ui_set_focus(7102); return; }
	}
	/* other ways */
	text(F_BODYB, 40, 244, "2. Brand not there?", C_TEXT, 255);
	int y = 272;
	if (ui_tile(7050, 40, y, 131, 76, IC_RADAR, "Scan", ST_NORMAL, 0) & UI_CLICK) { scan_start(); return; }
	if (ui_tile(7053, 183, y, 131, 76, IC_STAR, "Smart match", ST_NORMAL, 0) & UI_CLICK) { have_cur = 0; smart_start(); return; }
	if (ui_tile(7051, 326, y, 131, 76, IC_SAVE, "Import file", ST_NORMAL, 0) & UI_CLICK) { go(S_IMPORT); return; }
	if (ui_tile(7052, 469, y, 131, 76, IC_PENCIL, "Type code", ST_NORMAL, 0) & UI_CLICK) { go(S_NEWREMOTE); return; }
	/* retro gear: scalers with IR remotes */
	if (ui_ibutton(7054, 40, 358, 276, 40, IC_REMOTE, have_cur ? "Add OSSC buttons" : "OSSC scaler remote", ST_NORMAL) & UI_CLICK) { add_ossc(); return; }
	text_fit(F_SMALL, 330, 368, 270, "RetroTINK? Use Import file", C_DIM, 255);

	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Pick your TV brand. The Wii sends a test code.",
		"If the TV reacts, you get a full remote straight away.",
		"Scan: tries every code until the TV reacts.",
		"Smart match: answers a few tests and it guesses your TV from 900+ code sets.",
		"Import: use a remote file from the internet (Flipper Zero or LIRC).",
		"Type the code: if you already know it.",
	};
	ui_help("New remote", help, 6);
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) go_back();
}

static void test_step(int blocked)
{
	const Brand *b = &BRANDS[wz_brand];
	icon(IC_TV, 58, 50, 30, C_ACCENT);
	char t[48]; snprintf(t, sizeof t, "%s TV", b->brand);
	text(F_TITLE, 86, 30, t, C_TEXT, 255);
	text(F_SMALL, 88, 64, b->note[0] ? b->note : "Let's check it works", C_DIM, 255);

	/* big instructions card */
	gfx_round(40, 96, 560, 130, 16, C_CARD_HI, 255);
	int mute = key_of(b, "MUTE"), code = mute >= 0 ? mute : key_of(b, "VOL+");
	const char *what = mute >= 0 ? "MUTE" : "VOLUME UP";
	text(F_BODYB, 64, 110, "1.  Turn the TV on", C_TEXT, 255);
	char l2[64]; snprintf(l2, sizeof l2, "2.  Press Test (it sends %s)", what);
	text(F_BODYB, 64, 146, l2, C_TEXT, 255);
	text(F_BODYB, 64, 182, "3.  Did the TV react?", C_TEXT, 255);

	int y = 246;
	if (ui_ibutton(7102, 40, y, 176, 56, IC_PLAY, "Test", ST_NORMAL) & UI_CLICK) {
		if (code >= 0) send_code_press(b->proto, b->addr, (u8)code, 0);
		wz_tested++;
	}
	if (ui_ibutton(7103, 232, y, 176, 56, IC_CHECK, "TV reacted", ST_NORMAL) & UI_CLICK) { make_remote(b); return; }
	if (ui_ibutton(7104, 424, y, 176, 56, IC_CLOSE, "No reaction", ST_NORMAL) & UI_CLICK) { wz_tested = -1; }
	if (wz_tested < 0) {
		gfx_round(40, 318, 560, 90, 14, C_SOFT_WARN, 255);
		text(F_BODYB, 60, 328, "No luck? Try these:", C_TEXT, 255);
		text(F_SMALL, 60, 356, "Point the Wii straight at the TV and test again.", C_TEXT, 255);
		text(F_SMALL, 60, 378, "Still nothing? Go Back and pick Scan for codes.", C_TEXT, 255);
	} else if (wz_tested > 0) {
		char m[48]; snprintf(m, sizeof m, "Sent %d time%s", wz_tested, wz_tested == 1 ? "" : "s");
		text(F_SMALL, 128, 308, m, C_DIM, 255);
	}

	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Test sends one safe code (MUTE or VOLUME UP).",
		"If the TV reacts, press It worked! for a full remote.",
		"Sensor bar must point at the TV's IR window.",
		"Old sets of some brands use other codes. Scan finds them.",
	};
	ui_help("Brand test", help, 4);
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) { wz_brand = -1; ui_set_focus(7000); }
}

void scr_wizard(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	if (wz_brand < 0) hub(blocked); else test_step(blocked);
}

/* ================================================================== */
/* Smart match                                                        */
/* ================================================================== */
/* v1.9.1: 1) Check: tests the buttons already on your remote first (names can be wrong).
           2) Match: asks the question that tells it the most, and hunts for buttons you don't have.
           3) Save: nothing on your remote changes until you look over the list and press Save. */
#include "presets.h"

static int sm_have, sm_fid, sm_phase;   /* sm_phase: 0 = checking your buttons, 1 = matching */
static u8 sm_p, sm_c;
static u16 sm_a;
static float sm_conf;
static int sm_sent, sm_hex, sm_other, sm_skip_db;
static SGroup sm_g[SMART_MAXG];

/* the buttons to check, in a safe order */
static s16 ck_list[MAX_BTNS];
static s8 ck_done[MAX_BTNS];   /* 0 not yet, 1 works, 2 did nothing, 3 does something else, 4 skipped */
static int ck_n, ck_pos;

/* changes waiting for Save */
enum { PD_ADD, PD_RENAME, PD_DEAD };
typedef struct { u8 type, proto, cmd, guessed; s16 btn; u16 addr; char name[NAME_LEN]; } Pend;
#define MAX_PEND 96
static Pend pend[MAX_PEND];
static int npend;
static int sm_review, sm_review_top, sm_discard_armed, sm_after, sm_review_age;   /* sm_after: where Save goes. 0 back, 1 the remote */
static int sm_new_remote;   /* no TV picked: Save makes a new remote */
static int sm_use_pct = -1;   /* how sure the match was when you pressed Use (-1 = not used) */

/* what Undo takes back, newest last */
enum { H_CHECK, H_MATCH, H_SKIPCK };
typedef struct { u8 kind, proto, cmd; s16 ck; u16 addr; } Hist;
static Hist hist[160];
static int nhist;
static void hist_push(int kind, int ck, u8 p, u16 a, u8 c)
{
	if (nhist == 160) { memmove(hist, hist + 1, 159 * sizeof *hist); nhist--; }
	hist[nhist++] = (Hist){ (u8)kind, p, c, (s16)ck, a };
}

static const char *fid_friendly(int f)
{
	static const char *N[FN_COUNT] = { "Power", "Mute", "Volume up", "Volume down", "Channel up", "Channel down", "Menu", "OK",
		"Up", "Down", "Left", "Right", "Back", "Input / AV", "Info", "Sleep", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "Picture mode", "Last channel",
		"Power on", "Power off", "AV 1", "AV 2", "AV 3", "Component", "HDMI 1", "HDMI 2", "TV / aerial" };
	return f >= 0 && f < FN_COUNT ? N[f] : "?";
}

/* names that only mean "we don't know yet" */
static int placeholder_name(const char *n)
{
	return !strncasecmp(n, "UNKNOWN", 7) || !strncasecmp(n, "Found ", 6) || !strncasecmp(n, "Code ", 5);
}

/* ---- the "What did it do?" list: which button, when you can tell ---- */
#define WHAT_N 12
static const char *WHAT_IT[WHAT_N] = { "Volume up", "Volume down", "Mute", "Channel up", "Channel down", "Input / AV",
	"Menu opened", "Power (TV off / on)", "Picture mode", "Info on screen", "A number / channel", "Not sure (skip)" };
static const int WHAT_IC[WHAT_N] = { IC_VOLUP, IC_VOLDN, IC_MUTE, IC_CH_UP, IC_CH_DN, IC_INPUT, IC_MENU, IC_POWER, IC_PALETTE, IC_HELP, IC_REMOTE, IC_CLOSE };
static const int WHAT_FID[WHAT_N] = { FN_VOLUP, FN_VOLDN, FN_MUTE, FN_CHUP, FN_CHDN, FN_INPUT, FN_MENU, FN_POWER, FN_PICTURE, FN_INFO, -1, -1 };
static const int WHAT_R[WHAT_N] = { R_VOLUME, R_VOLUME, R_MUTE, R_CHANNEL, R_CHANNEL, R_INPUT, R_MENU, R_POWER, R_MENU, R_OTHER, R_CHANNEL, R_OTHER };
/* "Input / AV" and "Power" ask one more question: discrete codes are the ones macros need */
#define IN_N 9
static const char *IN_IT[IN_N] = { "Input button (steps through)", "AV 1", "AV 2", "AV 3", "Component", "HDMI 1", "HDMI 2", "TV / aerial", "Not sure which" };
static const int IN_IC[IN_N] = { IC_INPUT, IC_INPUT, IC_INPUT, IC_INPUT, IC_INPUT, IC_INPUT, IC_INPUT, IC_TV, IC_CLOSE };
static const int IN_FID[IN_N] = { FN_INPUT, FN_AV1, FN_AV2, FN_AV3, FN_COMP, FN_HDMI1, FN_HDMI2, FN_TVTUNER, FN_INPUT };
#define PW_N 3
static const char *PW_IT[PW_N] = { "Power (turns it off and on)", "Power on only", "Power off only" };
static const int PW_IC[PW_N] = { IC_POWER, IC_POWER, IC_POWER };
static const int PW_FID[PW_N] = { FN_POWER, FN_PWRON, FN_PWROFF };
static int sm_other_from;   /* which question the extra list belongs to (1 match, 2 check) */

/* ---- pending changes ---- */
static int pend_find_code(u8 p, u16 a, u8 c) { for (int k = 0; k < npend; k++) if (pend[k].type == PD_ADD && pend[k].proto == p && pend[k].addr == a && pend[k].cmd == c) return k; return -1; }
static int pend_find_btn(int b) { for (int k = 0; k < npend; k++) if (pend[k].type != PD_ADD && pend[k].btn == b) return k; return -1; }
static int btn_with_code(u8 p, u16 a, u8 c)
{
	if (!have_cur || sm_new_remote) return -1;
	for (int b = 0; b < cur.nbtn; b++) if (!cur.btn[b].nsteps && cur.btn[b].proto == p && cur.btn[b].addr == a && cur.btn[b].cmd == c) return b;
	return -1;
}
static void pend_add(u8 p, u16 a, u8 c, const char *name, int guessed)
{
	int b = btn_with_code(p, a, c);
	if (b >= 0) {   /* already on the remote: give a placeholder its real name, and bring back a hidden one */
		if (placeholder_name(cur.btn[b].name) || cur.btn[b].dead) {
			int k = pend_find_btn(b); if (k < 0 && npend < MAX_PEND) k = npend++;
			if (k >= 0) { pend[k] = (Pend){ PD_RENAME, p, c, (u8)guessed, (s16)b, a, "" }; snprintf(pend[k].name, NAME_LEN, "%s", name); }
		}
		return;
	}
	int k = pend_find_code(p, a, c);
	if (k < 0) { if (npend >= MAX_PEND) return; k = npend++; }
	pend[k] = (Pend){ PD_ADD, p, c, (u8)guessed, -1, a, "" };
	snprintf(pend[k].name, NAME_LEN, "%s", name);
}
static void pend_btn(int type, int b, const char *name)
{
	int k = pend_find_btn(b);
	if (k < 0) { if (npend >= MAX_PEND) return; k = npend++; }
	pend[k] = (Pend){ (u8)type, cur.btn[b].proto, cur.btn[b].cmd, 0, (s16)b, cur.btn[b].addr, "" };
	snprintf(pend[k].name, NAME_LEN, "%s", name ? name : "");
}
static void pend_drop_btn(int b) { int k = pend_find_btn(b); if (k >= 0) { pend[k] = pend[--npend]; } }

/* ---- pause and come back: Smart match remembers where you were, per remote ---- */
static void sm_seed(void);
static int sm_resume;   /* the "carry on?" question is showing */
static int sm_res_answers, sm_res_pend, sm_res_same;
static void sm_ses_path(char *p, int n) { snprintf(p, n, "%s/smart_%s.ses", data_dir, (have_cur && !sm_new_remote && cur.file[0]) ? cur.file : "new"); }
static unsigned sm_sig(void)
{
	unsigned h = 2166136261u;
	if (!have_cur || sm_new_remote) return 1;
	for (int i = 0; i < cur.nbtn; i++) {
		const RBtn *b = &cur.btn[i];
		unsigned v = b->proto | (b->addr << 8) | ((unsigned)b->cmd << 24);
		h = (h ^ v) * 16777619u; h = (h ^ b->dead ^ (b->nsteps << 1)) * 16777619u;
		for (const char *q = b->name; *q; q++) h = (h ^ (u8)*q) * 16777619u;
	}
	return h;
}
static void sm_ses_delete(void) { char p[160]; sm_ses_path(p, sizeof p); remove(p); }
static void sm_ses_save(void)
{
	char p[160]; sm_ses_path(p, sizeof p);
	FILE *f = fopen(p, "w");
	if (!f) return;
	fprintf(f, "sig=%u\nphase=%d\nck_pos=%d\nskip_db=%d\nuse_pct=%d\n", sm_sig(), sm_phase, ck_pos, sm_skip_db, sm_use_pct);
	for (int k = 0; k < ck_n; k++) fprintf(f, "ck=%d,%d\n", ck_list[k], ck_done[k]);
	for (int k = 0; k < npend; k++) fprintf(f, "pd=%d,%d,%d,%d,%d,%d,%s\n", pend[k].type, pend[k].proto, pend[k].addr, pend[k].cmd, pend[k].guessed, pend[k].btn, pend[k].name);
	smart_write(f);
	fclose(f);
}
/* is there a paused session? how far along, and is the remote still the same? */
static int sm_ses_peek(void)
{
	char p[160], line[160]; sm_ses_path(p, sizeof p);
	FILE *f = fopen(p, "r");
	if (!f) return 0;
	unsigned sig = 0; sm_res_answers = 0; sm_res_pend = 0;
	while (fgets(line, sizeof line, f)) {
		if (!strncmp(line, "sig=", 4)) sig = strtoul(line + 4, NULL, 10);
		else if (!strncmp(line, "pd=", 3)) sm_res_pend++;
		else if (!strncmp(line, "ev=2,", 5)) sm_res_answers++;
	}
	fclose(f);
	sm_res_same = sig == sm_sig();
	return sm_res_answers > 0 || sm_res_pend > 0;
}
static void sm_ses_load(void)
{
	char p[160], line[160]; sm_ses_path(p, sizeof p);
	FILE *f = fopen(p, "r");
	if (!f) return;
	int same = sm_res_same, nck = 0;
	if (same) { smart_reset(); npend = 0; }
	else sm_seed();   /* the remote changed: its buttons as they are now, plus your old answers */
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\r\n")] = 0;
		if (!strncmp(line, "ev=", 3)) { if (same || !strncmp(line, "ev=2,", 5) || !strncmp(line, "ev=3,", 5)) smart_read_line(line); }
		else if (!same) continue;   /* your remote changed: keep the answers, drop the rest */
		else if (!strncmp(line, "phase=", 6)) sm_phase = atoi(line + 6);
		else if (!strncmp(line, "ck_pos=", 7)) ck_pos = atoi(line + 7);
		else if (!strncmp(line, "skip_db=", 8)) sm_skip_db = atoi(line + 8);
		else if (!strncmp(line, "use_pct=", 8)) sm_use_pct = atoi(line + 8);
		else if (!strncmp(line, "ck=", 3)) { int b, d; if (sscanf(line + 3, "%d,%d", &b, &d) == 2 && b >= 0 && b < cur.nbtn && nck < MAX_BTNS) { ck_list[nck] = (s16)b; ck_done[nck++] = (s8)d; } }
		else if (!strncmp(line, "pd=", 3) && npend < MAX_PEND) {
			int t, pr, a, c, g, b, off = 0;
			if (sscanf(line + 3, "%d,%d,%d,%d,%d,%d,%n", &t, &pr, &a, &c, &g, &b, &off) >= 6 && off && t >= PD_ADD && t <= PD_DEAD && (t == PD_ADD || (b >= 0 && b < cur.nbtn))) {
				pend[npend] = (Pend){ (u8)t, (u8)pr, (u8)c, (u8)g, (s16)b, (u16)a, "" };
				snprintf(pend[npend].name, NAME_LEN, "%s", line + 3 + off);
				npend++;
			}
		}
	}
	fclose(f);
	if (same) { ck_n = nck; if (ck_pos > ck_n) ck_pos = ck_n; if (sm_phase == 0 && ck_pos >= ck_n) sm_phase = 1; }
	else {   /* the remote changed: check its buttons again, except ones you already answered */
		for (int k = 0; k < ck_n; k++) { RBtn *bb = &cur.btn[ck_list[k]]; if (smart_status(bb->proto, bb->addr, bb->cmd) == 3) ck_done[k] = 1; }
		ck_pos = 0; while (ck_pos < ck_n && ck_done[ck_pos]) ck_pos++;
		sm_phase = ck_pos < ck_n ? 0 : 1;
	}
}

/* ---- check step: your saved buttons, safest first ---- */
static float ck_order(const RBtn *b)
{
	int f = fid_from_name(b->name), r = resp_from_name(b->name);
	if (r == R_POWER) return 9;
	if (r == R_INPUT) return 8;
	if (placeholder_name(b->name)) return 10;   /* your unnamed buttons after all the named ones */
	switch (f) {
	case FN_MUTE: return 0; case FN_VOLUP: case FN_VOLDN: return 1; case FN_MENU: return 2;
	case FN_INFO: case FN_PICTURE: case FN_SLEEP: return 3;
	case FN_OK: case FN_UP: case FN_DOWN: case FN_LEFT: case FN_RIGHT: case FN_BACK: return 2.5f;   /* right after Menu: its menu is open */
	case FN_CHUP: case FN_CHDN: case FN_PREVCH: return 5;
	default: return f >= FN_D0 && f <= FN_D9 ? 5.5f : f >= 0 ? 4 : 7;
	}
}
static int ck_risky(const RBtn *b) { int r = resp_from_name(b->name); return r == R_POWER || r == R_INPUT; }
static int ck_menu_key(const RBtn *b) { int f = fid_from_name(b->name); return f == FN_OK || f == FN_UP || f == FN_DOWN || f == FN_LEFT || f == FN_RIGHT || f == FN_BACK; }

/* what your saved buttons say, as claims (not the final word) */
static void sm_seed(void)
{
	smart_reset();
	ck_n = 0; ck_pos = 0;
	if (!have_cur || sm_new_remote) return;
	for (int i = 0; i < cur.nbtn; i++) {
		RBtn *b = &cur.btn[i];
		if (b->nsteps) continue;
		if (b->dead) { smart_claim(b->proto, b->addr, b->cmd, R_NONE, -1); continue; }
		smart_claim(b->proto, b->addr, b->cmd, placeholder_name(b->name) ? R_OTHER : resp_from_name(b->name), fid_from_name(b->name));
		/* one check per code (two names for the same code: check it once) */
		int dup = 0;
		for (int k = 0; k < ck_n && !dup; k++) { RBtn *o = &cur.btn[ck_list[k]]; dup = o->proto == b->proto && o->addr == b->addr && o->cmd == b->cmd; }
		if (!dup && ck_n < MAX_BTNS) ck_list[ck_n++] = (s16)i;
	}
	for (int a = 1; a < ck_n; a++) { s16 t = ck_list[a]; int c = a; while (c > 0 && ck_order(&cur.btn[ck_list[c - 1]]) > ck_order(&cur.btn[t])) { ck_list[c] = ck_list[c - 1]; c--; } ck_list[c] = t; }
	memset(ck_done, 0, sizeof ck_done);
}

static void ck_next(void);
void smart_start(void)
{
	sm_new_remote = !have_cur;
	/* read your remotes and wiirremote/library again (you may have mapped or added a TV since), and
	   never match this remote against its own copy */
	smart_library_reload();
	smart_library_exclude(have_cur ? cur.file : "");
	npend = 0; nhist = 0; sm_use_pct = -1; sm_review = 0; sm_discard_armed = 0; sm_after = 0;
	sm_seed();
	sm_phase = ck_n ? 0 : 1;
	sm_have = 0; sm_sent = 0; sm_other = 0;
	ui_set_focus(7200);
	go(S_SMART);
	sm_resume = sm_ses_peek();   /* paused last time? ask before sending anything */
	if (sm_resume) { ui_set_focus(9000); return; }
	if (sm_phase == 0) ck_next();   /* send the first saved button straight away (not Power / Input) */
}

static void first_brand(const char *brands, char *out, int n)
{
	int k = 0;
	while (brands[k] && brands[k] != ',' && k < n - 1) { out[k] = brands[k]; k++; }
	out[k] = 0;
	if (!strcasecmp(out, "Unknown brand") || !out[0]) snprintf(out, n, "My");
}

/* ---- Save: the only place the remote changes ---- */
static void sm_apply(void)
{
	int added = 0, renamed = 0, hidden = 0;
	if (sm_new_remote || !have_cur) {
		char base[NAME_LEN] = "TV", nm[NAME_LEN], b[NAME_LEN];
		SGroup g[SMART_MAXG];
		if (smart_groups(g, 1) && g[0].pct > 30) { first_brand(PRESETS_DB[g[0].best].brands, b, sizeof b); snprintf(base, sizeof base, "%.14s TV", b); }
		u8 hp = P_NEC; u16 ha = 0;
		if (!smart_home(&hp, &ha) && npend) { hp = pend[0].proto; ha = pend[0].addr; }
		unique_remote_name(base, nm);
		remote_new(&cur, nm, hp, ha);
		have_cur = 1; sm_new_remote = 0;
	} else undo_save("Smart match");   /* - on the remote puts everything back */
	/* 1) hide the ones that did nothing, 2) rename, 3) add. An old button in the way of a name gets "old " */
	for (int k = 0; k < npend; k++) if (pend[k].type == PD_DEAD && pend[k].btn < cur.nbtn) { cur.btn[pend[k].btn].dead = 1; hidden++; }
	for (int k = 0; k < npend; k++) if (pend[k].type == PD_RENAME && pend[k].btn < cur.nbtn) {
		RBtn *b = &cur.btn[pend[k].btn];
		int other = remote_find_btn(&cur, pend[k].name);
		if (other >= 0 && other != pend[k].btn && cur.btn[other].dead) { char o[NAME_LEN]; snprintf(o, sizeof o, "old %.14s", cur.btn[other].name); snprintf(cur.btn[other].name, NAME_LEN, "%s", o); }
		char un[NAME_LEN];
		if (other >= 0 && other != pend[k].btn && !cur.btn[other].dead) unique_name(&cur, pend[k].name, un); else snprintf(un, sizeof un, "%s", pend[k].name);
		snprintf(b->name, NAME_LEN, "%s", un); b->dead = 0; renamed++;
	}
	for (int k = 0; k < npend; k++) if (pend[k].type == PD_ADD) {
		int other = remote_find_btn(&cur, pend[k].name);
		if (other >= 0 && cur.btn[other].dead) { char o[NAME_LEN]; snprintf(o, sizeof o, "old %.14s", cur.btn[other].name); snprintf(cur.btn[other].name, NAME_LEN, "%s", o); }
		char un[NAME_LEN]; unique_name(&cur, pend[k].name, un);
		if (remote_add_btn(&cur, un, pend[k].proto, pend[k].addr, pend[k].cmd) >= 0) added++;
	}
	remote_automap(&cur);
	save_cur();
	npend = 0;
	sm_ses_delete();   /* done: nothing to come back to (the old file name, before a new remote got one) */
	{ char p[160]; snprintf(p, sizeof p, "%s/smart_new.ses", data_dir); remove(p); }
	remote_screen_reset();
	home_refresh();
	char t[64]; snprintf(t, sizeof t, "Saved: %d added, %d renamed, %d hidden", added, renamed, hidden);
	ui_toast(t);
}

static void sm_leave(void)
{
	if (npend) { sm_review = 1; sm_review_top = 0; sm_discard_armed = 0; sm_review_age = 0; in->down = 0; ui_set_focus(7290); return; }
	if (smart_answers()) { sm_ses_save(); ui_toast("Your answers are kept for next time"); }
	go_back();
}

/* "Use": the whole remote from a match, waiting for Save */
static void sm_use(int best)
{
	const PSet *s = &PRESETS_DB[best];
	u8 m[FN_COUNT];
	smart_build(best, m);
	sm_use_pct = 0;
	for (int k = 0; k < 3; k++) if (sm_g[k].best == best) sm_use_pct = (int)(sm_g[k].pct + 0.5f);
	for (int f = 0; f < FN_COUNT; f++) {
		if (m[f] == 0xFF) continue;
		int shown = smart_answer_fid(s->proto, s->addr, m[f]) == f;
		pend_add(s->proto, s->addr, m[f], FUNC_NAME[f], !shown);
	}
	sm_after = 1;
	sm_review = 1; sm_review_top = 0; sm_discard_armed = 0; sm_review_age = 0; in->down = 0; ui_set_focus(7290);
}

/* ---- answers ---- */
static void sm_answer(int resp, int fid, const char *name)
{
	if (fid >= 0 && resp != R_NONE) smart_want(fid, 0);   /* found it */
	smart_note_fid(sm_p, sm_a, sm_c, resp, fid, 1);
	hist_push(H_MATCH, -1, sm_p, sm_a, sm_c);
	if (resp == R_MUTE) send_code_press(sm_p, sm_a, sm_c, 0);   /* un-mute */
	if (resp != R_NONE && name) pend_add(sm_p, sm_a, sm_c, name, 0);
	sm_have = 0; sm_other = 0;
	sm_ses_save();
}

static void ck_next(void)
{
	while (ck_pos < ck_n && ck_done[ck_pos]) ck_pos++;
	sm_sent = 0; sm_other = 0;
	if (ck_pos >= ck_n) { sm_phase = 1; sm_have = 0; ui_toast("Checked. Now finding the rest"); return; }
	RBtn *b = &cur.btn[ck_list[ck_pos]];
	if (!ck_risky(b)) { send_code_press(b->proto, b->addr, b->cmd, 0); sm_sent = 1; }   /* Power / Input: only when you say */
	ui_set_focus(ck_risky(b) ? 7200 : 7201);
}
static void ck_answer(int result, int resp, int fid, const char *rename)
{
	int bi = ck_list[ck_pos]; RBtn *b = &cur.btn[bi];
	smart_note_fid(b->proto, b->addr, b->cmd, resp, fid, 1);
	hist_push(H_CHECK, ck_pos, 0, 0, 0);
	if (resp == R_MUTE) send_code_press(b->proto, b->addr, b->cmd, 0);
	pend_drop_btn(bi);
	if (result == 2) pend_btn(PD_DEAD, bi, NULL);
	/* your name was wrong: find the right code for that button later */
	{ int nf = fid_from_name(b->name); if (!placeholder_name(b->name) && nf >= 0 && (result == 2 || (result == 3 && fid != nf))) smart_want(nf, 1); }
	if (fid >= 0 && resp != R_NONE) smart_want(fid, 0);
	if (result == 3 && rename && strcasecmp(rename, b->name)) pend_btn(PD_RENAME, bi, rename);
	ck_done[ck_pos] = (s8)result;
	ck_pos++;
	ck_next();
	sm_ses_save();
}
static void sm_undo(void)
{
	if (!nhist) { ui_toast("Nothing to undo"); return; }
	Hist h = hist[--nhist];
	if (h.kind == H_MATCH) {
		smart_forget_last();
		int k = pend_find_code(h.proto, h.addr, h.cmd); if (k >= 0) pend[k] = pend[--npend];
		int b = btn_with_code(h.proto, h.addr, h.cmd); if (b >= 0) pend_drop_btn(b);
		sm_have = 0; sm_other = 0;
		sm_ses_save();
		ui_toast("Answer taken back");
		return;
	}
	/* a check: back to that button, and it counts as your saved name again */
	RBtn *b = &cur.btn[ck_list[h.ck]];
	if (h.kind == H_CHECK) {
		smart_forget_last();
		pend_drop_btn(ck_list[h.ck]);
		if (b->dead) smart_claim(b->proto, b->addr, b->cmd, R_NONE, -1);
		else smart_claim(b->proto, b->addr, b->cmd, placeholder_name(b->name) ? R_OTHER : resp_from_name(b->name), fid_from_name(b->name));
	}
	ck_done[h.ck] = 0; ck_pos = h.ck; sm_phase = 0; sm_other = 0; sm_sent = 0; sm_have = 0;
	sm_ses_save();
	ui_toast("Back to that button");
}

/* ---- the Save list ---- */
static int review_line(int k, char *out, int n)
{
	const Pend *p = &pend[k];
	char code[24]; snprintf(code, sizeof code, p->addr > 255 ? "%s %04X/%02X" : "%s %02X/%02X", PROTO_NAME[p->proto], p->addr, p->cmd);
	if (p->type == PD_ADD) snprintf(out, n, "+ Add %s  (%s)%s", p->name, code, p->guessed ? "  not tested" : "");
	else if (p->type == PD_RENAME) snprintf(out, n, "~ Rename %s to %s", cur.btn[p->btn].name, p->name);
	else snprintf(out, n, "x Hide %s: did nothing  (%s)", cur.btn[p->btn].name, code);
	return p->type;
}
static void sm_review_panel(void)
{
	ui_suspend(0);
	ui_dim();
	if (sm_review_age < 3) { sm_review_age++; in->down = 0; }   /* the press that opened it doesn't also Save */
	int x = 30, y = 24, w = 580, h = 400;
	ui_panel(x, y, w, h);
	char t[64];
	if (sm_new_remote || !have_cur) snprintf(t, sizeof t, "Save as a new remote?");
	else snprintf(t, sizeof t, "Save changes to %s?", cur.name);
	text_fit(F_TITLE, x + 24, y + 12, w - 48, t, C_TEXT, 255);
	int na = 0, nr = 0, nh = 0, ng = 0;
	for (int k = 0; k < npend; k++) { if (pend[k].type == PD_ADD) { na++; ng += pend[k].guessed; } else if (pend[k].type == PD_RENAME) nr++; else nh++; }
	char sum[96]; snprintf(sum, sizeof sum, "%d to add (%d not tested yet), %d to rename, %d to hide", na, ng, nr, nh);
	text_fit(F_SMALL, x + 26, y + 52, w - 52, sum, C_DIM, 255);
	/* the list, 8 at a time: Up / Down scroll */
	int per = (sm_use_pct >= 0 && sm_use_pct < 60) ? 7 : 8;
	if (in->down & BTN_UP) { if (sm_review_top > 0) sm_review_top--; }
	if (in->down & BTN_DOWN) { if (sm_review_top + per < npend) sm_review_top++; }
	for (int k = sm_review_top; k < npend && k < sm_review_top + per; k++) {
		char ln[96]; int ty = review_line(k, ln, sizeof ln);
		int ly = y + 80 + (k - sm_review_top) * 30;
		gfx_round(x + 20, ly, w - 40, 26, 8, C_CARD, 255);
		text_fit(F_SMALL, x + 32, ly + 3, w - 64, ln, ty == PD_ADD ? (pend[k].guessed ? C_DIM : C_GOOD) : ty == PD_RENAME ? C_ACCENT : C_TEXT, 255);
	}
	if (npend > per) { char sc[32]; snprintf(sc, sizeof sc, "%d-%d of %d  (Up / Down)", sm_review_top + 1, sm_review_top + per < npend ? sm_review_top + per : npend, npend); text_r(F_SMALL, x + w - 26, y + 52, sc, C_DIM, 255); }
	if (sm_use_pct >= 0 && sm_use_pct < 60) {
		char wn[96]; snprintf(wn, sizeof wn, "Only %d%% sure of this match: untested buttons may be wrong.", sm_use_pct);
		text_fit(F_SMALL, x + 26, y + 306, w - 52, wn, C_BAD, 255);
	}
	text_fit(F_SMALL, x + 26, y + 326, w - 52, "Nothing changes until you press Save. Pause (or HOME) = finish it later.", C_ACCENT, 255);
	int by = y + h - 50;
	int fs = ui_ibutton(7290, x + 20, by, 108, 40, IC_SAVE, "Save", ST_PRIMARY);
	int fk = ui_button(7291, x + 134, by, 170, 40, "Keep testing", ST_NORMAL);
	int fp = ui_ibutton(7293, x + 310, by, 112, 40, IC_PAUSE, "Pause", ST_NORMAL);
	int fd = ui_button(7292, x + 428, by, 132, 40, sm_discard_armed ? "Sure?" : "Discard", sm_discard_armed ? ST_DANGER : ST_NORMAL);
	if (fp & UI_CLICK) { sm_ses_save(); sm_review = 0; sm_after = 0; ui_toast("Paused: open Smart match to carry on"); go_back(); return; }
	if (fs & UI_CLICK) {
		int after = sm_after;
		sm_apply(); sm_review = 0;
		if (after) go(S_REMOTE); else go_back();
		return;
	}
	if (fk & UI_CLICK) { sm_review = 0; sm_after = 0; ui_set_focus(7200); return; }
	if (fd & UI_CLICK) {
		if (!sm_discard_armed) { sm_discard_armed = 1; return; }
		npend = 0; sm_review = 0; sm_ses_delete(); ui_toast("Nothing was changed"); go_back(); return;
	}
	/* HOME = pause and leave (nothing lost), B = back to testing */
	if (in->down & BTN_HOME) { sm_ses_save(); sm_review = 0; sm_after = 0; in->down = 0; ui_toast("Paused: open Smart match to carry on"); go_back(); return; }
	if (in->down & BTN_B) { sm_review = 0; sm_after = 0; in->down = 0; ui_set_focus(7200); }
}

/* the matches on the right, and the library switch */
static void sm_matches(void)
{
	text(F_BODYB, 350, 92, "Best matches", C_TEXT, 255);
	int ng = smart_groups(sm_g, 3);
	for (int k = 0; k < ng; k++) {
		const PSet *s = &PRESETS_DB[sm_g[k].best];
		int y = 118 + k * 70;
		gfx_round(350, y, 250, 64, 14, k == 0 ? C_CARD_HI : C_CARD, 255);
		gfx_round_line(350, y, 250, 64, 14, 1, C_LINE, 255);
		text_fit(F_BODYB, 362, y + 4, 170, s->brands, C_TEXT, 255);
		char info[40]; snprintf(info, sizeof info, s->addr > 255 ? "%s %04X" : "%s %02X", PROTO_NAME[s->proto], s->addr);
		text_fit(F_SMALL, 362, y + 28, 150, info, C_DIM, 255);
		float pc = sm_g[k].pct;
		gfx_round(362, y + 50, 150, 6, 3, C_INSET, 255);
		int bw = (int)(150 * pc / 100); if (bw > 0) gfx_round(362, y + 50, bw < 6 ? 6 : bw, 6, 3, pc >= 60 ? C_GOOD : C_ACCENT, 255);
		char pt[12]; snprintf(pt, sizeof pt, "%d%%", (int)(pc + 0.5f));
		text_r(F_BODYB, 590, y + 4, pt, pc >= 60 ? C_GOOD : C_TEXT, 255);
		if (ui_button(7240 + k, 526, y + 30, 64, 28, "Use", ST_NORMAL) & UI_CLICK) { sm_use(sm_g[k].best); return; }
	}
	char nl[48]; snprintf(nl, sizeof nl, "Own layout, not listed: %d%%", (int)(smart_none_pct() + 0.5f));
	text_fit(F_SMALL, 350, 330, 250, nl, C_DIM, 255);
	if (ui_ibutton(7250, 350, 352, 250, 34, IC_RADAR, sm_skip_db ? "Library guesses: off" : "Library guesses: on", ST_NORMAL) & UI_CLICK) { sm_skip_db = !sm_skip_db; sm_have = 0; }
	if (npend) { char pn[40]; snprintf(pn, sizeof pn, "%d change%s waiting for Save", npend, npend == 1 ? "" : "s"); text_fit(F_SMALL, 350, 394, 250, pn, C_ACCENT, 255); }
	else if (smart_none_pct() > 60 && smart_answers() >= 4) text_fit(F_SMALL, 350, 394, 250, "Not listed: it now guesses from your TV's layout", C_DIM, 255);
}

void scr_smart(void)
{
	int blocked = help_is_open() || sm_review || sm_resume;
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_STAR, 58, 50, 30, C_ACCENT);
	text(F_TITLE, 86, 30, "Smart match", C_TEXT, 255);
	char sub[80];
	int wl[FN_COUNT], nw = smart_wanted(wl, FN_COUNT);
	if (sm_phase == 0 && ck_pos < ck_n && placeholder_name(cur.btn[ck_list[ck_pos]].name))
		snprintf(sub, sizeof sub, "Step 2 of 4: what your unnamed buttons do   -   %d of %d", ck_pos + 1, ck_n);
	else if (sm_phase == 0) snprintf(sub, sizeof sub, "Step 1 of 4: checking your named buttons   -   %d of %d", ck_pos + 1 > ck_n ? ck_n : ck_pos + 1, ck_n);
	else if (nw) snprintf(sub, sizeof sub, "Step 3 of 4: finding the right code for %s%s", fid_friendly(wl[0]), nw > 1 ? " and more" : "");
	else snprintf(sub, sizeof sub, "Step 4 of 4: looking for more buttons   -   %d answers", smart_answers());
	text(F_SMALL, 88, 64, sub, C_DIM, 255);

	if (sm_phase == 0 && ck_pos < ck_n) {
		/* ---- step 1: is this saved button right? ---- */
		RBtn *b = &cur.btn[ck_list[ck_pos]];
		int f = fid_from_name(b->name), risky = ck_risky(b), ph = placeholder_name(b->name);
		gfx_round(40, 92, 292, 100, 16, C_CARD_HI, 255);
		gfx_round_line(40, 92, 292, 100, 16, 1, C_LINE, 255);
		text(F_SMALL, 56, 98, sm_sent ? "Your saved button, just sent:" : "Your saved button:", C_DIM, 255);
		text_fit(F_TITLE, 56, 116, 260, b->name, C_TEXT, 255);
		char cd[48]; snprintf(cd, sizeof cd, b->addr > 255 ? "%s %04X / %02X   -   sent %dx" : "%s %02X / %02X   -   sent %dx", PROTO_NAME[b->proto], b->addr, b->cmd, sm_sent);
		text_fit(F_SMALL, 56, 162, 262, cd, C_DIM, 255);
		char q[64];
		if (!sm_sent && risky) snprintf(q, sizeof q, "%s can leave the Wii picture. Send it?", resp_from_name(b->name) == R_POWER ? "Power" : "Input");
		else if (ck_menu_key(b)) snprintf(q, sizeof q, "Open the TV menu, Send, did it %s?", f == FN_BACK ? "close" : "move");
		else if (ph || f < 0) snprintf(q, sizeof q, "Did the TV do anything?");
		else snprintf(q, sizeof q, "Did the TV do %s?", fid_friendly(f));
		text_fit(F_BODYB, 40, 200, 292, q, C_TEXT, 255);
		int y = 226;
		char yl[40]; if (ph || f < 0) snprintf(yl, sizeof yl, "Yes, it works"); else snprintf(yl, sizeof yl, "Yes, %s", fid_friendly(f));
		int fa = ui_button(7200, 40, y, 142, 42, sm_sent ? "Send again" : "Send", sm_sent ? ST_NORMAL : ST_PRIMARY);
		int fy = ui_button(7201, 190, y, 142, 42, yl, ST_NORMAL);
		int fn = ui_button(7202, 40, y + 50, 142, 42, "Did nothing", ST_NORMAL);
		int fs = ui_button(7203, 190, y + 50, 142, 42, "Something else", ST_NORMAL);
		int fk = ui_button(7204, 40, y + 100, 142, 42, "Skip", ST_NORMAL);
		int ft = ui_button(7205, 190, y + 100, 142, 42, "Trust the rest", ST_NORMAL);
		text_fit(F_SMALL, 40, 376, 292, risky ? "TV went off? Press Send again to turn it back on" : ck_menu_key(b) ? "Arrows and OK only work with a menu open" : "Names can be wrong: checking each", C_ACCENT, 255);
		if (!blocked && !sm_other) {
			if (fa & UI_CLICK) { send_code_press(b->proto, b->addr, b->cmd, 0); sm_sent++; }
			if (fy & UI_CLICK) {
				if (ph) { sm_other = 2; ui_set_focus(9000); in->down = 0; }   /* works, but as what? */
				else ck_answer(1, f >= 0 ? resp_for_fid(f) : resp_from_name(b->name), f, NULL);
			}
			if (fn & UI_CLICK) {
				if (ck_menu_key(b)) { hist_push(H_SKIPCK, ck_pos, 0, 0, 0); ck_done[ck_pos] = 4; ck_pos++; ck_next(); ui_toast("Kept: this key only works inside a TV menu"); }
				else ck_answer(2, R_NONE, -1, NULL);
			}
			if (fs & UI_CLICK) { sm_other = 2; ui_set_focus(9000); in->down = 0; }
			if (fk & UI_CLICK) { hist_push(H_SKIPCK, ck_pos, 0, 0, 0); ck_done[ck_pos] = 4; ck_pos++; ck_next(); }
			if (ft & UI_CLICK) { sm_phase = 1; sm_have = 0; ui_toast("OK. The rest count as half-sure"); }
		}
	} else {
		/* ---- step 2: the next question: a library guess, or a layout guess on your TV's address ---- */
		if (sm_phase == 0) sm_phase = 1;
		if (!sm_have && !blocked) {
			sm_hex = 0;
			sm_have = smart_suggest2(&sm_p, &sm_a, &sm_c, &sm_fid, &sm_conf, !sm_skip_db);
			if (sm_have && sm_conf < 0.4f) sm_hex = 1;   /* not sure which button: ask what it did */
			if (!sm_have) {
				/* nothing to go on: try codes in order on the likely address */
				static int sm_hexc = -1;
				if (!smart_home_confirmed(&sm_p, &sm_a) && !smart_home(&sm_p, &sm_a)) { if (have_cur && !sm_new_remote) { sm_p = cur.proto; sm_a = cur.addr; } else { sm_p = P_NEC; sm_a = 0; } }
				for (int k = 0; k < 256; k++) { sm_hexc = (sm_hexc + 1) & 0xFF; if (smart_status(sm_p, sm_a, (u8)sm_hexc) < 2) break; }
				sm_c = (u8)sm_hexc; sm_fid = -1; sm_hex = 1; sm_have = 1;
			}
			sm_sent = 0; sm_other = 0;
			/* "Pressing Volume up...": send it straight away */
			send_code_press(sm_p, sm_a, sm_c, 0); sm_sent = 1;
			ui_set_focus(7200);
		}
		gfx_round(40, 92, 292, 100, 16, C_CARD_HI, 255);
		gfx_round_line(40, 92, 292, 100, 16, 1, C_LINE, 255);
		char lab[48];
		if (sm_hex && sm_fid >= 0) snprintf(lab, sizeof lab, "Trying a code (maybe %s)...", fid_friendly(sm_fid));
		else snprintf(lab, sizeof lab, sm_hex ? "Trying a new code..." : "Pressing...");
		text_fit(F_SMALL, 56, 98, 262, lab, C_DIM, 255);
		char big[32];
		if (sm_hex) snprintf(big, sizeof big, "Code %02X", sm_c); else snprintf(big, sizeof big, "%s", fid_friendly(sm_fid));
		text_fit(F_TITLE, 56, 116, 260, big, C_TEXT, 255);
		char cd[48]; snprintf(cd, sizeof cd, sm_a > 255 ? "%s %04X / %02X   -   sent %dx" : "%s %02X / %02X   -   sent %dx", PROTO_NAME[sm_p], sm_a, sm_c, sm_sent);
		text_fit(F_SMALL, 56, 162, 262, cd, C_DIM, 255);

		text(F_BODYB, 40, 200, sm_hex ? "Did the TV do anything?" : "Did the TV do that?", C_TEXT, 255);
		int y = 226;
		char yl[40]; if (sm_hex) snprintf(yl, sizeof yl, "It did something"); else snprintf(yl, sizeof yl, "Yes, %s", fid_friendly(sm_fid));
		int fa = ui_button(7200, 40, y, 142, 42, "Send again", ST_NORMAL);
		int fy = ui_button(7201, 190, y, 142, 42, yl, ST_NORMAL);
		int fn = ui_button(7202, 40, y + 50, 142, 42, "Did nothing", ST_NORMAL);
		int fs = ui_button(7203, 190, y + 50, 142, 42, sm_hex ? "Skip this code" : "Something else", ST_NORMAL);
		int fx = ui_button(7204, 40, y + 100, 142, 42, npend ? "Finish & save" : "Finish", npend ? ST_PRIMARY : ST_NORMAL);
		int fu = ui_button(7205, 190, y + 100, 142, 42, "Undo answer", ST_NORMAL);
		text_fit(F_SMALL, 40, 376, 292, "Picture gone? Keep pressing Send again", C_ACCENT, 255);

		if (!blocked && !sm_other) {
			/* Send again: A can be pressed over and over until the picture comes back */
			if (fa & UI_CLICK) { send_code_press(sm_p, sm_a, sm_c, 0); sm_sent++; }
			if (fy & UI_CLICK) {
				if (sm_hex) { sm_other = 1; ui_set_focus(9000); in->down = 0; }
				else { sm_answer(resp_for_fid(sm_fid), sm_fid, FUNC_NAME[sm_fid]); return; }
			}
			if (fn & UI_CLICK) { sm_answer(R_NONE, -1, NULL); return; }
			if (fs & UI_CLICK) { if (sm_hex) { smart_skip(sm_p, sm_a, sm_c); sm_have = 0; } else { sm_other = 1; ui_set_focus(9000); in->down = 0; } }
			if (fx & UI_CLICK) { sm_leave(); return; }
			if (fu & UI_CLICK) sm_undo();
		}
	}

	if (!sm_review) sm_matches();
	if (screen != S_SMART) return;

	/* "it did something, but not that": what did it do? */
	if (sm_other) {
		int which = sm_other >= 3 ? sm_other_from : sm_other;
		int ch = -1, fid = -1, r = R_OTHER, notsure = 0;
		const char *nm = NULL;
		if (sm_other == 3) {
			ch = menu_popup(&sm_other, IN_IT, IN_IC, IN_N, "Which input did it pick?");
			if (ch < 0) return;
			fid = IN_FID[ch]; r = R_INPUT; nm = FUNC_NAME[fid];
		} else if (sm_other == 4) {
			ch = menu_popup(&sm_other, PW_IT, PW_IC, PW_N, "Which kind of power?");
			if (ch < 0) return;
			fid = PW_FID[ch]; r = R_POWER; nm = FUNC_NAME[fid];
		} else {
			ch = menu_popup(&sm_other, WHAT_IT, WHAT_IC, WHAT_N, "What did it do?");
			if (ch < 0) return;
			if (WHAT_FID[ch] == FN_INPUT || WHAT_FID[ch] == FN_POWER) {   /* one more question */
				sm_other_from = which; sm_other = WHAT_FID[ch] == FN_INPUT ? 3 : 4; ui_set_focus(9000); in->down = 0;
				return;
			}
			fid = WHAT_FID[ch]; r = WHAT_R[ch];
			nm = fid >= 0 ? FUNC_NAME[fid] : ch == 10 ? "NUMBER" : NULL;
			notsure = ch == WHAT_N - 1;
		}
		sm_other = 0;
		if (which == 2 && sm_phase == 0 && ck_pos < ck_n) {
			if (notsure) ck_answer(4, R_OTHER, -1, NULL);   /* not sure: it did something, keep the name */
			else ck_answer(fid >= 0 && fid == fid_from_name(cur.btn[ck_list[ck_pos]].name) ? 1 : 3, r, fid, nm);
		} else if (notsure) sm_answer(R_OTHER, -1, NULL);
		else sm_answer(r, fid, nm);
		return;
	}
	if (sm_review) { sm_review_panel(); return; }
	if (sm_resume) {
		char t[64], a0[48];
		snprintf(t, sizeof t, "Carry on where you left off?");
		snprintf(a0, sizeof a0, "Carry on (%d answers)", sm_res_answers);
		const char *it[2] = { a0, "Start fresh" };
		static const int ic[2] = { IC_PLAY, IC_CLOSE };
		int ch = menu_popup(&sm_resume, it, ic, 2, t);
		char sub2[80];
		if (!sm_res_same) snprintf(sub2, sizeof sub2, "Your remote changed since: answers kept, the rest starts again");
		else snprintf(sub2, sizeof sub2, "%d change%s waiting for Save", sm_res_pend, sm_res_pend == 1 ? "" : "s");
		text_c(F_SMALL, SCR_W / 2, 330, sub2, C_ACCENT, 255);
		if (ch == 0) { sm_ses_load(); sm_resume = 0; sm_have = 0; if (sm_phase == 0) ck_next(); ui_set_focus(7200); ui_toast("Carrying on"); }
		else if (ch == 1) { sm_resume = 0; sm_ses_delete(); if (sm_phase == 0) ck_next(); ui_set_focus(7200); }
		else if (!sm_resume) { if (sm_phase == 0) ck_next(); ui_set_focus(7200); }   /* closed: start fresh, the paused one is kept */
		return;
	}

	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("-", "Undo");
	ui_footer_hint("HOME", npend ? "Save / back" : "Back");
	static const char *help[] = {
		"Step 1 checks the buttons already on your remote: names can be wrong.",
		"Step 2 presses a button for you and asks what the TV did.",
		"Lost the Wii picture (input changed)? Keep pressing Send again.",
		"The % on the right is how sure it is. Use = every button it knows.",
		"Not listed? It guesses from your TV's own layout (Vol- next to Vol+...).",
		"Nothing changes on your remote until you press Save.",
		"It avoids Power, Input and Channel so you keep seeing the Wii.",
	};
	ui_help("Smart match", help, 7);
	if (!blocked && !sm_other && (in->down & BTN_MINUS)) sm_undo();
	if (!blocked && (in->down & BTN_HOME)) sm_leave();
}

/* ================================================================== */
/* Console profiles: one tap sets the TV up for a games console       */
/* ================================================================== */
static int cs_sel = -1, cs_pick, cs_pick_page, cs_menu;

void cons_text_done(int purpose, const char *txt)
{
	if (purpose == K_CONS_NAME) {
		if (cs_sel < 0 || cs_sel >= cur.ncons) {
			if (cur.ncons >= MAX_CONS) { ui_toast("8 consoles max"); go(S_CONSOLES); return; }
			cs_sel = cur.ncons++;
			cur.cons_btn[cs_sel] = -1; cur.cons_note[cs_sel][0] = 0;
		}
		snprintf(cur.cons_name[cs_sel], NAME_LEN, "%s", txt[0] ? txt : "Console");
	} else if (cs_sel >= 0 && cs_sel < cur.ncons) {
		snprintf(cur.cons_note[cs_sel], CONS_NOTE, "%s", txt);
	}
	save_cur();
	go(S_CONSOLES);
}

static void cons_apply(int k)
{
	int b = cur.cons_btn[k];
	if (b >= 0 && b < cur.nbtn) press_btn(b, 0);
	char m[48]; snprintf(m, sizeof m, b >= 0 ? "%s: sent %s" : "%s (no TV button set)", cur.cons_name[k], b >= 0 ? cur.btn[b].name : "");
	ui_toast(m);
}

void scr_consoles(void)
{
	if (cs_sel >= cur.ncons) { cs_sel = -1; cs_menu = cs_pick = 0; }   /* profile gone (other TV, deleted) */
	int blocked = help_is_open() || cs_menu || cs_pick;
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_TV, 58, 50, 30, C_ACCENT);
	text(F_TITLE, 86, 30, "Consoles", C_TEXT, 255);
	text(F_SMALL, 88, 64, "One tap sets the TV up for each console", C_DIM, 255);
	for (int k = 0; k <= cur.ncons && k < MAX_CONS; k++) {
		int x = 40 + (k % 2) * 284, y = 92 + (k / 2) * 76;
		if (k == cur.ncons) {
			if (ui_tile(7300 + k, x, y, 276, 68, IC_PLUS, "New console", ST_NORMAL, 0) & UI_CLICK) { cs_sel = -1; kb_open(K_CONS_NAME, "PS2", "Console name", NAME_LEN - 1); return; }
			break;
		}
		int f = ui_item(7300 + k, x, y, 276, 68);
		gfx_round(x, y, 276, 68, 14, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, 255);
		gfx_round_line(x, y, 276, 68, 14, 1, C_LINE, 255);
		text_fit(F_BODYB, x + 14, y + 6, 180, cur.cons_name[k], C_TEXT, 255);
		int b = cur.cons_btn[k];
		char s1[40]; snprintf(s1, sizeof s1, "Sends: %s", b >= 0 && b < cur.nbtn ? cur.btn[b].name : "nothing yet");
		text_fit(F_SMALL, x + 14, y + 30, 250, s1, C_DIM, 255);
		text_fit(F_SMALL, x + 14, y + 48, 250, cur.cons_note[k][0] ? cur.cons_note[k] : "No notes", C_DIM, 255);
		if (f & UI_FOCUS) ui_focus_ring(x, y, 276, 68, 14);
		if (f & UI_CLICK) { cs_sel = k; cons_apply(k); cs_menu = 1; ui_set_focus(9000); }
	}
	ui_footer_begin();
	ui_footer_hint("A", "Use");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Make one for each console: PS2, N64, Wii...",
		"Pick a TV button or macro it sends (e.g. go to AV2, picture mode Game).",
		"Notes: write your settings, e.g. Colour 42 Tint -3.",
		"Tip: make a macro first (Remote > More > New macro).",
	};
	if (!blocked) ui_help("Consoles", help, 4);

	if (cs_menu && cs_sel >= 0 && cs_sel < cur.ncons) {
		/* show the notes big, with what you can change */
		static const char *items[] = { "Send again", "Game Night setup", "Pick TV button / macro", "Write notes", "Rename", "Delete", "Close" };
		static const int ics[] = { IC_PLAY, IC_GAMEPAD, IC_REMOTE, IC_NOTE, IC_PENCIL, IC_TRASH, IC_CLOSE };
		char t[40]; snprintf(t, sizeof t, "%s", cur.cons_name[cs_sel]);
		int ch = menu_popup(&cs_menu, items, ics, 7, t);
		if (ch == 1) { cs_menu = 0; game_night_start(cs_sel); return; }
		if (ch > 1) ch--;
		if (cur.cons_note[cs_sel][0]) {
			gfx_round(40, 14, 560, 30, 12, C_HUD, 235);
			text_fit(F_SMALL, 52, 19, 536, cur.cons_note[cs_sel], C_WHITE, 255);
		}
		switch (ch) {
		case 0: cons_apply(cs_sel); break;
		case 1: cs_menu = 0; cs_pick = 1; cs_pick_page = 0; ui_set_focus(-1); break;
		case 2: cs_menu = 0; kb_open(K_CONS_NOTE, cur.cons_note[cs_sel], "Your settings for this console", NOTE_LEN - 1); return;
		case 3: cs_menu = 0; kb_open(K_CONS_NAME, cur.cons_name[cs_sel], "Console name", NAME_LEN - 1); return;
		case 4:
			memmove(&cur.cons_name[cs_sel], &cur.cons_name[cs_sel + 1], (cur.ncons - cs_sel - 1) * sizeof cur.cons_name[0]);
			memmove(&cur.cons_btn[cs_sel], &cur.cons_btn[cs_sel + 1], (cur.ncons - cs_sel - 1) * sizeof cur.cons_btn[0]);
			memmove(&cur.cons_note[cs_sel], &cur.cons_note[cs_sel + 1], (cur.ncons - cs_sel - 1) * sizeof cur.cons_note[0]);
			cur.ncons--; save_cur(); cs_menu = 0; cs_sel = -1; ui_toast("Deleted"); break;
		case 5: cs_menu = 0; break;
		}
		return;
	}
	if (cs_pick) {
		ui_suspend(0);
		ui_dim();
		ui_panel(30, 30, 580, 390);
		text(F_TITLE, 56, 44, "Which button?", C_TEXT, 255);
		int per = 16, pages = (cur.nbtn + per - 1) / per; if (pages < 1) pages = 1;
		for (int i = cs_pick_page * per; i < cur.nbtn && i < (cs_pick_page + 1) * per; i++) {
			int k = i - cs_pick_page * per;
			RBtn *b = &cur.btn[i];
			int ic = icon_for_name(b->name); if (ic < 0 && b->nsteps) ic = IC_SEQ;
			if (ui_tile(7400 + i, 50 + (k % 4) * 136, 94 + (k / 4) * 62, 128, 54, ic, b->name, ST_KEY, 0) & UI_CLICK) {
				cur.cons_btn[cs_sel] = i; save_cur(); cs_pick = 0; ui_toast("Saved");
			}
		}
		int by = 360;
		if (pages > 1) {
			if (ui_tile(7490, 250, by, 60, 46, IC_LEFT, "", ST_NORMAL, 0) & UI_CLICK) cs_pick_page = (cs_pick_page + pages - 1) % pages;
			if (ui_tile(7491, 330, by, 60, 46, IC_RIGHT, "", ST_NORMAL, 0) & UI_CLICK) cs_pick_page = (cs_pick_page + 1) % pages;
		}
		if (ui_ibutton(7492, 460, by, 130, 46, IC_BACK, "Cancel", ST_NORMAL) & UI_CLICK) cs_pick = 0;
		if (in->down & (BTN_HOME | BTN_B)) { cs_pick = 0; in->down = 0; }
		return;
	}
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) go_back();
}

/* ================================================================== */
/* Remote Setup & Mapping: every remote tool in one place             */
/* ================================================================== */
void scr_setup(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	icon(IC_REMOTE, 58, 50, 30, C_ACCENT);
	text(F_TITLE, 86, 30, "Remote Setup", C_TEXT, 255);
	char sub[48]; snprintf(sub, sizeof sub, have_cur ? "TV: %s" : "Pick a TV on Home for the TV tools", cur.name);
	text_fit(F_SMALL, 88, 64, 440, sub, have_cur ? C_ACCENT : C_DIM, 255);
	/* the three things most people need, big; everything else under More tools */
	static int more_open;
	int blocked2 = blocked || more_open;
	struct { int ic; const char *label, *sub; int need_tv; } T[3] = {
		{ IC_PLUS, "New remote", "Brand or codes", 0 },
		{ IC_STAR, "Smart match", "Checks, then finds more", 0 },
		{ IC_SEARCH, "Find buttons", "More buttons for this TV", 1 },
	};
	for (int k = 0; k < 3; k++) {
		int x = 40 + k * 188, y = 96;
		int dim = T[k].need_tv && !have_cur;
		int f = ui_tile(7500 + k, x, y, 180, 150, T[k].ic, T[k].label, k == 1 ? ST_PRIMARY : ST_NORMAL, 0);
		text_c(F_SMALL, x + 90, y + 160, T[k].sub, C_DIM, 255);
		if (dim) gfx_round(x, y, 180, 150, 16, C_BG1, 120);
		if (!(f & UI_CLICK) || blocked2) continue;
		if (dim) { ui_toast("Pick a TV on Home first"); continue; }
		if (k == 0) { wizard_start(); return; }
		if (k == 1) { smart_start(); return; }
		if (k == 2) { finder_start(); return; }
	}
	if ((ui_ibutton(7510, 40, 300, 560, 48, IC_MORE, "More tools", ST_NORMAL) & UI_CLICK) && !blocked2) { more_open = 1; ui_set_focus(9000); in->down = 0; }
	static const char *MI[7] = { "Find codes", "Import a file", "Controller map", "Controllers", "Keyboard keys", "Service menu codes", "Cancel" };
	static const int MIC[7] = { IC_RADAR, IC_SAVE, IC_GAMEPAD, IC_GAMEPAD, IC_MENU, IC_WRENCH, IC_CLOSE };
	int ch = menu_popup(&more_open, MI, MIC, 7, "More tools");
	if (ch >= 2 && ch <= 5 && ch != 3 && !have_cur) { ui_toast("Pick a TV on Home first"); ch = -1; }
	switch (ch) {
	case 0: scan_start(); return;
	case 1: go(S_IMPORT); return;
	case 2: map_start(); return;
	case 3: go(S_PADS); return;
	case 4: go(S_KEYMAP); return;
	case 5: go(S_PRESETS); return;
	}
	if (more_open) return;
	ui_footer_begin();
	ui_footer_hint("A", "Open");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"New remote: pick a brand, scan, or import a file.",
		"Find codes: tries every code until the TV reacts.",
		"Smart match: guesses the rest of your remote from a few tests.",
		"Find buttons: add more buttons to the TV you picked.",
		"Controller map: which Wii button sends which TV button.",
		"Controllers: give each controller a job.",
	};
	ui_help("Remote Setup", help, 6);
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) go_back();
}

/* ================================================================== */
/* Input shortcut: one button that jumps straight to one input        */
/* ================================================================== */
static int im_start = -1, im_input = -1, im_times = 2, im_wait = 800;
static int im_power = -1, im_pic = -1, im_picn = 1, im_cons = -1;   /* Game Night: power first, picture mode last, for a console */

static int find_any(const char *names)
{
	char a[96]; snprintf(a, sizeof a, "%s", names);
	for (char *t = strtok(a, "|"); t; t = strtok(NULL, "|")) { int i = remote_find_btn(&cur, t); if (i >= 0 && !cur.btn[i].nsteps) return i; }
	return -1;
}

void input_macro_start(void)
{
	im_start = find_any("1|CH+|TV");
	im_input = find_any("INPUT|TV/AV|AV|SOURCE");
	im_times = 2; im_wait = 800; im_power = -1; im_pic = -1; im_picn = 1; im_cons = -1;
	go(S_INPUTMAC);
}
/* Game Night: the same builder, plus TV on first and a picture mode at the end, saved to a console */
void game_night_start(int cons)
{
	input_macro_start();
	im_cons = cons;
	im_power = find_any("POWER|POWER ON");
	im_pic = find_any("PICTURE|P.MODE|PMODE|PICTURE MODE");
}
int game_night_cons(void) { return im_cons; }

static int im_build(Code *c)
{
	int n = 0;
	if (im_power >= 0) { c[n].proto = cur.btn[im_power].proto; c[n].addr = cur.btn[im_power].addr; c[n].cmd = cur.btn[im_power].cmd; c[n].wait = 6000; n++; }   /* old CRTs take a while to warm up */
	if (im_start >= 0) { c[n].proto = cur.btn[im_start].proto; c[n].addr = cur.btn[im_start].addr; c[n].cmd = cur.btn[im_start].cmd; c[n].wait = 1500; n++; }
	for (int k = 0; k < im_times && im_input >= 0 && n < MAX_STEPS; k++) {
		c[n].proto = cur.btn[im_input].proto; c[n].addr = cur.btn[im_input].addr; c[n].cmd = cur.btn[im_input].cmd; c[n].wait = im_wait; n++;
	}
	for (int k = 0; k < im_picn && im_pic >= 0 && n < MAX_STEPS; k++) {
		c[n].proto = cur.btn[im_pic].proto; c[n].addr = cur.btn[im_pic].addr; c[n].cmd = cur.btn[im_pic].cmd; c[n].wait = 900; n++;
	}
	return n;
}

static void cycle_btn(int *v, int d)
{
	/* -1 = none, then every plain button */
	int n = cur.nbtn;
	for (int k = 0; k <= n; k++) {
		*v += d; if (*v < -1) *v = n - 1; if (*v >= n) *v = -1;
		if (*v < 0 || !cur.btn[*v].nsteps) return;
	}
}

void scr_inputmac(void)
{
	int blocked = help_is_open();
	if (blocked) ui_suspend(1);
	gfx_bg();
	int gn = im_cons >= 0 && im_cons < cur.ncons;
	icon(gn ? IC_GAMEPAD : IC_INPUT, 58, 50, 30, C_ACCENT);
	char tt[40]; snprintf(tt, sizeof tt, gn ? "Game Night: %s" : "Input shortcut", gn ? cur.cons_name[im_cons] : "");
	text_fit(F_TITLE, 86, 30, 450, tt, C_TEXT, 255);
	text(F_SMALL, 88, 64, gn ? "One press: TV on, right input, right picture mode" : "One button that goes straight to one input (e.g. AV2 = PS2)", C_DIM, 255);
	char v[24]; int d, y = gn ? 88 : 96;
	if (gn) {
		snprintf(v, sizeof v, "%s", im_power >= 0 ? cur.btn[im_power].name : "Skip");
		if (value_row(7598, y, "0. TV on first", v, &d)) cycle_btn(&im_power, d);
		y += 44;
	}
	snprintf(v, sizeof v, "%s", im_start >= 0 ? cur.btn[im_start].name : "None");
	if (value_row(7600, y, "1. Start from TV", v, &d)) cycle_btn(&im_start, d);
	y += 50;
	snprintf(v, sizeof v, "%s", im_input >= 0 ? cur.btn[im_input].name : "Pick one");
	if (value_row(7602, y, "2. Input button", v, &d)) cycle_btn(&im_input, d);
	y += 50;
	snprintf(v, sizeof v, "%d time%s", im_times, im_times == 1 ? "" : "s");
	if (value_row(7604, y, "3. Press it", v, &d)) { im_times += d; if (im_times < 1) im_times = 1; if (im_times > 8) im_times = 8; }
	y += gn ? 44 : 50;
	snprintf(v, sizeof v, "%d ms", im_wait);
	if (value_row(7606, y, "4. Wait between", v, &d)) { im_wait += d * 100; if (im_wait < 300) im_wait = 300; if (im_wait > 3000) im_wait = 3000; }
	if (gn) {
		y += 44;
		if (im_pic >= 0) snprintf(v, sizeof v, "%.10s x%d", cur.btn[im_pic].name, im_picn); else snprintf(v, sizeof v, "Skip");
		if (value_row(7608, y, "5. Picture mode", v, &d)) { if (im_pic < 0 || (d > 0 && im_picn >= 4) || (d < 0 && im_picn <= 1)) { cycle_btn(&im_pic, d); im_picn = 1; } else im_picn += d; }
		y += 50;
	} else {
		y += 58;
		text_fit(F_SMALL, 40, y, 560, "Old TVs: a number (or CH+) jumps to the TV first, then AV counts from there.", C_DIM, 255);
		y += 30;
	}
	Code c[MAX_STEPS]; int n = im_build(c);
	if (ui_ibutton(7610, 40, y, 180, 46, IC_PLAY, "Test it", ST_NORMAL) & UI_CLICK) { if (n) { queue_codes(c, n); ui_toast("Sending... watch the TV"); } }
	if (ui_ibutton(7611, 230, y, 200, 46, IC_SAVE, "Save as button", ST_NORMAL) & UI_CLICK) {
		if (im_input < 0) ui_toast("Pick the Input button first");
		else {
			memcpy(pend_macro, c, n * sizeof(Code)); pend_macro_n = n;
			if (gn) {
				/* save straight onto the console */
				char nm[NAME_LEN], base[NAME_LEN]; snprintf(base, sizeof base, "%.14s night", cur.cons_name[im_cons]); unique_name(&cur, base, nm);
				int b = remote_add_macro(&cur, nm, pend_macro, pend_macro_n);
				if (b >= 0) { cur.cons_btn[im_cons] = b; save_cur(); ui_toast("Game Night ready: tap the console"); }
				else ui_toast("This remote is full");
				im_cons = -1; go_back(); return;
			}
			kb_open(K_MACRO, "PS2", "Name it (e.g. PS2, N64)", NAME_LEN - 1); return;
		}
	}
	if (ui_ibutton(7612, 440, y, 160, 46, IC_CLOSE, "Cancel", ST_NORMAL) & UI_CLICK) { go_back(); return; }
	ui_footer_begin();
	ui_footer_hint("A", "Change");
	ui_footer_hint("HOME", "Back");
	static const char *help[] = {
		"Most old TVs step AV1, AV2, AV3... each time you press Input.",
		"Start from TV makes it always count from the same place.",
		"Test it, then Save. The new button goes on your remote.",
		"Use it in Consoles for one-tap console switching.",
	};
	ui_help("Input shortcut", help, 4);
	if (!blocked && (in->down & BTN_HOME)) go_back();
}
#ifdef HOST
/* test hook: where Smart match is */
void sm_test_state(int *phase, const char **ckname, u8 *p, u16 *a, u8 *c, int *fid)
{
	*phase = sm_phase; *ckname = (sm_phase == 0 && ck_pos < ck_n) ? cur.btn[ck_list[ck_pos]].name : "";
	*p = sm_p; *a = sm_a; *c = sm_c; *fid = sm_fid;
}
#endif
#ifdef HOST
void sm_debug(void) { printf("smart: review %d resume %d other %d help %d phase %d npend %d armed %d age %d\n", sm_review, sm_resume, sm_other, help_is_open(), sm_phase, npend, sm_discard_armed, sm_review_age); }
#endif
