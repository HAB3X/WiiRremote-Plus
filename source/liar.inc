/*
   liar.inc - LIAR'S DICE, in the Back Room.
   You and up to five of the dealers, a cup of dice each. Bid how many of a face are under ALL
   the cups, raise, or call "Liar!". Everyone plays their own way, remembers who bluffed, has a
   tell when they lie, and talks to everyone else at the table.
   Included in casino_games.inc (after table_ai.inc). WiiRremote Plus by Habex. GPL-3.0.
*/
static const int LD_BUY[5] = { 50, 100, 250, 500, 1000 };
enum { LS_ROLL, LS_TURN, LS_REVEAL, LS_AFTER, LS_OVER };
static struct {
	int dice[TB_MAX], start_dice;
	u8 d[TB_MAX][5];
	int state, turn, starter;
	int bq, bf, bidder, nbids;
	int hs[48], hq[48], hf[48];
	int caller, spot;
	u64 act_at, turn_at, state_at;
	int reveal_i, reveal_cnt; u64 reveal_t;
	int loser, gainer, cnt, verdict;
	int pali, pali_next, pali_used[TB_MAX];
	int bluffs[TB_MAX], honest[TB_MAX], lost_to[TB_MAX][TB_MAX];
	int out_order[TB_MAX], nout;
	int pot, buyin, done_paid, net;
	int sel_q, sel_f;
	int you_slow, waited;
	int psy;
	int rolled; u64 roll_t[TB_MAX];
	int watching, leave_open, fast;
	int last_bluff;
	int pk, pq, pf, pbl;     /* what the AI has decided, done after it 'thinks' */
	int go_on;               /* you pressed Next round */          /* was the latest AI bid a bluff (for the reveal lines) */
	int rounds;
} LD;

/* ---- rules ---- */
static int ld_wild(void) { return S.ld_wild && !LD.pali; }
static int ld_alive(int s) { return s >= 0 && s < tb_n && LD.dice[s] > 0; }
static int ld_nalive(void) { int n = 0; for (int s = 0; s < tb_n; s++) n += ld_alive(s); return n; }
static int ld_total(void) { int t = 0; for (int s = 0; s < tb_n; s++) t += LD.dice[s]; return t; }
static int ld_next(int s) { for (int k = 1; k <= tb_n; k++) { int n = (s + k) % tb_n; if (ld_alive(n)) return n; } return s; }
static int ld_match(u8 v, int f) { return v == f || (ld_wild() && v == 1 && f != 1); }
static int ld_count_seat(int s, int f) { int c = 0; for (int i = 0; i < LD.dice[s]; i++) c += ld_match(LD.d[s][i], f); return c; }
static int ld_count(int f) { int c = 0; for (int s = 0; s < tb_n; s++) c += ld_count_seat(s, f); return c; }
static int ld_legal(int seat, int q, int f)
{
	if (q < 1 || f < 1 || f > 6 || q > ld_total()) return 0;
	int bq = LD.bq, bf = LD.bf;
	if (!bq) return !(ld_wild() && f == 1);   /* no opening on ones while they're wild */
	if (LD.pali && LD.dice[seat] > 1) return f == bf && q > bq;   /* palifico: the face is locked */
	if (!ld_wild()) return q > bq || (q == bq && f > bf);
	if (bf == 1 && f == 1) return q > bq;
	if (bf == 1) return q >= bq * 2 + 1;
	if (f == 1) return q >= (bq + 1) / 2;
	return q > bq || (q == bq && f > bf);
}
static int ld_qmin(int seat, int f) { for (int q = 1; q <= ld_total(); q++) if (ld_legal(seat, q, f)) return q; return 0; }

/* ---- odds ---- */
static float ld_binom_ge(int n, int k, float p)
{
	if (k <= 0) return 1; if (k > n) return 0;
	float c = 1, tot = 0;   /* sum of C(n,i) p^i (1-p)^(n-i), i >= k */
	for (int i = 0; i <= n; i++) {
		if (i) c = c * (n - i + 1) / i;
		if (i >= k) tot += c * powf(p, (float)i) * powf(1 - p, (float)(n - i));
	}
	return tot > 1 ? 1 : tot;
}
static float ld_binom_eq(int n, int k, float p)
{
	if (k < 0 || k > n) return 0;
	float c = 1; for (int i = 1; i <= k; i++) c = c * (n - k + i) / i;
	return c * powf(p, (float)k) * powf(1 - p, (float)(n - k));
}

/* how each of them plays: when they call, how often they bluff, how they like Spot on, how big they raise, how long they think */
static const struct { float call, bluff, spot, jump, trust; int think; float tell, fake_tell; } LDP[W_N] = {
	{ 0.34f, 0.22f, 0.30f, 0.15f, 1.0f, 1700, 0.15f, 0.20f },   /* Moth: reads, bluffs calmly. His "tell" means nothing */
	{ 0.45f, 0.30f, 0.36f, 0.35f, 0.7f,  900, 0.42f, 0.14f },   /* Lynx: quick on the trigger */
	{ 0.29f, 0.12f, 0.27f, 0.05f, 0.9f, 2300, 0.40f, 0.12f },   /* Crow: slow, honest, patient */
	{ 0.38f, 0.34f, 0.32f, 0.20f, 0.6f, 1000, 0.45f, 0.18f },   /* Squid: copies whoever is winning */
	{ 0.22f, 0.08f, 0.22f, 0.05f, 1.2f, 2000, 0.36f, 0.10f },   /* Vixen: only calls when she's sure */
	{ 0.38f, 0.40f, 0.34f, 0.55f, 0.8f, 1100, 0.42f, 0.18f },   /* Viper: raises big to push people around */
};

/* what a player thinks the chance is that q of face f are out there */
static float ld_est(int me, int q, int f, float *pexact)
{
	int w = tb_who[me] < 0 ? 0 : tb_who[me];
	float p = (ld_wild() && f != 1) ? 1.0f / 3 : 1.0f / 6;
	int known = ld_count_seat(me, f), unknown = ld_total() - LD.dice[me];
	/* Phantom Moth has "seen" one of your dice */
	if (tb_who[me] == W_MOTH && LD.psy >= 0 && LD.psy < LD.dice[0]) { unknown--; known += ld_match(LD.d[0][LD.psy], f); }
	/* what the others' bids say about their cups */
	float bonus = 0;
	for (int i = 0; i < LD.nbids; i++) {
		int s = LD.hs[i]; if (s == me || LD.hf[i] != f) continue;
		float tr = 1.0f - 0.22f * LD.bluffs[s] + 0.08f * LD.honest[s];
		if (s == 0 && LD.you_slow) tr *= 0.8f;
		if (tr < 0.3f) tr = 0.3f; if (tr > 1.3f) tr = 1.3f;
		bonus += 0.45f * tr * LDP[w].trust;
	}
	float capb = unknown * 0.4f; if (bonus > capb) bonus = capb; if (bonus > 2.2f) bonus = 2.2f;
	float need = q - known - bonus;
	int k0 = (int)floorf(need); float fr = need - k0;
	float P = ld_binom_ge(unknown, k0, p) * (1 - fr) + ld_binom_ge(unknown, k0 + 1, p) * fr;
	if (pexact) *pexact = ld_binom_eq(unknown, (int)(q - known - bonus + 0.5f), p);
	return P;
}

