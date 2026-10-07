/*
   holdem.inc - TEXAS HOLD'EM, in the Back Room. A sit-and-go: you and up to five of the dealers
   start with the same stack, the blinds rise, last one with chips takes the prize.
   Each of them plays their own style, works out their odds by dealing out the rest of the hand
   a hundred-odd times in their head, remembers how you play, bluffs, has tells, and talks.
   Included in casino_games.inc (after table_ai.inc). WiiRremote Plus by Habex. GPL-3.0.
*/
enum { HS_DEAL, HS_BET, HS_SHOW, HS_AFTER, HS_OVER };
#define HE_STACK 1000
static struct {
	int stack[TB_MAX], bet[TB_MAX], in_pot[TB_MAX];
	int folded[TB_MAX], allin[TB_MAX], acted[TB_MAX], out[TB_MAX];
	u8 hole[TB_MAX][2], board[5]; int nboard;
	u8 deck[52]; int dp;
	int button, turn, street, cur_bet, min_raise, aggressor;
	int sb, bb, hands, level;
	int state; u64 act_at, state_at, turn_at, deal_t;
	int winner_mask, win_rank; char win_desc[40]; int won_amt;
	int raise_to;
	int buyin, done_paid, net, prize1, prize2;
	int out_order[TB_MAX], nout;
	/* what they've learned about everyone this game */
	int played[TB_MAX], raised[TB_MAX], dealt[TB_MAX], shown_bluff[TB_MAX], caught[TB_MAX];
	int bluffing[TB_MAX];        /* is their last bet a bluff (they know, you don't) */
	int tilt[TB_MAX];
	int fast, leave_open, you_slow, waited, you_folded_to_show;
	int moth_card;               /* which of your two cards the Moth has "seen" (-1 none) */
	int showed[TB_MAX];          /* cards turned up at the end */
	u8 last_act[TB_MAX]; char act_txt[TB_MAX][24]; u64 act_at_t[TB_MAX];
	int acted_cnt, played_mask, raised_mask, raise_prev;
} HE;

/* ---------------- hands ---------------- */
static int he_v(u8 c) { int r = c % 13; return r == 0 ? 14 : r + 1; }
static int he_s(u8 c) { return c / 13; }
/* score a 5-card hand: category in the top bits, then the ranks that break ties */
static int he_eval5(const u8 *c)
{
	int v[5], s0 = he_s(c[0]), flush = 1, cnt[15] = { 0 };
	for (int i = 0; i < 5; i++) { v[i] = he_v(c[i]); cnt[v[i]]++; if (he_s(c[i]) != s0) flush = 0; }
	for (int i = 0; i < 5; i++) for (int j = i + 1; j < 5; j++) if (v[j] > v[i]) { int t = v[i]; v[i] = v[j]; v[j] = t; }
	int straight = 0, top = 0;
	if (cnt[v[0]] == 1 && cnt[v[1]] == 1 && cnt[v[2]] == 1 && cnt[v[3]] == 1 && cnt[v[4]] == 1) {
		if (v[0] - v[4] == 4) { straight = 1; top = v[0]; }
		else if (v[0] == 14 && v[1] == 5 && v[4] == 2) { straight = 1; top = 5; }
	}
	/* order by how many of each, then by rank */
	int o[5], no = 0;
	for (int k = 4; k >= 1; k--) for (int r = 14; r >= 2; r--) if (cnt[r] == k) for (int i = 0; i < k; i++) o[no++] = r;
	int kick = (o[0] << 16) | (o[1] << 12) | (o[2] << 8) | (o[3] << 4) | o[4];
	int cat;
	if (straight && flush) { cat = 8; kick = top << 16; }
	else if (cnt[o[0]] == 4) cat = 7;
	else if (cnt[o[0]] == 3 && cnt[o[3]] == 2) cat = 6;
	else if (flush) cat = 5;
	else if (straight) { cat = 4; kick = top << 16; }
	else if (cnt[o[0]] == 3) cat = 3;
	else if (cnt[o[0]] == 2 && cnt[o[2]] == 2) cat = 2;
	else if (cnt[o[0]] == 2) cat = 1;
	else cat = 0;
	return (cat << 20) | kick;
}
/* the best 5 of 7 */
static int he_eval7(const u8 *c, int n)
{
	int best = 0; u8 h[5];
	if (n <= 5) { if (n < 5) return 0; return he_eval5(c); }
	for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) {
		int k = 0; for (int i = 0; i < n; i++) if (i != a && i != b) h[k++] = c[i];
		if (n == 6) { /* only drop one */ }
		int sc = he_eval5(h);
		if (sc > best) best = sc;
	}
	if (n == 6) { for (int a = 0; a < 6; a++) { int k = 0; for (int i = 0; i < 6; i++) if (i != a) h[k++] = c[i]; int sc = he_eval5(h); if (sc > best) best = sc; } }
	return best;
}
static const char *const HE_CAT[9] = { "high card", "a pair", "two pair", "three of a kind", "a straight", "a flush", "a full house", "four of a kind", "a straight flush" };
static int he_score(int s)
{
	u8 c[7]; int n = 0; c[n++] = HE.hole[s][0]; c[n++] = HE.hole[s][1];
	for (int i = 0; i < HE.nboard; i++) c[n++] = HE.board[i];
	return he_eval7(c, n);
}
static void he_desc(char *o, int n, int score)
{
	int cat = score >> 20, r = (score >> 16) & 15;
	static const char *const RN[15] = { "", "", "twos", "threes", "fours", "fives", "sixes", "sevens", "eights", "nines", "tens", "jacks", "queens", "kings", "aces" };
	if (cat == 1) snprintf(o, n, "a pair of %s", RN[r]);
	else if (cat == 3) snprintf(o, n, "three %s", RN[r]);
	else if (cat == 7) snprintf(o, n, "four %s", RN[r]);
	else if (cat == 8 && r == 14) snprintf(o, n, "a royal flush");
	else snprintf(o, n, "%s", HE_CAT[cat]);
}

/* ---------------- who's still in ---------------- */
static int he_in(int s) { return s >= 0 && s < tb_n && !HE.out[s]; }
static int he_live(int s) { return he_in(s) && !HE.folded[s]; }
static int he_can_act(int s) { return he_live(s) && !HE.allin[s]; }
static int he_nlive(void) { int n = 0; for (int s = 0; s < tb_n; s++) n += he_live(s); return n; }
static int he_nin(void) { int n = 0; for (int s = 0; s < tb_n; s++) n += he_in(s); return n; }
static int he_nact(void) { int n = 0; for (int s = 0; s < tb_n; s++) n += he_can_act(s); return n; }
static int he_next_in(int s) { for (int k = 1; k <= tb_n; k++) { int n = (s + k) % tb_n; if (he_in(n)) return n; } return s; }
static int he_next_act(int s) { for (int k = 1; k <= tb_n; k++) { int n = (s + k) % tb_n; if (he_can_act(n)) return n; } return -1; }
static int he_pot(void) { int p = 0; for (int s = 0; s < tb_n; s++) p += HE.in_pot[s]; return p; }

