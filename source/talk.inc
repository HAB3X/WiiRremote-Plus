/*
   talk.inc - how the dealers talk. Nothing here is a fixed script:
   every line is built on the spot from pieces ([this|that] choices and {slots} filled from
   what just happened), picked by who is talking, their mood, and what happened tonight.
   Each dealer has a mood that drifts, a few stories they tell over a visit, and a memory of
   the night (your busts, your best win, what you did at the other tables).
   Included in casino.c. WiiRremote Plus by Habex. GPL-3.0.
*/

enum { W_MOTH, W_LYNX, W_CROW, W_SQUID, W_VIXEN, W_VIPER, W_N, W_ANY = 9 };
static const char *const WHO_NAME[W_N] = { "Phantom Moth", "Trigger Lynx", "Thunder Crow", "Mirror Squid", "Silent Vixen", "Crimson Viper" };
static const char *const WHO_GAME[W_N] = { "blackjack table", "roulette wheel", "slot machines", "poker machine", "Hi-Lo table", "baccarat table" };
static int who_now(void) { return persona + 1; }   /* persona -1 = Phantom Moth */
void talk_expand(char *out, int n, const char *t);
static const char *bj_hint(void);

/* ------------------------------------------------------------------ */
/* what just happened (the games fill this in before a round ends)     */
/* ------------------------------------------------------------------ */
enum {
	/* results and details */
	F_BJ = 1 << 0, F_BUST = 1 << 1, F_DBUST = 1 << 2, F_DBJ = 1 << 3, F_DOUBLE = 1 << 4, F_SPLIT = 1 << 5, F_SURR = 1 << 6,
	F_CLOSE = 1 << 7, F_HUGE = 1 << 8, F_STREAK = 1 << 9, F_DSTREAK = 1 << 10, F_COMEBACK = 1 << 11, F_AGAIN = 1 << 12,
	F_SPECIAL = 1 << 13, F_ZERO = 1 << 14, F_NEAR = 1 << 15, F_TIE = 1 << 16, F_CASH = 1 << 17, F_MINOR = 1 << 18,
	/* while dealing and deciding (blackjack) */
	F_PAIR = 1 << 19, F_TWENTY = 1 << 20, F_LOW = 1 << 21, F_UPACE = 1 << 22, F_UPWEAK = 1 << 23, F_STIFF = 1 << 24,
	F_WRONG = 1 << 25, F_HITHIGH = 1 << 26,
	/* greetings */
	F_FIRST = 1 << 19, F_BACK = 1 << 20, F_RETURN = 1 << 21, F_RELHI = 1 << 22, F_RELLO = 1 << 23, F_GUP = 1 << 24, F_GDOWN = 1 << 25,
	/* waiting */
	F_LONG = 1 << 26, F_LATE = 1 << 27, F_MORNING = 1 << 28, F_BROKE = 1 << 29, F_RICH = 1 << 30,
	/* memory of tonight */
	M_PEAK = 1 << 0, M_HANDS = 1 << 1, M_BEST = 1 << 2, M_BUSTS = 1 << 3, M_H10 = 1 << 8, M_H25 = 1 << 9, M_H50 = 1 << 10,
	/* Phantom Moth's little slips (what he could know) */
	/* who a table line is aimed at (Liar's Dice, Hold'em) */
	TG_MOTH = 1 << 20, TG_YOU = 1 << 26, TG_ALL = 0x7F << 20,
	/* Liar's Dice */
	LF_BLUFF = 1 << 0, LF_JUMP = 1 << 1, LF_ONES = 1 << 2, LF_CLOSE = 1 << 3, LF_WAYOFF = 1 << 4, LF_LAST = 1 << 5, LF_FEW = 1 << 6,
	LF_REVENGE = 1 << 7, LF_OPEN = 1 << 8, LF_LIAR = 1 << 9, LF_LEADER = 1 << 10, LF_SLOW = 1 << 11, LF_HIGH = 1 << 12, LF_SAFE = 1 << 13,
	LF_PALI = 1 << 14, LF_FAST = 1 << 15,
	H_NAME = 1 << 0, H_SAVES = 1 << 1, H_BACKUP = 1 << 2, H_MANY = 1 << 3, H_FEW = 1 << 4, H_ONLINE = 1 << 5, H_FIT = 1 << 6, H_LAST = 1 << 7, H_EMPTY = 1 << 8,
};
static struct {
	int game, flags;
	int pv, dv, pb;             /* totals: yours, the dealer's, yours before the last card */
	int num, run;               /* roulette number, poker rank, Hi-Lo run */
	char what[40], card[24], sym[16], hand[24];
	int up;                     /* dealer's up card rank (2..11) */
} ev;

/* ------------------------------------------------------------------ */
/* tonight: each dealer's mood and memory for this visit               */
/* ------------------------------------------------------------------ */
typedef struct {
	int met, hands, wins, losses, busts, bjs, net, best, pst, dst, loss_run, quiet_run, said_since_story;
	float mood, base, chat;
	int thread, tpos, threads_done;
	u32 once;                  /* memory remarks already made */
	int gossiped;
} Tonight;
static Tonight TN[W_N];
static int sess_peak, last_who = -1;
static u64 wait_since;
static u32 trng = 777;
static int tr(int n) { trng = trng * 1103515245u + 12345u; return n > 0 ? (int)((trng >> 16) % (u32)n) : 0; }
static int chance(int pct) { return tr(100) < pct; }

/* each dealer: how often they talk, add openers and endings, call you by name, and how fast they get angry */
static const struct { u8 chat, open, tag, addr; float temper; const char *nick; } VOICE[W_N] = {
	{ 80, 30, 22, 55, 1.0f, "[{N}|{N}|{N}|my friend]" },
	{ 85, 45, 30, 60, 0.8f, "[{N}|partner|partner|kid|{N}]" },
	{ 60, 30, 18, 40, 0.6f, "[little one|little one|{N}]" },
	{ 95, 50, 35, 60, 0.7f, "[{N}|friend|pal|darling|{N}]" },
	{ 45, 12, 10, 25, 0.5f, "[{N}]" },
	{ 75, 35, 28, 50, 1.2f, "[brother|brother|{N}]" },
};

/* ------------------------------------------------------------------ */
/* words                                                              */
/* ------------------------------------------------------------------ */
static const char *const NUMW[] = { "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve",
	"thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty" };