/* ---- saying things at the table ---- */
static int ld_hum_alive(void) { for (int s = 0; s < tb_n; s++) if (is_human(s) && ld_alive(s)) return 1; return 0; }
static void ld_best_sel(void) { int best = 2, bc = -1; for (int f = 2; f <= 6; f++) { int c = ld_count_seat(ME, f); if (c > bc) { bc = c; best = f; } } LD.sel_f = best; LD.sel_q = 1; }
static void ld_ctx_bid(int q, int f) { ctx_q = q; ctx_f = f; }
static u32 ld_flags_common(void) { u32 f = 0; if (ld_total() <= 5) f |= LF_FEW; if (LD.pali) f |= LF_PALI; return f; }
static int ld_leader(void) { int b = -1, bd = -1; for (int s = 0; s < tb_n; s++) if (LD.dice[s] > bd) { bd = LD.dice[s]; b = s; } return b; }

/* two of them have a go at each other (or at you) */
static void ld_banter(void)
{
	if (S.quips < 2 || tb_busy()) return;
	int ai[TB_MAX], na = 0;
	for (int s = 1; s < tb_n; s++) if (ld_alive(s) && is_ai(s)) ai[na++] = s;
	if (!na) return;
	int a = ai[tr(na)], b;
	if (na >= 2 && chance(75)) { do b = ai[tr(na)]; while (b == a); }
	else b = ld_alive(0) ? 0 : -1;
	if (b < 0) return;
	u32 f = ld_flags_common();
	if (LD.bluffs[b] >= 1) f |= LF_LIAR;
	if (LD.lost_to[a][b]) f |= LF_REVENGE;
	if (b == ld_leader()) f |= LF_LEADER;
	if (tb_say(a, I_LD_JAB, f, b, 250) && is_ai(b) && chance(78)) {
		u32 g = ld_flags_common(); if (LD.bluffs[a] >= 1) g |= LF_LIAR; if (LD.lost_to[b][a]) g |= LF_REVENGE;
		tb_say(b, I_LD_REPLY, g, a, 400);
	}
}

/* ---- rounds ---- */
static void ld_new_round(void)
{
	LD.state = LS_ROLL; LD.state_at = plat_ms();
	LD.bq = LD.bf = 0; LD.bidder = -1; LD.nbids = 0; LD.caller = -1; LD.spot = 0;
	LD.rolled = 0; LD.you_slow = 0; LD.act_at = 0; LD.psy = -1; LD.waited = 0;
	LD.rounds++;
	for (int s = 1; s < tb_n; s++) if (ld_alive(s)) LD.roll_t[s] = plat_ms() + 200 + tr(900);
	if (tb_p2 && !ld_alive(tb_view) && ld_alive(1 - tb_view)) tb_hand = 1 - tb_view;
	if (!ld_alive(tb_view) && tb_hand < 0) LD.rolled = 1;
}
static void ld_roll_all(void)
{
	for (int s = 0; s < tb_n; s++) for (int i = 0; i < 5; i++) LD.d[s][i] = 1 + tr(6);
	music_sfx(SFX_SLAM, 0);
	LD.state = LS_TURN; LD.turn = LD.starter; LD.turn_at = plat_ms(); LD.act_at = 0;
	/* your first choice: your best face */
	ld_best_sel();
	/* the Moth takes a peek at your cup */
	int moth = -1; for (int s = 1; s < tb_n; s++) if (tb_who[s] == W_MOTH && ld_alive(s)) moth = s;
	if (moth > 0 && ld_alive(0) && LD.dice[0]) {
		LD.psy = tr(LD.dice[0]);
		if (S.quips >= 2 && chance(28) && LD.rounds > 1) {
			int f = LD.d[0][LD.psy]; ctx_f = f; ctx_cnt = 0; for (int i = 0; i < LD.dice[0]; i++) ctx_cnt += LD.d[0][i] == f;
			char t[240];
			if (compose(t, sizeof t, W_MOTH, I_LD_READ, ctx_cnt >= 2 ? LF_JUMP : 0, 0)) tb_push(moth, t, 300, 1);
		}
	}
	/* a word as the cups come down */
	if (S.quips >= 2 && chance(40)) { int s = 1 + tr(tb_n - 1); if (ld_alive(s)) tb_say(s, LD.pali ? I_LD_PALI : I_LD_ROLL, ld_flags_common(), -1, 200); }
}

static void ld_bid(int s, int q, int f, int bluff)
{
	int prev = LD.bidder, pq = LD.bq;
	if (LD.nbids < 48) { LD.hs[LD.nbids] = s; LD.hq[LD.nbids] = q; LD.hf[LD.nbids] = f; LD.nbids++; }
	LD.bq = q; LD.bf = f; LD.bidder = s;
	music_sfx(SFX_CHIP, 0);
	if (is_ai(s)) {
		/* they say it: the bid, maybe a jab at whoever bid before, maybe their tell */
		ld_ctx_bid(q, f);
		int w = tb_who[s];
		u32 fl = ld_flags_common();
		if (!pq) fl |= LF_OPEN;
		if (pq && q >= pq + 2 && f != 1) fl |= LF_JUMP;
		if (f == 1) fl |= LF_ONES;
		float e = ld_est(s, q, f, NULL);
		if (e < 0.5f) fl |= LF_HIGH; else if (e > 0.85f) fl |= LF_SAFE;
		char t[240], tl[80] = "";
		speaker = w; addr_used = 0;
		ctx_tgt = prev >= 0 ? tb_name(prev) : "";
		u32 tg = prev >= 0 ? (tb_who[prev] < 0 ? TG_YOU : (u32)(TG_MOTH << tb_who[prev])) : 0;
		int ok = compose(t, sizeof t, w, pq ? I_LD_BID : I_LD_OPEN, fl | tg, 0);
		if (!ok) ok = compose(t, sizeof t, w, pq ? I_LD_BID : I_LD_OPEN, fl, 0);
		if (!ok) { snprintf(t, sizeof t, "%d %s.", q, "of them"); }
		/* the tell: most of them give something away when they lie, now and then when they don't */
		if (S.quips >= 1 && (bluff ? chance((int)(LDP[w].tell * 100)) : chance((int)(LDP[w].fake_tell * 100)))) {
			const Line *T = find_line(w, I_LD_TELL, 0, 0);
			if (T) { speaker = w; talk_expand(tl, sizeof tl, T->t); }
		}
		char both[300];
		if (tl[0]) snprintf(both, sizeof both, tl[0] == '(' ? "%s %s" : "%s %s", tl[0] == '(' ? tl : t, tl[0] == '(' ? t : tl);
		else snprintf(both, sizeof both, "%s", t);
		tidy(both);
		tb_push(s, both, 0, 0);
		LD.last_bluff = bluff;
	} else {
		/* your bid: someone may have something to say about it */
		LD.last_bluff = 0;
		if (S.quips >= 2 && chance(42)) {
			int r = ld_next(s);
			if (is_ai(r)) {
				u32 fl = ld_flags_common();
				float e = ld_est(r, q, f, NULL);
				if (e < 0.45f) fl |= LF_HIGH; else if (e > 0.85f) fl |= LF_SAFE;
				if (pq && q >= pq + 2) fl |= LF_JUMP;
				if (f == 1) fl |= LF_ONES;
				if (LD.you_slow) fl |= LF_SLOW;
				if (plat_ms() - LD.turn_at < 1500) fl |= LF_FAST;
				if (LD.bluffs[s]) fl |= LF_LIAR;
				ld_ctx_bid(q, f);
				/* only one of those things */
				u32 opts[10]; int no = 0; for (int b = 0; b < 16; b++) if (fl & (1u << b)) opts[no++] = 1u << b;
				u32 one = no ? opts[tr(no)] : 0;
				tb_say(r, I_LD_PBID, one, 0, 150);
			}
		}
	}
	LD.turn = ld_next(s); LD.turn_at = plat_ms(); LD.act_at = 0; LD.waited = 0;
	if (chance(13)) ld_banter();
}

