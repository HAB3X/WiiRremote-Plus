/*
   casino.c - WiiRremote Casino: blackjack with 4 tables, 6 card sets,
   split / double / insurance / surrender, hints, a count trainer, stats and achievements.
   Chips are just for fun. Settings > Extras > Casino.
   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "app.h"
#include <math.h>
#include <stdarg.h>
#include <time.h>

/* ================================================================== */
/* tables and card sets                                               */
/* ================================================================== */
typedef struct {
	const char *name, *blurb;
	int decks, min, max, need;      /* need = chips you must have to sit down */
	int h17;                        /* dealer hits soft 17 */
	int bj_num, bj_den;             /* blackjack pays num:den */
	int surrender, das;             /* late surrender, double after split */
	u32 felt_top, felt_bot, rail;
	int chip[5];
	int variant;                    /* 0 normal, 1 Spanish 21, 2 Free Bet, 3 Switch */
} Table;
static const Table TABLES[7] = {
	{ "Single Deck", "1 deck  -  dealer stands on 17  -  pays 3 to 2", 1, 5, 100, 0, 0, 3, 2, 0, 1,
	  RGB(26, 118, 72), RGB(12, 74, 44), RGB(170, 128, 70), { 5, 10, 25, 50, 100 } },
	{ "Classic", "6 decks  -  give up allowed  -  pays 3 to 2", 6, 25, 500, 0, 0, 3, 2, 1, 1,
	  RGB(22, 84, 150), RGB(10, 48, 96), RGB(150, 150, 170), { 25, 50, 100, 250, 500 } },
	{ "Vegas", "2 decks  -  dealer hits soft 17  -  pays 6 to 5", 2, 10, 1000, 0, 1, 6, 5, 0, 1,
	  RGB(96, 40, 140), RGB(52, 18, 86), RGB(220, 180, 80), { 10, 25, 100, 250, 1000 } },
	{ "High Roller", "8 decks  -  give up allowed  -  pays 3 to 2", 8, 100, 5000, 2500, 1, 3, 2, 1, 1,
	  RGB(130, 24, 36), RGB(70, 10, 18), RGB(230, 196, 110), { 100, 250, 500, 1000, 5000 } },
	{ "Spanish 21", "No tens in the deck  -  your 21 always wins  -  bonuses", 6, 10, 500, 0, 1, 3, 2, 1, 1,
	  RGB(170, 60, 30), RGB(100, 30, 12), RGB(230, 190, 90), { 10, 25, 50, 100, 250 }, 1 },
	{ "Free Bet", "Free doubles on 9-11, free splits  -  dealer 22 pushes", 6, 10, 500, 0, 1, 3, 2, 0, 1,
	  RGB(30, 110, 110), RGB(10, 60, 60), RGB(200, 200, 210), { 10, 25, 50, 100, 250 }, 2 },
	{ "Switch", "Two hands, swap the top cards  -  blackjack pays 1 to 1  -  22 pushes", 6, 10, 500, 0, 1, 1, 1, 0, 1,
	  RGB(60, 60, 140), RGB(24, 24, 80), RGB(210, 200, 230), { 10, 25, 50, 100, 250 }, 3 },
};
#define NT 7

enum { CS_CLASSIC, CS_ROYAL, CS_FOUR, CS_NEON, CS_WII, CS_BIG, CS_COUNT };
static const char *CS_NAME[CS_COUNT] = { "Classic", "Royal", "Four colour", "Neon", "Wii", "Big print" };
static const char *CS_TIP[CS_COUNT] = { "Red and black, blue back", "Ivory cards, crimson and gold back", "Every suit its own colour",
	"Glowing suits on dark cards", "Clean white, like the Wii Menu", "Huge numbers, easy to read on a CRT" };

/* ================================================================== */
/* saved stuff: casino.txt                                            */
/* ================================================================== */
enum { AC_FIRST_WIN, AC_BJ, AC_STREAK5, AC_SPLIT_WIN, AC_DOUBLE_WIN, AC_5CARD, AC_10K, AC_HIGHROLLER, AC_SUITE, AC_ROUL, AC_777, AC_ROYAL, AC_HILO10, AC_BACC_TIE, AC_LIAR, AC_SPOTON, AC_HOLDEM, AC_BLUFF, AC_COUNT };
static const char *AC_NAME[AC_COUNT] = { "First win", "Blackjack!", "Hot streak (5 wins)", "Split and win", "Double and win",
	"5-card 21", "10,000 chips", "Beat the High Roller", "Into the Suite", "Roulette: straight up", "Triple seven", "Royal flush",
	"Hi-Lo: 10 in a row", "Baccarat tie", "Last cup standing", "Spot on!", "Hold'em champion", "Bluffed them all" };
static struct {
	int played, won, lost, pushed, bjs, best_win, best_streak, max_chips, streak;
	int ach, cardset, hints, count, speed, quips, last_bet[NT];
	int last_game, last_sec, last_tb, bet_k[6], stop_loss, win_goal, bonus_day, chips2, player, hist[40], nhist, rel[6], snd;
	int ld_opp, ld_buy, ld_dice, ld_wild, ld_spot, ld_pali, ld_played, ld_won;
	int he_opp, he_buy, he_played, he_won, he_hands;
	int free, free_now, real_chips;   /* free play: practice chips, your real ones are put aside */
	/* comfort settings (Options > More options) */
	int talk_spd, skip_anim, big_text, big_check, helper, brk, vol_m, vol_v, vol_f, rum;
	int p2;   /* a second person at Liar's Dice / Hold'em (pass the remote), not saved */
	int fav, recent[3];               /* favourite games (bits), last 3 played */
	int gs_p[16], gs_w[16], gs_best[16], gs_sec[16];   /* per game: played, won, best win, seconds */
	int srb[3][48];                   /* saved roulette layouts, one per wheel */
} S = { .talk_spd = 1, .vol_m = 3, .vol_v = 3, .vol_f = 3, .rum = 1, .recent = { -1, -1, -1 }, .speed = 1, .quips = 2, .snd = 3, .ld_opp = 0x23, .ld_buy = 1, .ld_dice = 5, .ld_wild = 1, .ld_spot = 1, .ld_pali = 0, .he_opp = 0x2D, .he_buy = 1, .last_game = -1, .chips2 = 1000, .bet_k = { 1, 1, 1, 1, 1, 1 } };   /* quips: 0 off, 1 quiet, 2 full */
static int loaded;
/* rumble in the casino can be switched off on its own */
static void cas_rumble(int ms) { if (S.rum) plat_rumble(ms); }
#define plat_rumble cas_rumble
/* animations: skipped, or sped up while B is held */
static int ff_held;
static int anim_k(void) { return S.skip_anim || ff_held ? 25 : 100; }
/* how fast the dealers talk: slow, normal, fast, instant */
static int tspd_k(void) { static const u8 K[4] = { 140, 100, 55, 0 }; return K[S.talk_spd & 3]; }
/* speech bubbles: normal or big text */
static const Font *bub_font(void) { return S.big_text ? F_BODY : F_SMALL; }
static int bub_lh(void) { return S.big_text ? 22 : 17; }
/* the chat log: the last 20 things anyone said (press - to read it) */
static struct { s8 who; char t[200]; } CL[20]; static int cl_n, cl_open;
static void chatlog_add(int who, const char *t)
{
	if (!t || !t[0]) return;
	if (cl_n == 20) { memmove(CL, CL + 1, 19 * sizeof CL[0]); cl_n = 19; }
	CL[cl_n].who = who; snprintf(CL[cl_n].t, sizeof CL[cl_n].t, "%s", t); cl_n++;
}
/* are you sure? (big bets, if that option is on) */
static void (*bigc_fn)(void); static int bigc_open, bigc_amt, bigc_pass;
static int big_ok(int amount, void (*fn)(void))
{
	if (bigc_pass) { bigc_pass = 0; return 1; }
	if (!S.big_check || S.free_now || amount * 4 <= cfg.chips) return 1;
	bigc_fn = fn; bigc_amt = amount; bigc_open = 1; ui_set_focus(9000);
	return 0;
}

static void stats_path(char *p, int n) { snprintf(p, n, "%s/casino.txt", base_dir); }
static void stats_load(void)
{
	loaded = 1;
	char p[128]; stats_path(p, sizeof p);
	FILE *f = fopen(p, "r"); if (!f) return;
	char line[512];
	while (fgets(line, sizeof line, f)) {
		char *e = strchr(line, '='); if (!e) continue; *e = 0; int v = atoi(e + 1);
		if (!strcmp(line, "played")) S.played = v; else if (!strcmp(line, "won")) S.won = v; else if (!strcmp(line, "lost")) S.lost = v;
		else if (!strcmp(line, "pushed")) S.pushed = v; else if (!strcmp(line, "bjs")) S.bjs = v; else if (!strcmp(line, "best_win")) S.best_win = v;
		else if (!strcmp(line, "best_streak")) S.best_streak = v; else if (!strcmp(line, "max_chips")) S.max_chips = v;
		else if (!strcmp(line, "ach")) S.ach = v; else if (!strcmp(line, "cardset")) S.cardset = ((v % CS_COUNT) + CS_COUNT) % CS_COUNT;
		else if (!strcmp(line, "dealer")) S.quips = v ? 2 : 0;   /* older files: on / off */
		else if (!strcmp(line, "chat")) S.quips = v < 0 ? 0 : v > 2 ? 2 : v;
		else if (!strcmp(line, "lastg")) S.last_game = v; else if (!strcmp(line, "lasts")) S.last_sec = v; else if (!strcmp(line, "lastt")) S.last_tb = v;
		else if (!strncmp(line, "bk", 2) && line[2] >= '0' && line[2] < '6') S.bet_k[line[2] - '0'] = v & 3;
		else if (!strncmp(line, "rel", 3) && line[3] >= '0' && line[3] < '6') S.rel[line[3] - '0'] = v;
		else if (!strcmp(line, "sl")) S.stop_loss = v & 3; else if (!strcmp(line, "wg")) S.win_goal = v & 3;
		else if (!strcmp(line, "bday")) S.bonus_day = v; else if (!strcmp(line, "chips2")) S.chips2 = v; else if (!strcmp(line, "player")) S.player = v & 1;
		else if (!strcmp(line, "hist")) { char *q = e + 1; S.nhist = 0; while (*q && S.nhist < 40) { S.hist[S.nhist++] = atoi(q); q = strchr(q, ','); if (!q) break; q++; } }
		else if (!strcmp(line, "snd")) S.snd = v & 3;
		else if (!strcmp(line, "tspd")) S.talk_spd = v & 3; else if (!strcmp(line, "skip")) S.skip_anim = v & 1; else if (!strcmp(line, "bigtxt")) S.big_text = v & 1;
		else if (!strcmp(line, "bigchk")) S.big_check = v & 1; else if (!strcmp(line, "helper")) S.helper = v & 1; else if (!strcmp(line, "brk")) S.brk = v % 3;
		else if (!strcmp(line, "volm")) S.vol_m = v & 3; else if (!strcmp(line, "volv")) S.vol_v = v & 3; else if (!strcmp(line, "volf")) S.vol_f = v & 3; else if (!strcmp(line, "rum")) S.rum = v & 1;
		else if (!strcmp(line, "fav")) S.fav = v; else if (!strcmp(line, "recent")) sscanf(e + 1, "%d,%d,%d", &S.recent[0], &S.recent[1], &S.recent[2]);
		else if (!strncmp(line, "gs", 2) && line[2] >= 'a' && line[2] <= 'z') { int k = atoi(line + 3); if (k >= 0 && k < 16) { int *A = line[2] == 'p' ? S.gs_p : line[2] == 'w' ? S.gs_w : line[2] == 'b' ? S.gs_best : S.gs_sec; A[k] = v; } }
		else if (!strncmp(line, "srb", 3) && line[3] >= '0' && line[3] < '3') { int t = line[3] - '0'; char *q = e + 1; memset(S.srb[t], 0, sizeof S.srb[t]); while (*q) { int sp = atoi(q); char *c = strchr(q, ':'); if (!c) break; int am = atoi(c + 1); if (sp >= 0 && sp < 48) S.srb[t][sp] = am; q = strchr(c, ','); if (!q) break; q++; } }
		else if (!strcmp(line, "free")) S.free = v & 1; else if (!strcmp(line, "free_now")) S.free_now = v & 1; else if (!strcmp(line, "real_chips")) S.real_chips = v;
		else if (!strcmp(line, "ld_opp")) S.ld_opp = v & 63; else if (!strcmp(line, "ld_buy")) S.ld_buy = v % 5; else if (!strcmp(line, "ld_dice")) S.ld_dice = v < 3 || v > 5 ? 5 : v;
		else if (!strcmp(line, "ld_wild")) S.ld_wild = v & 1; else if (!strcmp(line, "ld_spot")) S.ld_spot = v & 1; else if (!strcmp(line, "ld_pali")) S.ld_pali = v & 1;
		else if (!strcmp(line, "ld_played")) S.ld_played = v; else if (!strcmp(line, "ld_won")) S.ld_won = v;
		else if (!strcmp(line, "he_opp")) S.he_opp = v & 63; else if (!strcmp(line, "he_buy")) S.he_buy = v % 5;
		else if (!strcmp(line, "he_played")) S.he_played = v; else if (!strcmp(line, "he_won")) S.he_won = v; else if (!strcmp(line, "he_hands")) S.he_hands = v;
		else if (!strcmp(line, "hints")) S.hints = v; else if (!strcmp(line, "count")) S.count = v; else if (!strcmp(line, "speed")) S.speed = ((v % 3) + 3) % 3;
		else if (!strncmp(line, "bet", 3) && line[3] >= '0' && line[3] < '0' + NT) S.last_bet[line[3] - '0'] = v;
	}
	fclose(f);
}
static void stats_save(void)
{
	char p[128]; stats_path(p, sizeof p);
	FILE *f = fopen(p, "w"); if (!f) return;
	fprintf(f, "played=%d\nwon=%d\nlost=%d\npushed=%d\nbjs=%d\nbest_win=%d\nbest_streak=%d\nmax_chips=%d\nach=%d\ncardset=%d\nhints=%d\ncount=%d\nspeed=%d\n",
	        S.played, S.won, S.lost, S.pushed, S.bjs, S.best_win, S.best_streak, S.max_chips, S.ach, S.cardset, S.hints, S.count, S.speed);
	fprintf(f, "chat=%d\nlastg=%d\nlasts=%d\nlastt=%d\nsl=%d\nwg=%d\nbday=%d\nchips2=%d\nplayer=%d\n", S.quips, S.last_game, S.last_sec, S.last_tb, S.stop_loss, S.win_goal, S.bonus_day, S.chips2, S.player);
	fprintf(f, "snd=%d\nfree=%d\nfree_now=%d\nreal_chips=%d\n", S.snd, S.free, S.free_now, S.real_chips);
	fprintf(f, "tspd=%d\nskip=%d\nbigtxt=%d\nbigchk=%d\nhelper=%d\nbrk=%d\nvolm=%d\nvolv=%d\nvolf=%d\nrum=%d\nfav=%d\nrecent=%d,%d,%d\n",
	        S.talk_spd, S.skip_anim, S.big_text, S.big_check, S.helper, S.brk, S.vol_m, S.vol_v, S.vol_f, S.rum, S.fav, S.recent[0], S.recent[1], S.recent[2]);
	for (int k = 0; k < 16; k++) if (S.gs_p[k]) fprintf(f, "gsp%d=%d\ngsw%d=%d\ngsb%d=%d\ngss%d=%d\n", k, S.gs_p[k], k, S.gs_w[k], k, S.gs_best[k], k, S.gs_sec[k]);
	for (int t = 0; t < 3; t++) { int any = 0; for (int i = 0; i < 48; i++) any |= S.srb[t][i]; if (!any) continue; fprintf(f, "srb%d=", t); int first = 1; for (int i = 0; i < 48; i++) if (S.srb[t][i]) { fprintf(f, "%s%d:%d", first ? "" : ",", i, S.srb[t][i]); first = 0; } fprintf(f, "\n"); }
	fprintf(f, "ld_opp=%d\nld_buy=%d\nld_dice=%d\nld_wild=%d\nld_spot=%d\nld_pali=%d\nld_played=%d\nld_won=%d\n", S.ld_opp, S.ld_buy, S.ld_dice, S.ld_wild, S.ld_spot, S.ld_pali, S.ld_played, S.ld_won);
	fprintf(f, "he_opp=%d\nhe_buy=%d\nhe_played=%d\nhe_won=%d\nhe_hands=%d\n", S.he_opp, S.he_buy, S.he_played, S.he_won, S.he_hands);
	for (int k = 0; k < 6; k++) fprintf(f, "bk%d=%d\nrel%d=%d\n", k, S.bet_k[k], k, S.rel[k]);
	fprintf(f, "hist=");
	for (int k = 0; k < S.nhist; k++) fprintf(f, "%s%d", k ? "," : "", S.hist[k]);
	fprintf(f, "\n");
	for (int t = 0; t < NT; t++) fprintf(f, "bet%d=%d\n", t, S.last_bet[t]);
	fclose(f);
}
static char ach_msg[48]; static u64 ach_until;
static void achieve(int a)
{
	if (S.ach & (1 << a)) return;
	S.ach |= 1 << a;
	snprintf(ach_msg, sizeof ach_msg, "Achievement: %s", AC_NAME[a]);
	ach_until = plat_ms() + 3500;
	plat_rumble(120);
	music_sfx(SFX_ACH, 400);
}