static void numw(char *o, int n, int v)
{
	static const char *const TENS[] = { "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety" };
	if (v < 0) v = -v;
	if (v <= 20) snprintf(o, n, "%s", NUMW[v]);
	else if (v < 100) snprintf(o, n, "%s%s%s", TENS[v / 10], v % 10 ? "-" : "", v % 10 ? NUMW[v % 10] : "");
	else snprintf(o, n, "%d", v);
}
static const char *const ORD[] = { "zeroth", "first", "second", "third", "fourth", "fifth", "sixth", "seventh", "eighth", "ninth", "tenth" };
static const char *const RANKN[13] = { "ace", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "jack", "queen", "king" };
static const char *const RANKP[13] = { "aces", "twos", "threes", "fours", "fives", "sixes", "sevens", "eights", "nines", "tens", "jacks", "queens", "kings" };
static void card_name(char *o, int n, u8 c) { int r = c % 13; snprintf(o, n, "%s %s", (r == 0 || r == 7) ? "an" : "a", RANKN[r]); }

/* the time of day, from the clock the app shows */
static int clock_hour(void)
{
	char b[24]; plat_clock(b, sizeof b);
	int h = atoi(b);
	if (strstr(b, "PM") && h < 12) h += 12;
	if (strstr(b, "AM") && h == 12) h = 0;
	return h;
}

/* ------------------------------------------------------------------ */
/* the line builder: [a|b|c] picks one, {slot} fills in a detail      */
/* ------------------------------------------------------------------ */
static int speaker;          /* who is talking right now */
static int addr_used;
static int ctx_n;            /* a count for {n} and {ord} */
static int ctx_mem;          /* chips for {peak} / {best} */
static int ctx_net;
static const char *ctx_act = "hit", *ctx_acting = "hitting";
static int ctx_other = -1, ctx_gnet;
static const char *ctx_tgt = ""; static int ctx_q, ctx_f, ctx_cnt, ctx_amt;
static const char *ctx_hand = "";
static void slot(const char *k, char *o, int n)
{
	o[0] = 0;
	if (!strcmp(k, "N")) snprintf(o, n, "%s", nm());
	else if (!strcmp(k, "A")) {
		/* calling you by name, now and then, once a line at most */
		if (addr_used || !chance(VOICE[speaker].addr)) return;
		addr_used = 1;
		char a[48];
		talk_expand(a, sizeof a, VOICE[speaker].nick);
		snprintf(o, n, ", %s", a);
	}
	else if (!strcmp(k, "pv")) numw(o, n, ev.pv);
	else if (!strcmp(k, "dv")) numw(o, n, ev.dv);
	else if (!strcmp(k, "pb")) numw(o, n, ev.pb);
	else if (!strcmp(k, "wv")) numw(o, n, ev.pv > ev.dv ? ev.pv : ev.dv);
	else if (!strcmp(k, "lv")) numw(o, n, ev.pv > ev.dv ? ev.dv : ev.pv);
	else if (!strcmp(k, "num")) numw(o, n, ev.num);
	else if (!strcmp(k, "run")) numw(o, n, ev.run);
	else if (!strcmp(k, "what")) snprintf(o, n, "%s", ev.what);
	else if (!strcmp(k, "card")) snprintf(o, n, "%s", ev.card);
	else if (!strcmp(k, "sym")) snprintf(o, n, "%s", ev.sym);
	else if (!strcmp(k, "hand")) snprintf(o, n, "%s", ev.hand);
	else if (!strcmp(k, "aup")) { int u = ev.up == 11 ? 0 : ev.up - 1; if (u < 0 || u > 12) u = 0; snprintf(o, n, "%s %s", (u == 0 || u == 7) ? "an" : "a", RANKN[u]); }
	else if (!strcmp(k, "prank")) snprintf(o, n, "%s", ev.num >= 0 && ev.num < 13 ? RANKP[ev.num] : "cards");
	else if (!strcmp(k, "net")) snprintf(o, n, "%d", ctx_net < 0 ? -ctx_net : ctx_net);
	else if (!strcmp(k, "pst")) numw(o, n, TN[speaker].pst);
	else if (!strcmp(k, "dst")) numw(o, n, TN[speaker].dst);
	else if (!strcmp(k, "n")) numw(o, n, ctx_n);
	else if (!strcmp(k, "ord")) snprintf(o, n, "%s", ctx_n >= 0 && ctx_n <= 10 ? ORD[ctx_n] : "umpteenth");
	else if (!strcmp(k, "hands")) numw(o, n, TN[speaker].hands);
	else if (!strcmp(k, "mem")) snprintf(o, n, "%d", ctx_mem);
	else if (!strcmp(k, "chips")) snprintf(o, n, "%d", cfg.chips);
	else if (!strcmp(k, "mins")) snprintf(o, n, "%d", sess_t0 ? (int)((plat_ms() - sess_t0) / 60000) : 0);
	else if (!strcmp(k, "act")) snprintf(o, n, "%s", ctx_act);
	else if (!strcmp(k, "acting")) snprintf(o, n, "%s", ctx_acting);
	else if (!strcmp(k, "other")) snprintf(o, n, "%s", ctx_other >= 0 ? WHO_NAME[ctx_other] : "someone");
	else if (!strcmp(k, "ogame")) snprintf(o, n, "%s", ctx_other >= 0 ? WHO_GAME[ctx_other] : "tables");
	else if (!strcmp(k, "gnet")) snprintf(o, n, "%d", ctx_gnet);
	else if (!strcmp(k, "tod")) { int h = clock_hour(); snprintf(o, n, "%s", h >= 5 && h < 12 ? "this morning" : h >= 12 && h < 17 ? "this afternoon" : "tonight"); }
	else if (!strcmp(k, "game")) snprintf(o, n, "%s", persona >= 0 ? WHO_GAME[persona + 1] : WHO_GAME[0]);
	else if (!strcmp(k, "save")) snprintf(o, n, "%s", D.nsave_named ? D.save_names[tr(D.nsave_named)] : "your games");
	else if (!strcmp(k, "miis")) numw(o, n, D.miis);
	else if (!strcmp(k, "tgt")) snprintf(o, n, "%s", ctx_tgt);
	else if (!strcmp(k, "q")) numw(o, n, ctx_q);
	else if (!strcmp(k, "cnt")) numw(o, n, ctx_cnt);
	else if (!strcmp(k, "f")) { static const char *const FP[7] = { "", "ones", "twos", "threes", "fours", "fives", "sixes" }; snprintf(o, n, "%s", FP[ctx_f > 0 && ctx_f < 7 ? ctx_f : 1]); }
	else if (!strcmp(k, "f1")) { static const char *const FS[7] = { "", "one", "two", "three", "four", "five", "six" }; snprintf(o, n, "%s", FS[ctx_f > 0 && ctx_f < 7 ? ctx_f : 1]); }
	else if (!strcmp(k, "amt")) snprintf(o, n, "%d", ctx_amt);
	else if (!strcmp(k, "hnd")) snprintf(o, n, "%s", ctx_hand);
	else if (!strcmp(k, "last")) snprintf(o, n, "%s", D.last_played[0] ? D.last_played : "something");
}