static void ld_challenge(int s, int spot)
{
	LD.caller = s; LD.spot = spot;
	int b = LD.bidder;
	ld_ctx_bid(LD.bq, LD.bf);
	if (is_ai(s)) {
		u32 fl = ld_flags_common();
		if (LD.bluffs[b]) fl |= LF_LIAR;
		if (LD.lost_to[s][b]) fl |= LF_REVENGE;
		tb_say(s, spot ? I_LD_SPOT : I_LD_CALL, fl, b, 250);
	}
	music_sfx(SFX_LIAR, 0);
	plat_rumble(80);
	/* the one being called answers */
	if (is_ai(b) && S.quips >= 2 && chance(70)) tb_say(b, I_LD_CALLED, LD.last_bluff && chance(70) ? LF_BLUFF : 0, s, 150);
	LD.state = LS_REVEAL; LD.reveal_i = -1; LD.reveal_cnt = 0; LD.reveal_t = 0; LD.verdict = 0;
}

/* the AI's turn */
static void ld_ai_decide(int s)
{
	int w = tb_who[s];
	float lost = (float)(LD.start_dice - LD.dice[s]);
	float bluff = LDP[w].bluff + 0.04f * lost + (talk_mood(w) <= -1 ? 0.08f : 0);
	float pex = 0, pbid = 1;
	if (LD.bq) {
		pbid = ld_est(s, LD.bq, LD.bf, &pex);
		if (LD.bidder == 0 && LD.you_slow) pbid -= 0.05f;
		if (LD.bidder >= 0) pbid -= 0.04f * LD.bluffs[LD.bidder];
	}
	/* the best raise, and a believable lie */
	int bq = 0, bf = 0, lq = 0, lf = 0; float be = -1, lv = -1;
	int lead = -1, leadn = 0;   /* the face everyone's been bidding (Squid copies it) */
	{ int cnt[7] = { 0 }; for (int i = 0; i < LD.nbids; i++) cnt[LD.hf[i]]++; for (int f = 1; f <= 6; f++) if (cnt[f] > leadn) { leadn = cnt[f]; lead = f; } }
	for (int f = 1; f <= 6; f++) {
		int q0 = ld_qmin(s, f); if (!q0) continue;
		for (int q = q0; q <= q0 + 2 && q <= ld_total(); q++) {
			if (!ld_legal(s, q, f)) continue;
			float e = ld_est(s, q, f, NULL);
			int known = ld_count_seat(s, f);
			float sc = e + 0.03f * known - 0.025f * (q - q0) + (tr(100) - 50) / 900.0f;
			if (w == W_SQUID && f == lead) sc += 0.05f;
			if (w == W_VIPER && q == q0 + 1) sc += LDP[w].jump * 0.2f;
			if (!LD.bq && ld_wild() && f == 1) continue;
			if (sc > be) { be = sc; bq = q; bf = f; }
			if (known == 0 && e >= 0.32f && e > lv) { lv = e; lq = q; lf = f; }
		}
	}
	float noise = (tr(100) - 50) / 700.0f;
	if (LD.bq && S.ld_spot && pex > LDP[w].spot && pbid > 0.3f && pbid < 0.8f && chance(55)) { { LD.pk = 2 + 1; LD.pbl = 0; return; } }
	if (LD.bq && (pbid < LDP[w].call + noise || !bq || (be < 0.28f && pbid < 0.55f))) { { LD.pk = 2 + 0; LD.pbl = 0; return; } }
	if (lq && chance((int)(bluff * 100))) { { LD.pk = 1; LD.pq = lq; LD.pf = lf; LD.pbl = 1; return; } }
	if (!bq) { if (LD.bq) LD.pk = 2; else { LD.pk = 1; LD.pq = 1; LD.pf = 2; } LD.pbl = 0; return; }
	int known = ld_count_seat(s, bf);
	LD.pk = 1; LD.pq = bq; LD.pf = bf; LD.pbl = be < 0.5f && known == 0;
}

/* do what they decided */
static void ld_ai_exec(int s)
{
	if (LD.pk == 1) ld_bid(s, LD.pq, LD.pf, LD.pbl);
	else if (LD.pk >= 2) ld_challenge(s, LD.pk == 3);
	LD.pk = 0;
}

/* ---- the reveal ---- */
static void ld_reveal_tick(void)
{
	u64 now = plat_ms();
	if (LD.reveal_i < 0) {
		if (tb_busy()) return;
		LD.reveal_i = 0; LD.reveal_t = now + 300;
		return;
	}
	if (LD.reveal_i < tb_n) {
		if (now < LD.reveal_t) return;
		int s = LD.reveal_i;
		if (ld_alive(s)) {
			int c = ld_count_seat(s, LD.bf);
			for (int k = 0; k < c; k++) music_sfx(SFX_TICK, 120 + k * 110);
			LD.reveal_cnt += c;
			music_sfx(SFX_FLIP, 0);
			LD.reveal_t = now + (520 + c * 110) * anim_k() / 100;
		}
		LD.reveal_i++;
		return;
	}
	if (!LD.verdict) {
		if (now < LD.reveal_t + 300) return;
		LD.verdict = 1; LD.state_at = now;
		LD.cnt = LD.reveal_cnt;
		int b = LD.bidder, c = LD.caller;
		int ok = LD.cnt >= LD.bq;
		LD.loser = -1; LD.gainer = -1;
		ld_ctx_bid(LD.bq, LD.bf); ctx_cnt = LD.cnt;
		u32 fl = ld_flags_common();
		if (abs(LD.cnt - LD.bq) == 1) fl |= LF_CLOSE; else if (abs(LD.cnt - LD.bq) >= 3) fl |= LF_WAYOFF;
		if (LD.spot) {
			if (LD.cnt == LD.bq) {
				LD.gainer = c;
				if (LD.dice[c] < LD.start_dice) LD.dice[c]++;
				music_sfx(SFX_BIGWIN, 0); plat_rumble(200);
				if (c == 0) achieve(AC_SPOTON);
				tb_say(c, I_LD_SPOTWIN, fl, b, 300);
				int w2 = 1 + tr(tb_n - 1); if (w2 != c && ld_alive(w2)) tb_say(w2, I_LD_WATCH, fl | LF_SAFE, c, 300);
			} else {
				LD.loser = c;
				music_sfx(SFX_LOSE, 0);
				tb_say(c, I_LD_SPOTFAIL, fl, b, 300);
				if (is_ai(b) && chance(60)) tb_say(b, I_LD_GLOAT, fl, c, 300);
			}
		} else {
			if (ok) { LD.loser = c; LD.honest[b]++; } else { LD.loser = b; LD.bluffs[b]++; }
			int win = LD.loser == c ? b : c;
			music_sfx(LD.loser == 0 ? SFX_LOSE : win == 0 ? SFX_WIN : SFX_REEL, 0);
			plat_rumble(LD.loser == 0 ? 250 : 60);
			LD.lost_to[LD.loser][win]++;
			/* the loser speaks, the winner rubs it in, somebody else chimes in */
			if (LD.loser == b) tb_say(b, I_LD_CAUGHT, fl, c, 300);
			else tb_say(c, I_LD_WRONG, fl, b, 300);
			if (chance(70)) tb_say(win, I_LD_GLOAT, fl | (LD.lost_to[win][LD.loser] ? LF_REVENGE : 0), LD.loser, 300);
			if (chance(45)) {
				int by[TB_MAX], nb = 0; for (int s = 1; s < tb_n; s++) if (ld_alive(s) && s != b && s != c) by[nb++] = s;
				if (nb) tb_say(by[tr(nb)], I_LD_WATCH, fl, LD.loser, 300);
			}
		}
		if (LD.loser >= 0) {
			LD.dice[LD.loser]--;
			int L = LD.loser;
			if (!LD.dice[L]) {
				LD.out_order[LD.nout++] = L;
				if (L > 0) tb_say(L, I_LD_OUT, fl, -1, 400);
				int by[TB_MAX], nb = 0; for (int s = 1; s < tb_n; s++) if (ld_alive(s)) by[nb++] = s;
				if (nb) tb_say(by[tr(nb)], L == 0 ? I_LD_YOUOUT : I_LD_SEEOUT, fl, L, 400);
			} else if (LD.dice[L] == 1 && S.ld_pali && !LD.pali_used[L] && ld_nalive() > 2) {
				LD.pali_used[L] = 1; LD.pali_next = 1;   /* next round */
				if (L > 0) tb_say(L, I_LD_PALI, fl | LF_LAST, -1, 300);
			}
		}
		LD.state = LS_AFTER; LD.go_on = 0;
		ui_set_focus(9565);
	}
}