/* ================================================================== */
/* the game                                                           */
/* ================================================================== */
enum { V_FLOOR, V_TABLE, V_DECKS, V_STATS, V_ROUL, V_SLOTS, V_POKER, V_HILO, V_BACC, V_SUMMARY, V_LIAR, V_HOLDEM, V_WAR, V_BIGWHEEL, V_KENO, V_SETUP };
enum { G_BET, G_DEAL, G_INSURE, G_PLAY, G_DEALER, G_DONE };
typedef struct { u8 c; u64 t; } Card;           /* t = when it lands on the table */
typedef struct { Card k[12]; int n, bet, doubled, done, split_aces, surrendered, res, win, free; } Hand;   /* free = chips the house put in (Free Bet) */

static int view = V_FLOOR, back_view = V_FLOOR, tb, g = G_BET;
static u8 shoe[8 * 52]; static int shoe_n, shoe_pos, cut;
static Hand ph[4]; static int nh, cur_h;
static Card dh[14]; static int dn;
static int bet, insurance, running, hole_counted;
static u64 hole_at;   /* when the dealer turned the hole card over */
static int bj_switch_ok;   /* Switch: you can still swap the top cards */
static u64 settle_at, next_t;
static char msg[96]; static int msg_tone;       /* 1 good, -1 bad, 0 neutral */
static int reset_arm, opt_open;
static u32 rng = 12345;
static u32 rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }
static int speed_ms(void) { static const int SP[3] = { 420, 260, 120 }; int v = SP[S.speed % 3] * anim_k() / 100; return v < 40 ? 40 : v; }

static void shoe_fill(void)
{
	int n = TABLES[tb].decks * 52;
	int m = 0;
	for (int i = 0; i < n; i++) if (!(TABLES[tb].variant == 1 && i % 13 == 9)) shoe[m++] = i % 52;   /* Spanish 21: no tens (picture cards stay) */
	n = m;
	for (int i = n - 1; i > 0; i--) { int j = rnd() % (i + 1); u8 t = shoe[i]; shoe[i] = shoe[j]; shoe[j] = t; }
	shoe_n = n; shoe_pos = 0; running = 0;
	cut = n * 3 / 4 + (int)(rnd() % (n / 10 + 1));
	if (cut > n - 16) cut = n - 16;
}
static int hilo(u8 c) { int r = c % 13; return (r >= 1 && r <= 5) ? 1 : (r == 0 || r >= 9) ? -1 : 0; }
static u8 take(int counted) { if (shoe_pos >= shoe_n) shoe_fill(); u8 c = shoe[shoe_pos++]; if (counted) running += hilo(c); return c; }

static int value_of(const Card *k, int n, int *soft)
{
	int t = 0, aces = 0;
	for (int i = 0; i < n; i++) { int r = k[i].c % 13; if (r == 0) { aces++; t += 11; } else t += r >= 9 ? 10 : r + 1; }
	while (t > 21 && aces) { t -= 10; aces--; }
	if (soft) *soft = aces > 0 && t <= 21;
	return t;
}
static int rank10(u8 c) { int r = c % 13; return r == 0 ? 11 : r >= 9 ? 10 : r + 1; }
static int is_bj(const Hand *h) { return nh == 1 && h->n == 2 && value_of(h->k, 2, NULL) == 21; }
static int dealer_bj(void) { return dn == 2 && value_of(dh, 2, NULL) == 21; }
static void say(int tone, const char *m) { msg_tone = tone; snprintf(msg, sizeof msg, "%s", m); }

/* ================================================================== */
/* Phantom Moth, the dealer who reads minds (Casino options > Dealer talks) */
/* ================================================================== */
static char q_text[220]; static u64 q_at;
static void talk_pump(void);
static void talk_reset(void);
static void talk_sit(int w);
static int sess_hands, mind_step, dd_loaded, n_reads, dd_empty;
static Dossier D;                /* what Phantom Moth "sees": read from this Wii, kept in memory only */
static char reads[16][220];
static u8 read_topic[16];
static u64 rum_at; static int rum_left;
static u64 creep_at;              /* when a mind-read line started: the table goes dark for a moment */
static int pw_streak, dw_streak;  /* hands in a row the player / the dealer has won this sitting */
static int anger;                 /* 0 calm, 1 annoyed, 2 furious (shakes) */

/* What he remembers between visits: only WHICH things he has told you (topic numbers) and the score.
   Never your name, age, games or anything else: those are read fresh from the Wii each time. */
enum { T_NAME, T_MII, T_FIT, T_GAME, T_RUMBLE, T_TWO, T_GCWII, T_PLAYTIME, T_CROWD, T_ONLINE, T_BACKUP, T_CIOS, T_ROMS, T_REGION, T_DONE, T_COUNT };
static struct { int visits, told, net, beat_him, beat_you; } M;
static int m_loaded, visit_counted;
static void vex_path(char *p, int n) { snprintf(p, n, "%s/vex.txt", base_dir); }
static void vex_load(void)
{
	if (m_loaded) return;
	m_loaded = 1;
	char p[128]; vex_path(p, sizeof p);
	FILE *f = fopen(p, "r"); if (!f) return;
	char line[64];
	while (fgets(line, sizeof line, f)) {
		char *e = strchr(line, '='); if (!e) continue; *e = 0; int v = atoi(e + 1);
		if (!strcmp(line, "visits")) M.visits = v; else if (!strcmp(line, "told")) M.told = v; else if (!strcmp(line, "net")) M.net = v;
		else if (!strcmp(line, "beat_him")) M.beat_him = v; else if (!strcmp(line, "beat_you")) M.beat_you = v;
	}
	fclose(f);
}
static void vex_save(void)
{
	char p[128]; vex_path(p, sizeof p);
	FILE *f = fopen(p, "w"); if (!f) return;
	fprintf(f, "visits=%d\ntold=%d\nnet=%d\nbeat_him=%d\nbeat_you=%d\n", M.visits, M.told, M.net, M.beat_him, M.beat_you);
	fclose(f);
}