/* expand a template into out */
static const char *tx_alt(const char *p, char *o, int *k, int n, int depth);
static const char *tx_run(const char *p, char *o, int *k, int n, int depth, int stop_at_bar)
{
	while (*p) {
		if (stop_at_bar && (*p == '|' || *p == ']')) return p;
		if (*p == '[') { p = tx_alt(p + 1, o, k, n, depth + 1); continue; }
		if (*p == '{') {
			const char *e = strchr(p, '}');
			if (!e) break;
			char key[16]; int kl = (int)(e - p - 1); if (kl > 15) kl = 15;
			memcpy(key, p + 1, kl); key[kl] = 0;
			char v[96]; slot(key, v, sizeof v);
			for (char *q = v; *q && *k < n - 1; q++) o[(*k)++] = *q;
			p = e + 1; continue;
		}
		if (*k < n - 1) o[(*k)++] = *p;
		p++;
	}
	return p;
}
/* inside [ ... ]: count the choices at this level, pick one, skip the rest */
static const char *tx_alt(const char *p, char *o, int *k, int n, int depth)
{
	int cnt = 1, lvl = 0;
	for (const char *q = p; *q; q++) {
		if (*q == '[') lvl++;
		else if (*q == ']') { if (!lvl) break; lvl--; }
		else if (*q == '|' && !lvl) cnt++;
	}
	int want = tr(cnt), idx = 0;
	for (;;) {
		if (idx == want) p = tx_run(p, o, k, n, depth, 1);
		else {   /* skip this choice */
			int l = 0;
			while (*p && !(l == 0 && (*p == '|' || *p == ']'))) { if (*p == '[') l++; else if (*p == ']') l--; p++; }
		}
		if (*p == '|') { p++; idx++; continue; }
		if (*p == ']') p++;
		return p;
	}
}
/* tidy: spaces, stray commas, capitals where a sentence starts */
static void tidy(char *s)
{
	char *w = s;
	for (char *r = s; *r; r++) {
		if (*r == ' ' && (w == s || w[-1] == ' ')) continue;
		if ((*r == ',' || *r == '!' || *r == '?' || (*r == '.' && r[1] != '.')) && w > s && w[-1] == ' ') w--;
		if (*r == ',' && w > s && (w[-1] == ',' || w[-1] == '.' || w[-1] == '!' || w[-1] == '?')) continue;
		*w++ = *r;
	}
	while (w > s && w[-1] == ' ') w--;
	*w = 0;
	int start = 1;
	for (char *r = s; *r; r++) {
		if (start && *r >= 'a' && *r <= 'z' && !(r[1] >= 'A' && r[1] <= 'Z')) { *r -= 32; start = 0; }
		else if (*r != ' ' && *r != '"') start = 0;
		if (*r == ')' && r[1] == ' ') start = 1;
		if ((*r == '.' || *r == '!' || *r == '?') && r[1] == ' ') {
			int ellipsis = *r == '.' && r > s && r[-1] == '.';
			if (!ellipsis) start = 1;
		}
	}
}
void talk_expand(char *out, int n, const char *t)
{
	int k = 0;
	tx_run(t, out, &k, n, 0, 0);
	out[k] = 0;
}

/* ------------------------------------------------------------------ */
/* the lines                                                          */
/* ------------------------------------------------------------------ */
enum { I_HELLO, I_WIN, I_LOSE, I_PUSH, I_DEALT, I_DECIDE, I_IDLE, I_OPEN, I_TAG, I_AFTER, I_MEM, I_HINT, I_MASK, I_READIN,
       I_CUTIN, I_QWIN, I_QLOSE, I_QPUSH, I_FLOOR, I_RESUME, I_BJSIT,
       /* Liar's Dice */
       I_LD_HELLO, I_LD_OPEN, I_LD_BID, I_LD_TELL, I_LD_CALL, I_LD_SPOT, I_LD_CALLED, I_LD_CAUGHT, I_LD_WRONG, I_LD_GLOAT, I_LD_WATCH,
       I_LD_OUT, I_LD_SEEOUT, I_LD_PALI, I_LD_WIN, I_LD_YOUWIN, I_LD_JAB, I_LD_REPLY, I_LD_PBID, I_LD_READ, I_LD_SPOTWIN, I_LD_SPOTFAIL,
       I_LD_ROLL, I_LD_WAIT, I_LD_YOUOUT,
       /* Texas Hold'em */
       I_HE_HELLO, I_HE_FOLD, I_HE_CHECK, I_HE_CALL, I_HE_BET, I_HE_RAISE, I_HE_ALLIN, I_HE_TELL, I_HE_WINPOT, I_HE_LOSEPOT, I_HE_SHOW,
       I_HE_BADBEAT, I_HE_BLUFFWIN, I_HE_SEEBLUFF, I_HE_OUT, I_HE_SEEOUT, I_HE_WIN, I_HE_YOUWIN, I_HE_JAB, I_HE_REPLY, I_HE_PACT,
       I_HE_FLOP, I_HE_WAIT, I_HE_READ, I_HE_YOUOUT, I_HE_MUCK,
       I_N };