/* you went out and left before the end: that's a loss */
static void ld_record_out(void)
{
	if (LD.done_paid) return;
	LD.done_paid = 1; S.ld_played++; S.played++; S.lost++;
	style_note(LD.buyin, -1); track_round(-LD.buyin); gstat(-LD.buyin); stats_save(); config_save();
}

/* ---- the end ---- */
static void ld_finish(void)
{
	int winner = -1; for (int s = 0; s < tb_n; s++) if (ld_alive(s)) winner = s;
	LD.state = LS_OVER; LD.state_at = plat_ms();
	if (LD.done_paid) return;
	LD.done_paid = 1;
	int net = -LD.buyin;
	int second = LD.nout ? LD.out_order[LD.nout - 1] : -1;
	if (winner == 0) net = LD.pot - LD.buyin;
	else if (tb_n >= 4 && second == 0) net = 0;   /* second place gets the buy-in back */
	cfg.chips += net + LD.buyin;
	LD.net = net;
	S.ld_played++; if (winner == 0) { S.ld_won++; achieve(AC_LIAR); }
	style_note(LD.buyin, net > 0 ? 1 : net < 0 ? -1 : 0);
	track_round(net); gstat(net);
	if (winner == 0) S.won++; else if (net < 0) S.lost++; else S.pushed++;
	S.played++;
	if (net > S.best_win) S.best_win = net;
	if (cfg.chips > S.max_chips) S.max_chips = cfg.chips;
	stats_save(); config_save();
	if (is_ai(winner)) tb_say(winner, I_LD_WIN, 0, -1, 300);
	else if (winner == 0 && !tb_p2) {
		int a = 1 + tr(tb_n - 1); tb_say(a, I_LD_YOUWIN, 0, 0, 300);
		if (tb_n > 2 && chance(60)) { int b2 = 1 + tr(tb_n - 1); if (b2 != a) tb_say(b2, I_LD_YOUWIN, 0, 0, 300); }
	}
}

static void ld_start(void)
{
	if (!big_ok(LD_BUY[S.ld_buy % 5], ld_start)) return;
	/* who's playing */
	tb_n = 1; tb_who[0] = -1; tb_p2 = S.p2; tb_view = 0; tb_hand = -1;
	if (tb_p2) tb_who[tb_n++] = -2;   /* Player 2 plays on the house */
	for (int w = 0; w < W_N && tb_n < TB_MAX; w++) if (S.ld_opp & (1 << w)) tb_who[tb_n++] = w;
	if (tb_n < 2) { tb_who[1] = W_MOTH; tb_n = 2; S.ld_opp = 1; }
	tb_layout(); tb_clear();
	memset(&LD, 0, sizeof LD);
	LD.start_dice = S.ld_dice < 3 || S.ld_dice > 5 ? 5 : S.ld_dice;
	for (int s = 0; s < tb_n; s++) LD.dice[s] = LD.start_dice;
	LD.buyin = LD_BUY[S.ld_buy % 5]; LD.pot = LD.buyin * tb_n;
	cfg.chips -= LD.buyin;
	LD.starter = tr(tb_n);
	for (int s = 1; s < tb_n; s++) if (is_ai(s)) tonight_start(tb_who[s]);
	view = V_LIAR; persona = -1;
	S.last_game = -1; stats_save(); config_save();
	ld_new_round();
	LD.state_at = plat_ms();
	/* everyone says hello, some to each other */
	if (S.quips) {
		int order[TB_MAX], n = 0; for (int s = 1; s < tb_n; s++) if (is_ai(s)) order[n++] = s;
		for (int i = n - 1; i > 0; i--) { int j = tr(i + 1); int t = order[i]; order[i] = order[j]; order[j] = t; }
		int say_n = n < 3 ? n : 2 + tr(n - 1);
		for (int i = 0; i < say_n; i++) tb_say(order[i], I_LD_HELLO, 0, chance(35) && n > 1 ? order[(i + 1) % n] : -1, 200);
	}
	ui_set_focus(9540);
}