static const char *q_who = "Phantom Moth";   /* who is talking: Phantom Moth, or the dealer at this game */
static int q_speed, q_widx;
static int talk_mood(int w);
static u32 shade(u32 c, int d);
static void quip(const char *t) { snprintf(q_text, sizeof q_text, "%s", t); q_at = plat_ms(); q_who = "Phantom Moth"; q_speed = 30 * tspd_k() / 100; q_widx = 0; chatlog_add(0, t); }
static const char *nm(void) { if (S.player) return "Player 2"; return D.nick[0] ? D.nick : D.mii_name[0] ? D.mii_name : "friend"; }
static const char *pick(const char *const *l, int n) { return l[rnd() % n]; }
static void add_read(int topic, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void add_read(int topic, const char *fmt, ...)
{
	if (n_reads >= 16) return;
	if (M.told & (1 << topic) && topic != T_RUMBLE && topic != T_DONE) return;   /* already told you: he'll bring it up differently */
	read_topic[n_reads] = topic;
	va_list ap; va_start(ap, fmt); vsnprintf(reads[n_reads++], 220, fmt, ap); va_end(ap);
}
/* leaving: forget the details (they're re-read next time), keep only what's in vex.txt */
static int phase, hints_used;
static void dealer_forget(void) { phase = 0; hints_used = 0; memset(&D, 0, sizeof D); memset(reads, 0, sizeof reads); n_reads = 0; dd_loaded = 0; mind_step = 0; sess_hands = 0; pw_streak = dw_streak = anger = 0; visit_counted = 0; }
static void dealer_read(void)
{
	if (dd_loaded) return;
	dd_loaded = 1;
	plat_dossier(&D);
	/* names from the Wii are only ever shown as text: strip the characters the line builder uses */
	{
		char *F[6] = { D.nick, D.region, D.mii_name, D.fit_name, D.ssid, D.last_played };
		for (int i = 0; i < 6; i++) for (char *c = F[i]; *c; c++) if (strchr("[]{}|%", *c)) *c = ' ';
		for (int k = 0; k < 8; k++) { for (char *c = D.save_names[k]; *c; c++) if (strchr("[]{}|%", *c)) *c = ' '; for (char *c = D.backup_names[k]; *c; c++) if (strchr("[]{}|%", *c)) *c = ' '; }
	}
	static const char *MON[13] = { "", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
	const char *N = nm();
	int found = D.nick[0] || D.miis || D.wii_saves || D.fit_found || D.wii_backups || D.gc_backups || D.roms || D.last_played[0] || D.gc_saves;
	dd_empty = !found;
	if (!found) {
		/* he reaches for you and finds nothing. He does not like that */
		add_read(T_NAME, "Your name... it isn't there. Your Wii doesn't know what to call you. ...Neither do I.");
		add_read(T_MII, "Not one Mii. Not even your own face. What are you hiding?");
		add_read(T_GAME, "I reach into your saves and... nothing. Empty. Have you never played a single game? Impossible.");
		if (!(M.told & (1 << T_RUMBLE))) add_read(T_RUMBLE, "Put your Wii Remote down on the table. I will now move it... with my mind.");
		add_read(T_BACKUP, "Your SD card is clean. Too clean. Nobody is this clean.");
		add_read(T_CROWD, "No save files. No Miis. No name. Either something is blocking me... or something is very wrong with you.");
		add_read(T_DONE, "...Are you even real? Why can't I READ you?!");
		return;
	}
	if (D.nick[0]) add_read(T_NAME, "[Your Wii whispered your name to me|I heard your name in the static|Your Wii told me who you are]... %s. [It tells me so many things.|It tells me everything.|We talk, your Wii and I.]", N);
	else add_read(T_NAME, "Your Wii has no name. How strange. I will call you %s.", N);
	if (D.mii_found) {
		const char *tall = D.mii_height > 95 ? "tall" : D.mii_height < 35 ? "short" : "of average height";
		if (D.mii_bmonth >= 1 && D.mii_bmonth <= 12 && D.mii_bday)
			add_read(T_MII, "[Your Mii tells me more.|I see your Mii.|Your little Mii is so honest.] A %s. %s. Born on the %d of %s. [Am I wrong|Tell me I'm wrong|Is that you], %s?", D.mii_female ? "woman" : "man", tall, D.mii_bday, MON[D.mii_bmonth], N);
		else add_read(T_MII, "Your Mii tells me more. A %s. %s. Am I wrong, %s?", D.mii_female ? "woman" : "man", tall, N);
	} else add_read(T_MII, "I look for your Mii and... nothing. Not even a face. Strange. Very strange, %s.", N);
	if (D.fit_found && D.fit_age) add_read(T_FIT, "[Wii Fit remembers you|Wii Fit told me about you], %s. %d years old. %d cm tall. [It weighed you, too. I won't say.|It knows your weight too. Relax. I won't say it.]", D.fit_name[0] ? D.fit_name : N, D.fit_age, D.fit_height_cm);
	else if (D.fit_found) add_read(T_FIT, "Wii Fit remembers you, %s. %d cm tall. It weighed you, too. I won't say.", N, D.fit_height_cm);
	if (D.nsave_named) add_read(T_GAME, "[You play|You love] %s, don't you, %s? [I can see it in your save files.|Your save files are very chatty.]", D.save_names[rnd() % D.nsave_named], N);
	else if (D.wii_saves) add_read(T_GAME, "%d games have left their mark in your Wii's memory, %s. I can feel them.", D.wii_saves, N);
	else add_read(T_GAME, "I reach into your saves... empty? No. That can't be right. Either I can't read you, or you've wiped yourself clean, %s.", N);
	if (!(M.told & (1 << T_RUMBLE))) add_read(T_RUMBLE, "%s. Put your Wii Remote down on the table. I will now move it... with my mind.", N);
	if (D.nsave_named >= 2) add_read(T_TWO, "%s... and %s. %d save files in all. So many unfinished journeys, %s.", D.save_names[0], D.save_names[1], D.wii_saves, N);
	int gc = D.gc_backups + D.gc_saves, wii = D.wii_saves + D.wii_backups;
	if (gc > wii && gc) add_read(T_GCWII, "You prefer the GameCube, %s. A little purple box beats where your heart should be.", N);
	else if (gc) add_read(T_GCWII, "Wii games. GameCube games. You cannot choose, can you, %s?", N);
	if (D.last_played[0] && D.last_minutes > 0)
		add_read(T_PLAYTIME, "[Last time, you spent|Before this, you spent] %d minute%s in %s. [I was watching.|Don't ask how I know.] [I am always watching.|]", D.last_minutes, D.last_minutes == 1 ? "" : "s", D.last_played);
	else if (D.last_played[0]) add_read(T_PLAYTIME, "The last thing you played was %s. I was watching.", D.last_played);
	if (D.miis >= 6 || D.fit_profiles >= 3) add_read(T_CROWD, "%d Miis live on this Wii. A crowded house. Do the others know you're here, %s?", D.miis, N);
	else if (D.miis >= 1 && D.miis <= 2) add_read(T_CROWD, "Only %d Mii on this Wii. You play alone, don't you, %s? ...So do I.", D.miis, N);
	if (D.online && D.ssid[0]) add_read(T_ONLINE, "Your Wii reaches the outside world through \"%s\". Don't worry. I prefer it in here.", D.ssid);
	else if (D.online) add_read(T_ONLINE, "Your Wii has been online, %s. It remembers the way out. I don't.", N);
	else add_read(T_ONLINE, "Your Wii has never been online. Nobody knows you're here, %s. Only me.", N);
	int bk = D.wii_backups + D.gc_backups;
	if (bk > 1 && D.nbackup) add_read(T_BACKUP, "%s... %d games sit on your SD card. %s among them. You dumped every one from your own discs. Surely.", N, bk, D.backup_names[rnd() % D.nbackup]);
	else if (bk == 1 && D.nbackup) add_read(T_BACKUP, "I see %s on your SD card, %s. I won't tell Nintendo. This time.", D.backup_names[0], N);
	else add_read(T_BACKUP, "Your SD card holds no game backups. How... honest of you, %s.", N);
	if (D.cios) add_read(T_CIOS, "cIOS 249 lives on this Wii, %s. We both know what that's for.", N);
	if (D.roms) add_read(T_ROMS, "%d old cartridges live on your SD card, %s. Nostalgia is a heavy thing to carry.", D.roms, N);
	if (D.region[0]) add_read(T_REGION, "%s %s Wii. %s", strchr("AEIOU", D.region[0]) ? "An" : "A", D.region, strcmp(D.region, "Japanese") ? "You are a long way from Kyoto." : "Home, sweet home.");
	if (n_reads && !(M.told & (1 << T_DONE))) add_read(T_DONE, "I have seen everything I need to see, %s. ...Deal.", N);
}

/* something he told you on an earlier visit, brought up again */
static int callback_line(char *t, int n)
{
	int opts[T_COUNT], no = 0;
	for (int k = 0; k < T_COUNT; k++) if ((M.told >> k) & 1) opts[no++] = k;
	if (!no) return 0;
	const char *N = nm();
	switch (opts[rnd() % no]) {
	case T_NAME: snprintf(t, n, "%s. I never forgot your name. I never forget anything.", N); return 1;
	case T_MII: snprintf(t, n, "Your Mii still looks like you, %s. Or do you look like it?", N); return 1;
	case T_FIT: if (D.fit_height_cm) { snprintf(t, n, "Still %d cm tall, %s? You look smaller tonight.", D.fit_height_cm, N); return 1; } break;
	case T_GAME: if (D.nsave_named) { snprintf(t, n, "Still playing %s, %s? I told you I could see it.", D.save_names[rnd() % D.nsave_named], N); return 1; } break;
	case T_RUMBLE: snprintf(t, n, "Did your Wii Remote ever stop shaking, %s? Mine didn't.", N); return 1;
	case T_PLAYTIME: if (D.last_played[0]) { snprintf(t, n, "Back from %s so soon, %s? I noticed.", D.last_played, N); return 1; } break;
	case T_CROWD:
		if (D.miis >= 6) snprintf(t, n, "Did you tell the other %d Miis about me, %s?", D.miis - 1, N);
		else snprintf(t, n, "Still alone, %s? Good. So am I.", N);
		return 1;
	case T_ONLINE: snprintf(t, n, "I told you. Nobody out there knows you're in here, %s.", N); return 1;
	case T_BACKUP: { int bk = D.wii_backups + D.gc_backups; if (bk) { snprintf(t, n, "Your SD card still holds %d 'backups', %s. I counted again.", bk, N); return 1; } break; }
	case T_CIOS: snprintf(t, n, "cIOS 249 is still there, %s. I checked.", N); return 1;
	default: break;
	}
	return 0;
}

static void quip_hello(void)
{
	if (!S.quips) return;
	if (S.quips == 1) { quip("Welcome to the casino."); return; }
	vex_load();
	char t[220];
	if (!visit_counted) { visit_counted = 1; M.visits++; vex_save(); }
	if (M.visits > 1 && M.told) {
		dealer_read();
		const char *N = nm();
		if (M.net > 300) snprintf(t, sizeof t, "You came back, %s. Last time you took %d chips from me. I have not forgotten. I will NEVER forget.", N, M.net);
		else if (M.net < -300) snprintf(t, sizeof t, "Welcome back, %s. Come to lose another %d chips? Please. Sit.", N, -M.net);
		else if (!callback_line(t, sizeof t)) snprintf(t, sizeof t, "Welcome back, %s. Visit number %d. I remember everything we said.", N, M.visits);
		quip(t);
		creep_at = plat_ms();
		return;
	}
	static const char *const L[] = { "Welcome to my casino. Every game in here is mine. Pick one.", "Ah. A new face. Or have we met before?",
		"Come in. The house is patient. The house is always patient.", "Evening. I'll be your host tonight. Every night, really." };
	quip(pick(L, 4));
}

#if 0   /* old fixed lines, replaced by talk.inc */
static int mood_line(char *t, int n)
{
	const char *N = nm();
	if (pw_streak >= 5) {
		static const char *const F[] = { "ENOUGH, %s! These cards are MINE! Where are you hiding them?!", "HOW?! I read your MIND, %s! This is not POSSIBLE!",
			"Stop. Winning. %s. I am warning you.", "The shoe is cursed. YOU cursed it, %s!" };
		snprintf(t, n, pick(F, 4), N); anger = 2; plat_rumble(300); return 1;
	}
	if (pw_streak >= 3) {
		static const char *const A[] = { "Again, %s? ...Again?", "Luck. That is all it is, %s. Just luck.", "Hm. You are starting to annoy me, %s.",
			"Keep smiling, %s. It won't last." };
		snprintf(t, n, pick(A, 4), N); anger = 1; return 1;
	}
	if (dw_streak >= 5) {
		static const char *const G[] = { "Are you even trying, %s? My deck plays better than you.", "Please, %s. Keep going. I love this part.",
			"%d in a row. Should I let you win one? No.", "Your chips feel so much better in my tray, %s." };
		const char *f = pick(G, 4);
		if (strstr(f, "%d")) snprintf(t, n, f, dw_streak); else snprintf(t, n, f, N);
		return 1;
	}
	if (dw_streak >= 3) {
		static const char *const G[] = { "Ha. As I foresaw, %s.", "You never learn, do you, %s?", "Another one for me. Thank you, %s.",
			"I can hear your heart sinking, %s." };
		snprintf(t, n, pick(G, 4), N); return 1;
	}
	return 0;
}

#endif

/* ================================================================== */
/* how you play: every dealer notices your bets, your time, your chips */
/* ================================================================== */
static int persona = -1;          /* who is dealing at this game (-1 = Phantom Moth) */
static int sec;                   /* 0 = Main Floor, 1 = High Roller Suite */
static int floor_room;            /* which area the floor shows: 0 main, 1 Suite, 2 the Back Room */
enum { SY_BIG, SY_TINY, SY_CHASE, SY_RICH, SY_BROKE, SY_TIME, SY_N };
static u64 sess_t0;
static int st_last_stake, st_last_res, st_chase, st_cool[SY_N], st_time_said, st_pending = -1, st_minutes;
/* [0] = Phantom Moth, then Dusty, Corvus, Mimic, Lupa, Talon. SY_TIME lines take the minutes first, then your name */
static const char *const STYLE[6][SY_N] = {
	{ "All of that on one hand, %s? I felt your pulse jump from over here.", "Such tiny bets, %s. Afraid of me?", "Lost, so you bet MORE. I know that feeling. I feed on it.",
	  "So many chips, %s. They make a lovely sound when they fall.", "Almost nothing left, %s. How does it feel? No. Don't tell me. I already know.", "%d minutes at my tables, %s. Nobody leaves early. Nobody leaves." },
	{ "Whoa, big bet, %s. Heart's poundin', huh?", "Little bets, partner. Wheel don't even notice those.", "Chasin' your losses, %s? Seen a lot of folks try that. Seen a lot of folks walk home.",
	  "Sittin' on a pile there, %s. Better guard it.", "Runnin' on fumes, %s. Last few chips?", "%d minutes at the wheel, %s. You're a regular now." },
	{ "A big bet, %s. The machine noticed.", "Small coins, %s. The machine barely wakes up for those.", "You lost, and you bet more. The machine loves this part.",
	  "Your pile is tall, %s. Machines like tall piles.", "So few chips left, %s. Pull gently.", "%d minutes, %s. The machine has been watching the whole time." },
	{ "Whoa, big spender! Is that really you, %s? Doesn't sound like you.", "Tiny bets! Let me do my 'excited' voice. ...There.", "Lost, then bet bigger? That's what I'd do if I were you. And I could be.",
	  "Look at all those chips! Can I do your voice when I cash them in?", "Nearly broke, %s. I could do an impression of a bank. Want a loan? No.", "%d minutes, %s! I've done eleven impressions since you sat down." },
	{ "Everything on one hand, %s. Bold.", "Small bets. Careful. Fine.", "You lost, so you bet bigger. That's how people end up broke.",
	  "Plenty of chips, %s. Don't waste them.", "Last few chips, %s. Make them count.", "%d minutes and you still haven't blinked, %s." },
	{ "Now THAT is a Suite bet, %s. Maybe you belong up here.", "Tiny bets? In MY Suite? Go back downstairs, %s.", "Chasing losses. That's how kings fall, %s. I would know.",
	  "Rich, are we? Not rich enough to beat me.", "The Suite is no place for someone that broke, %s.", "%d minutes up here, %s. Still not a king." },
};
/* note a finished round; the comment is saved for the dealer to say */
static void style_note(int stake, int res)
{
	u64 now = plat_ms();
	if (!sess_t0) sess_t0 = now;
	for (int k = 0; k < SY_N; k++) if (st_cool[k] > 0) st_cool[k]--;
	int before = cfg.chips + (res >= 0 ? 0 : stake);   /* roughly what you had when you bet */
	if (before < 1) before = 1;
	if (stake > st_last_stake && st_last_res < 0 && st_last_stake) st_chase++; else if (res >= 0) st_chase = 0;
	st_pending = -1;
	int mins = (int)((now - sess_t0) / 60000);
	static const int TM[4] = { 10, 30, 60, 120 };
	for (int i = 0; i < 4; i++) if (mins >= TM[i] && !(st_time_said & (1 << i))) { st_time_said |= 1 << i; st_pending = SY_TIME; st_minutes = mins; }
	if (st_pending < 0) {
		if (cfg.chips < 100 && !st_cool[SY_BROKE]) st_pending = SY_BROKE;
		else if (stake * 10 >= before * 4 && stake >= 50 && !st_cool[SY_BIG]) st_pending = SY_BIG;
		else if (st_chase >= 2 && !st_cool[SY_CHASE]) st_pending = SY_CHASE;
		else if (cfg.chips > 10000 && !st_cool[SY_RICH]) st_pending = SY_RICH;
		else if (stake * 50 <= before && cfg.chips > 1000 && !st_cool[SY_TINY]) st_pending = SY_TINY;
	}
	if (st_pending >= 0) st_cool[st_pending] = st_pending == SY_RICH ? 12 : 6;
	st_last_stake = stake; st_last_res = res;
}
/* the dealer's comment on how you play, if there is one waiting */
static int style_line(char *t, int n)
{
	if (st_pending < 0) return 0;
	int who = persona + 1, k = st_pending;
	st_pending = -1;
	if (k == SY_TIME) snprintf(t, n, STYLE[who][k], st_minutes, nm());
	else snprintf(t, n, STYLE[who][k], nm());
	return 1;
}

#if 0   /* old fixed lines, replaced by talk.inc */
/* offhand hints: he lets slip that he knows things, then acts like he didn't */
static int hint_line(char *t, int n, int *used)
{
	const char *N = nm();
	int opts[10], no = 0;
	if (!(*used & 1)) opts[no++] = 0;
	if (!(*used & 2) && D.wii_saves) opts[no++] = 1;
	if (!(*used & 4) && (D.wii_backups + D.gc_backups)) opts[no++] = 2;
	if (!(*used & 8) && D.miis) opts[no++] = 3;
	if (!(*used & 16) && D.online) opts[no++] = 4;
	if (!(*used & 32) && D.fit_found) opts[no++] = 5;
	if (!(*used & 64) && D.last_played[0]) opts[no++] = 6;
	if (!no) return 0;
	int k = opts[rnd() % no];
	*used |= 1 << k;
	switch (k) {
	case 0: snprintf(t, n, "Your turn, %s. ...What? Did you not tell me your name?", N); break;
	case 1: snprintf(t, n, "Lot of hours on that Wii of yours. ...Or so I imagine."); break;
	case 2: snprintf(t, n, "Your SD card feels heavy tonight. Never mind. Deal."); break;
	case 3: snprintf(t, n, D.miis > 2 ? "Who else plays on this Wii? ...Forget I asked." : "Quiet in your house, isn't it? Just you. ...Just a guess."); break;
	case 4: snprintf(t, n, "Is that your Wi-Fi humming? No? My mistake."); break;
	case 5: snprintf(t, n, "You've been exercising. ...Your posture. That's all. Just your posture."); break;
	default: snprintf(t, n, "You were playing something before this. I can still smell it on you."); break;
	}
	return 1;
}

/* after each hand. The pacing:
   1. small talk for the first hands
   2. offhand hints now and then: he knows things, then acts like he doesn't
   3. MASK OFF: a winning or losing streak, or just when he feels like it, and he reads you, one thing per hand
   4. after that he gloats, rages and brings it all back up, even on later visits */
static int quiet_line(int res)
{
	/* Quiet: just a word about the result */
	static const char *const W[] = { "Well played.", "You win.", "Enjoy it." };
	static const char *const Lo[] = { "The house wins.", "Unlucky.", "Better luck next time." };
	static const char *const P[] = { "A push.", "Even." };
	quip(res > 0 ? pick(W, 3) : res < 0 ? pick(Lo, 3) : pick(P, 2));
	return 1;
}
static void quip_after(int res, int bust, int pv, int net)
{
	vex_load();
	if (res > 0) { pw_streak++; dw_streak = 0; M.beat_him++; }
	else if (res < 0) { dw_streak++; pw_streak = 0; M.beat_you++; }
	M.net += net;
	if (pw_streak < 3) anger = 0;
	vex_save();
	if (!S.quips) return;
	if (S.quips == 1) { quiet_line(res); return; }
	sess_hands++;
	char t[220];
	int returning = M.visits > 1 && M.told;
	const char *N;
	if (sess_hands < 3) {
		static const char *const SM[] = { "Busy night. Mostly ghosts.", "This table has seen fortunes come and go. Mostly go.",
			"Relax. It is only chips. For now.", "Do you come here often? I feel like you do.", "The cards are warm tonight.",
			"Nice weather in here. It is always nice weather in here." };
		static const char *const W[] = { "Well played. Beginner's luck, perhaps.", "Enjoy it." };
		static const char *const Lo[] = { "Unlucky. It happens.", "The cards can be cruel." };
		quip(res > 0 ? pick(W, 2) : res < 0 && (rnd() & 1) ? pick(Lo, 2) : pick(SM, 6));
		return;
	}
	dealer_read();
	N = nm();
	if (phase == 2) {
		/* mask off: one thing per hand until he has said it all */
		if (mind_step < n_reads) {
			int step = mind_step++;
			if (read_topic[step] == T_RUMBLE) { rum_at = plat_ms() + 4500; rum_left = dd_empty ? 1 : 6; }
			M.told |= 1 << read_topic[step]; vex_save();
			quip(reads[step]);
			creep_at = plat_ms();
			plat_tone(55);          /* a low hum while he speaks */
			return;
		}
		phase = 3;
	}
	if (phase < 2 && mind_step < n_reads) {
		/* what sets him off: a streak either way, or nothing at all */
		int streak = pw_streak >= 3 || dw_streak >= 3;
		int trigger = (sess_hands >= 7 && streak && (rnd() % 10) < 5) || (sess_hands >= 9 && (rnd() % 100) < 8) || sess_hands >= 22;
		if (trigger) {
			phase = 2;
			static const char *const OFF[] = { "Enough small talk, %s. Let me show you what I really see.",
				"You think this is about cards, %s? Put your hands where I can see them. I am going to read you.",
				"Stop. Look at me, %s. ...I know you. I know EVERYTHING about you.",
				"The mask comes off now, %s. Let's see what your Wii has been telling me." };
			snprintf(t, sizeof t, pick(OFF, 4), N);
			quip(t);
			creep_at = plat_ms();
			plat_tone(55);
			plat_rumble(200);
			return;
		}
	}
	if (style_line(t, sizeof t)) { quip(t); return; }
	if (mood_line(t, sizeof t) && (rnd() % 10) < 7) { quip(t); return; }
	if (phase < 2 && sess_hands >= 3 && (rnd() % 10) < 4 && hint_line(t, sizeof t, &hints_used)) { quip(t); return; }
	if ((phase >= 3 || returning) && (rnd() % 4) == 0 && callback_line(t, sizeof t)) { quip(t); creep_at = plat_ms(); return; }
	if (bust && pv > 0 && phase >= 2) { snprintf(t, sizeof t, "You hit on %d. I knew you would, %s. I always know.", pv, N); quip(t); return; }
	static const char *const W[] = { "Hm. Luck is a fickle friend.", "Enjoy it. The house always remembers.", "You win. As I allowed.", "Fine. Take it." };
	static const char *const Lo[] = { "As I foresaw.", "Your chips were never really yours.", "I saw that card coming before the shoe did.", "Again? You never learn." };
	static const char *const P[] = { "A draw. How dull.", "Neither of us wins. Neither of us leaves." };
	quip(res > 0 ? pick(W, 4) : res < 0 ? pick(Lo, 4) : pick(P, 2));
}

#endif

/* wrap text into lines that fit */
static int wrap_lines(const char *t, int maxw, char out[][64], int maxl)
{
	int n = 0; char line[64] = ""; const char *p = t;
	while (*p && n < maxl) {
		char word[64]; int k = 0;
		while (*p == ' ') p++;
		while (*p && *p != ' ' && k < 62) word[k++] = *p++;
		word[k] = 0; if (!k) break;
		char test[128]; snprintf(test, sizeof test, "%s%s%s", line, line[0] ? " " : "", word);
		if (text_w(bub_font(), test) > maxw && line[0]) { snprintf(out[n++], 64, "%s", line); snprintf(line, sizeof line, "%s", word); }
		else snprintf(line, sizeof line, "%s", test);
	}
	if (line[0] && n < maxl) snprintf(out[n++], 64, "%s", line);
	return n;
}

/* talking speed: each dealer has their own pace, with real pauses at commas and full stops */

static int char_ms(const char *t, int i)
{
	char c = t[i]; int d = q_speed;
	if (!q_speed) return 0;
	if (c == '.' || c == '!' || c == '?') d += t[i + 1] == '.' ? 70 : t[i + 1] ? 260 : 0;
	else if (c == ',') d += 120;
	return d;
}
static u64 type_ms(const char *t) { u64 s = 0; for (int i = 0; t[i]; i++) s += char_ms(t, i); return s; }
static int typed_chars(const char *t, u64 el) { u64 s = 0; int i; for (i = 0; t[i]; i++) { s += char_ms(t, i); if (s > el) break; } return i; }

/* is Phantom Moth in the middle of reading you? */
static int creeping(void)
{
	if (!creep_at || !q_text[0]) return 0;
	u64 end = q_at + type_ms(q_text) + 3500;
	return plat_ms() < end;
}
static void creep_shade(void)
{
	u64 now = plat_ms();
	if (!S.quips || !creep_at) return;
	u64 end = q_at + type_ms(q_text) + 3500;
	int a;
	if (now < creep_at + 700) a = (int)((now - creep_at) * 150 / 700);
	else if (now < end) a = 150;
	else if (now < end + 800) a = 150 - (int)((now - end) * 150 / 800);
	else { a = 0; creep_at = 0; plat_tone(0); }
	if (now >= end) plat_tone(0);
	if (a > 0) gfx_rect(0, 0, SCR_W, SCR_H, RGB(4, 0, 8), a);
}

static void quip_draw(void)
{
	if (!S.quips || !q_text[0]) return;
	u64 now = plat_ms();
	int len = strlen(q_text);
	int shown = now > q_at ? typed_chars(q_text, now - q_at) : 0;
	if (now > q_at + type_ms(q_text) + 9000) return;
	/* a little voice: a blip for every other letter, each dealer at their own pitch */
	{
		static u64 blip_at; static int blip_n;
		if (blip_at != q_at) { blip_at = q_at; blip_n = 0; }
		for (; blip_n < shown && blip_n < len; blip_n++) {
			char c = q_text[blip_n];
			if (((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) && !(blip_n & 1)) music_voice(q_widx, c, creeping());
		}
	}
	if (shown > len) shown = len;
	char part[220]; snprintf(part, sizeof part, "%.*s", shown, q_text);
	int x = 26, y = 82, w = 218, lh = bub_lh();
	char Lf[7][64]; int nf = wrap_lines(q_text, w - 18, Lf, 7);
	int h = 30 + nf * lh; if (h < 118) h = 118;
	/* the bubble fades up as they start talking */
	int age = (int)(now - q_at), fa = age < 160 ? age * 255 / 160 : 255;
	if (age < 160) y += (160 - age) / 40;
	int md = talk_mood(q_widx), cr = creeping();
	if (md == -2) { x += (int)(rnd() % 5) - 2; y += (int)(rnd() % 3) - 1; }   /* shaking with rage */
	/* each dealer's colour: Moth violet, Lynx amber, Crow slate, Squid teal, Vixen ice, Viper crimson */
	static const u32 ACC[6] = { RGB(170, 90, 220), RGB(230, 170, 70), RGB(120, 140, 170), RGB(60, 200, 190), RGB(170, 220, 250), RGB(220, 60, 70) };
	u32 acc = md == -2 ? RGB(255, 40, 40) : md == -1 ? RGB(230, 120, 60) : ACC[q_widx % 6];
	gfx_round(x + 2, y + 4, w, h, 14, RGB(0, 0, 0), 90 * fa / 255);
	gfx_round(x, y, w, h, 14, md == -2 ? RGB(40, 6, 10) : RGB(16, 10, 26), 215 * fa / 255);
	gfx_round_line(x, y, w, h, 14, md <= -1 ? 2 : 1, acc, 220 * fa / 255);
	gfx_rect(x + 10, y + 23, 60, 1, acc, 120 * fa / 255);
	char who[48];
	static const char *const MW[5] = { "  (furious)", "  (annoyed)", "", "", "  (smug)" };
	snprintf(who, sizeof who, "%s%s", q_who, cr ? "  (reading you)" : MW[md + 2]);
	text(F_SMALL, x + 10, y + 5, who, cr || md < 0 ? RGB(255, 110, 140) : shade(acc, 40), fa);
	char L[7][64]; int n = wrap_lines(part, w - 18, L, 7);
	/* while reading you, the words flicker: a red ghost and a shiver now and then */
	int glitch = (cr && ((now / 90) % 17 == 0 || (now / 70) % 23 == 0)) || anger == 2;
	for (int i = 0; i < n; i++) {
		int jx = glitch ? (int)(rnd() % 5) - 2 : 0;
		if (cr) text(bub_font(), x + 11 + jx, y + 26 + i * lh, L[i], RGB(255, 30, 70), glitch ? 200 : 70);
		text(bub_font(), x + 9 + jx, y + 26 + i * lh, L[i], RGB(240, 232, 250), 255);
	}
}

#if 0
/* he talks to you between hands too */
static void idle_talk(void)
{
	if (S.quips < 2) return;
	u64 now = plat_ms();
	static u64 next;
	if (!next || now < q_at) next = now + 14000;
	if (now < next || now < q_at + (u64)strlen(q_text) * 28 + 9000) return;
	next = now + 16000 + rnd() % 12000;
	char t[220];
	if (phase >= 3 && (rnd() & 1) && callback_line(t, sizeof t)) { quip(t); return; }
	if (phase < 2 && sess_hands >= 3 && (rnd() & 1)) { dealer_read(); if (hint_line(t, sizeof t, &hints_used)) { quip(t); return; } }
	static const char *const IDLE[] = { "Take your time. I have all night. I have every night.", "Nervous? Your hands say yes.",
		"The shoe is getting lighter. Can you feel it?", "Place a bet. Any bet. I'm curious what you'll do.",
		"I like this table. Nobody ever leaves it happy.", "Do you hear that? No? Good.", "Chips don't count themselves. Well. Mine do.",
		"You're thinking about leaving. Don't." };
	quip(pick(IDLE, 8));
}

#endif

/* the mind-reading trick: the Wii Remote shakes on its own */
static void quip_tick(void)
{
	talk_pump();
	if (!rum_left) return;
	u64 now = plat_ms();
	if (now >= rum_at) {
		plat_rumble(dd_empty ? 40 : 220);
		rum_left--; rum_at = now + 380;
		if (!rum_left) {
			char t[160];
			if (dd_empty) snprintf(t, sizeof t, "...Why isn't it MOVING? What are you? What have you DONE to your Wii?");
			else snprintf(t, sizeof t, "You felt that, didn't you, %s? Now... back to the cards.", nm());
			quip(t);
		}
	}
}

/* leaving the casino: stop the hum and forget everything he read */
/* free play: swap your real chips out for practice ones, and back */
#define FREE_CHIPS 50000
static void free_on(void) { if (S.free_now) return; S.real_chips = cfg.chips; cfg.chips = FREE_CHIPS; S.free_now = 1; stats_save(); config_save(); }
static void free_off(void) { if (!S.free_now) return; cfg.chips = S.real_chips; S.free_now = 0; stats_save(); config_save(); }
void casino_leave(void) { free_off(); music_play(-1, 0, 0); plat_music(0); plat_tone(0); creep_at = 0; rum_left = 0; q_text[0] = 0; dealer_forget(); talk_reset(); }

/* ================================================================== */
/* the session: time, chips in and out, goals, the summary            */
/* ================================================================== */
static int sess_start_chips, sess_rounds, sess_best, goal_open, goal_done, d_amt;
static u64 d_until;
static int cas_busy(void);
static void open_game(int gm);
static void sit(int t);
/* after every round of every game */
static void win_fx(int net, int stake);
static void gstat(int net);
static void track_round(int net)
{
	sess_rounds++;
	win_fx(net, st_last_stake);
	if (net < 0) music_sfx(SFX_LOSE, 250);
	if (net > sess_best) sess_best = net;
	if (net) { d_amt = net; d_until = plat_ms() + 1800; }
	/* chip history (for the graph) */
	if (S.nhist == 40) { memmove(S.hist, S.hist + 1, 39 * sizeof(int)); S.nhist = 39; }
	S.hist[S.nhist++] = cfg.chips;
	/* how the dealer at this game feels about you */
	int who = persona + 1;
	if (who >= 0 && who < 6) { S.rel[who] += net > 0 ? 1 : net < 0 ? -1 : 0; if (S.rel[who] > 99) S.rel[who] = 99; if (S.rel[who] < -99) S.rel[who] = -99; }
	/* stop-loss and win goal */
	if (!goal_done && sess_start_chips > 0) {
		static const int SLP[4] = { 0, 25, 50, 75 }, WGP[4] = { 0, 50, 100, 200 };
		if (S.stop_loss && cfg.chips * 100 <= sess_start_chips * (100 - SLP[S.stop_loss])) goal_open = 1;
		else if (S.win_goal && cfg.chips * 100 >= sess_start_chips * (100 + WGP[S.win_goal])) goal_open = 2;
		if (goal_open) {
			goal_done = 1; ui_set_focus(9000);
			if (S.quips) quip(goal_open == 1 ? "Your stop-loss. Quitting? Wise. Or cowardly. I can never tell." : "Your win goal. Leaving while you're ahead? ...I respect that. I hate it. But I respect it.");
		}
	}
}
/* the +50 / -25 that floats off your chips */
static void delta_draw(int x, int y)
{
	u64 now = plat_ms();
	if (now >= d_until || !d_amt) return;
	int left = (int)(d_until - now), a = left > 400 ? 255 : left * 255 / 400;
	char t[16]; snprintf(t, sizeof t, "%+d", d_amt);
	text_c(F_BODYB, x, y - (1800 - left) / 90, t, d_amt > 0 ? RGB(90, 240, 140) : RGB(255, 90, 90), a);
}

#include "talk.inc"

void casino_start(void)
{
	if (!loaded) stats_load();
	free_off();              /* in case the app closed during free play */
	if (S.free) free_on();
	rng ^= (u32)plat_ms() * 2654435761u;
	view = V_FLOOR; reset_arm = 0; opt_open = 0;
	sess_t0 = plat_ms(); st_time_said = 0; st_pending = -1; st_chase = 0; st_last_stake = 0;
	go(S_CASINO);
	ui_set_focus(8200);
	q_text[0] = 0;
	talk_sit(0);
	/* a gift once a day */
	{
		time_t t = time(NULL);
		int day = t > 1600000000 ? (int)(t / 86400) : 0;
		if (day && day != S.bonus_day) {
			S.bonus_day = day; cfg.chips += 500; config_save(); stats_save();
			ui_toast("Daily bonus: 500 chips");
			if (S.quips >= 2) quip("Here. 500 chips. A gift. Everything I give, I take back.");
		}
	}
	sess_start_chips = cfg.chips; sess_rounds = 0; sess_best = 0; goal_open = goal_done = 0;
	/* back to the game you were playing */
	if (S.last_game >= 0 && S.last_game < 6) {
		sec = S.last_sec && (cfg.chips >= 100) ? 1 : 0;
		if (S.last_game == 0) { if (S.last_tb >= 0 && S.last_tb < NT && cfg.chips >= TABLES[S.last_tb].min && (S.last_tb != 3 || sec)) { persona = -1; sit(S.last_tb); } }
		else if (!(S.last_game == 5 && !sec)) open_game(S.last_game);
	}
}

/* leaving: a summary first, if you played */
static void leave_casino(void)
{
	S.last_game = view == V_FLOOR ? -1 : S.last_game;
	stats_save();
	if (sess_rounds) { view = V_SUMMARY; ui_set_focus(8650); }
	else go_back();
}

static void sit(int t)
{
	S.last_game = 0; S.last_tb = t; S.last_sec = sec; stats_save();
	tb = t; view = V_TABLE; g = G_BET; nh = 0; dn = 0; msg[0] = 0;
	shoe_fill();
	const Table *T = &TABLES[tb];
	bet = (S.last_bet[tb] >= T->min && S.last_bet[tb] <= T->max) ? S.last_bet[tb] : T->min;
	if (bet > cfg.chips) bet = 0;
	ui_set_focus(8300);
}

/* ---- dealing ---- */
static void deal_to(Card *k, int *n, int counted)
{
	u64 now = plat_ms(); if (next_t < now) next_t = now;
	k[*n].c = take(counted); k[*n].t = next_t; (*n)++;
	music_sfx(SFX_CARD, (int)(next_t - now));
	next_t += speed_ms();
}
static void deal(void)
{
	const Table *T = &TABLES[tb];
	if (bet < T->min) { ui_toast("Tap the chips to make a bet first"); return; }
	if (bet > cfg.chips) { ui_toast("Not enough chips for that bet"); return; }
	if (shoe_pos >= cut) { shoe_fill(); ui_toast("New shoe: the cards were shuffled"); }
	S.last_bet[tb] = bet;
	int sw = T->variant == 3;
	if (sw && bet * 2 > cfg.chips) { ui_toast("Switch plays two hands: you need twice the bet"); return; }
	if (!big_ok(sw ? bet * 2 : bet, deal)) return;
	cfg.chips -= sw ? bet * 2 : bet;
	memset(ph, 0, sizeof ph); nh = sw ? 2 : 1; cur_h = 0; dn = 0; insurance = 0; hole_counted = 0; msg[0] = 0;
	ph[0].bet = bet; ph[1].bet = sw ? bet : 0; hole_at = 0;
	next_t = plat_ms() + 80;
	if (sw) {   /* Switch: two hands, then you may swap their top cards */
		deal_to(ph[0].k, &ph[0].n, 1); deal_to(ph[1].k, &ph[1].n, 1); deal_to(dh, &dn, 1);
		deal_to(ph[0].k, &ph[0].n, 1); deal_to(ph[1].k, &ph[1].n, 1); deal_to(dh, &dn, 0);
		bj_switch_ok = 1;
	} else {
		deal_to(ph[0].k, &ph[0].n, 1); deal_to(dh, &dn, 1); deal_to(ph[0].k, &ph[0].n, 1); deal_to(dh, &dn, 0);   /* hole card: counted when shown */
		bj_switch_ok = 0;
	}
	g = G_DEAL;
	plat_rumble(15);
}

/* the dealer turns the hole card and draws */
static void dealer_turn(void)
{
	int any_live = 0;
	{ u64 t = plat_ms(); if (next_t < t) next_t = t; hole_at = next_t; next_t += 320; music_sfx(SFX_FLIP, (int)(hole_at - t)); }   /* the hole card flips, then he draws */
	for (int i = 0; i < nh; i++) if (!ph[i].surrendered && value_of(ph[i].k, ph[i].n, NULL) <= 21) any_live = 1;
	if (any_live && !(nh == 1 && is_bj(&ph[0])) && !dealer_bj()) {
		for (;;) {
			int soft, v = value_of(dh, dn, &soft);
			if ((v < 17 || (v == 17 && soft && TABLES[tb].h17)) && dn < 13) deal_to(dh, &dn, 1);
			else break;
		}
	}
	if (!hole_counted) { running += hilo(dh[1].c); hole_counted = 1; }
	u64 now = plat_ms();
	settle_at = (next_t > now ? next_t : now) + 350;
	g = G_DEALER;
}

static void settle(void)
{
	const Table *T = &TABLES[tb];
	int dv = value_of(dh, dn, NULL), dbj = dealer_bj();
	int net = 0, wins = 0, losses = 0, pushes = 0;
	for (int i = 0; i < nh; i++) {
		Hand *h = &ph[i];
		int pv = value_of(h->k, h->n, NULL);
		int stake = h->bet * (h->doubled ? 2 : 1);
		int real = stake - h->free;          /* the house's free chips are never yours to lose */
		int var = T->variant, d22 = (var == 2 || var == 3) && dv == 22;
		int back;
		if (h->surrendered) { back = real / 2; h->res = -2; }
		else if (pv > 21) { back = 0; h->res = -1; }
		else if (is_bj(h) && (!dbj || var == 1)) { back = stake + stake * T->bj_num / T->bj_den; h->res = 2; }
		else if (is_bj(h) && dbj) { back = stake; h->res = 0; }
		else if (dbj) { back = 0; h->res = -1; }
		else if (d22) { back = real; h->res = 0; }   /* Free Bet and Switch: a dealer 22 is a push */
		else if (dv > 21 || pv > dv || (var == 1 && pv == 21)) {
			/* Spanish 21 bonuses: 5, 6, 7+ card 21, and 6-7-8 or 7-7-7 */
			int num = 1, den = 1;
			if (var == 1 && pv == 21 && !h->doubled) {
				if (h->n >= 7) num = 3; else if (h->n == 6) num = 2; else if (h->n == 5) { num = 3; den = 2; }
				else if (h->n == 3) {
					int r[3] = { rank10(h->k[0].c), rank10(h->k[1].c), rank10(h->k[2].c) };
					int s6 = (r[0] + r[1] + r[2] == 21) && (r[0] == 7 && r[1] == 7 && r[2] == 7 ? 1 : (r[0] != r[1] && r[1] != r[2] && r[0] != r[2] && r[0] >= 6 && r[0] <= 8 && r[1] >= 6 && r[1] <= 8 && r[2] >= 6 && r[2] <= 8));
					if (s6) { num = 3; den = 2; }
				}
			}
			back = real + stake * num / den; h->res = 1;
		}
		else if (pv == dv) { back = real; h->res = 0; }
		else { back = 0; h->res = -1; }
		h->win = back - real;
		cfg.chips += back; net += back - real;
		if (h->res > 0) { wins++; if (nh > 1) achieve(AC_SPLIT_WIN); if (h->doubled) achieve(AC_DOUBLE_WIN); if (h->n >= 5 && pv == 21) achieve(AC_5CARD); }
		else if (h->res == 0) pushes++; else losses++;
		if (h->res == 2) { S.bjs++; achieve(AC_BJ); }
	}
	if (insurance) { if (dbj) { cfg.chips += insurance * 3; net += insurance * 2; } else net -= insurance; }
	S.played += nh; S.won += wins; S.lost += losses; S.pushed += pushes;
	{ int stake = 0; for (int i = 0; i < nh; i++) stake += ph[i].bet * (ph[i].doubled ? 2 : 1); style_note(stake + insurance, net > 0 ? 1 : net < 0 ? -1 : 0); }
	track_round(net); gstat(net);
	if (net > S.best_win) S.best_win = net;
	if (wins && !losses) { S.streak++; if (S.streak > S.best_streak) S.best_streak = S.streak; } else if (losses) S.streak = 0;
	if (wins) achieve(AC_FIRST_WIN);
	if (S.streak >= 5) achieve(AC_STREAK5);
	if (cfg.chips > S.max_chips) S.max_chips = cfg.chips;
	if (cfg.chips >= 10000) achieve(AC_10K);
	if (tb == 3 && net > 0) achieve(AC_HIGHROLLER);
	char m[96];
	if (nh == 1) {
		Hand *h = &ph[0]; int pv = value_of(h->k, h->n, NULL);
		if (h->res == 2) snprintf(m, sizeof m, "Blackjack! You win %d", h->win);
		else if (h->res == -2) snprintf(m, sizeof m, "You gave up. Half your bet back");
		else if (pv > 21) snprintf(m, sizeof m, "Bust with %d. You lose %d", pv, -h->win);
		else if (dbj && h->res < 0) snprintf(m, sizeof m, "Dealer has blackjack. You lose %d", -h->win);
		else if (h->res == 1 && dv > 21) snprintf(m, sizeof m, "Dealer busts with %d. You win %d", dv, h->win);
		else if (h->res == 1) snprintf(m, sizeof m, "%d beats %d. You win %d", pv, dv, h->win);
		else if (h->res == 0) snprintf(m, sizeof m, "Push at %d. Bet back", pv);
		else snprintf(m, sizeof m, "Dealer %d beats %d. You lose %d", dv, pv, -h->win);
	} else snprintf(m, sizeof m, "%d won, %d lost, %d push  -  %s%d chips", wins, losses, pushes, net >= 0 ? "+" : "", net);
	if (insurance) { size_t l = strlen(m); snprintf(m + l, sizeof m - l, dbj ? "  (insurance paid)" : "  (insurance lost)"); }
	say(net > 0 ? 1 : net < 0 ? -1 : 0, m);
	{
		/* what the dealer saw */
		Hand *h = &ph[0];
		int pv = value_of(h->k, h->n, NULL);
		memset(&ev, 0, sizeof ev);
		ev.game = 0; ev.pv = pv; ev.dv = dv; ev.pb = value_of(h->k, h->n - 1, NULL); ev.up = rank10(dh[0].c);
		if (h->n) card_name(ev.card, sizeof ev.card, h->k[h->n - 1].c);
		u32 f = 0;
		if (h->res == 2) f |= F_BJ;
		if (pv > 21) f |= F_BUST;
		if (dbj) f |= F_DBJ;
		if (dv > 21 && h->res > 0) f |= F_DBUST;
		if (h->doubled) f |= F_DOUBLE;
		if (nh > 1) f |= F_SPLIT;
		if (h->surrendered) f |= F_SURR;
		if (pv <= 21 && dv <= 21 && abs(pv - dv) == 1 && !h->surrendered) f |= F_CLOSE;
		if (h->res == 0 && pv == dv) f |= F_TIE;
		ev.flags = f;
		int stake = 0; for (int i = 0; i < nh; i++) stake += ph[i].bet * (ph[i].doubled ? 2 : 1);
		talk_round(net > 0 ? 1 : net < 0 ? -1 : 0, net, stake);
	}
	if (net > 0) { if (!S.snd) plat_beep(1); plat_rumble(60); }
	stats_save(); config_save();
	g = G_DONE;
	ui_set_focus(8300);
}

/* after the first 4 cards are out: insurance, peek, or play */
static void after_deal(void)
{
	int up = dh[0].c % 13;
	if (up == 0 && cfg.chips >= ph[0].bet / 2 && ph[0].bet >= 2 && !is_bj(&ph[0])) { g = G_INSURE; ui_set_focus(8421); return; }
	if ((up == 0 || up >= 9) && dealer_bj()) { dealer_turn(); return; }       /* dealer peeks */
	if (is_bj(&ph[0])) { dealer_turn(); return; }
	g = G_PLAY; ui_set_focus(8400);
	talk_dealt();
}

static void next_hand(void)
{
	ph[cur_h].done = 1;
	while (cur_h < nh && ph[cur_h].done) cur_h++;
	if (cur_h >= nh) { cur_h = nh - 1; dealer_turn(); return; }
	Hand *h = &ph[cur_h];
	if (h->n < 2) deal_to(h->k, &h->n, 1);   /* a split hand gets its second card */
	if (h->split_aces || value_of(h->k, h->n, NULL) == 21) next_hand();
}

static void act_hit(void)
{
	talk_decide("Hit");
	bj_switch_ok = 0;
	Hand *h = &ph[cur_h];
	deal_to(h->k, &h->n, 1);
	if (value_of(h->k, h->n, NULL) >= 21 || h->n >= 11) next_hand();
}
static void act_stand(void) { talk_decide("Stand"); bj_switch_ok = 0; next_hand(); }
/* Free Bet: doubles on a hard 9, 10 or 11 are free, and so are splits (not tens) */
static int free_double(void) { Hand *h = &ph[cur_h]; int soft, v = value_of(h->k, h->n, &soft); return TABLES[tb].variant == 2 && h->n == 2 && !soft && v >= 9 && v <= 11; }
static int free_split(void) { Hand *h = &ph[cur_h]; return TABLES[tb].variant == 2 && rank10(h->k[0].c) != 10; }
static int can_double(void)
{
	Hand *h = &ph[cur_h];
	int cards_ok = TABLES[tb].variant == 1 ? h->n >= 2 && !h->doubled : h->n == 2;   /* Spanish 21: double on any number of cards */
	return cards_ok && !h->split_aces && (free_double() || cfg.chips >= h->bet) && (nh == 1 || TABLES[tb].das || TABLES[tb].variant == 3);
}
static void act_double(void)
{
	talk_decide("Double");
	Hand *h = &ph[cur_h];
	if (free_double()) h->free += h->bet; else cfg.chips -= h->bet;
	h->doubled = 1;
	deal_to(h->k, &h->n, 1);
	next_hand();
}
static int can_split(void) { Hand *h = &ph[cur_h]; return h->n == 2 && nh < 4 && rank10(h->k[0].c) == rank10(h->k[1].c) && (free_split() || cfg.chips >= h->bet); }
static void act_split(void)
{
	talk_decide("Split");
	for (int i = nh; i > cur_h + 1; i--) ph[i] = ph[i - 1];
	Hand *h = &ph[cur_h], *n2 = &ph[cur_h + 1];
	memset(n2, 0, sizeof *n2);
	n2->bet = h->bet;
	if (free_split()) n2->free = h->bet; else cfg.chips -= h->bet;
	n2->k[0] = h->k[1]; n2->k[0].t = plat_ms(); music_sfx(SFX_CARD, 0); n2->n = 1; h->n = 1; nh++;
	int aces = h->k[0].c % 13 == 0;
	h->split_aces = n2->split_aces = aces;
	deal_to(h->k, &h->n, 1);
	if (aces || value_of(h->k, h->n, NULL) == 21) next_hand();
}
static int can_surrender(void) { return TABLES[tb].surrender && nh == 1 && ph[0].n == 2 && ph[0].k[0].c < 255; }
static void act_surrender(void) { ph[0].surrendered = 1; ph[0].done = 1; cur_h = 0; dealer_turn(); }

/* ---- basic strategy hint ---- */
static const char *bj_hint(void)
{
	Hand *h = &ph[cur_h];
	int up = rank10(dh[0].c);   /* 2..11 */
	int soft, v = value_of(h->k, h->n, &soft);
	int dbl = can_double(), spl = can_split(), sur = can_surrender();
	if (spl) {
		int r = rank10(h->k[0].c);
		if (r == 11 || r == 8) return "Split";
		if (r == 9 && up != 7 && up <= 9) return "Split";
		if ((r == 7 || r == 2 || r == 3) && up <= 7) return "Split";
		if (r == 6 && up <= 6) return "Split";
		if (r == 4 && (up == 5 || up == 6)) return "Split";
	}
	if (sur && !soft && ((v == 16 && up >= 9) || (v == 15 && up == 10))) return "Give up";
	if (soft && v < 21) {
		if (v >= 19) return (v == 19 && up == 6 && dbl) ? "Double" : "Stand";
		if (v == 18) { if (up >= 3 && up <= 6) return dbl ? "Double" : "Stand"; return up <= 8 ? "Stand" : "Hit"; }
		if (v == 17) return (up >= 3 && up <= 6 && dbl) ? "Double" : "Hit";
		if (v >= 15) return (up >= 4 && up <= 6 && dbl) ? "Double" : "Hit";
		return (up >= 5 && up <= 6 && dbl) ? "Double" : "Hit";
	}
	if (v >= 17) return "Stand";
	if (v >= 13) return up <= 6 ? "Stand" : "Hit";
	if (v == 12) return (up >= 4 && up <= 6) ? "Stand" : "Hit";
	if (v == 11) return dbl ? "Double" : "Hit";
	if (v == 10) return (up <= 9 && dbl) ? "Double" : "Hit";
	if (v == 9) return (up >= 3 && up <= 6 && dbl) ? "Double" : "Hit";
	return "Hit";
}

/* ================================================================== */
/* drawing: cards in 6 styles                                         */
/* ================================================================== */
#define CW 64
#define CH 90
static void suit_shape(int s, int cx, int cy, int r, u32 c, int a)
{
	switch (s) {
	case 0:   /* hearts */
		gfx_circle(cx - r / 2, cy - r / 4, r / 2 + 1, c, a); gfx_circle(cx + r / 2, cy - r / 4, r / 2 + 1, c, a);
		gfx_tri(cx - r - 1, cy - r / 8, cx + r + 1, cy - r / 8, cx, cy + r, c, a);
		break;
	case 1:   /* diamonds */
		gfx_tri(cx, cy - r, cx - r * 3 / 4, cy, cx + r * 3 / 4, cy, c, a);
		gfx_tri(cx, cy + r, cx - r * 3 / 4, cy, cx + r * 3 / 4, cy, c, a);
		break;
	case 2:   /* clubs */
		gfx_circle(cx, cy - r / 2, r / 2, c, a); gfx_circle(cx - r / 2, cy + r / 6, r / 2, c, a); gfx_circle(cx + r / 2, cy + r / 6, r / 2, c, a);
		gfx_tri(cx, cy, cx - r / 3, cy + r, cx + r / 3, cy + r, c, a);
		break;
	default:  /* spades */
		gfx_circle(cx - r / 2, cy + r / 6, r / 2 + 1, c, a); gfx_circle(cx + r / 2, cy + r / 6, r / 2 + 1, c, a);
		gfx_tri(cx - r - 1, cy + r / 8, cx + r + 1, cy + r / 8, cx, cy - r, c, a);
		gfx_tri(cx, cy + r / 4, cx - r / 3, cy + r, cx + r / 3, cy + r, c, a);
		break;
	}
}

static u32 suit_col(int set, int s)
{
	switch (set) {
	case CS_FOUR: { static const u32 C[4] = { RGB(214, 40, 52), RGB(30, 100, 220), RGB(24, 140, 64), RGB(24, 26, 34) }; return C[s]; }
	case CS_NEON: { static const u32 C[4] = { RGB(255, 80, 170), RGB(80, 220, 255), RGB(140, 255, 90), RGB(190, 140, 255) }; return C[s]; }
	case CS_WII:  return s < 2 ? RGB(230, 60, 70) : RGB(40, 150, 220);
	case CS_ROYAL: return s < 2 ? RGB(170, 20, 40) : RGB(30, 26, 22);
	default: return s < 2 ? RGB(214, 40, 52) : RGB(24, 26, 34);
	}
}

static void card_back(int set, int x, int y)
{
	switch (set) {
	case CS_ROYAL:
		gfx_round(x, y, CW, CH, 8, RGB(245, 236, 214), 255);
		gfx_round(x + 4, y + 4, CW - 8, CH - 8, 6, RGB(140, 20, 36), 255);
		for (int k = 0; k < 7; k++) {   /* gold lattice */
			gfx_line(x + 6, y + 6 + k * 13, x + CW - 6, y + 6 + k * 13 + 26, 1, RGB(220, 180, 90), 110);
			gfx_line(x + CW - 6, y + 6 + k * 13, x + 6, y + 6 + k * 13 + 26, 1, RGB(220, 180, 90), 110);
		}
		gfx_round(x, y + CH - 4, CW, 4, 2, RGB(245, 236, 214), 255);
		gfx_round_line(x + 4, y + 4, CW - 8, CH - 8, 6, 2, RGB(230, 196, 110), 255);
		break;
	case CS_FOUR:
		gfx_round(x, y, CW, CH, 8, RGB(245, 246, 250), 255);
		gfx_round_grad(x + 4, y + 4, CW - 8, CH - 8, 6, RGB(20, 160, 150), RGB(8, 100, 100), 255);
		for (int i = 0; i < 4; i++) suit_shape(i, x + 20 + (i % 2) * 24, y + 30 + (i / 2) * 30, 7, RGB(200, 255, 245), 170);
		break;
	case CS_NEON:
		gfx_round(x, y, CW, CH, 8, RGB(14, 12, 28), 255);
		gfx_round_line(x + 3, y + 3, CW - 6, CH - 6, 6, 2, RGB(255, 80, 170), 255);
		gfx_round_line(x + 8, y + 8, CW - 16, CH - 16, 5, 1, RGB(80, 220, 255), 200);
		suit_shape(3, x + CW / 2, y + CH / 2, 12, RGB(190, 140, 255), 255);
		break;
	case CS_WII:
		gfx_round(x, y, CW, CH, 10, RGB(250, 252, 255), 255);
		for (int k = 0; k < 6; k++) gfx_rect(x + 6, y + 10 + k * 13, CW - 12, 3, RGB(200, 226, 245), 255);
		gfx_circle(x + CW / 2, y + CH / 2, 15, RGB(40, 170, 235), 255);
		gfx_circle(x + CW / 2, y + CH / 2, 10, RGB(250, 252, 255), 255);
		gfx_round_line(x, y, CW, CH, 10, 1, RGB(170, 200, 225), 255);
		break;
	case CS_BIG:
		gfx_round(x, y, CW, CH, 8, RGB(245, 246, 250), 255);
		gfx_round_grad(x + 4, y + 4, CW - 8, CH - 8, 6, RGB(40, 150, 80), RGB(20, 100, 50), 255);
		text_c(F_BODYB, x + CW / 2, y + CH / 2 - 12, "21", RGB(220, 255, 230), 255);
		break;
	default:
		gfx_round(x, y, CW, CH, 8, RGB(240, 242, 248), 255);
		gfx_round_grad(x + 4, y + 4, CW - 8, CH - 8, 6, RGB(60, 150, 230), RGB(24, 96, 180), 255);
		for (int k = 0; k < 4; k++) gfx_ring(x + CW / 2, y + CH / 2, 6 + k * 8, 1, RGB(150, 210, 255), 120);
		break;
	}
}

/* pip spots on a 3 x 13 grid (row in half steps, 0 = top, 12 = bottom); index = rank value 2..10 */
static const signed char PIPS[11][10][2] = {
	{ { -1 } }, { { -1 } },
	{ { 1, 0 }, { 1, 12 }, { -1 } },
	{ { 1, 0 }, { 1, 6 }, { 1, 12 }, { -1 } },
	{ { 0, 0 }, { 2, 0 }, { 0, 12 }, { 2, 12 }, { -1 } },
	{ { 0, 0 }, { 2, 0 }, { 1, 6 }, { 0, 12 }, { 2, 12 }, { -1 } },
	{ { 0, 0 }, { 2, 0 }, { 0, 6 }, { 2, 6 }, { 0, 12 }, { 2, 12 }, { -1 } },
	{ { 0, 0 }, { 2, 0 }, { 1, 3 }, { 0, 6 }, { 2, 6 }, { 0, 12 }, { 2, 12 }, { -1 } },
	{ { 0, 0 }, { 2, 0 }, { 1, 3 }, { 0, 6 }, { 2, 6 }, { 1, 9 }, { 0, 12 }, { 2, 12 }, { -1 } },
	{ { 0, 0 }, { 2, 0 }, { 0, 4 }, { 2, 4 }, { 1, 6 }, { 0, 8 }, { 2, 8 }, { 0, 12 }, { 2, 12 }, { -1 } },
	{ { 0, 0 }, { 2, 0 }, { 1, 2 }, { 0, 4 }, { 2, 4 }, { 0, 8 }, { 2, 8 }, { 1, 10 }, { 0, 12 }, { 2, 12 } },
};

static void card_face(int set, int x, int y, u8 c)
{
	static const char *RK[13] = { "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };
	int r = c % 13, s = c / 13;
	u32 col = suit_col(set, s);
	u32 face = set == CS_NEON ? RGB(22, 20, 40) : set == CS_ROYAL ? RGB(250, 244, 228) : RGB(250, 250, 252);
	u32 edge = set == CS_NEON ? col : set == CS_WII ? RGB(170, 200, 225) : RGB(200, 204, 214);
	int rad = set == CS_WII ? 10 : 8;
	gfx_round(x, y, CW, CH, rad, face, 255);
	gfx_round_line(x, y, CW, CH, rad, set == CS_NEON ? 2 : 1, edge, 255);
	if (set == CS_BIG) {
		text_c(F_TITLE, x + CW / 2, y + 10, RK[r], col, 255);
		suit_shape(s, x + CW / 2, y + CH - 24, 13, col, 255);
		return;
	}
	/* corner index */
	text_c(F_BODYB, x + 10, y + 3, RK[r], col, 255);
	suit_shape(s, x + 10, y + 32, 5, col, 255);
	if (r == 0) { suit_shape(s, x + CW / 2 + 5, y + CH / 2 + 2, s == 3 ? 16 : 13, col, 255); return; }
	if (r >= 10) {
		/* court card: a framed panel with a crown and the letter */
		u32 pan = set == CS_NEON ? RGB(40, 30, 70) : set == CS_ROYAL ? RGB(240, 222, 170) : set == CS_WII ? RGB(225, 240, 252) : RGB(236, 238, 246);
		gfx_round(x + 20, y + 10, CW - 26, CH - 20, 6, pan, 255);
		gfx_round_line(x + 20, y + 10, CW - 26, CH - 20, 6, 1, col, 255);
		int cx = x + 20 + (CW - 26) / 2;
		u32 gold = set == CS_NEON ? col : RGB(210, 160, 40);
		gfx_rect(cx - 11, y + 30, 22, 5, gold, 255);
		gfx_tri(cx - 11, y + 30, cx - 11, y + 18, cx - 4, y + 30, gold, 255);
		gfx_tri(cx - 4, y + 30, cx, y + 16, cx + 4, y + 30, gold, 255);
		gfx_tri(cx + 4, y + 30, cx + 11, y + 18, cx + 11, y + 30, gold, 255);
		text_c(F_TITLE, cx, y + 38, RK[r], col, 255);
		return;
	}
	/* number cards: real pips */
	int v = r + 1;
	for (int k = 0; k < 10 && PIPS[v][k][0] >= 0; k++) {
		int px = x + 28 + PIPS[v][k][0] * 12, py = y + 14 + PIPS[v][k][1] * 62 / 12;
		suit_shape(s, px, py, 5, col, 255);
	}
}

static void card_draw(int set, int x, int y, u8 c, int hidden)
{
	gfx_shadow(x, y, CW, CH, 8);
	if (hidden) card_back(set, x, y); else card_face(set, x, y, c);
}

/* a card flies in from the shoe (top right) */
static int card_pos(const Card *k, int tx, int ty, int *x, int *y)
{
	u64 now = plat_ms();
	if (now < k->t) return 0;
	int dt = (int)(now - k->t), dur = speed_ms() * 3 / 4 + 60;
	if (dt >= dur) { *x = tx; *y = ty; return 1; }
	float p = dt / (float)dur; p = 1 - (1 - p) * (1 - p);
	*x = 546 + (int)((tx - 546) * p); *y = 150 + (int)((ty - 150) * p);
	return 1;
}

static u32 shade(u32 c, int d);
/* a clay chip: thickness, edge inserts, an inlay ring and a bit of shine */
static void chip_draw(int cx, int cy, int r, int v, int focus)
{
	u32 c = v >= 5000 ? RGB(230, 120, 30) : v >= 1000 ? RGB(210, 170, 40) : v >= 500 ? RGB(130, 60, 200) : v >= 250 ? RGB(220, 90, 150) :
	        v >= 100 ? RGB(30, 30, 36) : v >= 50 ? RGB(40, 120, 220) : v >= 25 ? RGB(30, 150, 70) : v >= 10 ? RGB(40, 90, 200) : RGB(210, 40, 50);
	int th = r > 14 ? 3 : 2;
	gfx_circle(cx + 1, cy + th + 2, r, RGB(0, 0, 0), 100);          /* shadow */
	gfx_circle(cx, cy + th, r, shade(c, -70), 255);                 /* the edge you see under it */
	gfx_circle(cx, cy, r, c, 255);
	/* six white edge inserts */
	int ir = r > 14 ? 3 : r > 8 ? 2 : 1, rr = r - ir - 1;
	for (int k = 0; k < 6; k++)
		for (int j = -2; j <= 2; j++) { float a = k * 1.0472f + 0.26f + j * 0.09f; gfx_circle(cx + (int)(cosf(a) * rr), cy + (int)(sinf(a) * rr), ir, RGB(245, 245, 240), 240); }
	/* the inlay */
	int in = r - r * 2 / 5;
	gfx_circle(cx, cy, in + 1, shade(c, -50), 255);
	gfx_circle(cx, cy, in, RGB(246, 244, 236), 255);
	gfx_circle(cx, cy, in - 2, c, 255);
	if (r > 14) gfx_ring(cx, cy, in - 4, 1, shade(c, 60), 150);
	/* shine */
	gfx_circle(cx - r / 3, cy - r / 3, r / 3 + 1, RGB(255, 255, 255), 34);
	gfx_ring(cx, cy, r, 1, shade(c, -90), 200);
	if (r > 14) {
		char t[8]; if (v >= 1000) snprintf(t, sizeof t, "%dk", v / 1000); else snprintf(t, sizeof t, "%d", v);
		text_c(F_SMALL, cx + 1, cy - 8, t, RGB(0, 0, 0), 120);
		text_c(F_SMALL, cx, cy - 9, t, RGB(255, 255, 255), 255);
	}
	if (focus) { gfx_ring(cx, cy, r + 5, 5, C_ACCENT, 70); gfx_ring(cx, cy, r + 4, 3, C_ACCENT, 255); }
}

#include "casino_fx.inc"

/* ================================================================== */
/* screens                                                            */
/* ================================================================== */
static void header(const char *title, const char *sub)
{
	icon(IC_STAR, 58, 50, 30, RGB(230, 196, 110));
	text_fit(F_TITLE, 86, 28, 260, title, C_TEXT, 255);
	if (sub) text_fit(F_SMALL, 88, 64, 330, sub, C_DIM, 255);
	if (S.free_now && cfg.chips < 1000) cfg.chips = FREE_CHIPS;   /* free play never runs out */
	char cs[32]; snprintf(cs, sizeof cs, S.free_now ? "Free play" : "%d chips", cfg.chips);
	int w = text_w(F_BODYB, cs) + 30;
	gfx_round(536 - w, 34, w, 32, 16, C_INSET, 255);
	text_c(F_BODYB, 536 - w / 2, 38, cs, RGB(255, 214, 90), 255);
}

static void open_view(int v, int focus) { back_view = view; view = v; ui_set_focus(focus); }
static void close_view(void)
{
	view = (back_view == V_DECKS || back_view == V_STATS) ? V_FLOOR : back_view;
	ui_set_focus(view == V_FLOOR ? 8210 : view == V_TABLE ? 8300 : -1);
}

static int stats_page;
/* the second page of options: comfort and helpers */
static int opt2_open;
static void options2_popup(void)
{
	static const char *TS[4] = { "Slow", "Normal", "Fast", "Instant" }, *VL[4] = { "Off", "Low", "Medium", "Full" }, *BR[3] = { "Off", "30 min", "60 min" };
	char t[12][40];
	snprintf(t[0], 40, "Talk speed: %s", TS[S.talk_spd & 3]);
	snprintf(t[1], 40, "Skip animations: %s", S.skip_anim ? "On" : "Off");
	snprintf(t[2], 40, "Big text: %s", S.big_text ? "On" : "Off");
	snprintf(t[3], 40, "Big bet check: %s", S.big_check ? "On" : "Off");
	snprintf(t[4], 40, "Learning help: %s", S.helper ? "On" : "Off");
	snprintf(t[5], 40, "Break reminder: %s", BR[S.brk % 3]);
	snprintf(t[6], 40, "Music: %s", VL[S.vol_m & 3]);
	snprintf(t[7], 40, "Voices: %s", VL[S.vol_v & 3]);
	snprintf(t[8], 40, "Sound effects: %s", VL[S.vol_f & 3]);
	snprintf(t[9], 40, "Rumble: %s", S.rum ? "On" : "Off");
	snprintf(t[10], 40, "Game stats");
	const char *it[11]; for (int i = 0; i < 11; i++) it[i] = t[i];
	static const int ic[11] = { IC_NOTE, IC_PLAY, IC_SEARCH, IC_CHECK, IC_HELP, IC_MOON, IC_VOLUP, IC_VOLUP, IC_VOLUP, IC_GAMEPAD, IC_MENU };
	int k = menu_popup(&opt2_open, it, ic, 11, "More options");
	if (k < 0) return;
	switch (k) {
	case 0: S.talk_spd = (S.talk_spd + 1) & 3; break;
	case 1: S.skip_anim ^= 1; break;
	case 2: S.big_text ^= 1; break;
	case 3: S.big_check ^= 1; break;
	case 4: S.helper ^= 1; break;
	case 5: S.brk = (S.brk + 1) % 3; break;
	case 6: S.vol_m = (S.vol_m + 3) & 3; break;
	case 7: S.vol_v = (S.vol_v + 3) & 3; break;
	case 8: S.vol_f = (S.vol_f + 3) & 3; break;
	case 9: S.rum ^= 1; if (S.rum) plat_rumble(80); break;
	case 10: open_view(V_STATS, 8600); stats_page = 2; stats_save(); return;
	}
	opt2_open = 1; ui_set_focus(9000 + k); stats_save();
}

static void options_popup(void)
{
	char a[32], b[32], c[32], d[32], e[40], f2[40];
	static const char *SP[3] = { "Slow", "Normal", "Fast" };
	snprintf(a, sizeof a, "Strategy hints: %s", S.hints ? "On" : "Off");
	snprintf(b, sizeof b, "Card count: %s", S.count ? "On" : "Off");
	snprintf(c, sizeof c, "Deal speed: %s", SP[S.speed % 3]);
	static const char *CHAT_N[3] = { "Off", "Quiet", "Full" };
	static const char *SL[4] = { "Off", "down 25%", "down 50%", "down 75%" }, *WG[4] = { "Off", "up 50%", "double", "triple" };
	snprintf(d, sizeof d, "Dealer chat: %s", CHAT_N[S.quips % 3]);
	snprintf(e, sizeof e, "Stop-loss: %s", SL[S.stop_loss & 3]);
	snprintf(f2, sizeof f2, "Win goal: %s", WG[S.win_goal & 3]);
	static const char *SND_N[4] = { "Sound: Off", "Sound: Effects", "Music: Quiet", "Music: On" };
	const char *g2 = SND_N[S.snd & 3];
	const char *it[] = { a, b, c, d, e, f2, "Card sets", "Stats", g2, S.free ? "Free play: On" : "Free play: Off", "More options...", "Chat log" };
	static const int ic[] = { IC_HELP, IC_SEARCH, IC_PLAY, IC_NOTE, IC_CLOSE, IC_STAR, IC_STAR, IC_MENU, IC_VOLUP, IC_GAMEPAD, IC_GEAR, IC_MORE };
	int k = menu_popup(&opt_open, it, ic, 12, "Casino options");
	if (k == 10) { opt2_open = 1; ui_set_focus(9000); }
	if (k == 11) { cl_open = 1; }
	if (k == 9) {
		if (cas_busy()) { ui_toast("Finish this round first"); opt_open = 1; }
		else { S.free = !S.free; if (S.free) { free_on(); ui_toast("Free play: practice chips. Your real chips are safe"); } else { free_off(); ui_toast("Free play off: your real chips are back"); } opt_open = 1; ui_set_focus(9009); stats_save(); }
	}
	if (k == 8) { S.snd = (S.snd + 1) & 3; opt_open = 1; ui_set_focus(9008); stats_save(); }
	if (k == 0) { S.hints = !S.hints; opt_open = 1; ui_set_focus(9000); stats_save(); }
	if (k == 1) { S.count = !S.count; opt_open = 1; ui_set_focus(9001); stats_save(); }
	if (k == 2) { S.speed = (S.speed + 1) % 3; opt_open = 1; ui_set_focus(9002); stats_save(); }
	if (k == 3) { S.quips = (S.quips + 2) % 3; opt_open = 1; ui_set_focus(9003); stats_save(); if (S.quips) talk_sit(persona + 1); else q_text[0] = 0; }
	if (k == 4) { S.stop_loss = (S.stop_loss + 1) & 3; opt_open = 1; ui_set_focus(9004); stats_save(); }
	if (k == 5) { S.win_goal = (S.win_goal + 1) & 3; opt_open = 1; ui_set_focus(9005); stats_save(); }
	if (k == 6) open_view(V_DECKS, 8500 + S.cardset);
	if (k == 7) open_view(V_STATS, 8600);
}

static void decks(int blocked)
{
	header("Card sets", "Pick the look of your cards");
	for (int k = 0; k < CS_COUNT; k++) {
		int x = 40 + (k % 3) * 190, y = 88 + (k / 3) * 150, w = 178, h = 142;
		int f = ui_item(8500 + k, x, y, w, h);
		gfx_round_grad(x, y, w, h, 16, (f & UI_FOCUS) ? C_FOCUS_TOP : C_CARD_HI, (f & UI_FOCUS) ? C_FOCUS_BOT : C_CARD, 255);
		gfx_round_line(x, y, w, h, 16, k == S.cardset ? 3 : 1, k == S.cardset ? C_ACCENT : C_LINE, 255);
		card_draw(k, x + 10, y + 10, 0 + 13 * 3, 0);          /* ace of spades */
		card_draw(k, x + 56, y + 16, 12 + 13 * 0, 0);         /* king of hearts */
		card_draw(k, x + 102, y + 10, 0, 1);                  /* the back */
		text(F_BODYB, x + 14, y + 110, CS_NAME[k], C_TEXT, 255);
		if (k == S.cardset) icon(IC_CHECK, x + w - 22, y + 121, 18, C_ACCENT);
		if (f & UI_FOCUS) ui_focus_ring(x, y, w, h, 16);
		if (f & UI_CLICK) { S.cardset = k; stats_save(); ui_toast(CS_TIP[k]); }
	}
	ui_footer_begin();
	ui_footer_hint("A", "Use this set");
	ui_footer_hint("B", "Back");
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) close_view();
}

const char *casino_rel_word(int who);
static void stats_history(int blocked)
{
	header("Stats", "Your chips over the last 40 rounds");
	/* the chip graph */
	int gx = 26, gy = 88, gw = 586, gh = 180;
	gfx_round(gx, gy, gw, gh, 16, C_CARD, 255);
	if (S.nhist < 2) text_c(F_BODYB, gx + gw / 2, gy + gh / 2 - 12, "Play a few rounds to see your chips here", C_DIM, 255);
	else {
		int lo = S.hist[0], hi = S.hist[0];
		for (int i = 0; i < S.nhist; i++) { if (S.hist[i] < lo) lo = S.hist[i]; if (S.hist[i] > hi) hi = S.hist[i]; }
		if (hi == lo) { hi++; lo--; }
		int px = gx + 70, pw = gw - 90, py = gy + 16, ph = gh - 40;
		for (int k = 0; k <= 2; k++) {
			int yy = py + ph * k / 2, v = hi - (hi - lo) * k / 2;
			gfx_rect(px, yy, pw, 1, C_LINE, 255);
			char t[16]; snprintf(t, sizeof t, "%d", v); text_r(F_SMALL, px - 8, yy - 9, t, C_DIM, 255);
		}
		int prevx = 0, prevy = 0;
		for (int i = 0; i < S.nhist; i++) {
			int x = px + pw * i / (S.nhist - 1), y = py + ph - (int)((long long)(S.hist[i] - lo) * ph / (hi - lo));
			if (i) gfx_line(prevx, prevy, x, y, 2, S.hist[i] >= S.hist[i - 1] ? C_GOOD : C_BAD, 255);
			prevx = x; prevy = y;
		}
		gfx_circle(prevx, prevy, 5, C_ACCENT, 255);
		char t[48]; snprintf(t, sizeof t, "Now: %d", S.hist[S.nhist - 1]);
		text_r(F_BODYB, gx + gw - 16, gy + gh - 22, t, C_TEXT, 255);
	}
	/* how the dealers feel about you */
	text(F_BODYB, 26, 278, "The dealers", C_TEXT, 255);
	static const char *DN[6] = { "Phantom Moth", "Trigger Lynx", "Thunder Crow", "Mirror Squid", "Silent Vixen", "Crimson Viper" };
	for (int k = 0; k < 6; k++) {
		int x = 26 + (k % 2) * 296, y = 304 + (k / 2) * 30;
		gfx_round(x, y, 290, 26, 12, C_INSET, 255);
		text(F_SMALL, x + 12, y + 3, DN[k], C_TEXT, 255);
		int r = S.rel[k];
		const char *w = casino_rel_word(k);
		text_r(F_SMALL, x + 280, y + 3, w, r >= 15 ? C_GOOD : r <= -15 ? C_BAD : C_DIM, 255);
	}
	if (ui_ibutton(8602, 26, 396, 200, 30, IC_MENU, "Your record", ST_NORMAL) & UI_CLICK) { stats_page = 0; ui_set_focus(8601); }
	ui_footer_begin();
	ui_footer_hint("B", "Back");
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) { stats_page = 0; close_view(); }
}

/* stats for each game */
static const char *game_name(int g);
static void stats_games(int blocked)
{
	header("Stats by game", "Rounds, wins, best win and time played");
	gfx_round(26, 88, 588, 330, 16, C_CARD, 255);
	text(F_SMALL, 40, 94, "Game", C_DIM, 255); text_r(F_SMALL, 330, 94, "Rounds", C_DIM, 255); text_r(F_SMALL, 420, 94, "Won", C_DIM, 255);
	text_r(F_SMALL, 510, 94, "Best win", C_DIM, 255); text_r(F_SMALL, 600, 94, "Time", C_DIM, 255);
	int row = 0;
	for (int g = 0; g < 16 && row < 13; g++) {
		if (!S.gs_p[g]) continue;
		int y = 116 + row * 22; row++;
		char a[16], b[16], c[16], d[16];
		snprintf(a, sizeof a, "%d", S.gs_p[g]); snprintf(b, sizeof b, "%d%%", S.gs_w[g] * 100 / S.gs_p[g]);
		snprintf(c, sizeof c, "%d", S.gs_best[g]); snprintf(d, sizeof d, "%dh %02dm", S.gs_sec[g] / 3600, S.gs_sec[g] / 60 % 60);
		text(F_SMALL, 40, y, game_name(g), C_TEXT, 255); text_r(F_SMALL, 330, y, a, C_TEXT, 255); text_r(F_SMALL, 420, y, b, C_TEXT, 255);
		text_r(F_SMALL, 510, y, c, RGB(255, 214, 90), 255); text_r(F_SMALL, 600, y, d, C_TEXT, 255);
	}
	if (!row) text_c(F_SMALL, 320, 240, "Play some games and they'll show up here.", C_DIM, 255);
	ui_footer_begin();
	ui_footer_hint("B", "Back");
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) { stats_page = 0; ui_set_focus(8601); in->down = 0; }
}
static void stats_view(int blocked)
{
	if (stats_page == 2) { stats_games(blocked); return; }
	if (stats_page) { stats_history(blocked); return; }
	header("Stats", "Your record across every game");
	gfx_round(26, 88, 250, 312, 16, C_CARD, 255);
	if (ui_ibutton(8601, 34, 356, 234, 36, IC_PLAY, "Chip history", ST_NORMAL) & UI_CLICK) { stats_page = 1; ui_set_focus(8602); return; }
	int hands = S.won + S.lost + S.pushed;
	const char *L[8]; char V[8][24]; int n = 0;
	L[n] = "Rounds played"; snprintf(V[n++], 24, "%d", S.played);
	L[n] = "Won"; snprintf(V[n++], 24, "%d (%d%%)", S.won, hands ? S.won * 100 / hands : 0);
	L[n] = "Lost"; snprintf(V[n++], 24, "%d", S.lost);
	L[n] = "Pushes"; snprintf(V[n++], 24, "%d", S.pushed);
	L[n] = "Blackjacks"; snprintf(V[n++], 24, "%d", S.bjs);
	L[n] = "Biggest win"; snprintf(V[n++], 24, "%d", S.best_win);
	L[n] = "Best streak"; snprintf(V[n++], 24, "%d", S.best_streak);
	L[n] = "Most chips"; snprintf(V[n++], 24, "%d", S.max_chips);
	for (int i = 0; i < n; i++) {
		text(F_SMALL, 40, 100 + i * 31, L[i], C_DIM, 255);
		text_r(F_BODYB, 262, 96 + i * 31, V[i], C_TEXT, 255);
	}
	text(F_BODYB, 290, 88, "Achievements", C_TEXT, 255);
	int got = 0; for (int a = 0; a < AC_COUNT; a++) got += (S.ach >> a) & 1;
	char gs[16]; snprintf(gs, sizeof gs, "%d / %d", got, AC_COUNT); text_r(F_SMALL, 612, 92, gs, C_DIM, 255);
	for (int a = 0; a < AC_COUNT; a++) {
		int on = (S.ach >> a) & 1, col = a / 9, row = a % 9;
		int x = 290 + col * 164, y = 116 + row * 27;
		gfx_round(x, y, 158, 24, 10, on ? C_SOFT_GOOD : C_INSET, 255);
		icon(on ? IC_STAR : IC_CLOSE, x + 14, y + 12, 12, on ? RGB(255, 214, 90) : C_DIM);
		text_fit(F_SMALL, x + 28, y + 2, 124, AC_NAME[a], on ? C_TEXT : C_DIM, 255);
	}
	if (ui_ibutton(8600, 290, 360, 322, 38, IC_TRASH, reset_arm ? "Press again to reset" : "Reset stats", ST_NORMAL) & UI_CLICK) {
		if (!reset_arm) reset_arm = 1;
		else {
			int cs = S.cardset, hi = S.hints, co = S.count, sp = S.speed, qu = S.quips, sl = S.stop_loss, wg = S.win_goal, bd = S.bonus_day, c2 = S.chips2, pl = S.player;
			memset(&S, 0, sizeof S);
			S.cardset = cs; S.hints = hi; S.count = co; S.speed = sp; S.quips = qu; S.stop_loss = sl; S.win_goal = wg; S.bonus_day = bd; S.chips2 = c2; S.player = pl;
			S.last_game = -1; for (int k = 0; k < 6; k++) S.bet_k[k] = 1;
			stats_save(); reset_arm = 0; ui_toast("Stats reset");
		}
	}
	ui_footer_begin();
	ui_footer_hint("1", "By game");
	ui_footer_hint("2", "Chip history");
	ui_footer_hint("B", "Back");
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) { reset_arm = 0; close_view(); }
	else if (!blocked && (in->down & BTN_2)) { stats_page = 1; ui_set_focus(8602); }
	else if (!blocked && (in->down & BTN_1)) { stats_page = 2; }
}