/* ---------------- how they play ---------------- */
static const struct { float tight, aggr, bluff, sticky, tell, fake_tell; int think; } HEP[W_N] = {
	{ 0.50f, 0.55f, 0.16f, 0.50f, 0.15f, 0.20f, 1900 },   /* Moth: balanced, and he peeks */
	{ 0.35f, 0.70f, 0.26f, 0.60f, 0.40f, 0.14f, 1000 },   /* Lynx: loose, loves action */
	{ 0.65f, 0.30f, 0.05f, 0.40f, 0.40f, 0.12f, 2400 },   /* Crow: tight, careful */
	{ 0.40f, 0.45f, 0.18f, 0.75f, 0.45f, 0.18f, 1100 },   /* Squid: calls everything, copies you */
	{ 0.68f, 0.72f, 0.08f, 0.35f, 0.36f, 0.10f, 2100 },   /* Vixen: tight, and dangerous when she bets */
	{ 0.30f, 0.85f, 0.30f, 0.55f, 0.42f, 0.18f, 1200 },   /* Viper: bullies with big bets */
};

/* how often they win from here: deal out the rest of the hand in their head, many times */
static float he_equity(int me, int sims)
{
	u8 used[52] = { 0 }, pool[52];
	used[HE.hole[me][0]] = used[HE.hole[me][1]] = 1;
	for (int i = 0; i < HE.nboard; i++) used[HE.board[i]] = 1;
	int known_opp = -1, known_card = 0;
	if (tb_who[me] == W_MOTH && HE.moth_card >= 0 && he_live(0)) { known_opp = 0; known_card = HE.hole[0][HE.moth_card]; used[known_card] = 1; }
	int np = 0; for (int c = 0; c < 52; c++) if (!used[c]) pool[np++] = c;
	int opp[TB_MAX], no = 0; for (int s = 0; s < tb_n; s++) if (s != me && he_live(s)) opp[no++] = s;
	if (!no) return 1;
	float win = 0;
	for (int k = 0; k < sims; k++) {
		/* shuffle just what we need */
		int need = (5 - HE.nboard) + no * 2;
		for (int i = 0; i < need && i < np; i++) { int j = i + tr(np - i); u8 t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
		int p = 0;
		u8 bd[5]; for (int i = 0; i < HE.nboard; i++) bd[i] = HE.board[i];
		u8 oh[TB_MAX][2];
		for (int o = 0; o < no; o++) {
			if (opp[o] == known_opp) { oh[o][0] = known_card; oh[o][1] = pool[p++]; }
			else { oh[o][0] = pool[p++]; oh[o][1] = pool[p++]; }
		}
		for (int i = HE.nboard; i < 5; i++) bd[i] = pool[p++];
		u8 c[7]; c[0] = HE.hole[me][0]; c[1] = HE.hole[me][1]; for (int i = 0; i < 5; i++) c[2 + i] = bd[i];
		int mine = he_eval7(c, 7), best = 0, ties = 0;
		for (int o = 0; o < no; o++) { c[0] = oh[o][0]; c[1] = oh[o][1]; int sc = he_eval7(c, 7); if (sc > best) { best = sc; ties = 0; } else if (sc == best) ties++; }
		if (mine > best) win += 1; else if (mine == best) win += 1.0f / (ties + 2);
	}
	return win / sims;
}

/* ---------------- dealing ---------------- */
static void he_new_hand(void)
{
	HE.hands++; S.he_hands++;
	if (HE.hands > 1 && (HE.hands - 1) % 8 == 0) { HE.level++; HE.sb = HE.sb * 3 / 2; HE.bb = HE.sb * 2; }
	for (int s = 0; s < tb_n; s++) { HE.bet[s] = HE.in_pot[s] = HE.folded[s] = HE.allin[s] = HE.acted[s] = 0; HE.bluffing[s] = 0; HE.showed[s] = 0; HE.act_txt[s][0] = 0; }
	for (int i = 0; i < 52; i++) HE.deck[i] = i;
	for (int i = 51; i > 0; i--) { int j = tr(i + 1); u8 t = HE.deck[i]; HE.deck[i] = HE.deck[j]; HE.deck[j] = t; }
	HE.dp = 0; HE.nboard = 0; HE.street = 0; HE.winner_mask = 0; HE.you_folded_to_show = 0; HE.played_mask = HE.raised_mask = 0; HE.acted_cnt = 0;
	HE.button = he_next_in(HE.button);
	for (int s = 0; s < tb_n; s++) if (he_in(s)) { HE.hole[s][0] = HE.deck[HE.dp++]; HE.hole[s][1] = HE.deck[HE.dp++]; HE.dealt[s]++; }
	/* the blinds (heads up: the button is the small blind) */
	int sbs = he_nin() == 2 ? HE.button : he_next_in(HE.button), bbs = he_next_in(sbs);
	int a = HE.sb < HE.stack[sbs] ? HE.sb : HE.stack[sbs]; HE.stack[sbs] -= a; HE.bet[sbs] = HE.in_pot[sbs] = a; if (!HE.stack[sbs]) HE.allin[sbs] = 1;
	a = HE.bb < HE.stack[bbs] ? HE.bb : HE.stack[bbs]; HE.stack[bbs] -= a; HE.bet[bbs] = HE.in_pot[bbs] = a; if (!HE.stack[bbs]) HE.allin[bbs] = 1;
	HE.cur_bet = HE.bb; HE.min_raise = HE.bb; HE.aggressor = bbs;
	HE.turn = he_next_act(bbs); if (HE.turn < 0) HE.turn = bbs;
	HE.state = HS_BET; HE.state_at = HE.turn_at = HE.deal_t = plat_ms(); HE.act_at = 0; HE.waited = 0; HE.you_slow = 0;
	HE.moth_card = -1;
	for (int s = 1; s < tb_n; s++) if (tb_who[s] == W_MOTH && he_in(s)) HE.moth_card = tr(2);
	for (int k = 0; k < 2 * tb_n; k++) music_sfx(SFX_CARD, 40 + k * 70);
	HE.raise_to = HE.cur_bet + HE.min_raise;
	/* the Moth tells you what you're holding, now and then */
	if (HE.moth_card >= 0 && he_live(0) && S.quips >= 2 && chance(12) && HE.hands > 2) {
		int ms = 1; while (ms < tb_n && tb_who[ms] != W_MOTH) ms++;
		char t[240], cn[24]; card_name(cn, sizeof cn, HE.hole[0][HE.moth_card]);
		ctx_hand = cn; speaker = W_MOTH;
		if (compose(t, sizeof t, W_MOTH, I_HE_PACT, 0, 0)) tb_push(ms, t, 400, 1);
	}
}
static void he_street(void)
{
	for (int s = 0; s < tb_n; s++) { HE.bet[s] = 0; HE.acted[s] = 0; }
	HE.cur_bet = 0; HE.min_raise = HE.bb;
	HE.street++;
	int add = HE.street == 1 ? 3 : 1;
	HE.dp++;   /* burn */
	for (int i = 0; i < add && HE.nboard < 5; i++) { HE.board[HE.nboard++] = HE.deck[HE.dp++]; music_sfx(SFX_CARD, i * 120); }
	HE.deal_t = plat_ms();
	HE.turn = he_next_act(HE.button);
	HE.turn_at = plat_ms(); HE.act_at = 0; HE.waited = 0; HE.you_slow = 0;
	HE.raise_to = HE.bb;
	/* somebody says something about the cards */
	if (S.quips >= 2 && chance(28) && !tb_busy()) {
		u32 f = 0; int suits[4] = { 0 }, ranks[15] = { 0 }, ace = 0;
		for (int i = 0; i < HE.nboard; i++) { suits[he_s(HE.board[i])]++; ranks[he_v(HE.board[i])]++; if (he_v(HE.board[i]) == 14) ace = 1; }
		for (int k = 0; k < 4; k++) if (suits[k] >= 3) f |= LF_JUMP;          /* flush on the board */
		for (int r = 2; r <= 14; r++) if (ranks[r] >= 2) f |= LF_CLOSE;       /* paired board */
		if (ace) f |= LF_ONES;
		if (HE.street == 3) f |= LF_LAST;
		int who[TB_MAX], nw = 0; for (int s = 1; s < tb_n; s++) if (he_in(s) && is_ai(s)) who[nw++] = s;
		if (nw) { u32 opts[4]; int no = 0; for (int b = 0; b < 16; b++) if (f & (1u << b)) opts[no++] = 1u << b; tb_say(who[tr(nw)], I_HE_FLOP, no ? opts[tr(no)] : 0, -1, 200); }
	}
}

/* ---------------- betting ---------------- */
static void he_put(int s, int amount)
{
	if (amount >= HE.stack[s]) { amount = HE.stack[s]; HE.allin[s] = 1; }
	HE.stack[s] -= amount; HE.bet[s] += amount; HE.in_pot[s] += amount;
	if (amount) music_sfx(SFX_CHIP, 0);
}
enum { HA_FOLD, HA_CHECK, HA_CALL, HA_RAISE, HA_ALLIN };
static int he_hum_in(void) { for (int s = 0; s < tb_n; s++) if (is_human(s) && he_in(s)) return 1; return 0; }
static void he_say_action(int s, int act, int amount, int bluff, float eq)
{
	if (!is_ai(s) || !S.quips) return;
	int w = tb_who[s];
	static const u8 PCT[5] = { 30, 12, 22, 60, 95 };   /* how often they say something for each */
	if (!chance(PCT[act] * (S.quips >= 2 ? 1 : 0))) return;
	static const int IN[5] = { I_HE_FOLD, I_HE_CHECK, I_HE_CALL, I_HE_RAISE, I_HE_ALLIN };
	u32 f = 0;
	if (HE.street == 0) f |= LF_OPEN;
	if (HE.street == 3) f |= LF_LAST;
	if (amount > he_pot() * 3 / 4 && act >= HA_RAISE) f |= LF_JUMP;
	if (eq > 0.8f) f |= LF_SAFE; else if (eq < 0.35f) f |= LF_HIGH;
	ctx_amt = amount;
	int tgt = HE.aggressor != s && act != HA_CHECK ? HE.aggressor : -1;
	char t[240], tl[80] = "";
	speaker = w; addr_used = 0;
	ctx_tgt = tgt >= 0 ? tb_name(tgt) : "";
	u32 tg = tgt >= 0 ? (tb_who[tgt] < 0 ? TG_YOU : (u32)(TG_MOTH << tb_who[tgt])) : 0;
	int use = act == HA_RAISE && HE.street && HE.raise_prev == 0 ? I_HE_BET : IN[act];
	if (!compose(t, sizeof t, w, use, f | tg, 0) && !compose(t, sizeof t, w, use, f, 0)) return;
	if (act >= HA_RAISE && (bluff ? chance((int)(HEP[w].tell * 100)) : chance((int)(HEP[w].fake_tell * 100)))) {
		const Line *T = find_line(w, I_HE_TELL, 0, 0);
		if (T) { speaker = w; talk_expand(tl, sizeof tl, T->t); }
	}
	char both[300];
	if (tl[0] == '(') snprintf(both, sizeof both, "%s %s", tl, t); else if (tl[0]) snprintf(both, sizeof both, "%s %s", t, tl); else snprintf(both, sizeof both, "%s", t);
	tidy(both);
	tb_push(s, both, 0, 0);
}
static void he_mark(int s, const char *txt) { snprintf(HE.act_txt[s], sizeof HE.act_txt[s], "%s", txt); HE.act_at_t[s] = plat_ms(); }

static void he_do(int s, int act, int raise_to, int bluff, float eq)
{
	int to_call = HE.cur_bet - HE.bet[s];
	char m[24];
	if (HE.street == 0 && act != HA_FOLD && act != HA_CHECK && !(HE.played_mask & (1 << s))) { HE.played[s]++; HE.played_mask |= 1 << s; }
	switch (act) {
	case HA_FOLD: HE.folded[s] = 1; he_mark(s, "Fold"); music_sfx(SFX_FLIP, 0); break;
	case HA_CHECK: he_mark(s, "Check"); music_sfx(SFX_TICK, 0); break;
	case HA_CALL: he_put(s, to_call); snprintf(m, sizeof m, HE.allin[s] ? "All in" : "Call %d", to_call); he_mark(s, m); break;
	case HA_RAISE: case HA_ALLIN: {
		int target = act == HA_ALLIN ? HE.bet[s] + HE.stack[s] : raise_to;
		int add = target - HE.bet[s];
		int prev = HE.cur_bet; HE.raise_prev = prev;
		he_put(s, add);
		if (HE.bet[s] > HE.cur_bet) {
			if (HE.bet[s] - prev >= HE.min_raise) HE.min_raise = HE.bet[s] - prev;
			HE.cur_bet = HE.bet[s]; HE.aggressor = s;
			for (int k = 0; k < tb_n; k++) if (k != s) HE.acted[k] = 0;
			if (HE.street == 0 && !(HE.raised_mask & (1 << s))) { HE.raised[s]++; HE.raised_mask |= 1 << s; }
		}
		HE.bluffing[s] = bluff;
		snprintf(m, sizeof m, HE.allin[s] ? "All in %d" : prev ? "Raise to %d" : "Bet %d", HE.bet[s]); he_mark(s, m);
		break; }
	}
	HE.acted[s] = 1; HE.acted_cnt++;
	he_say_action(s, act, act == HA_CALL ? to_call : HE.bet[s], bluff, eq);
	/* somebody reacts to a big move from you */
	if (is_human(s) && act >= HA_RAISE && S.quips >= 2 && chance(45)) {
		int r = he_next_act(s);
		if (is_ai(r)) {
			u32 f = 0;
			if (HE.dealt[s] > 4 && HE.played[s] * 10 > HE.dealt[s] * 6) f |= LF_LIAR;     /* you play every hand */
			else if (HE.dealt[s] > 6 && HE.played[s] * 10 < HE.dealt[s] * 2) f |= LF_SAFE; /* you only play big hands */
			if (HE.shown_bluff[s]) f |= LF_REVENGE;
			if (HE.you_slow) f |= LF_SLOW; else if (plat_ms() - HE.turn_at < 1500) f |= LF_FAST;
			if (act == HA_ALLIN) f |= LF_JUMP;
			u32 opts[8]; int no = 0; for (int b = 0; b < 16; b++) if (f & (1u << b)) opts[no++] = 1u << b;
			tb_say(r, I_HE_READ, no ? opts[tr(no)] : 0, 0, 150);
		}
	}
	/* next */
	if (he_nlive() <= 1) { HE.turn = -1; return; }
	int n = he_next_act(s);
	/* is the betting round over? everyone who can act has acted and matched */
	int done = 1;
	for (int k = 0; k < tb_n; k++) if (he_can_act(k) && (!HE.acted[k] || HE.bet[k] < HE.cur_bet)) done = 0;
	if (done || n < 0) HE.turn = -1; else HE.turn = n;
	HE.turn_at = plat_ms(); HE.act_at = 0; HE.waited = 0; HE.you_slow = 0;
}

/* an AI's turn: they decide first, then take their time (how long is a tell too) */
static int hp_act, hp_to, hp_bluff; static float hp_eq;
static void he_pend(int s, int act, int to, int bluff, float eq) { (void)s; hp_act = act; hp_to = to; hp_bluff = bluff; hp_eq = eq; }
static void he_ai(int s)
{
	int w = tb_who[s];
	int to_call = HE.cur_bet - HE.bet[s], pot = he_pot();
	int opp = he_nlive() - 1;
	float eq = he_equity(s, HE.street == 0 ? 90 : 140);
	/* what they think of whoever is betting */
	int ag = HE.aggressor;
	if (ag >= 0 && ag != s && to_call > 0) {
		if (HE.shown_bluff[ag]) eq += 0.04f * HE.shown_bluff[ag];
		if (ag == 0 && HE.dealt[0] > 4 && HE.played[0] * 10 > HE.dealt[0] * 6) eq += 0.04f;   /* you play too many hands */
		if (ag == 0 && HE.you_slow) eq += 0.02f;
	}
	float tilt = HE.tilt[s] * 0.05f;
	float aggr = HEP[w].aggr + tilt, bluff = HEP[w].bluff + tilt;
	if (w == W_SQUID && HE.raised[0] * 3 > HE.dealt[0]) aggr += 0.15f;   /* copies your style */
	float fair = 1.0f / (opp + 1);          /* an even share of the pot */
	float strong = eq - fair;              /* how much better than average */
	float pot_odds = to_call > 0 ? (float)to_call / (pot + to_call) : 0;
	float noise = (tr(100) - 50) / 600.0f;
	int stack = HE.stack[s];
	/* bet sizes */
	int base = pot * (50 + tr(40)) / 100; if (w == W_VIPER) base = pot * (80 + tr(50)) / 100;
	if (base < HE.bb) base = HE.bb;
	int raise_to = HE.cur_bet + (HE.min_raise > base ? HE.min_raise : base);
	if (HE.street == 0) raise_to = HE.cur_bet + HE.bb * (2 + tr(2)) + (w == W_VIPER ? HE.bb : 0);
	if (raise_to - HE.bet[s] >= stack) raise_to = HE.bet[s] + stack;
	if (to_call == 0) {
		if (strong > 0.18f - aggr * 0.1f + noise && chance((int)(aggr * 100))) { he_pend(s, raise_to - HE.bet[s] >= stack ? HA_ALLIN : HA_RAISE, raise_to, 0, eq); return; }
		if (HE.street > 0 && opp <= 2 && chance((int)(bluff * 100))) { he_pend(s, HA_RAISE, raise_to, 1, eq); return; }
		he_pend(s, HA_CHECK, 0, 0, eq); return;
	}
	/* facing a bet */
	float need = pot_odds + (HEP[w].tight - 0.5f) * 0.12f - (HEP[w].sticky - 0.5f) * 0.1f + noise;
	if (HE.street == 0) need += HEP[w].tight * 0.1f - 0.03f;
	if (strong > 0.3f && chance((int)(aggr * 90))) {
		if (to_call >= stack || raise_to - HE.bet[s] >= stack || (eq > 0.8f && chance(35))) he_pend(s, HA_ALLIN, 0, 0, eq);
		else he_pend(s, HA_RAISE, raise_to, 0, eq);
		return;
	}
	if (eq >= need) { he_pend(s, to_call >= stack ? HA_ALLIN : HA_CALL, 0, 0, eq); return; }
	if (HE.street < 3 && opp <= 2 && chance((int)(bluff * 45)) && raise_to - HE.bet[s] < stack) { he_pend(s, HA_RAISE, raise_to, 1, eq); return; }
	he_pend(s, HA_FOLD, 0, 0, eq);
}

/* ---------------- the end of a hand ---------------- */
static void he_award(void)
{
	/* side pots: pay out layer by layer, from the smallest all-in up */
	int left[TB_MAX]; for (int s = 0; s < tb_n; s++) left[s] = HE.in_pot[s];
	int score[TB_MAX]; for (int s = 0; s < tb_n; s++) score[s] = he_live(s) ? he_score(s) : -1;
	HE.winner_mask = 0; HE.won_amt = 0; int best_all = -1;
	for (;;) {
		int lvl = 0x7fffffff; for (int s = 0; s < tb_n; s++) if (left[s] > 0 && he_live(s) && left[s] < lvl) lvl = left[s];
		if (lvl == 0x7fffffff) {   /* only folded money left: it goes to the best live hand */
			int rest = 0; for (int s = 0; s < tb_n; s++) { rest += left[s]; left[s] = 0; }
			if (rest) { int b = -1; for (int s = 0; s < tb_n; s++) if (he_live(s) && (b < 0 || score[s] > score[b])) b = s; if (b >= 0) { HE.stack[b] += rest; HE.won_amt += b == 0 ? rest : 0; } }
			break;
		}
		int layer = 0; for (int s = 0; s < tb_n; s++) { int t = left[s] < lvl ? left[s] : lvl; layer += t; left[s] -= t; }
		int best = -1; for (int s = 0; s < tb_n; s++) if (he_live(s) && HE.in_pot[s] >= lvl && score[s] > best) best = score[s];
		int nw = 0; for (int s = 0; s < tb_n; s++) if (he_live(s) && HE.in_pot[s] >= lvl && score[s] == best) nw++;
		for (int s = 0; s < tb_n; s++) if (he_live(s) && HE.in_pot[s] >= lvl && score[s] == best) { HE.stack[s] += layer / nw; HE.winner_mask |= 1 << s; if (s == 0) HE.won_amt += layer / nw; }
		if (best > best_all) best_all = best;
	}
	HE.win_rank = best_all;
}
static void he_showdown(void)
{
	HE.state = HS_SHOW; HE.state_at = plat_ms();
	int live = he_nlive();
	int pot = he_pot();
	if (live >= 2) {
		for (int s = 0; s < tb_n; s++) if (he_live(s)) HE.showed[s] = 1;
		while (HE.nboard < 5) HE.board[HE.nboard++] = HE.deck[HE.dp++];
	}
	int before[TB_MAX]; for (int s = 0; s < tb_n; s++) before[s] = HE.stack[s] + HE.in_pot[s];
	he_award();
	int w = -1; for (int s = 0; s < tb_n; s++) if (HE.winner_mask & (1 << s)) { w = s; break; }
	if (w < 0) return;
	he_desc(HE.win_desc, sizeof HE.win_desc, HE.win_rank);
	ctx_hand = HE.win_desc; ctx_amt = pot;
	music_sfx(HE.winner_mask & 1 ? SFX_WIN : SFX_REEL, 300);
	if (HE.winner_mask & 1) plat_rumble(80);
	/* who lost the most to the winner */
	int loser = -1, lmax = 0; for (int s = 0; s < tb_n; s++) if (!(HE.winner_mask & (1 << s)) && HE.in_pot[s] > lmax) { lmax = HE.in_pot[s]; loser = s; }
	if (live >= 2) {
		/* shown down: was the winner or the loser bluffing? */
		for (int s = 0; s < tb_n; s++) if (HE.showed[s] && HE.bluffing[s] && !(HE.winner_mask & (1 << s))) HE.caught[s]++;
		if (loser >= 0 && HE.showed[loser] && he_score(loser) >= (2 << 20) && pot > HE.bb * 8) {
			if (is_ai(loser)) tb_say(loser, I_HE_BADBEAT, 0, w, 400);
			else if (is_ai(w)) tb_say(w, I_HE_WINPOT, LF_CLOSE, 0, 400);
		} else {
			if (is_ai(w) && chance(75)) tb_say(w, I_HE_WINPOT, pot > HE.bb * 15 ? LF_JUMP : 0, loser, 400);
			if (is_ai(loser) && chance(45)) tb_say(loser, I_HE_LOSEPOT, HE.bluffing[loser] ? LF_BLUFF : 0, w, 400);
		}
	} else {
		/* everyone folded: sometimes they show */
		if (is_ai(w) && HE.bluffing[w] && chance(35)) {
			HE.showed[w] = 1; HE.shown_bluff[w]++;
			tb_say(w, I_HE_BLUFFWIN, 0, loser, 300);
			int by = 1 + tr(tb_n - 1); if (by != w && he_in(by)) tb_say(by, I_HE_SEEBLUFF, 0, w, 300);
		} else if (is_ai(w) && chance(22)) tb_say(w, I_HE_MUCK, 0, -1, 300);
		else if (w == 0 && HE.bluffing[0] == 0 && chance(30) && is_ai(loser)) tb_say(loser, I_HE_LOSEPOT, 0, 0, 300);
	}
	/* tilt: losing a big pot gets to them */
	for (int s = 1; s < tb_n; s++) {
		if (!is_ai(s)) continue;
		int lost = before[s] - HE.stack[s];
		if (lost > HE_STACK / 3) { HE.tilt[s] += 2; TN[tb_who[s]].mood -= 0.8f; }
		else if (HE.tilt[s] > 0 && chance(40)) HE.tilt[s]--;
		if (HE.winner_mask & (1 << s)) { TN[tb_who[s]].mood += 0.3f; if (TN[tb_who[s]].mood > 2) TN[tb_who[s]].mood = 2; }
		if (TN[tb_who[s]].mood < -2) TN[tb_who[s]].mood = -2;
	}
	if (HE.winner_mask & 1 && HE.bluffing[0] && live < 2) { static int bluffs_won; if (++bluffs_won >= 3) achieve(AC_BLUFF); }
	/* who's out */
	for (int s = 0; s < tb_n; s++) if (he_in(s) && HE.stack[s] <= 0) {
		HE.out[s] = 1; HE.out_order[HE.nout++] = s;
		if (is_ai(s)) tb_say(s, I_HE_OUT, 0, w, 500);
		int by[TB_MAX], nb = 0; for (int k = 1; k < tb_n; k++) if (he_in(k) && is_ai(k)) by[nb++] = k;
		if (nb) tb_say(by[tr(nb)], is_human(s) ? I_HE_YOUOUT : I_HE_SEEOUT, 0, s, 500);
	}
}
static void he_finish(void)
{
	int winner = -1; for (int s = 0; s < tb_n; s++) if (he_in(s)) winner = s;
	HE.state = HS_OVER; HE.state_at = plat_ms();
	if (HE.done_paid) return;
	HE.done_paid = 1;
	int second = HE.nout ? HE.out_order[HE.nout - 1] : -1;
	int net = -HE.buyin;
	if (winner == 0) net = HE.prize1 - HE.buyin;
	else if (second == 0 && HE.prize2) net = HE.prize2 - HE.buyin;
	cfg.chips += net + HE.buyin;
	HE.net = net;
	S.he_played++; if (winner == 0) { S.he_won++; achieve(AC_HOLDEM); }
	style_note(HE.buyin, net > 0 ? 1 : net < 0 ? -1 : 0); track_round(net); gstat(net);
	S.played++; if (net > 0) S.won++; else if (net < 0) S.lost++; else S.pushed++;
	if (net > S.best_win) S.best_win = net;
	if (cfg.chips > S.max_chips) S.max_chips = cfg.chips;
	stats_save(); config_save();
	if (is_ai(winner)) tb_say(winner, I_HE_WIN, 0, -1, 400);
	else if (winner == 0 && !tb_p2) { int a = 1 + tr(tb_n - 1); tb_say(a, I_HE_YOUWIN, 0, 0, 400); }
}
static void he_record_out(void)
{
	if (HE.done_paid) return;
	HE.done_paid = 1; S.he_played++; S.played++; S.lost++;
	style_note(HE.buyin, -1); track_round(-HE.buyin); gstat(-HE.buyin); stats_save(); config_save();
}
static int holdem_busy(void) { return HE.state != HS_OVER && he_in(0); }

static void he_banter(void)
{
	if (S.quips < 2 || tb_busy()) return;
	int ai[TB_MAX], na = 0;
	for (int s = 1; s < tb_n; s++) if (he_in(s) && is_ai(s)) ai[na++] = s;
	if (!na) return;
	int a = ai[tr(na)], b;
	if (na >= 2 && chance(72)) { do b = ai[tr(na)]; while (b == a); } else b = he_in(0) ? 0 : -1;
	if (b < 0) return;
	u32 f = 0;
	if (HE.stack[b] > HE.stack[a] * 2) f |= LF_LEADER;
	if (HE.shown_bluff[b] || HE.caught[b]) f |= LF_LIAR;
	if (HE.tilt[a] >= 2) f |= LF_REVENGE;
	if (tb_say(a, I_HE_JAB, f, b, 300) && is_ai(b) && chance(78)) {
		u32 g = 0; if (HE.stack[a] > HE.stack[b] * 2) g |= LF_LEADER; if (HE.shown_bluff[a] || HE.caught[a]) g |= LF_LIAR;
		tb_say(b, I_HE_REPLY, g, a, 400);
	}
}

static void he_start(void)
{
	if (!big_ok(LD_BUY[S.he_buy % 5], he_start)) return;
	tb_n = 1; tb_who[0] = -1; tb_p2 = S.p2; tb_view = 0; tb_hand = -1;
	if (tb_p2) tb_who[tb_n++] = -2;   /* Player 2 plays on the house */
	for (int w = 0; w < W_N && tb_n < TB_MAX; w++) if (S.he_opp & (1 << w)) tb_who[tb_n++] = w;
	if (tb_n < 2) { tb_who[1] = W_MOTH; tb_n = 2; S.he_opp = 1; }
	tb_layout(); tb_clear();
	memset(&HE, 0, sizeof HE);
	for (int s = 0; s < tb_n; s++) HE.stack[s] = HE_STACK;
	HE.sb = 10; HE.bb = 20;
	HE.buyin = LD_BUY[S.he_buy % 5];
	cfg.chips -= HE.buyin;
	int pool = HE.buyin * tb_n;
	if (tb_n >= 4) { HE.prize1 = pool * 70 / 100; HE.prize2 = pool - HE.prize1; } else HE.prize1 = pool;
	HE.button = tr(tb_n);
	for (int s = 1; s < tb_n; s++) if (is_ai(s)) tonight_start(tb_who[s]);
	view = V_HOLDEM; persona = -1;
	S.last_game = -1; stats_save(); config_save();
	if (S.quips) {
		int order[TB_MAX], n = 0; for (int s = 1; s < tb_n; s++) if (is_ai(s)) order[n++] = s;
		for (int i = n - 1; i > 0; i--) { int j = tr(i + 1); int t = order[i]; order[i] = order[j]; order[j] = t; }
		int say_n = n < 3 ? n : 2 + tr(n - 1);
		for (int i = 0; i < say_n; i++) tb_say(order[i], I_HE_HELLO, 0, chance(35) && n > 1 ? order[(i + 1) % n] : -1, 200);
	}
	HE.state = HS_AFTER; HE.state_at = plat_ms() - 1000;   /* deal the first hand shortly */
	ui_set_focus(9602);
}

/* ---------------- drawing ---------------- */
static void mini_card(int x, int y, u8 c, int face, int a)
{
	/* a small card for the seats: 30 x 42 */
	gfx_round(x + 1, y + 2, 30, 42, 5, RGB(0, 0, 0), 80 * a / 255);
	if (!face) {
		gfx_round_grad(x, y, 30, 42, 5, RGB(60, 110, 200), RGB(30, 60, 140), a);
		gfx_round_line(x + 3, y + 3, 24, 36, 3, 1, RGB(200, 220, 255), 120 * a / 255);
		return;
	}
	gfx_round_grad(x, y, 30, 42, 5, RGB(255, 255, 252), RGB(226, 222, 212), a);
	int r = c % 13, s = c / 13;
	static const char *const RS[13] = { "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };
	u32 col = (s == 1 || s == 2) ? RGB(200, 20, 30) : RGB(20, 20, 26);
	if (S.cardset == CS_FOUR) col = suit_col(CS_FOUR, s);
	text_c(F_SMALL, x + 15, y + 1, RS[r], col, a);
	suit_shape(s, x + 15, y + 29, 7, col, a);
}
static void he_seat_draw(int s)
{
	int x = tb_x[s], y = tb_y[s];
	u64 now = plat_ms();
	int w = tb_who[s], in = he_in(s), turn = HE.state == HS_BET && HE.turn == s;
	if (s == 0) {
		x = 94; y = 288;   /* you: above your cards, bottom left */
		if (turn) gfx_round(x - 64, y - 8, 128, 30, 15, RGB(255, 214, 90), 60 + (int)(sinf(now / 200.0f) * 30));
		gfx_round(x - 58, y - 4, 116, 22, 11, RGB(0, 0, 0), 150);
		char t[40]; const char *who = tb_p2 ? "Player 1" : "You"; if (in) snprintf(t, sizeof t, "%s  -  %d", who, HE.stack[0]); else snprintf(t, sizeof t, "%s  -  out", who);
		text_c(F_SMALL, x, y - 3, t, in ? RGB(255, 240, 200) : C_DIM, 255);
	} else {
		u32 acc = w < 0 ? RGB(255, 214, 90) : TB_ACC[w];
		if (turn) { int g = 90 + (int)(sinf(now / 180.0f) * 40); gfx_circle(x, y + 4, 40, acc, g / 3); gfx_ring(x, y + 4, 38, 2, acc, g); }
		if (in && !HE.folded[s]) {
			int face = HE.showed[s];
			mini_card(x - 30, y - 16, HE.hole[s][0], face, 255);
			mini_card(x + 0, y - 14, HE.hole[s][1], face, 255);
		} else if (in) { mini_card(x - 16, y - 12, 0, 0, 70); }
		gfx_round(x - 46, y + 28, 92, 32, 10, RGB(0, 0, 0), 160);
		gfx_round_line(x - 46, y + 28, 92, 32, 10, 1, acc, in ? 200 : 60);
		text_c(F_SMALL, x, y + 27, tb_name(s), in ? shade(acc, 40) : C_DIM, 255);
		char st[16]; if (in) snprintf(st, sizeof st, "%d", HE.stack[s]); else snprintf(st, sizeof st, "out");
		text_c(F_SMALL, x, y + 42, st, RGB(255, 214, 90), in ? 255 : 90);
		if (turn && is_ai(s) && !tb_busy()) for (int k = 0; k < 3; k++) gfx_circle(x - 10 + k * 10, y - 30, 3, RGB(255, 255, 255), ((now / 250) % 3) == (u64)k ? 255 : 90);
	}
	/* their bet in front of them, and what they just did */
	int bx = x + (320 - x) * 38 / 100, by = y + (212 - y) * 38 / 100;
	if (y > 150 && s) { bx = x + (x < 320 ? 58 : -58); by = y - 10; }   /* side seats: beside them, clear of the board */
	if (s == 0) { bx = 200; by = 300; }
	if (HE.bet[s] > 0 && HE.state == HS_BET) { chip_stack(bx, by, HE.bet[s], 9); char b[12]; snprintf(b, sizeof b, "%d", HE.bet[s]); text_c(F_SMALL, bx + 26, by - 10, b, RGB(255, 240, 200), 230); }
	if (HE.act_txt[s][0] && now < HE.act_at_t[s] + 2500) {
		int a = now > HE.act_at_t[s] + 2000 ? (int)((HE.act_at_t[s] + 2500 - now) * 255 / 500) : 255;
		int tw = text_w(F_SMALL, HE.act_txt[s]) + 16;
		int ty = s == 0 ? 262 : y - 42;
		gfx_round(x - tw / 2, ty, tw, 20, 10, RGB(0, 0, 0), 170 * a / 255);
		text_c(F_SMALL, x, ty + 1, HE.act_txt[s], RGB(255, 255, 255), a);
	}
	if (HE.button == s && in) { int dx = x + (s == 0 ? 70 : 40), dy = y + (s == 0 ? 8 : -20); gfx_circle(dx, dy, 9, RGB(250, 250, 250), 255); text_c(F_SMALL, dx, dy - 9, "D", RGB(20, 20, 20), 255); }
}

static void holdem_game(int blocked)
{
	u64 now = plat_ms();
	felt_draw(24, 58, 592, 282, 141, RGB(20, 70, 100), RGB(8, 30, 50), RGB(96, 54, 30));
	text(F_BODYB, 30, 16, "Texas Hold'em", RGB(250, 244, 226), 255);
	{
		char t[48]; snprintf(t, sizeof t, "Blinds %d/%d", HE.sb, HE.bb);
		gfx_round(236, 16, 128, 30, 15, RGB(0, 0, 0), 120); text_c(F_SMALL, 300, 21, t, RGB(230, 220, 200), 255);
		snprintf(t, sizeof t, "Prize %d", HE.prize1);
		gfx_round(372, 16, 150, 30, 15, RGB(0, 0, 0), 120); chip_draw(390, 31, 11, 500, 0); text(F_BODYB, 408, 19, t, RGB(255, 214, 90), 255);
	}
	/* the board */
	int pot = he_pot();
	if (HE.state != HS_OVER || pot) {
		char p[32]; snprintf(p, sizeof p, "Pot %d", pot);
		if (pot) { gfx_round(270, 168, 100, 24, 12, RGB(0, 0, 0), 140); text_c(F_SMALL, 320, 170, p, RGB(255, 214, 90), 255); }
	}
	for (int i = 0; i < 5; i++) {
		int x = 156 + i * 66, y = 196;
		if (i < HE.nboard) {
			Card k = { HE.board[i], HE.deal_t + (u64)(i >= (HE.street == 1 ? 0 : HE.nboard - 1) ? (i % 3) * 120 : 0) };
			if (HE.state == HS_SHOW) k.t = 0;
			card_anim(&k, x, y, 0);
			if (HE.state == HS_SHOW && HE.winner_mask && he_nlive() >= 2) {
				/* light up the winning five */
				(void)0;
			}
		} else gfx_round(x, y, CW, CH, 8, RGB(0, 0, 0), 50);
	}
	for (int s = 1; s < tb_n; s++) he_seat_draw(s);
	he_seat_draw(0);
	/* your cards */
	gfx_round(18, 330, 152, 96, 14, RGB(0, 0, 0), 140);
	int hand = tb_hand >= 0;
	/* two people: your cards only show on your own turn (or when they're shown down) */
	int peek = !hand && (!tb_p2 || (HE.state == HS_BET && HE.turn == ME) || HE.showed[ME]);
	if (he_in(ME) && HE.state != HS_OVER) {
		for (int i = 0; i < 2; i++) {
			int x = 26 + i * 70, y = 334;
			if (!peek) { gfx_round_grad(x, y, CW, CH, 8, RGB(60, 110, 200), RGB(30, 60, 140), 255); gfx_round_line(x + 4, y + 4, CW - 8, CH - 8, 5, 1, RGB(200, 220, 255), 120); }
			else if (HE.folded[ME]) { card_draw(S.cardset, x, y, HE.hole[ME][i], 0); gfx_round(x, y, CW, CH, 8, RGB(0, 0, 0), 150); }
			else { Card k = { HE.hole[ME][i], 0 }; card_anim(&k, x, y, 0); }
		}
		if (tb_p2 && !peek) { char t[32]; snprintf(t, sizeof t, "%s's cards", tb_name(ME)); gfx_round(18, 310, 152, 22, 11, RGB(0, 0, 0), 160); text_c(F_SMALL, 94, 311, t, RGB(255, 240, 200), 255); }
		if (S.helper && peek && !HE.folded[ME] && HE.state == HS_BET) {   /* learning help: your chances, and what a call costs */
			static int key = -1; static float eq;
			int k = HE.hands * 100 + HE.nboard * 10 + he_nlive();
			if (k != key) { key = k; eq = he_equity(ME, 150); }
			int to_call = HE.cur_bet - HE.bet[ME], pot = he_pot();
			char o[80];
			if (to_call > 0) snprintf(o, sizeof o, "Win chance %d%%.  Call %d to win %d: worth it above %d%%", (int)(eq * 100), to_call, pot + to_call, to_call * 100 / (pot + to_call));
			else snprintf(o, sizeof o, "Win chance %d%%", (int)(eq * 100));
			int tw = text_w(F_SMALL, o) + 20;
			gfx_round(320 - tw / 2, 292, tw, 22, 11, RGB(20, 40, 70), 225);
			text_c(F_SMALL, 320, 293, o, RGB(170, 220, 255), 255);
		}
		if (peek && HE.nboard >= 3 && !HE.folded[ME]) { char d[40]; he_desc(d, sizeof d, he_score(ME)); gfx_round(18, 310, 152, 22, 11, RGB(0, 0, 0), 160); text_c(F_SMALL, 94, 311, d, RGB(255, 240, 200), 255); }
	} else text_c(F_SMALL, 94, 368, he_in(ME) ? "" : tb_p2 ? "Out" : "You're out", C_DIM, 255);
	/* the result */
	if (HE.state == HS_SHOW || HE.state == HS_AFTER) {
		if (HE.winner_mask) {
			char t[80]; int w = 0; while (!(HE.winner_mask & (1 << w))) w++;
			if (he_nlive() >= 2) snprintf(t, sizeof t, "%s win%s with %s", w == YOU ? "You" : tb_name(w), w == YOU ? "" : "s", HE.win_desc);
			else snprintf(t, sizeof t, "%s win%s the pot", w == YOU ? "You" : tb_name(w), w == YOU ? "" : "s");
			int tw = text_w(F_BODYB, t) + 30;
			gfx_round(320 - tw / 2, 292, tw, 30, 15, is_human(w) ? RGB(40, 150, 90) : RGB(60, 50, 80), 230);
			text_c(F_BODYB, 320, 295, t, C_WHITE, 255);
		}
	}
	/* controls */
	int x0 = 180;
	if (HE.state == HS_OVER) {
		int winner = -1; for (int s = 0; s < tb_n; s++) if (he_in(s)) winner = s;
		char t[64];
		if (winner == 0 && !tb_p2) snprintf(t, sizeof t, "You win the table! +%d chips", HE.net);
		else if (is_human(winner)) snprintf(t, sizeof t, winner ? "Player 2 wins the table!" : "Player 1 wins the table! +%d chips", HE.net);
		else snprintf(t, sizeof t, "%s wins the table.%s", tb_name(winner), HE.net > -HE.buyin ? "  2nd place prize" : "");
		text_c(F_BODYB, 400, 340, t, winner == 0 ? RGB(120, 240, 150) : RGB(255, 230, 190), 255);
		if ((ui_ibutton(9610, x0 + 20, 378, 200, 42, IC_PLAY, "Play again", ST_NORMAL) & UI_CLICK) && !blocked) { if (cfg.chips < LD_BUY[S.he_buy % 5]) ui_toast("Not enough chips for the buy-in"); else he_start(); }
		if ((ui_ibutton(9611, x0 + 228, 378, 200, 42, IC_BACK, "Leave table", ST_NORMAL) & UI_CLICK) && !blocked) { view = V_FLOOR; ui_set_focus(8200); tb_clear(); }
	} else if (!he_hum_in()) {
		text(F_SMALL, x0 + 20, 344, "Watching the rest of the game", RGB(220, 210, 190), 255);
		if ((ui_ibutton(9612, x0 + 20, 378, 200, 42, IC_PLAY, HE.fast ? "Normal speed" : "Speed up", ST_NORMAL) & UI_CLICK) && !blocked) HE.fast = !HE.fast;
		if ((ui_ibutton(9611, x0 + 228, 378, 200, 42, IC_BACK, "Leave table", ST_NORMAL) & UI_CLICK) && !blocked) { he_record_out(); view = V_FLOOR; ui_set_focus(8200); tb_clear(); }
	} else {
		int mine = HE.state == HS_BET && HE.turn == ME && !hand;
		int to_call = HE.cur_bet - HE.bet[ME];
		int minr = HE.cur_bet + HE.min_raise, maxr = HE.bet[ME] + HE.stack[ME];
		if (HE.raise_to < minr) HE.raise_to = minr; if (HE.raise_to > maxr) HE.raise_to = maxr;
		int st = mine ? ST_NORMAL : ST_GHOST;
		int a_fold = ui_button(9600, x0, 336, 104, 38, "Fold", st) & UI_CLICK;
		char cl[24]; if (to_call <= 0) snprintf(cl, sizeof cl, "Check"); else if (to_call >= HE.stack[ME]) snprintf(cl, sizeof cl, "Call all in"); else snprintf(cl, sizeof cl, "Call %d", to_call);
		int a_call = ui_button(9601, x0 + 110, 336, 140, 38, cl, st) & UI_CLICK;
		int a_allin = ui_button(9602, x0 + 256, 336, 180, 38, "All in", st) & UI_CLICK;
		int can_raise = maxr > HE.cur_bet && maxr >= minr;
		int rst = mine && can_raise ? ST_NORMAL : ST_GHOST;
		int a_dn = ui_button(9603, x0, 382, 44, 38, "-", rst) & UI_CLICK;
		gfx_round(x0 + 48, 382, 110, 38, 10, RGB(0, 0, 0), 150);
		char rv[16]; snprintf(rv, sizeof rv, "%d", HE.raise_to); text_c(F_BODYB, x0 + 103, 388, rv, RGB(255, 255, 255), 255);
		int a_up = ui_button(9604, x0 + 162, 382, 44, 38, "+", rst) & UI_CLICK;
		char rl[24]; snprintf(rl, sizeof rl, HE.cur_bet ? "Raise to %d" : "Bet %d", HE.raise_to);
		int a_raise = ui_button(9605, x0 + 212, 382, 224, 38, rl, rst) & UI_CLICK;
		int step = HE.bb;
		if (mine && !blocked && !HE.leave_open) {
			if (in->down & BTN_UP) a_up = 1;
			if (in->down & BTN_DOWN) a_dn = 1;
			if (in->down & BTN_1) a_call = 1;
			if (in->down & BTN_2) a_raise = can_raise;
			if (now - HE.turn_at > 10000) HE.you_slow = 1;
			if (a_up) HE.raise_to += step; if (a_dn) HE.raise_to -= step;
			if (a_fold) he_do(ME, to_call > 0 ? HA_FOLD : HA_CHECK, 0, 0, 0);
			else if (a_call) he_do(ME, to_call > 0 ? (to_call >= HE.stack[ME] ? HA_ALLIN : HA_CALL) : HA_CHECK, 0, 0, 0);
			else if (a_allin) he_do(ME, HA_ALLIN, 0, he_equity(ME, 60) < 0.3f, 0);
			else if (a_raise && can_raise) he_do(ME, HE.raise_to >= maxr ? HA_ALLIN : HA_RAISE, HE.raise_to, he_equity(ME, 60) < 0.3f, 0);
		}
	}
	tb_pump();
	for (int s = 1; s < tb_n; s++) tb_bubble(s);
	if (tb_handoff(blocked)) return;
	int speedk = HE.fast || !he_hum_in() ? 40 : S.speed == 0 ? 140 : S.speed == 2 ? 60 : 100;
	speedk = speedk * anim_k() / 100; if (speedk < 15) speedk = 15;
	if (!blocked && !HE.leave_open) switch (HE.state) {
	case HS_BET:
		if (he_nlive() <= 1) { he_showdown(); break; }
		if (HE.turn < 0) {
			if (tb_busy()) break;
			/* the betting round is over */
			if (HE.street >= 3 || he_nact() <= 1) {
				/* all in, or the river is done: run out the board, slowly for drama */
				if (HE.street < 3 && he_nact() <= 1) { if (now > HE.deal_t + 1300 * speedk / 100) { he_street(); HE.turn = -1; } break; }
				he_showdown();
			} else he_street();
			break;
		}
		if (is_human(HE.turn)) {
			if (tb_need(HE.turn)) break;
			if (!HE.waited && now - HE.turn_at > 25000 && !tb_busy()) { HE.waited = 1; int s = 1 + tr(tb_n - 1); if (he_in(s)) tb_say(s, I_HE_WAIT, 0, 0, 0); }
			break;
		}
		if (!HE.act_at) {
			int w = tb_who[HE.turn];
			he_ai(HE.turn);
			static const u8 LIE_PACE[W_N] = { 100, 130, 75, 85, 135, 70 };   /* different from the dice table */
			int pace = hp_bluff && chance(55) ? LIE_PACE[w] : !hp_bluff && chance(12) ? LIE_PACE[w] : 100;
			if (hp_act == HA_FOLD || hp_act == HA_CHECK) pace = pace * 70 / 100;
			HE.act_at = now + (u64)((HEP[w].think * (60 + tr(80)) / 100) * speedk / 100 * pace / 100);
		}
		if (now >= HE.act_at && !tb_busy()) { he_do(HE.turn, hp_act, hp_to, hp_bluff, hp_eq); }
		break;
	case HS_SHOW:
		if (now > HE.state_at + 2600 * speedk / 100 && !tb_busy()) { HE.state = HS_AFTER; HE.state_at = now; }
		break;
	case HS_AFTER:
		if (now > HE.state_at + 900 * speedk / 100 && !tb_busy()) {
			if (he_nin() <= 1) he_finish();
			else { if (chance(22)) he_banter(); if (!tb_busy()) he_new_hand(); }
		}
		break;
	default: break;
	}
	if (HE.leave_open) {
		static const char *IT[2] = { "Leave (lose your buy-in)", "Keep playing" };
		static const int IC2[2] = { IC_BACK, IC_PLAY };
		int k = menu_popup(&HE.leave_open, IT, IC2, 2, "Leave the table?");
		if (k == 0) { he_record_out(); view = V_FLOOR; ui_set_focus(8200); tb_clear(); }
	}
	ui_footer_begin();
	if (HE.state == HS_BET && HE.turn == ME) { ui_footer_hint("1", "Check / Call"); ui_footer_hint("2", "Raise"); }
	ui_footer_hint("HOME", "Leave");
	if (blocked || HE.leave_open) return;
	if (in->down & (BTN_HOME | BTN_B)) {
		if (HE.state == HS_OVER || !he_hum_in()) { he_record_out(); view = V_FLOOR; ui_set_focus(8200); tb_clear(); }
		else { HE.leave_open = 1; ui_set_focus(9001); }
	}
}