/* ---- drawing ---- */
static void die_draw(int cx, int cy, int sz, int face, int a, int hl)
{
	int x = cx - sz / 2, y = cy - sz / 2, r = sz / 5;
	gfx_round(x + 1, y + sz / 10 + 1, sz, sz, r, RGB(0, 0, 0), 90 * a / 255);
	gfx_round_grad(x, y, sz, sz, r, RGB(253, 251, 245), RGB(212, 205, 192), a);
	gfx_round_line(x, y, sz, sz, r, 1, RGB(140, 130, 116), a);
	if (hl) { gfx_round_line(x - 3, y - 3, sz + 6, sz + 6, r + 3, 2, RGB(255, 214, 90), a); gfx_round(x - 5, y - 5, sz + 10, sz + 10, r + 5, RGB(255, 214, 90), 40 * a / 255); }
	static const s8 PX[7][6][2] = {
		{ { 0 } }, { { 0, 0 } }, { { -1, -1 }, { 1, 1 } }, { { -1, -1 }, { 0, 0 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } }, { { -1, -1 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } } };
	int np = face >= 1 && face <= 6 ? face : 0, off = sz * 27 / 100, pr = sz / 10 < 2 ? 2 : sz / 10;
	u32 pc = face == 1 ? RGB(190, 20, 30) : RGB(30, 26, 30);
	for (int i = 0; i < np; i++) gfx_circle(cx + PX[face][i][0] * off, cy + PX[face][i][1] * off, face == 1 ? pr + 2 : pr, pc, a);
	gfx_tri(x + 2, y + 2, x + sz / 2, y + 2, x + 2, y + sz / 2, RGB(255, 255, 255), 50 * a / 255);
}
/* a leather dice cup, upside down on the table */
static void cup_draw(int cx, int base, int lift, int shake, int a)
{
	int h = 46, top = 30, bot = 44, y0 = base - h - lift;
	cx += shake;
	if (!lift) gfx_round(cx - bot / 2 - 2, base - 6, bot + 8, 12, 6, RGB(0, 0, 0), 90 * a / 255);
	for (int i = 0; i < h; i += 2) {
		int wdt = top + (bot - top) * i / h;
		u32 c = mix(RGB(120, 70, 40), RGB(70, 38, 20), i * 256 / h);
		gfx_rect(cx - wdt / 2, y0 + i, wdt, 2, c, a);
		gfx_rect(cx - wdt / 2 + wdt / 6, y0 + i, wdt / 7, 2, RGB(255, 230, 200), 38 * a / 255);   /* shine */
	}
	gfx_round(cx - top / 2 - 1, y0 - 3, top + 2, 7, 3, RGB(90, 52, 28), a);
	gfx_round(cx - bot / 2 - 2, y0 + h - 4, bot + 4, 7, 3, RGB(60, 32, 16), a);
	for (int k = 0; k < 6; k++) gfx_rect(cx - bot / 2 + 3 + k * (bot - 4) / 6, y0 + h - 2, 3, 1, RGB(230, 196, 110), 140 * a / 255);   /* stitching */
}

static void ld_seat_draw(int s)
{
	int x = tb_x[s], y = tb_y[s];
	u64 now = plat_ms();
	int w = tb_who[s];
	int alive = ld_alive(s), turn = LD.state == LS_TURN && LD.turn == s;
	int revealed = (LD.state == LS_REVEAL && LD.reveal_i > s) || LD.state == LS_AFTER || (LD.state == LS_OVER && alive);
	if (s == 0) {   /* you: just your cup and name, your dice are shown big below */
		char t[64]; const char *who = tb_p2 ? "Player 1" : "You";
		if (turn && tb_p2) snprintf(t, sizeof t, "Player 1's turn");
		else if (turn) snprintf(t, sizeof t, LD.bq ? "Your turn: raise the bid, or call Liar!" : "Your turn: make the first bid");
		else if (alive) snprintf(t, sizeof t, "%s  -  %d %s", who, LD.dice[0], LD.dice[0] == 1 ? "die" : "dice");
		else snprintf(t, sizeof t, "%s  -  out", who);
		int tw = text_w(F_SMALL, t) + 24;
		gfx_round(x - tw / 2, y - 4, tw, 22, 11, RGB(0, 0, 0), 170);
		text_c(F_SMALL, x, y - 3, t, turn ? RGB(255, 214, 90) : alive ? RGB(255, 240, 200) : C_DIM, 255);
		return;
	}
	u32 acc = w < 0 ? RGB(255, 214, 90) : TB_ACC[w];
	if (turn) {
		int g = 90 + (int)(sinf(now / 180.0f) * 40);
		gfx_circle(x, y + 4, 40, acc, g / 3);
		gfx_ring(x, y + 4, 38, 2, acc, g);
	}
	/* the cup: shaking while rolling, lifted at the reveal */
	int shake = 0;
	if (LD.state == LS_ROLL && alive && now < LD.roll_t[s] + 700 && now > LD.roll_t[s] - 700) shake = (int)(sinf(now / 25.0f) * 4);
	int lift = revealed ? 44 : 0;
	if (revealed && LD.state == LS_REVEAL && LD.reveal_i == s + 1) { u64 el = now - (LD.reveal_t - 520); lift = el < 200 ? (int)(el * 44 / 200) : 44; }
	if (alive) {
		if (revealed) {
			int n = LD.dice[s] + (LD.state == LS_AFTER && LD.loser == s ? 1 : 0);
			if (n > 5) n = 5;
			for (int i = 0; i < n; i++) {
				int dx = x - (n - 1) * 10 + i * 20;
				int gone = LD.state == LS_AFTER && LD.loser == s && i == n - 1;
				int fa = gone ? (int)(255 - (now - LD.state_at) / 6) : 255; if (fa < 0) fa = 0;
				die_draw(dx, y + 8, 18, LD.d[s][i], fa, LD.bf && ld_match(LD.d[s][i], LD.bf));
			}
			/* how many of the bid face this cup had */
			int c = 0; for (int i = 0; i < n && i < 5; i++) c += ld_match(LD.d[s][i], LD.bf);
			char cb[8]; snprintf(cb, sizeof cb, "+%d", c);
			gfx_circle(x + 34, y - 26, 12, c ? RGB(255, 200, 60) : RGB(80, 80, 90), 240);
			text_c(F_SMALL, x + 34, y - 35, cb, c ? RGB(40, 30, 10) : RGB(220, 220, 220), 255);
		}
		cup_draw(x, y + 20, lift, shake, 255);
	} else {
		cup_draw(x, y + 20, 0, 0, 90);
	}
	/* name plate with the dice they have left */
	gfx_round(x - 44, y + 26, 88, 32, 10, RGB(0, 0, 0), 160);
	gfx_round_line(x - 44, y + 26, 88, 32, 10, 1, acc, alive ? 200 : 60);
	text_c(F_SMALL, x, y + 26, tb_name(s), alive ? shade(acc, 40) : C_DIM, 255);
	for (int i = 0; i < LD.start_dice; i++) gfx_round(x - LD.start_dice * 6 + i * 12 + 1, y + 46, 9, 9, 2, i < LD.dice[s] ? RGB(240, 234, 220) : RGB(70, 60, 60), 255);
	/* thinking */
	if (turn && is_ai(s) && !tb_busy()) for (int k = 0; k < 3; k++) gfx_circle(x - 10 + k * 10, y - 38, 3, RGB(255, 255, 255), ((now / 250) % 3) == (u64)k ? 255 : 90);
}