/* one hand of cards, fanned */
static void hand_draw(const Card *k, int n, int cx, int y, int hide_hole, int active)
{
	int step = n > 5 ? 14 : 20;
	int w = CW + (n - 1) * step, x0 = cx - w / 2;
	if (active) gfx_round(x0 - 7, y - 7, w + 14, CH + 14, 12, RGB(255, 255, 255), 40);
	for (int i = 0; i < n; i++) {
		int x, yy;
		(void)x; (void)yy;
		if (k == dh && i == 1 && !hide_hole && hole_at) {   /* the dealer's hole card turning over */
			u64 now = plat_ms();
			if (now < hole_at) { card_draw(S.cardset, x0 + i * step, y, k[i].c, 1); card_gloss(x0 + i * step, y); }
			else card_flip_draw(S.cardset, x0 + i * step, y, k[i].c, (now - hole_at) / (float)FLIP_MS);
			continue;
		}
		card_anim(&k[i], x0 + i * step, y, hide_hole && i == 1);
	}
	if (active) gfx_round_line(x0 - 7, y - 7, w + 14, CH + 14, 12, 2, RGB(255, 214, 90), 255);
}
static int landed(const Card *k, int n) { return n == 0 || plat_ms() >= k[n - 1].t + (u64)(speed_ms() * 3 / 4 + 60); }
static int visible_total(const Card *k, int n, int *soft)
{
	Card v[14]; int m = 0; u64 now = plat_ms();
	for (int i = 0; i < n && m < 14; i++) if (now >= k[i].t) v[m++] = k[i];
	return value_of(v, m, soft);
}