typedef struct { u8 who, intent; s8 lo, hi; u32 cond; const char *t; u16 gk; } Line;
#define LN(w, i, lo, hi, c, t) { w, i, lo, hi, c, t, 0 }
#define LK(w, i, lo, hi, c, g, t) { w, i, lo, hi, c, t, g }   /* only at some games (it mentions the wheel, the machines...) */
enum { GK_BJ = 1, GK_WHEEL = 2, GK_SLOT = 4, GK_VP = 8, GK_HILO = 16, GK_BACC = 32, GK_DICE = 64, GK_LOTTO = 128,
       GK_TPOKER = 256, GK_WAR = 512, GK_BIGWHEEL = 1024, GK_TILES = 2048, GK_SPORT = 4096, GK_ROUL = 8192 };
static int talk_gk(void);
#include "talk_lines.inc"
#define NLINES (int)(sizeof LINES / sizeof LINES[0])

/* short stories each dealer tells over a visit, a piece at a time */
#include "talk_stories.inc"

static int recent[48], nrecent;
static int was_recent(int i) { for (int k = 0; k < nrecent; k++) if (recent[k] == i) return 1; return 0; }
static void mark_recent(int i) { if (nrecent == 48) { memmove(recent, recent + 1, 47 * sizeof(int)); nrecent = 47; } recent[nrecent++] = i; }

static int mood_i(int w)
{
	float m = TN[w].mood;
	return m <= -1.3f ? -2 : m <= -0.5f ? -1 : m < 0.5f ? 0 : m < 1.3f ? 1 : 2;
}
static int popc(u32 v) { int c = 0; while (v) { c += v & 1; v >>= 1; } return c; }

/* pick a line: the dealer's own lines first, the ones that fit what happened best */
static const Line *find_line(int w, int intent, u32 flags, int mood)
{
	int cand[96], wt[96], nc = 0, total = 0;
	for (int pass = 0; pass < 2 && !nc; pass++)
		for (int i = 0; i < NLINES && nc < 96; i++) {
			const Line *L = &LINES[i];
			if (L->intent != intent || (L->who != w && L->who != W_ANY)) continue;
			if (L->gk && !(L->gk & talk_gk())) continue;
			if (mood < L->lo || mood > L->hi) continue;
			if ((L->cond & flags) != L->cond) continue;
			if (!pass && was_recent(i)) continue;
			int x = (L->who == w ? 4 : 1) * (1 + 3 * popc(L->cond));
			cand[nc] = i; wt[nc] = x; total += x; nc++;
		}
	if (!nc) return NULL;
	/* the best fitting lines win most of the time */
	int best = 0; for (int i = 0; i < nc; i++) if (popc(LINES[cand[i]].cond) > best) best = popc(LINES[cand[i]].cond);
	if (best && chance(70)) {
		int nt = 0, tt = 0;
		for (int i = 0; i < nc; i++) if (popc(LINES[cand[i]].cond) == best) { cand[nt] = cand[i]; wt[nt] = wt[i]; tt += wt[i]; nt++; }
		nc = nt; total = tt;
	}
	int r = tr(total);
	for (int i = 0; i < nc; i++) { if (r < wt[i]) { mark_recent(cand[i]); return &LINES[cand[i]]; } r -= wt[i]; }
	mark_recent(cand[0]);
	return &LINES[cand[0]];
}

/* little human touches: trailing off, shouting, a pause */
static void humanize(char *s, int n, int w, int mood)
{
	int len = strlen(s);
	/* a pause: the first short sentence trails off into the next */
	if ((w == W_MOTH || w == W_CROW) && mood > -2 && chance(22)) {
		char *c = strstr(s, ". ");
		if (c && c - s > 2 && c - s < 26 && c[-1] != '.' && len + 3 < n) { memmove(c + 2, c, strlen(c) + 1); c[0] = '.'; c[1] = '.'; }
	}
	if (mood == -2) {
		/* furious: one word in capitals, and he means it */
		if (chance(55)) {
			int starts[40], ns = 0;
			for (int i = 0; s[i] && ns < 40; i++) if ((i == 0 || s[i - 1] == ' ') && s[i] != '{') {
				int l = 0; while (s[i + l] && s[i + l] != ' ' && s[i + l] != ',' && s[i + l] != '.' && s[i + l] != '!' && s[i + l] != '?') l++;
				if (l >= 4) starts[ns++] = i;
			}
			if (ns) { int i = starts[tr(ns)]; while (s[i] && s[i] != ' ' && s[i] != ',' && s[i] != '.' && s[i] != '!' && s[i] != '?') { if (s[i] >= 'a' && s[i] <= 'z') s[i] -= 32; i++; } }
		}
		if (len && s[len - 1] == '.' && !(len > 1 && s[len - 2] == '.') && chance(60)) s[len - 1] = '!';
	}
}

/* recently said words: the same opener, ending or whole line isn't used twice in a row */
static u32 said_hash[32]; static int nsaid;
static u32 hstr(const char *t) { u32 h = 2166136261u; for (; *t; t++) h = (h ^ (u8)*t) * 16777619u; return h; }
static int said_lately(const char *t) { u32 h = hstr(t); for (int i = 0; i < nsaid; i++) if (said_hash[i] == h) return 1; return 0; }
static void remember_said(const char *t) { if (nsaid == 32) { memmove(said_hash, said_hash + 1, 31 * sizeof(u32)); nsaid = 31; } said_hash[nsaid++] = hstr(t); }
static void expand_fresh(char *o, int n, const char *tmpl)
{
	for (int k = 0; k < 5; k++) { talk_expand(o, n, tmpl); if (!said_lately(o)) break; }
	remember_said(o);
}

/* build a whole line: maybe an opener, the line itself, maybe an ending */
static int compose(char *out, int n, int w, int intent, u32 flags, int allow_extras)
{
	int mood = mood_i(w);
	const Line *L = find_line(w, intent, flags, mood);
	if (!L) return 0;
	speaker = w; addr_used = 0;
	char core[240], op[64] = "", tg[64] = "";
	expand_fresh(core, sizeof core, L->t);
	if (allow_extras && S.quips >= 2) {
		if (chance(VOICE[w].open)) { const Line *O = find_line(w, I_OPEN, 0, mood); if (O) expand_fresh(op, sizeof op, O->t); }
		if (chance(VOICE[w].tag) && strlen(core) < 110) { const Line *T = find_line(w, I_TAG, 0, mood); if (T) expand_fresh(tg, sizeof tg, T->t); }
	}
	/* an opener like "Well," runs into the line: "Well, you win" not "Well, You win" */
	int ol = strlen(op);
	if (ol && op[ol - 1] == ',') {
		char *c = core; while (*c == ' ') c++;
		int wl = 0; while (c[wl] && c[wl] != ' ') wl++;
		int interj = wl && wl <= 8 && strchr(".!?", c[wl - 1]);
		if (interj || *c == '(' || *c == '"' || !strncmp(c, nm(), strlen(nm()))) op[ol - 1] = '.';
		else if (*c >= 'A' && *c <= 'Z' && !(c[1] >= 'A' && c[1] <= 'Z') && !(c[0] == 'I' && (c[1] == ' ' || c[1] == '\''))) *c += 32;
	}
	snprintf(out, n, "%s%s%s%s%s", op, op[0] ? " " : "", core, tg[0] ? " " : "", tg);
	tidy(out);
	humanize(out, n, w, mood);
	return 1;
}