static const char *const LD_FP[7] = { "", "ones", "twos", "threes", "fours", "fives", "sixes" };
/* the middle of the table: the bid, in plain words */
static void ld_bid_panel(void)
{
	int cx = 320, cy = 222;
	u64 now = plat_ms();
	if (LD.state == LS_ROLL) {
		if (ld_alive(ME) && !LD.rolled) {
			int p = 200 + (int)(sinf(now / 150.0f) * 55);
			text_c(F_BODYB, cx, cy - 24, tb_p2 ? (ME ? "Player 2: shake the Wii Remote" : "Player 1: shake the Wii Remote") : "Shake your Wii Remote", RGB(255, 240, 200), p);
			text_c(F_SMALL, cx, cy + 4, "(or press A) to roll your dice", RGB(220, 210, 190), 220);
		} else text_c(F_BODYB, cx, cy - 12, "Rolling...", RGB(255, 240, 200), 220);
		return;
	}
	if (LD.state == LS_AFTER) return;   /* the result panel covers this */
	/* house rules for this round */
	char rule[48];
	snprintf(rule, sizeof rule, "%s", LD.pali ? "PALIFICO: no wilds, face is locked" : S.ld_wild ? "Ones are wild" : "No wilds");
	text_c(F_SMALL, cx, cy - 78, rule, LD.pali ? RGB(255, 150, 150) : RGB(190, 220, 200), 200);
	if (LD.bq) {
		gfx_round(cx - 120, cy - 56, 240, 112, 16, RGB(0, 0, 0), 140);
		char t[48]; snprintf(t, sizeof t, "%s bid:", LD.bidder == YOU ? "You" : tb_name(LD.bidder));
		text_c(F_SMALL, cx, cy - 54, t, is_ai(LD.bidder) ? shade(TB_ACC[tb_who[LD.bidder]], 40) : RGB(255, 240, 200), 255);
		char q[8]; snprintf(q, sizeof q, "%d", LD.bq);
		text_c(F_TITLE, cx - 34, cy - 36, q, RGB(255, 255, 255), 255);
		text_c(F_BODYB, cx - 4, cy - 26, "x", RGB(200, 200, 200), 255);
		die_draw(cx + 36, cy - 16, 36, LD.bf, 255, 0);
		char plain[64]; snprintf(plain, sizeof plain, "at least %d %s under ALL the cups", LD.bq, LD.bq == 1 ? LD_FP[LD.bf] : LD_FP[LD.bf]);
		text_c(F_SMALL, cx, cy + 10, plain, RGB(235, 235, 225), 255);
		if (ld_alive(ME) && LD.state == LS_TURN && tb_hand < 0 && (!tb_p2 || LD.turn == ME)) {
			char h[64]; int mine = ld_count_seat(ME, LD.bf);
			if (tb_p2) snprintf(h, sizeof h, "Dice on the table: %d.  %s has %d of them.", ld_total(), tb_name(ME), mine);
			else snprintf(h, sizeof h, "Dice on the table: %d.  You have %d of them.", ld_total(), mine);
			text_c(F_SMALL, cx, cy + 30, h, RGB(255, 214, 90), 255);
			if (S.helper && LD.turn == ME) {   /* learning help: the odds, as you see them */
				float pt = ld_est(ME, LD.bq, LD.bf, NULL), pm = ld_legal(ME, LD.sel_q, LD.sel_f) ? ld_est(ME, LD.sel_q, LD.sel_f, NULL) : -1;
				char o[80];
				if (pm >= 0) snprintf(o, sizeof o, "Odds it's true: %d%%.  Your bid %dx%d holds: %d%%", (int)(pt * 100), LD.sel_q, LD.sel_f, (int)(pm * 100));
				else snprintf(o, sizeof o, "Odds this bid is true: %d%%", (int)(pt * 100));
				gfx_round(cx - 170, cy + 58, 340, 22, 11, RGB(20, 40, 70), 220);
				text_c(F_SMALL, cx, cy + 59, o, RGB(170, 220, 255), 255);
			}
		}
	} else if (LD.state == LS_TURN) {
		char t[64]; snprintf(t, sizeof t, LD.turn == YOU ? "Your turn to open the bidding" : "%s opens the bidding", tb_name(LD.turn));
		text_c(F_BODYB, cx, cy - 20, t, RGB(255, 240, 200), 230);
		if (is_human(LD.turn)) text_c(F_SMALL, cx, cy + 8, "Guess how many of one face are under ALL the cups", RGB(220, 210, 190), 230);
		char h[48]; snprintf(h, sizeof h, "Dice on the table: %d", ld_total()); text_c(F_SMALL, cx, cy + 28, h, RGB(255, 214, 90), 230);
	}
	if (LD.state == LS_REVEAL) {
		gfx_round(cx - 130, cy + 60, 260, 34, 17, RGB(0, 0, 0), 190);
		char t[64]; snprintf(t, sizeof t, "Counting %s... %d", LD_FP[LD.bf], LD.reveal_cnt);
		text_c(F_BODYB, cx, cy + 63, t, RGB(255, 240, 200), 255);
	}
}

/* this round's bids, top left, so you can follow along */
static void ld_bid_log(void)
{
	if (!LD.nbids || LD.state == LS_ROLL) return;
	int from = LD.nbids > 7 ? LD.nbids - 7 : 0, n = LD.nbids - from;
	gfx_round(14, 48, 128, 22 + n * 16, 10, RGB(0, 0, 0), 150);
	text(F_SMALL, 22, 49, "Bids this round", RGB(200, 200, 190), 255);
	for (int i = from; i < LD.nbids; i++) {
		char t[40]; snprintf(t, sizeof t, "%s  %d x %d", LD.hs[i] == YOU ? "You" : tb_name(LD.hs[i]), LD.hq[i], LD.hf[i]);
		int last = i == LD.nbids - 1;
		text(F_SMALL, 22, 66 + (i - from) * 16, t, last ? RGB(255, 214, 90) : RGB(230, 230, 225), last ? 255 : 200);
	}
}