static void table_screen(int blocked)
{
	const Table *T = &TABLES[tb];
	u64 now = plat_ms();
	/* felt */
	felt_draw(16, 14, 608, 400, 26, T->felt_top, T->felt_bot, T->rail);
	/* printed on the felt */
	if (g == G_BET) {   /* the betting circle */
		gfx_ring(320, 246, 34, 2, RGB(255, 255, 255), 70);
		gfx_ring(320, 246, 29, 1, RGB(255, 255, 255), 40);
		if (bet > 0) chip_stack(320, 250, bet, 20);
	} else if (nh == 1 && ph[0].n) chip_stack(320 - (CW + (ph[0].n - 1) * (ph[0].n > 5 ? 14 : 20)) / 2 - 34, 268, ph[0].bet * (ph[0].doubled ? 2 : 1), 14);
	if (nh == 0) text_c(F_SMALL, 320, 290, T->bj_num == 3 ? "BLACKJACK PAYS 3 TO 2" : "BLACKJACK PAYS 6 TO 5", RGB(255, 255, 255), 40);
	if (nh == 0) text_c(F_SMALL, 320, 306, T->h17 ? "Dealer hits soft 17  -  Insurance pays 2 to 1" : "Dealer stands on all 17s  -  Insurance pays 2 to 1", RGB(255, 255, 255), 30);
	/* the shoe, top right, where the cards fly from */
	card_draw(S.cardset, 546, 150, 0, 1);
	gfx_round(538, 180, 80, 66, 8, RGB(0, 0, 0), 90);
	gfx_round_grad(536, 176, 80, 64, 8, RGB(70, 46, 28), RGB(28, 18, 10), 255);
	gfx_round_line(536, 176, 80, 64, 8, 1, RGB(230, 196, 110), 160);
	text_c(F_SMALL, 576, 198, "SHOE", RGB(230, 196, 110), 200);
	text(F_BODYB, 36, 24, T->name, RGB(250, 244, 226), 255);
	char sub[64]; snprintf(sub, sizeof sub, "Bets %d - %d  -  Blackjack pays %s", T->min, T->max, T->bj_num == 3 ? "3 to 2" : "6 to 5");
	text(F_SMALL, 36, 50, sub, RGB(190, 220, 205), 255);
	/* chips */
	char cs[32]; snprintf(cs, sizeof cs, "%d", cfg.chips);
	gfx_round(330, 24, 124, 30, 15, RGB(0, 0, 0), 110);
	chip_draw(348, 39, 11, T->chip[2], 0);
	text(F_BODYB, 366, 27, cs, RGB(255, 214, 90), 255);
	delta_draw(392, 58);
	/* the shoe: how much is left */
	gfx_round(460, 24, 76, 30, 15, RGB(0, 0, 0), 110);
	text(F_SMALL, 472, 25, "Cards left", RGB(200, 200, 210), 255);
	int left = shoe_n - shoe_pos, pct = shoe_n ? left * 100 / shoe_n : 0;
	gfx_rect(472, 45, 52, 4, RGB(60, 60, 70), 255);
	gfx_rect(472, 45, 52 * pct / 100, 4, RGB(255, 214, 90), 255);
	/* card count trainer */
	if (S.count) {
		int dl10 = left * 10 / 52; if (dl10 < 5) dl10 = 5;
		char c[48]; snprintf(c, sizeof c, "Count %+d   True %+d", running, running * 10 / dl10);
		gfx_round(330, 58, 206, 22, 11, RGB(0, 0, 0), 110);
		text_c(F_SMALL, 433, 59, c, RGB(200, 240, 255), 255);
	}

	if (g == G_BET || g == G_DONE) talk_idle();

	/* dealer */
	if (dn) {
		int hide = g == G_DEAL || g == G_INSURE || g == G_PLAY;
		hand_draw(dh, dn, 320, 86, hide, 0);
		int soft, v = hide ? (now >= dh[0].t ? rank10(dh[0].c) : 0) : visible_total(dh, dn, &soft);
		if (v) {
			char t[24]; snprintf(t, sizeof t, "Dealer %d", v);
			int bx = S.quips ? 496 : 36;
			gfx_round(bx, 118, 104, 26, 13, RGB(0, 0, 0), 110); text_c(F_SMALL, bx + 52, 121, t, RGB(240, 240, 240), 255);
		}
	} else if (g == G_BET) {
		int mx = S.quips ? 428 : 320;
		text_c(F_TITLE, mx, 118, "Place your bet", RGB(250, 244, 226), 255);
		char r[64]; snprintf(r, sizeof r, "Dealer %s soft 17  -  %d deck%s", T->h17 ? "hits" : "stands on", T->decks, T->decks > 1 ? "s" : "");
		text_c(F_SMALL, mx, 164, r, RGB(190, 220, 205), 255);
		text_c(F_SMALL, mx, 188, "Tap chips to bet, then Deal", RGB(190, 220, 205), 255);
	}
	/* player hands */
	for (int i = 0; i < nh; i++) {
		int slot = 568 / nh, cx = nh == 1 ? 320 : 36 + slot * i + slot / 2;
		Hand *h = &ph[i];
		hand_draw(h->k, h->n, cx, 196, 0, g == G_PLAY && i == cur_h && nh > 1);
		int soft, v = visible_total(h->k, h->n, &soft);
		char t[40];
		if (h->surrendered) snprintf(t, sizeof t, "Gave up");
		else if (nh > 2) snprintf(t, sizeof t, "%d  -  %d", v, h->bet * (h->doubled ? 2 : 1));
		else snprintf(t, sizeof t, "%d%s  -  bet %d%s", v, soft && v < 21 ? " soft" : "", h->bet * (h->doubled ? 2 : 1), h->doubled ? " (x2)" : "");
		int tw = text_w(F_SMALL, t) + 20;
		u32 bg = g == G_DONE ? (h->res > 0 ? RGB(40, 150, 90) : h->res < 0 ? RGB(150, 50, 50) : RGB(70, 80, 100)) : RGB(0, 0, 0);
		gfx_round(cx - tw / 2, 292, tw, 24, 12, bg, g == G_DONE ? 230 : 110);
		text_c(F_SMALL, cx, 294, t, RGB(245, 245, 245), 255);
	}

	/* ---- the flow ---- */
	if (g == G_DEAL && landed(ph[0].k, ph[0].n) && landed(dh, dn)) after_deal();
	if (g == G_DEALER && now >= settle_at) settle();

	/* message */
	if (msg[0] && g == G_DONE) {
		int w = text_w(F_BODYB, msg) + 40; if (w > 580) w = 580;
		u32 bg = msg_tone > 0 ? RGB(40, 150, 90) : msg_tone < 0 ? RGB(150, 50, 50) : RGB(60, 70, 90);
		gfx_round(320 - w / 2, 322, w, 30, 15, bg, 240);
		text_fit(F_BODYB, 320 - w / 2 + 20, 325, w - 40, msg, C_WHITE, 255);
	}

	int y = 362;
	if (g == G_BET || g == G_DONE) {
		/* chips to bet with, then Deal */
		for (int k = 0; k < 5; k++) {
			int cx = 60 + k * 50;
			int f = ui_item(8310 + k, cx - 23, y - 2, 46, 50);
			chip_draw(cx, y + 22, 21, T->chip[k], f & UI_FOCUS);
			if ((f & UI_CLICK) && !opt_open) {
				if (g == G_DONE) { g = G_BET; nh = 0; dn = 0; bet = 0; msg[0] = 0; }
				if (bet + T->chip[k] > T->max) ui_toast("That's over the table limit");
				else if (bet + T->chip[k] > cfg.chips) ui_toast("Not enough chips");
				else { bet += T->chip[k]; plat_rumble(10); music_sfx(SFX_CHIP, 0); }
			}
		}
		char b[24]; snprintf(b, sizeof b, "Bet %d", g == G_DONE ? S.last_bet[tb] : bet);
		gfx_round(312, y + 2, 94, 42, 12, RGB(0, 0, 0), 120);
		text_c(F_BODYB, 359, y + 12, b, RGB(255, 214, 90), 255);
		if ((ui_tile(8301, 412, y, 52, 46, IC_CLOSE, "", ST_NORMAL, 0) & UI_CLICK) && !opt_open) { bet = 0; if (g == G_DONE) { g = G_BET; nh = dn = 0; msg[0] = 0; } }
		if ((ui_ibutton(8300, 472, y, 132, 46, IC_PLAY, g == G_DONE ? "Again" : "Deal", ST_NORMAL) & UI_CLICK) && !opt_open) {
			if (g == G_DONE) bet = S.last_bet[tb];
			if (bet > cfg.chips) bet = cfg.chips >= T->min ? T->min : 0;
			deal();
		}
	} else if (g == G_INSURE) {
		text_c(F_BODYB, 320, 324, "The dealer shows an Ace. Insurance?", C_WHITE, 255);
		char a[40]; snprintf(a, sizeof a, "Insure (%d)", ph[0].bet / 2);
		int answered = 0;
		if (ui_ibutton(8420, 140, y, 170, 46, IC_CHECK, a, ST_NORMAL) & UI_CLICK) { insurance = ph[0].bet / 2; cfg.chips -= insurance; answered = 1; }
		if (ui_ibutton(8421, 330, y, 170, 46, IC_CLOSE, "No thanks", ST_NORMAL) & UI_CLICK) { insurance = 0; answered = 1; }
		if (answered) {
			if (dealer_bj()) dealer_turn();
			else { if (insurance) ui_toast("No dealer blackjack. Insurance lost"); g = G_PLAY; ui_set_focus(8400); }
		}
	} else if (g == G_PLAY) {
		int ok[5] = { 1, 1, can_double(), can_split(), can_surrender() };
		const char *L[5] = { "Hit", "Stand", free_double() ? "Free double" : "Double", free_split() && can_split() ? "Free split" : "Split", TABLES[tb].variant == 3 ? "Switch" : "Give up" };
		if (TABLES[tb].variant == 3) ok[4] = bj_switch_ok && cur_h == 0 && ph[0].n == 2 && ph[1].n == 2;
		const char *hn = S.hints ? bj_hint() : NULL;
		int bw = 108;
		for (int k = 0; k < 5; k++) {
			int x = 40 + k * (bw + 5);
			int f = ui_button(8400 + k, x, y, bw, 46, L[k], ok[k] ? ST_NORMAL : ST_GHOST);
			if (hn && !strcmp(hn, L[k])) gfx_round_line(x - 3, y - 3, bw + 6, 52, 14, 2, RGB(255, 214, 90), 255);
			if ((f & UI_CLICK) && !opt_open) {
				if (!ok[k]) { ui_toast(k == 2 ? "Double: only on your first 2 cards, with enough chips" : k == 3 ? "Split: needs a pair, with enough chips" : "Give up: only at the start, at Classic and High Roller"); continue; }
				if (!landed(ph[cur_h].k, ph[cur_h].n)) continue;
				if (k == 4 && TABLES[tb].variant == 3) { Card t = ph[0].k[1]; ph[0].k[1] = ph[1].k[1]; ph[1].k[1] = t; ph[0].k[1].t = ph[1].k[1].t = 0; bj_switch_ok = 0; music_sfx(SFX_FLIP, 0); break; }
				bj_switch_ok = 0;
				if (k == 0) act_hit(); else if (k == 1) act_stand(); else if (k == 2) act_double(); else if (k == 3) act_split(); else act_surrender();
				break;
			}
		}
		if (hn) { char t[40]; snprintf(t, sizeof t, "Hint: %s", hn); text_c(F_SMALL, 320, 330, t, RGB(255, 214, 90), 255); }
	}

	creep_shade();
	quip_draw();
	quip_tick();

	/* achievement pop */
	if (now < ach_until) {
		int w = text_w(F_BODYB, ach_msg) + 50;
		gfx_round(320 - w / 2, 150, w, 40, 20, RGB(30, 30, 40), 235);
		gfx_round_line(320 - w / 2, 150, w, 40, 20, 2, RGB(255, 214, 90), 255);
		icon(IC_STAR, 320 - w / 2 + 22, 170, 18, RGB(255, 214, 90));
		text(F_BODYB, 320 - w / 2 + 38, 158, ach_msg, C_WHITE, 255);
	}

	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	if (g == G_PLAY) { ui_footer_hint("1", "Hit"); ui_footer_hint("2", "Stand"); }
	else if (g == G_BET || g == G_DONE) ui_footer_hint("1", "Same bet");
	ui_footer_hint("+", "Options");
	ui_footer_hint("HOME", "Lobby");
	if (blocked || opt_open) return;
	if (in->down & BTN_PLUS) { opt_open = 1; ui_set_focus(9000); return; }
	if (g == G_PLAY && landed(ph[cur_h].k, ph[cur_h].n)) {
		if (in->down & BTN_1) act_hit();
		else if (in->down & BTN_2) act_stand();
	} else if ((g == G_BET || g == G_DONE) && (in->down & BTN_1)) {
		bet = S.last_bet[tb] ? S.last_bet[tb] : T->min;
		if (bet > cfg.chips) bet = cfg.chips >= T->min ? T->min : 0;
		deal();
	}
	if (in->down & (BTN_HOME | BTN_B)) {
		if (g == G_PLAY || g == G_DEAL || g == G_INSURE || g == G_DEALER) { if (in->down & BTN_HOME) ui_toast("Finish this hand first"); }
		else { view = V_FLOOR; nh = dn = 0; g = G_BET; S.last_game = -1; stats_save(); ui_set_focus(8200); }
	}
}