/* ------------------------------------------------------------------ */
/* the speech queue: lines wait their turn, so nobody talks over       */
/* the line still being typed out                                     */
/* ------------------------------------------------------------------ */
typedef struct { char t[220]; s8 who; u8 creepy, shout; u64 at; } QLine;
static QLine tq[4]; static int tqn;
static void talk_push(const char *t, int who, int delay_ms, int creepy)
{
	if (!t || !t[0]) return;
#ifdef HOST
	{ extern int talk_print; if (talk_print) printf("  %-13s| %s\n", WHO_NAME[who < 0 ? 0 : who], t); }
#endif
	if (tqn == 4) { memmove(tq, tq + 1, 3 * sizeof(QLine)); tqn = 3; }
	QLine *q = &tq[tqn++];
	snprintf(q->t, sizeof q->t, "%s", t);
	q->who = who; q->creepy = creepy; q->shout = who >= 0 && mood_i(who) == -2;
	q->at = plat_ms() + (delay_ms > 0 ? delay_ms : 0);
	TN[who < 0 ? 0 : who].said_since_story++;
}
static void talk_clear(void) { tqn = 0; }
static void talk_pump(void)
{
	if (!tqn) return;
	/* a dealer only talks at their own table (the Moth can talk anywhere) */
	while (tqn && tq[0].who != W_MOTH && tq[0].who != who_now()) { memmove(tq, tq + 1, (tqn - 1) * sizeof(QLine)); tqn--; }
	if (!tqn) return;
	u64 now = plat_ms();
	if (now < tq[0].at) return;
	if (q_text[0]) { u64 done = q_at + type_ms(q_text) + 1400; if (now < done && now > q_at) return; }
	QLine *q = &tq[0];
	snprintf(q_text, sizeof q_text, "%s", q->t);
	q_at = now;
	q_who = WHO_NAME[q->who < 0 ? 0 : q->who];
	{	/* how fast each of them talks: the Moth and Crow take their time, Squid rattles on */
		static const u8 SPD[W_N] = { 34, 24, 40, 19, 33, 26 };
		q_widx = q->who < 0 ? 0 : q->who;
		q_speed = SPD[q_widx] * (q->shout ? 7 : 10) / 10;
		if (q->creepy) q_speed += 8;
		q_speed = q_speed * tspd_k() / 100;
		chatlog_add(q_widx, q->t);
	}
	if (q->creepy) { creep_at = now; plat_tone(55); }
	if (q->shout) plat_rumble(160);
	memmove(tq, tq + 1, (tqn - 1) * sizeof(QLine)); tqn--;
}
static void say_as(int w, int intent, u32 flags, int delay, int extras)
{
	char t[240];
	if (compose(t, sizeof t, w, intent, flags, extras)) talk_push(t, w, delay, 0);
}

/* ------------------------------------------------------------------ */
/* tonight                                                            */
/* ------------------------------------------------------------------ */
static void tonight_start(int w)
{
	Tonight *T = &TN[w];
	if (T->met) return;
	memset(T, 0, sizeof *T);
	T->met = 1;
	int rel = S.rel[w];
	T->base = (tr(100) - 50) / 100.0f - rel / 40.0f;   /* a good or bad night, and how you've treated them before */
	if (T->base > 1) T->base = 1; if (T->base < -1) T->base = -1;
	T->mood = T->base;
	T->chat = 0.7f + tr(60) / 100.0f;
	T->thread = tr(4);
	T->loss_run = 0;
}
static void mood_step(int w, int res, int big)
{
	Tonight *T = &TN[w];
	if (res > 0) T->mood -= (0.35f + (big ? 0.35f : 0)) * VOICE[w].temper;
	else if (res < 0) T->mood += 0.3f + (big ? 0.2f : 0);
	T->mood = T->mood * 0.92f + T->base * 0.08f;
	if (T->mood > 2) T->mood = 2; if (T->mood < -2) T->mood = -2;
	int m = mood_i(w);
	anger = m == -2 ? 2 : m == -1 && T->pst >= 2 ? 1 : 0;
}

/* a piece of tonight's story, if they're in the mood */
static int story_line(int w, char *t, int n)
{
	Tonight *T = &TN[w];
	if (T->threads_done >= 2 || mood_i(w) <= -1) return 0;
	const char *frag = T->tpos < 6 ? STORY[w][T->thread][T->tpos] : NULL;
	if (!frag) { T->threads_done++; T->thread = (T->thread + 1 + tr(3)) % 4; T->tpos = 0; frag = STORY[w][T->thread][0]; if (!frag || T->threads_done >= 2) return 0; }
	speaker = w; addr_used = 0;
	char body[220], lead[48] = "";
	talk_expand(body, sizeof body, frag);
	/* picking the story back up after something else was said */
	if (T->tpos > 0 && T->said_since_story > 0) { const Line *R = find_line(w, I_RESUME, 0, mood_i(w)); if (R) talk_expand(lead, sizeof lead, R->t); }
	snprintf(t, n, "%s%s%s", lead, lead[0] ? " " : "", body);
	tidy(t);
	T->tpos++;
	T->said_since_story = -1;
	return 1;
}