/* after the cups come up: what happened, in plain words, until you press on */
static void ld_result_panel(int blocked)
{
	if (LD.state != LS_AFTER) return;
	int b0 = LD.bidder, c0 = LD.caller, you0 = b0 == YOU || c0 == YOU;
	int x = 130, y = you0 ? 70 : 96, w = 380, h = you0 ? 268 : 232;
	gfx_round(x + 3, y + 5, w, h, 18, RGB(0, 0, 0), 120);
	gfx_round(x, y, w, h, 18, RGB(14, 12, 20), 238);
	int b = LD.bidder, c = LD.caller;
	int right = LD.spot ? LD.cnt == LD.bq : LD.cnt >= LD.bq;   /* was the bid right (or exact)? */
	/* you, first: were you right? */
	int you_in = b == YOU || c == YOU, you_right = 0;
	if (c == YOU) you_right = LD.spot ? right : !right;
	else if (b == YOU) you_right = LD.spot ? !right : right;
	u32 edge = you_in ? (you_right ? RGB(80, 220, 120) : RGB(240, 80, 80)) : RGB(230, 196, 110);
	gfx_round_line(x, y, w, h, 18, 3, edge, 255);
	char t[96];
	int ty = y + 8;
	if (you_in) { text_c(F_TITLE, 320, ty, you_right ? "You were right!" : "You were wrong", you_right ? RGB(110, 240, 140) : RGB(255, 110, 110), 255); ty += 40; }
	snprintf(t, sizeof t, "%s called %s on %s bid", c == YOU ? "You" : tb_name(c), LD.spot ? "SPOT ON" : "LIAR", b == YOU ? "your" : b >= 0 ? "" : "the");
	if (b >= 0 && b != YOU) { char n2[40]; snprintf(n2, sizeof n2, "%s's", tb_name(b)); snprintf(t, sizeof t, "%s called %s on %s bid", c == YOU ? "You" : tb_name(c), LD.spot ? "SPOT ON" : "LIAR", n2); }
	text_c(F_BODY, 320, ty, t, RGB(235, 230, 220), 255); ty += 26;
	/* the bid vs what was there */
	text(F_SMALL, x + 40, ty + 6, "The bid:", RGB(200, 200, 190), 255);
	char q[16]; snprintf(q, sizeof q, "%d x", LD.bq); text_r(F_BODYB, x + 214, ty + 2, q, RGB(255, 255, 255), 255);
	die_draw(x + 236, ty + 14, 24, LD.bf, 255, 0);
	ty += 30;
	text(F_SMALL, x + 40, ty + 6, "On the table:", RGB(200, 200, 190), 255);
	snprintf(q, sizeof q, "%d x", LD.cnt); text_r(F_BODYB, x + 214, ty + 2, q, right ? RGB(120, 240, 150) : RGB(255, 120, 120), 255);
	die_draw(x + 236, ty + 14, 24, LD.bf, 255, 1);
	if (S.ld_wild && !LD.pali && LD.bf != 1) text(F_SMALL, x + 262, ty + 6, "(with ones)", RGB(190, 190, 180), 220);
	ty += 32;
	/* the verdict */
	const char *v = LD.spot ? (right ? "Exactly right!" : "Not exact") : (right ? "The bid was TRUE" : "The bid was a LIE");
	text_c(F_BODYB, 320, ty, v, RGB(255, 214, 90), 255);
	ty += 26;
	if (LD.gainer >= 0) snprintf(t, sizeof t, "%s get%s a die back", LD.gainer == YOU ? "You" : tb_name(LD.gainer), LD.gainer == YOU ? "" : "s");
	else if (LD.loser >= 0) snprintf(t, sizeof t, "%s lose%s a die%s", LD.loser == YOU ? "You" : tb_name(LD.loser), LD.loser == YOU ? "" : "s", LD.dice[LD.loser] == 0 ? " and %s out" : "");
	if (LD.loser >= 0 && LD.dice[LD.loser] == 0) { char t2[96]; snprintf(t2, sizeof t2, t, LD.loser == YOU ? "you're" : "is"); snprintf(t, sizeof t, "%s", t2); }
	text_c(F_BODY, 320, ty, t, RGB(255, 230, 190), 255);
	/* carry on */
	if (ld_alive(0) || LD.state == LS_AFTER) {
		int f = ui_ibutton(9565, 235, y + h - 46, 170, 38, IC_PLAY, ld_nalive() <= 1 ? "Finish" : "Next round", ST_NORMAL);
		if (!blocked && ((f & UI_CLICK) || (in->down & BTN_A)) && plat_ms() > LD.state_at + 700) LD.go_on = 1;
	}
}

/* your dice and your controls */
static void ld_you_panel(int blocked)
{
	static int seen = -1; if (seen != ME) { seen = ME; if (LD.state == LS_TURN) ld_best_sel(); }
	int alive = ld_alive(ME);
	gfx_round(18, 346, 218, 78, 14, RGB(0, 0, 0), 140);
	char yl[40]; if (tb_p2) snprintf(yl, sizeof yl, alive ? "%s's dice" : "%s is out", tb_name(ME)); else snprintf(yl, sizeof yl, alive ? "Your dice" : "You're out");
	text(F_SMALL, 30, 348, yl, RGB(255, 240, 200), 255);
	/* two people: your dice only show on your own turn, so the other one can't peek */
	if (alive && LD.state != LS_ROLL && tb_hand < 0 && (!tb_p2 || LD.state != LS_TURN || LD.turn == ME)) for (int i = 0; i < LD.dice[ME]; i++) die_draw(46 + i * 40, 390, 32, LD.d[ME][i], 255, LD.bf && ld_match(LD.d[ME][i], LD.bf) && LD.state != LS_TURN);
	else if (alive) for (int i = 0; i < LD.dice[ME]; i++) cup_draw(46 + i * 40, 408, 0, (int)(sinf(plat_ms() / 30.0f + i) * 2), 180);
	int x0 = 250;
	if (LD.state == LS_OVER) {
		int winner = -1; for (int s = 0; s < tb_n; s++) if (ld_alive(s)) winner = s;
		char t[64];
		if (winner == 0 && !tb_p2) snprintf(t, sizeof t, "You win! +%d chips", LD.net);
		else if (is_human(winner)) snprintf(t, sizeof t, winner ? "Player 2 wins!" : "Player 1 wins! +%d chips", LD.net);
		else snprintf(t, sizeof t, "%s wins.  %s", tb_name(winner), LD.net == 0 ? "Buy-in back (2nd)" : "");
		text_c(F_BODYB, 433, 348, t, winner == 0 ? RGB(120, 240, 150) : RGB(255, 230, 190), 255);
		if ((ui_ibutton(9560, x0, 382, 180, 40, IC_PLAY, "Play again", ST_NORMAL) & UI_CLICK) && !blocked) {
			if (cfg.chips < LD_BUY[S.ld_buy % 5]) ui_toast("Not enough chips for the buy-in");
			else { ld_start(); }
		}
		if ((ui_ibutton(9561, x0 + 188, 382, 178, 40, IC_BACK, "Leave table", ST_NORMAL) & UI_CLICK) && !blocked) { view = V_FLOOR; ui_set_focus(8200); tb_clear(); }
		return;
	}
	if (!ld_hum_alive()) {
		text(F_SMALL, x0, 352, "Watching the rest of the game", RGB(220, 210, 190), 255);
		if ((ui_ibutton(9562, x0, 382, 180, 40, IC_PLAY, LD.fast ? "Normal speed" : "Speed up", ST_NORMAL) & UI_CLICK) && !blocked) LD.fast = !LD.fast;
		if ((ui_ibutton(9561, x0 + 188, 382, 178, 40, IC_BACK, "Leave table", ST_NORMAL) & UI_CLICK) && !blocked) { ld_record_out(); view = V_FLOOR; ui_set_focus(8200); tb_clear(); }
		return;
	}
	int mine = LD.state == LS_TURN && LD.turn == ME && alive && tb_hand < 0;
	/* quantity */
	if (mine && !ld_legal(ME, LD.sel_q, LD.sel_f)) { int q = ld_qmin(ME, LD.sel_f); if (q && LD.sel_q < q) LD.sel_q = q; }
	int st = mine ? ST_NORMAL : ST_GHOST;
	if ((ui_button(9540, x0, 348, 36, 34, "-", st) & UI_CLICK) && mine && LD.sel_q > 1) LD.sel_q--;
	gfx_round(x0 + 40, 348, 44, 34, 10, RGB(0, 0, 0), 150);
	if (LD.sel_q < 1) LD.sel_q = 1;
	char qs[8]; snprintf(qs, sizeof qs, "%d", LD.sel_q); text_c(F_BODYB, x0 + 62, 352, qs, RGB(255, 255, 255), 255);
	if ((ui_button(9541, x0 + 88, 348, 36, 34, "+", st) & UI_CLICK) && mine && LD.sel_q < ld_total()) LD.sel_q++;
	/* faces */
	for (int f = 1; f <= 6; f++) {
		int fx = x0 + 132 + (f - 1) * 39;
		int fl = ui_item(9541 + f, fx, 346, 36, 38);
		int ok = mine && ld_legal(ME, LD.sel_q, f);
		die_draw(fx + 18, 365, 30, f, ok || !mine ? 255 : 110, f == LD.sel_f);
		if (fl & UI_FOCUS) ui_focus_ring(fx, 346, 36, 38, 8);
		if ((fl & UI_CLICK) && mine) { LD.sel_f = f; int q = ld_qmin(ME, f); if (q && !ld_legal(ME, LD.sel_q, f)) LD.sel_q = q; }
	}
	int legal = mine && ld_legal(ME, LD.sel_q, LD.sel_f);
	char bl[32]; snprintf(bl, sizeof bl, "Bid %d x %d", LD.sel_q, LD.sel_f < 1 ? 1 : LD.sel_f);
	int do_bid = (ui_ibutton(9548, x0, 388, 130, 36, IC_CHECK, bl, legal ? ST_NORMAL : ST_GHOST) & UI_CLICK);
	int do_liar = (ui_button(9549, x0 + 136, 388, 110, 36, "Liar!", mine && LD.bq ? ST_NORMAL : ST_GHOST) & UI_CLICK);
	int do_spot = S.ld_spot && (ui_button(9550, x0 + 252, 388, 114, 36, "Spot on", mine && LD.bq ? ST_NORMAL : ST_GHOST) & UI_CLICK);
	if (blocked || LD.leave_open || !mine) return;
	if (in->down & BTN_UP && LD.sel_q < ld_total()) LD.sel_q++;
	if (in->down & BTN_DOWN && LD.sel_q > 1) LD.sel_q--;
	if (in->down & BTN_1) do_liar = 1;
	if (in->down & BTN_2) do_spot = S.ld_spot;
	if (in->down & BTN_PLUS) do_bid = 1;
	if (plat_ms() - LD.turn_at > 9000) LD.you_slow = 1;
	if (do_bid) { if (!legal) ui_toast(LD.pali && LD.dice[ME] > 1 ? "Palifico: raise the number, same face" : "That doesn't raise the bid"); else ld_bid(ME, LD.sel_q, LD.sel_f, 0); }
	else if (do_liar) { if (!LD.bq) ui_toast("Nobody has bid yet"); else ld_challenge(ME, 0); }
	else if (do_spot) { if (!LD.bq) ui_toast("Nobody has bid yet"); else ld_challenge(ME, 1); }
}