#include "casino_games.inc"

/* the music follows where you are; Phantom Moth reading you drags the band out of tune */
/* ---- are you sure? (big bets) ---- */
static void big_check_popup(void)
{
	if (!bigc_open) return;
	char y[48]; snprintf(y, sizeof y, "Yes, bet %d", bigc_amt);
	const char *IT[2] = { y, "No, go back" };
	static const int IC2[2] = { IC_CHECK, IC_CLOSE };
	int k = menu_popup(&bigc_open, IT, IC2, 2, "That's a big bet. Sure?");
	if (k == 0 && bigc_fn) { bigc_pass = 1; bigc_fn(); bigc_pass = 0; }
}
/* ---- the chat log ---- */
static void chatlog_draw(int blocked)
{
	if (!cl_open) return;
	ui_suspend(0); ui_dim();
	int x = 40, y = 24, w = 560, h = 396;
	ui_panel(x, y, w, h);
	text_c(F_BODYB, 320, y + 10, "What they said", C_TEXT, 255);
	int yy = y + h - 28;
	for (int i = cl_n - 1; i >= 0 && yy > y + 44; i--) {
		static const u32 AC[6] = { RGB(190, 130, 240), RGB(240, 190, 90), RGB(150, 170, 200), RGB(90, 220, 210), RGB(190, 230, 255), RGB(240, 90, 100) };
		char L[6][64]; int n = wrap_lines(CL[i].t, w - 150, L, 6);
		yy -= n * 17 + 6;
		if (yy < y + 44) break;
		text(F_SMALL, x + 16, yy, WHO_NAME[CL[i].who % 6], AC[CL[i].who % 6], 255);
		for (int k = 0; k < n; k++) text(F_SMALL, x + 130, yy + k * 17, L[k], C_TEXT, 255);
	}
	if (!cl_n) text_c(F_SMALL, 320, 200, "Nobody has said anything yet.", C_DIM, 255);
	text_c(F_SMALL, 320, y + h - 24, "B or - to close", C_DIM, 255);
	if (!blocked && (in->down & (BTN_B | BTN_MINUS | BTN_HOME | BTN_A))) { cl_open = 0; in->down = 0; }
}
/* ---- a gentle nudge to take a break ---- */
static void break_reminder(void)
{
	static u64 next; static int was;
	if (!S.brk) { next = 0; return; }
	u64 now = plat_ms(), every = (u64)(S.brk == 1 ? 30 : 60) * 60000;
	if (!next || was != S.brk) { next = now + every; was = S.brk; }
	if (now >= next) {
		next = now + every;
		char t[80]; snprintf(t, sizeof t, "You've been playing a while. Maybe stretch your legs?");
		ui_toast(t);
		if (S.quips >= 2) quip("Go on. Stretch. The cards will wait. I will wait. I always wait.");
	}
}
static int floor_backroom(void);
static void casino_audio(void)
{
	int m = S.snd & 3;
	if (!m) { music_play(-1, 0, 0); plat_music(0); return; }
	music_play(m >= 2 ? (view == V_LIAR || view == V_HOLDEM || (view == V_FLOOR && floor_backroom()) ? 2 : sec ? 1 : 0) : -1, m == 2 ? 1 : 2, 1);
	music_levels(S.vol_m, S.vol_v, S.vol_f);
	music_mood(creeping() ? 1.0f : 0.0f);
	plat_music(1);
	plat_music_pump();
}