/* ------------------------------------------------------------------ */
/* sitting down                                                       */
/* ------------------------------------------------------------------ */
static void talk_sit(int w)
{
	vex_load();
	if (!S.quips) return;
	dealer_read();
	talk_clear();
	int again = TN[w].met;
	tonight_start(w);
	wait_since = plat_ms();
	if (S.quips == 1) { char t[80]; snprintf(t, sizeof t, "%s", w == W_MOTH ? "Welcome to the table." : "Welcome. Place your bets."); talk_push(t, w, 0, 0); return; }
	u32 f = 0;
	int rel = S.rel[w];
	if (again) f |= F_RETURN;
	else if (rel == 0 && M.visits <= 1) f |= F_FIRST;
	else f |= F_BACK;
	if (rel >= 15) f |= F_RELHI; else if (rel <= -15) f |= F_RELLO;
	/* gossip: what happened at another table tonight */
	ctx_other = -1;
	if (!again) {
		int best = 0;
		for (int k = 0; k < W_N; k++) if (k != w && TN[k].met && TN[k].hands >= 3 && abs(TN[k].net) >= 200 && abs(TN[k].net) > best && !(TN[w].gossiped & (1 << k))) { best = abs(TN[k].net); ctx_other = k; }
		if (ctx_other >= 0 && chance(70)) { ctx_gnet = best; f |= TN[ctx_other].net > 0 ? F_GUP : F_GDOWN; TN[w].gossiped |= 1 << ctx_other; }
		else ctx_other = -1;
	}
	char t[240];
	int ok = compose(t, sizeof t, w, I_HELLO, f, 0);
	if (!ok) { f &= ~(F_GUP | F_GDOWN | F_RELHI | F_RELLO); ok = compose(t, sizeof t, w, I_HELLO, f, 0); }
	if (!ok) ok = compose(t, sizeof t, w, I_HELLO, 0, 0);
	if (ok) talk_push(t, w, 250, 0);
	/* now and then a second thought right after */
	if (!again && chance(35)) { char s[220]; if (story_line(w, s, sizeof s)) talk_push(s, w, 900, 0); }
	last_who = w;
}