static void liar_game(int blocked)
{
	u64 now = plat_ms();
	/* the table */
	felt_draw(24, 58, 592, 282, 141, RGB(22, 86, 64), RGB(8, 40, 30), RGB(96, 54, 30));
	text(F_BODYB, 30, 16, "Liar's Dice", RGB(250, 244, 226), 255);
	{
		char t[48]; snprintf(t, sizeof t, "Pot %d", LD.pot);
		gfx_round(330, 16, 150, 30, 15, RGB(0, 0, 0), 120);
		chip_draw(348, 31, 11, 100, 0);
		text(F_BODYB, 366, 19, t, RGB(255, 214, 90), 255);
	}
	int hand = tb_hand >= 0;
	for (int s = 1; s < tb_n; s++) ld_seat_draw(s);
	ld_seat_draw(0);
	ld_bid_panel();
	ld_you_panel(blocked || hand);
	ld_bid_log();
	tb_pump();
	for (int s = 1; s < tb_n; s++) tb_bubble(s);
	ld_result_panel(blocked || hand);
	if (tb_handoff(blocked)) return;
	int speedk = LD.fast || !ld_hum_alive() ? 45 : S.speed == 0 ? 140 : S.speed == 2 ? 60 : 100;
	speedk = speedk * anim_k() / 100; if (speedk < 15) speedk = 15;

	/* the game moves on */
	if (!blocked && !LD.leave_open) switch (LD.state) {
	case LS_ROLL: {
		int shook = in->shake > 90 || (in->down & BTN_A);
		if (ld_alive(ME) && !LD.rolled && shook) { LD.rolled = 1; plat_rumble(120); music_sfx(SFX_ROLL, 0); LD.roll_t[ME] = now; }
		int ai_done = 1; for (int s = 1; s < tb_n; s++) if (ld_alive(s) && is_ai(s)) { if (now < LD.roll_t[s]) ai_done = 0; else if (now < LD.roll_t[s] + 30) music_sfx(SFX_ROLL, 0); }
		if (LD.rolled && ai_done && now > LD.state_at + 900 && !tb_busy()) {
			LD.pali = LD.pali_next; LD.pali_next = 0;
			ld_roll_all();
		}
		break; }
	case LS_TURN:
		if (is_human(LD.turn)) {
			if (tb_need(LD.turn)) break;
			if (!LD.waited && now - LD.turn_at > 22000 && !tb_busy()) { LD.waited = 1; int s = 1 + tr(tb_n - 1); if (ld_alive(s)) tb_say(s, I_LD_WAIT, 0, 0, 0); }
			break;
		}
		if (!LD.act_at) {
			int w = tb_who[LD.turn];
			ld_ai_decide(LD.turn);
			/* how long they take is a tell too: some rush a lie, some stall on one */
			static const u8 LIE_PACE[W_N] = { 100, 72, 140, 88, 128, 80 };
			int pace = LD.pbl && chance(60) ? LIE_PACE[w] : 100;
			if (!LD.pbl && chance(12)) pace = LIE_PACE[w];   /* ...and sometimes when honest */
			LD.act_at = now + (u64)((LDP[w].think * (70 + tr(70)) / 100) * speedk / 100 * pace / 100);
		}
		if (now >= LD.act_at && !tb_busy()) ld_ai_exec(LD.turn);
		break;
	case LS_REVEAL: ld_reveal_tick(); break;
	case LS_AFTER:
		if ((LD.go_on || (!ld_hum_alive() && now > LD.state_at + 4500 * speedk / 100)) && (!tb_busy() || LD.go_on)) {
			LD.go_on = 0;
			if (ld_nalive() <= 1) ld_finish();
			else {
				int L = LD.loser >= 0 ? LD.loser : LD.gainer;
				LD.starter = L >= 0 && ld_alive(L) ? L : ld_next(L >= 0 ? L : 0);
				ld_new_round();
			}
		}
		break;
	default: break;
	}
	/* leaving mid-game */
	if (LD.leave_open) {
		static const char *IT[2] = { "Leave (lose your buy-in)", "Keep playing" };
		static const int IC2[2] = { IC_BACK, IC_PLAY };
		int k = menu_popup(&LD.leave_open, IT, IC2, 2, "Leave the table?");
		if (k == 0) {
			if (!LD.done_paid) { LD.done_paid = 1; S.ld_played++; S.played++; S.lost++; style_note(LD.buyin, -1); track_round(-LD.buyin); stats_save(); config_save(); }
			view = V_FLOOR; ui_set_focus(8200); tb_clear();
		}
	}
	ui_footer_begin();
	if (LD.state == LS_TURN && LD.turn == ME) { ui_footer_hint("+", "Bid"); ui_footer_hint("1", "Liar!"); if (S.ld_spot) ui_footer_hint("2", "Spot on"); }
	else if (LD.state == LS_ROLL && !LD.rolled) ui_footer_hint("A", "Roll");
	ui_footer_hint("HOME", "Leave");
	if (blocked || LD.leave_open) return;
	if (in->down & (BTN_HOME | BTN_B)) {
		if (LD.state == LS_OVER || !ld_hum_alive()) { ld_record_out(); view = V_FLOOR; ui_set_focus(8200); tb_clear(); }
		else { LD.leave_open = 1; ui_set_focus(9001); }
	}
}