void scr_casino(void)
{
	int blocked = help_is_open() || opt2_open || cl_open || bigc_open;
	if (blocked) ui_suspend(1);
	u64 now = plat_ms();
	ff_held = !blocked && (in->held & BTN_B) && cas_busy();   /* hold B to hurry things along */
	/* any button finishes the line the dealer is saying */
	if (!blocked && q_text[0] && in->down) {
		u64 full = type_ms(q_text);
		if (now < q_at + full) q_at = now > full ? now - full : 0;
	}
	/* hold HOME to leave the casino from anywhere (between rounds) */
	{
		static u64 home_t;
		if (!blocked && (in->held & BTN_HOME) && view != V_SUMMARY) {
			if (!home_t) home_t = now;
			else if (now - home_t > 700 && !cas_busy()) { home_t = 0; in->down = 0; leave_casino(); if (screen != S_CASINO) return; }
		} else home_t = 0;
	}
	casino_audio();
	if (view == V_DECKS || view == V_STATS || view == V_SUMMARY) gfx_bg(); else carpet_draw();
	if (view == V_TABLE) table_screen(blocked);
	else if (view == V_DECKS) decks(blocked);
	else if (view == V_STATS) stats_view(blocked);
	else if (view == V_ROUL) roulette(blocked);
	else if (view == V_SLOTS) slots(blocked);
	else if (view == V_POKER) poker(blocked);
	else if (view == V_HILO) hilo_game(blocked);
	else if (view == V_BACC) baccarat(blocked);
	else if (view == V_LIAR) liar_game(blocked);
	else if (view == V_WAR) casino_war(blocked);
	else if (view == V_BIGWHEEL) big_wheel(blocked);
	else if (view == V_KENO) keno(blocked);
	else if (view == V_HOLDEM) holdem_game(blocked);
	else if (view == V_SUMMARY) summary_screen(blocked);
	else floor_screen(blocked);
	if (screen != S_CASINO) return;
	if (!blocked && (in->down & BTN_MINUS)) { cl_open = 1; in->down &= ~BTN_MINUS; }   /* - reads the chat log (when there's no chip to undo) */
	if (view != V_SUMMARY && view != V_STATS && view != V_DECKS) { parts_draw(); win_banner_draw(); }
	if (opt_open) options_popup();
	else if (opt2_open) options2_popup();
	big_check_popup();
	chatlog_draw(blocked);
	break_reminder();
	/* stop-loss / win goal reached */
	if (goal_open && !opt_open) {
		static const char *GI[2] = { "Leave the casino", "Keep playing" };
		static const int GC[2] = { IC_CHECK, IC_PLAY };
		int was = goal_open;
		int k = menu_popup(&goal_open, GI, GC, 2, was == 1 ? "Stop-loss reached" : "Win goal reached!");
		if (k == 0) { leave_casino(); return; }
		if (k == 1 || !goal_open) goal_open = 0;
	}
	/* How to play: its own page for each game */
	static const char *H_FLOOR[] = {
		"Two areas: the Main Floor, and the High Roller Suite (2,500 chips to get in). Your chips work everywhere.",
		"Pick a game. Each one has its own dealer. Phantom Moth runs the place.",
		"+ = options: dealer chat, sound and music, stop-loss, win goal, deal speed, card sets.",
		"Playing: Player 1 / 2 = two people, separate chips. Hold HOME anywhere to leave.",
		"Free chips every day. Run out? Get 1,000 more for free.",
		"Phantom Moth reads your Wii fresh each visit. Nothing leaves the Wii. vex.txt only keeps topic numbers and the score.",
	};
	static const char *H_BJ[] = {
		"Get closer to 21 than the dealer without going over. Picture cards are 10, an Ace is 1 or 11.",
		"Tap chips to build your bet, then Deal. 1 = deal again with the same bet.",
		"Hit = another card. Stand = stop. Double = double the bet and get exactly one card.",
		"Split = a pair becomes 2 hands (up to 4). Split Aces get one card each.",
		"Give up = get half your bet back. Only at the start, at Classic and High Roller.",
		"Dealer shows an Ace: Insurance pays 2 to 1 if the dealer has blackjack.",
		"+ = strategy hints (gold outline) and the card count trainer.",
		"Card count: +1 for 2 to 6, -1 for 10 to Ace. A high true count is good for you.",
	};
	static const char *H_ROUL[] = {
		"Pick a chip, then put it on numbers or outside bets. Put as many as you like.",
		"One number pays 35 to 1. Red, black, odd, even, 1-18, 19-36 pay 1 to 1. Dozens pay 2 to 1.",
		"Spin (or 1). Zero is green: outside bets lose on zero.",
		"Rebet puts last spin's chips back. Auto repeats your bet 10 times.",
	};
	static const char *H_SLOTS[] = {
		"Pick your bet, then Spin (or 1). Three of a kind on the red line wins.",
		"The pay table is on the left. Three sevens is the jackpot.",
		"Any mix of 7, diamond and star pays 10x. Cherries pay on their own too.",
		"Auto keeps spinning until you turn it off (or hit your stop-loss).",
	};
	static const char *H_POKER[] = {
		"Deal 5 cards. Pick the ones to keep (they say HELD), then Draw.",
		"The cards you didn't keep are replaced. Your final hand pays from the table.",
		"You need at least a pair of Jacks to win anything.",
		"Double Bonus (in the Suite) pays extra for four of a kind, most for four Aces.",
	};
	static const char *H_HILO[] = {
		"Pick a stake and Start. Guess if the next card is higher or lower.",
		"Aces are high, 2s are low. Riskier guesses grow the pot more.",
		"Same card = nothing changes. Wrong guess = you lose the pot.",
		"Cash out any time to keep the pot.",
	};
	static const char *H_LIAR[] = {
		"Everyone has dice under a cup. You only see yours. Shake the Wii Remote (or A) to roll.",
		"Bid how many of one face are under ALL the cups, like four fives.",
		"Each bid must go higher: more dice, or the same number of a higher face.",
		"Don't believe it? Call Liar! (1). Wrong bid: the bidder loses a die. Right: you do.",
		"Spot on (2): the bid is exactly right. Right = win a die back. Wrong = lose one.",
		"Ones are wild. Palifico: at one die, the next round has no wilds and one face.",
		"Up / Down = how many. The dice = which face. + = bid. Last cup standing wins the pot.",
		"Everyone has a tell when they bluff. It's small. Watch how they talk, and how long they take.",
	};
	static const char *H_HOLDEM[] = {
		"You get 2 secret cards. 5 shared cards come out: 3, then 1, then 1.",
		"Best 5-card hand from your 2 and the 5 shared wins. Or be the last one who didn't fold.",
		"Fold = give up. Check = bet nothing. Call = match the bet. Raise = bet more.",
		"The blinds are forced bets that start each pot. They go up every 8 hands.",
		"Best to worst: straight flush, four of a kind, full house, flush, straight, three, two pair, pair.",
		"Lose all your chips and you're out. Last one with chips wins the prize.",
		"1 = check or call, 2 = raise, Up / Down = raise size.",
		"They all play differently, remember what you did, and have tells. Small ones.",
	};
	static const char *H_BACC[] = {
		"Bet on Player, Banker or Tie. The hand closest to 9 wins.",
		"Cards count their number. 10s and picture cards count 0. Only the last digit counts.",
		"The cards draw by fixed rules. You just watch.",
		"Player pays 1 to 1, Banker 0.95 to 1, Tie 8 to 1.",
	};
	if (opt_open || goal_open || tb_hand >= 0) return;   /* not while the remote is changing hands */
	switch (view) {
	case V_TABLE: ui_help("How to play: Blackjack", H_BJ, 8); break;
	case V_ROUL: ui_help("How to play: Roulette", H_ROUL, 4); break;
	case V_SLOTS: ui_help("How to play: Slots", H_SLOTS, 4); break;
	case V_POKER: ui_help("How to play: Video Poker", H_POKER, 4); break;
	case V_HILO: ui_help("How to play: Hi-Lo", H_HILO, 4); break;
	case V_BACC: ui_help("How to play: Baccarat", H_BACC, 4); break;
	case V_LIAR: ui_help("How to play: Liar's Dice", H_LIAR, 8); break;
	case V_WAR: { static const char *H[] = { "You and the dealer get one card each. Higher card wins, even money. Aces are high.", "A tie: go to war (bet the same again) or give up half your bet.", "At war: three cards are burned, one each. You win or tie: you win. Lower: you lose both bets.", "Tie bet (optional): pays 10 to 1 when the first two cards tie." }; ui_help("How to play: Casino War", H, 4); break; }
	case V_BIGWHEEL: { static const char *H[] = { "Put chips on the symbols. The wheel has 54 slots: lots of 1s, fewer of the rest.", "It pays what the symbol says: 1 pays 1 to 1, 20 pays 20 to 1. Joker and Star pay 40 to 1.", "Swing the Wii Remote to spin it. A harder swing spins it faster. Or press 1.", "The flapper at the top picks the winner." }; ui_help("How to play: the Big Wheel", H, 4); break; }
	case V_KENO: { static const char *H[] = { "Pick 1 to 10 numbers from 80. Then Play: 20 balls are drawn.", "The more of your numbers come up, the more you win. The pay table shows what your picks pay.", "Quick pick chooses for you. Play again with the same numbers: just press Play.", "Picking 10 and hitting none still pays 2x." }; ui_help("How to play: Keno", H, 4); break; }
	case V_HOLDEM: ui_help("How to play: Texas Hold'em", H_HOLDEM, 8); break;
	case V_FLOOR: ui_help("Casino", H_FLOOR, 6); break;
	default: break;
	}
}

#ifdef HOST
/* test rig: stack the shoe (the next cards dealt, in order) */
void casino_rig(const u8 *c, int n) { for (int i = 0; i < n; i++) shoe[shoe_pos + i] = c[i]; }
int casino_state(void) { return g; }
#endif