/* ------------------------------------------------------------------ */
/* Phantom Moth's reading (the mind-read part keeps its own pacing)    */
/* ------------------------------------------------------------------ */
static int moth_reading(void)
{
	char t[240];
	speaker = W_MOTH; addr_used = 0;
	if (phase == 2) {
		if (mind_step < n_reads) {
			int step = mind_step++;
			if (read_topic[step] == T_RUMBLE) { rum_at = plat_ms() + 4500; rum_left = dd_empty ? 1 : 6; }
			M.told |= 1 << read_topic[step]; vex_save();
			char body[240], lead[80] = "";
			talk_expand(body, sizeof body, reads[step]);
			if (step > 0 && chance(45)) { const Line *R = find_line(W_MOTH, I_READIN, dd_empty ? H_EMPTY : 0, 0); if (R) talk_expand(lead, sizeof lead, R->t); }
			snprintf(t, sizeof t, "%s%s%s", lead, lead[0] ? " " : "", body);
			tidy(t);
			talk_push(t, W_MOTH, 700, 1);
			return 1;
		}
		phase = 3;
	}
	if (phase < 2 && mind_step < n_reads) {
		Tonight *T = &TN[W_MOTH];
		int streak = T->pst >= 3 || T->dst >= 3;
		int trigger = (sess_hands >= 7 && streak && chance(50)) || (sess_hands >= 9 && chance(8)) || sess_hands >= 22;
		if (trigger) {
			phase = 2;
			if (compose(t, sizeof t, W_MOTH, I_MASK, dd_empty ? H_EMPTY : 0, 0)) { talk_push(t, W_MOTH, 700, 1); plat_rumble(200); }
			return 1;
		}
		/* before that: little slips. He knows things, then acts like he doesn't */
		if (sess_hands >= 3 && chance(28)) {
			u32 have = H_NAME;
			if (D.wii_saves) have |= H_SAVES;
			if (D.wii_backups + D.gc_backups) have |= H_BACKUP;
			if (D.miis > 2) have |= H_MANY; else if (D.miis) have |= H_FEW;
			if (D.online) have |= H_ONLINE;
			if (D.fit_found) have |= H_FIT;
			if (D.last_played[0]) have |= H_LAST;
			if (dd_empty) have = H_EMPTY;
			have &= ~(u32)hints_used;
			if (have) {
				/* one slip at a time: try each thing he could hint at, in a random order */
				int bits[9], nb = 0; for (int b = 0; b < 9; b++) if (have & (1u << b)) bits[nb++] = b;
				int b = bits[tr(nb)];
				const Line *L = find_line(W_MOTH, I_HINT, 1u << b, 0);
				if (L && L->cond) { hints_used |= L->cond; talk_expand(t, sizeof t, L->t); tidy(t); talk_push(t, W_MOTH, 900, 0); return 1; }
			}
		}
	}
	if ((phase >= 3 || (M.visits > 1 && M.told)) && chance(20) && callback_line(t, sizeof t)) {
		char x[240]; speaker = W_MOTH; addr_used = 0; talk_expand(x, sizeof x, t); tidy(x); talk_push(x, W_MOTH, 900, 1); return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* after every round, at every game                                   */
/* ------------------------------------------------------------------ */
static void talk_round(int res, int net, int stake)
{
	int w = who_now();
	vex_load();
	tonight_start(w);
	Tonight *T = &TN[w];
	T->hands++; T->net += net;
	if (res > 0) { T->wins++; T->pst++; T->dst = 0; if (net > T->best) T->best = net; }
	else if (res < 0) { T->losses++; T->dst++; T->pst = 0; }
	/* the casino's score against you, kept between visits (numbers only) */
	if (res > 0) { pw_streak++; dw_streak = 0; M.beat_him++; } else if (res < 0) { dw_streak++; pw_streak = 0; M.beat_you++; }
	M.net += net; vex_save();
	sess_hands++;
	if (cfg.chips > sess_peak) sess_peak = cfg.chips;
	int big = stake > 0 && (net >= stake * 3 || -net >= 400 || net >= 400);
	mood_step(w, res, big);
	wait_since = plat_ms();
	if (!S.quips) { ev.flags = 0; return; }

	/* the flags for what happened */
	u32 f = ev.flags;
	if (big) f |= F_HUGE;
	if (T->pst >= 3) f |= F_STREAK;
	if (T->dst >= 3) f |= F_DSTREAK;
	if (res > 0 && T->loss_run >= 3) f |= F_COMEBACK;
	T->loss_run = res < 0 ? T->loss_run + 1 : res > 0 ? 0 : T->loss_run;
	if (f & F_BUST) { T->busts++; if (T->busts >= 2) { f |= F_AGAIN; ctx_n = T->busts; } }
	if (f & F_BJ) { T->bjs++; if (T->bjs >= 2) { f |= F_AGAIN; ctx_n = T->bjs; } }
	ctx_net = net;
	int intent = res > 0 ? I_WIN : res < 0 ? I_LOSE : I_PUSH;

	if (S.quips == 1) {
		/* quiet: a word about the result, sometimes */
		if (chance(60)) say_as(w, res > 0 ? I_QWIN : res < 0 ? I_QLOSE : I_QPUSH, 0, 600, 0);
		ev.flags = 0;
		return;
	}
	if (w == W_MOTH) { dealer_read(); if (moth_reading()) { ev.flags = 0; return; } }

	char t[240];
	/* how you play: a remark saved up by style_note */
	if (st_pending >= 0 && chance(75) && style_line(t, sizeof t)) { char x[240]; speaker = w; addr_used = 0; talk_expand(x, sizeof x, t); tidy(x); talk_push(x, w, 700, 0); T->quiet_run = 0; ev.flags = 0; return; }
	/* something from earlier tonight */
	u32 mf = 0;
	if (sess_peak >= sess_start_chips + 300 && cfg.chips * 10 <= sess_peak * 6 && !(T->once & M_PEAK)) { mf |= M_PEAK; ctx_mem = sess_peak; }
	u32 hb = T->hands == 10 ? M_H10 : T->hands == 25 ? M_H25 : T->hands == 50 ? M_H50 : 0;
	if (hb && !(T->once & hb)) mf |= M_HANDS;
	if (T->best >= 200 && T->hands > T->wins + 4 && !(T->once & M_BEST) && res < 0) { mf |= M_BEST; ctx_mem = T->best; }
	if (T->busts >= 4 && !(T->once & M_BUSTS) && (f & F_BUST)) { mf |= M_BUSTS; ctx_n = T->busts; }
	if (mf && chance(55)) {
		u32 one = mf & (u32)-(s32)mf;   /* just one of them */
		if (compose(t, sizeof t, w, I_MEM, one, 1)) { T->once |= one == M_HANDS ? hb : one; talk_push(t, w, 700, 0); T->quiet_run = 0; ev.flags = 0; return; }
	}
	/* how much there is to say about this one */
	int sig = popc(f & (F_BJ | F_BUST | F_DBUST | F_DBJ | F_DOUBLE | F_SPLIT | F_SURR | F_CLOSE | F_HUGE | F_STREAK | F_DSTREAK | F_COMEBACK | F_SPECIAL | F_ZERO | F_NEAR | F_TIE | F_CASH));
	int quiet = sig ? 6 : 30;
	quiet = (int)(quiet * (80.0f / VOICE[w].chat) / T->chat);
	if (quiet > 70) quiet = 70;
	if (T->quiet_run >= 2) quiet = 0;
	if (chance(quiet)) {
		T->quiet_run++;
		/* nothing to say about the cards: maybe a bit of the story instead */
		if (!sig && chance(35) && story_line(w, t, sizeof t)) { talk_push(t, w, 900, 0); T->quiet_run = 0; }
		ev.flags = 0;
		return;
	}
	T->quiet_run = 0;
	if (compose(t, sizeof t, w, intent, f, 1)) {
		talk_push(t, w, 650, 0);
		/* an afterthought, a few seconds later */
		if (chance(12)) { char a[160]; if (compose(a, sizeof a, w, I_AFTER, 0, 0)) talk_push(a, w, 2200, 0); }
	}
	ev.flags = 0;
}

/* blackjack, while the hand is going: the first cards, and your choices */
static void talk_dealt(void)
{
	if (S.quips < 2 || !chance(24)) return;
	Hand *h = &ph[0];
	int soft, v = value_of(h->k, h->n, &soft);
	u32 f = 0;
	ev.pv = v; ev.up = rank10(dh[0].c);
	if (h->n == 2 && rank10(h->k[0].c) == rank10(h->k[1].c)) { f |= F_PAIR; ev.num = h->k[0].c % 13; }
	if (v == 20 && !soft) f |= F_TWENTY;
	if (v == 10 || v == 11) f |= F_LOW;
	if (ev.up == 11) f |= F_UPACE;
	if (ev.up >= 4 && ev.up <= 6) f |= F_UPWEAK;
	if (v >= 12 && v <= 16 && !soft && ev.up >= 7) f |= F_STIFF;
	if (!f) return;
	u32 one = f;   /* talk about one thing */
	u32 opts[8]; int no = 0; for (int b = 0; b < 32; b++) if (f & (1u << b)) opts[no++] = 1u << b;
	one = opts[tr(no)];
	say_as(W_MOTH, I_DEALT, one, 350, 0);
}
static void talk_decide(const char *act)
{
	if (S.quips < 2) return;
	const char *right = bj_hint();
	Hand *h = &ph[cur_h];
	int soft, v = value_of(h->k, h->n, &soft);
	ev.pv = v; ev.up = rank10(dh[0].c);
	u32 f = 0;
	if (!strcmp(act, "Double")) f |= F_DOUBLE;
	else if (!strcmp(act, "Split")) f |= F_SPLIT;
	if (strcmp(act, right)) f |= F_WRONG;
	if (!strcmp(act, "Hit") && !soft && v >= 15) f |= F_HITHIGH;
	int p = (f & F_WRONG) ? 38 : (f & (F_DOUBLE | F_SPLIT)) ? 30 : 0;
	if (!f || !chance(p)) return;
	static const char *const A[5][2] = { { "hit", "hitting" }, { "stand", "standing" }, { "double", "doubling" }, { "split", "splitting" }, { "give up", "giving up" } };
	int k = !strcmp(act, "Hit") ? 0 : !strcmp(act, "Stand") ? 1 : !strcmp(act, "Double") ? 2 : !strcmp(act, "Split") ? 3 : 4;
	ctx_act = A[k][0]; ctx_acting = A[k][1];
	u32 one = (f & F_WRONG) && chance(70) ? F_WRONG : (f & F_HITHIGH) ? F_HITHIGH : (f & F_DOUBLE) ? F_DOUBLE : (f & F_SPLIT) ? F_SPLIT : F_WRONG;
	say_as(W_MOTH, I_DECIDE, one, 200, 0);
}

/* ------------------------------------------------------------------ */
/* between rounds                                                     */
/* ------------------------------------------------------------------ */
static void talk_idle(void)
{
	if (S.quips < 2) return;
	int w = who_now();
	u64 now = plat_ms();
	static u64 next; static int next_for = -1;
	tonight_start(w);
	if (next_for != w || !next) { next = now + 12000 + tr(8000); next_for = w; }
	if (tqn || now < next || (q_text[0] && now < q_at + type_ms(q_text) + 7000)) return;
	next = now + (u64)((15000 + tr(14000)) / TN[w].chat);
	char t[240];
	/* Phantom Moth drops in on the other tables now and then */
	if (w != W_MOTH && chance(18)) { dealer_read(); if (compose(t, sizeof t, W_MOTH, I_CUTIN, 0, 0)) { talk_push(t, W_MOTH, 0, 0); return; } }
	if (w == W_MOTH) {
		dealer_read();
		if (phase >= 3 && chance(40) && callback_line(t, sizeof t)) { char x[240]; speaker = w; addr_used = 0; talk_expand(x, sizeof x, t); tidy(x); talk_push(x, w, 0, 1); return; }
	}
	if (chance(50) && story_line(w, t, sizeof t)) { talk_push(t, w, 0, 0); return; }
	u32 f = 0;
	if (now - wait_since > 45000) f |= F_LONG;
	int h = clock_hour();
	if (h >= 0 && h < 5) f |= F_LATE; else if (h >= 5 && h < 11) f |= F_MORNING;
	if (cfg.chips < 100) f |= F_BROKE; else if (cfg.chips > 10000) f |= F_RICH;
	/* talk about one thing at most */
	u32 opts[6]; int no = 0; for (int b = 26; b < 31; b++) if (f & (1u << b)) opts[no++] = 1u << b;
	u32 one = no && chance(60) ? opts[tr(no)] : 0;
	char x[240];
	if (compose(x, sizeof x, w, I_IDLE, one, 1)) talk_push(x, w, 0, 0);
}

/* back on the floor: Phantom Moth asks how it went */
static void talk_floor_return(int from)
{
	if (S.quips < 2 || from <= 0 || from >= W_N || !TN[from].met || TN[from].hands < 2) return;
	if (!chance(55)) return;
	ctx_other = from; ctx_gnet = abs(TN[from].net);
	u32 f = TN[from].net >= 150 ? F_GUP : TN[from].net <= -150 ? F_GDOWN : 0;
	say_as(W_MOTH, I_FLOOR, f, 500, 0);
}

/* picking a blackjack mode: he's already there, so no hello, just a word */
static void talk_bj_menu(void)
{
	if (!S.quips) return;
	dealer_read();
	tonight_start(W_MOTH);
	talk_clear();
	if (S.quips == 1) return;
	u32 f = TN[W_MOTH].hands ? F_RETURN : 0;
	say_as(W_MOTH, I_BJSIT, f, 0, 0);
}

/* leaving the casino: forget tonight */
static void talk_reset(void)
{
	tqn = 0; nrecent = 0; sess_peak = 0; last_who = -1;
	memset(TN, 0, sizeof TN);
}

#ifdef HOST
/* test: sit at a dealer and play made-up rounds, printing everything said */
int talk_print;
extern u64 sim_ms;
void casino_talk_test(int w, int rounds, u32 seed)
{
	trng = seed; rng = seed * 7 + 1;
	S.quips = 2;
	persona = w - 1;
	talk_print = 1;
	printf("=== %s ===\n", WHO_NAME[w]);
	talk_sit(w);
	for (int r = 0; r < rounds; r++) {
		sim_ms += 20000;
		tqn = 0; q_text[0] = 0;
		memset(&ev, 0, sizeof ev);
		int res = tr(100) < 45 ? 1 : tr(100) < 85 ? -1 : 0, stake = 25 + tr(4) * 25, net = res > 0 ? stake * (1 + (tr(10) == 0) * 4) : res < 0 ? -stake : 0;
		static const u32 FL[] = { F_BJ, F_BUST, F_DBUST, F_DBJ, F_DOUBLE, F_CLOSE, F_SPECIAL, F_ZERO, F_NEAR, F_MINOR, F_TIE, F_CASH, F_LONG, 0, 0, 0, 0 };
		u32 f = FL[tr(17)];
		if (res > 0) f &= ~(F_BUST | F_DBJ | F_ZERO | F_NEAR); else f &= ~(F_BJ | F_DBUST | F_CASH);
		ev.flags = f; ev.pv = 14 + tr(8); ev.dv = 17 + tr(5); ev.pb = 12 + tr(5); ev.num = tr(37); ev.run = 2 + tr(6); ev.up = 2 + tr(10);
		snprintf(ev.card, sizeof ev.card, "a seven"); snprintf(ev.what, sizeof ev.what, "three bells"); snprintf(ev.sym, sizeof ev.sym, "sevens"); snprintf(ev.hand, sizeof ev.hand, "three of a kind");
		cfg.chips += net;
		if (tr(4) == 0) st_pending = tr(SY_N);
		printf(" [round %d: %s %+d]\n", r + 1, res > 0 ? "win" : res < 0 ? "lose" : "push", net);
		talk_round(res, net, stake);
		if (w == W_MOTH && tr(3) == 0) { talk_dealt(); }
		if (tr(3) == 0) { sim_ms += 30000; tqn = 0; q_text[0] = 0; printf(" [waiting]\n"); talk_idle(); }
	}
	talk_print = 0;
}
#endif

static int talk_mood(int w) { return w >= 0 && w < W_N && TN[w].met ? mood_i(w) : 0; }

/* which kind of game we're at, so nobody talks about the wheel at the slot machines */
static int talk_gk(void)
{
	switch (view) {
	case V_TABLE: return GK_BJ;
	case V_ROUL: return GK_WHEEL | GK_ROUL;
	case V_SLOTS: return GK_SLOT;
	case V_POKER: return GK_VP;
	case V_HILO: return GK_HILO;
	case V_BACC: return GK_BACC;
	case V_WAR: return GK_WAR;
	case V_BIGWHEEL: return GK_BIGWHEEL | GK_WHEEL;
	case V_KENO: return GK_LOTTO;
	default: return persona < 0 ? GK_BJ : 0;
	}
}
