/*
   phase1.inc - more of the casino floor: Casino War, the Big Wheel and Keno.
   Included in casino_games.inc before the floor. WiiRremote Plus by Habex. GPL-3.0.
*/
static void open_simple(int ps, int v, int focus)
{
	msg[0] = 0;
	persona = ps; anger = 0; pw_streak = dw_streak = 0;
	if (S.quips) talk_sit(persona + 1);
	S.last_game = -1; stats_save();
	view = v; ui_set_focus(focus);
}

/* ================================================================== */
/* CASINO WAR: your card against the dealer's. Tie? Go to war.        */
/* ================================================================== */
static int war_state, war_bet, war_tie, war_tie_on, war_warbet;   /* state 0 bet, 1 dealt/tie choice, 2 done */
static Card war_p[2], war_d[2]; static int war_n;
static int war_rank(u8 c) { int r = c % 13; return r == 0 ? 14 : r + 1; }
static void war_open(void) { war_state = 0; war_n = 0; open_simple(PS_MIMIC, V_WAR, 9700); }
static void war_settle(int net, int stake, const char *m, int tone, u32 flags)
{
	say(tone, m);
	memset(&ev, 0, sizeof ev); ev.game = 6; ev.flags = flags;
	war_state = 2;
	round_done(net, stake);
}
static void war_deal(void)
{
	int bet = chip_val(), tie = war_tie_on ? chip_val() / 5 + 1 : 0;
	if (bet + tie > cfg.chips) { ui_toast("Not enough chips"); return; }
	if (!big_ok(bet + tie, war_deal)) return;
	cfg.chips -= bet + tie; war_bet = bet; war_tie = tie; war_warbet = 0;
	S.bet_k[0] = chip_sel;
	next_t = plat_ms();
	war_p[0].c = rnd() % 52; war_p[0].t = next_t; music_sfx(SFX_CARD, 0); next_t += speed_ms();
	war_d[0].c = rnd() % 52; war_d[0].t = next_t; music_sfx(SFX_CARD, speed_ms()); next_t += speed_ms();
	war_n = 1; war_state = 1; msg[0] = 0;
	int p = war_rank(war_p[0].c), d = war_rank(war_d[0].c);
	if (p != d) {
		int back = war_tie ? 0 : 0;
		char m[64];
		if (p > d) { cfg.chips += bet * 2 + back; snprintf(m, sizeof m, "Your card is higher. You win %d", bet - tie); war_settle(bet - tie, bet + tie, m, 1, 0); }
		else { snprintf(m, sizeof m, "Dealer's card is higher. You lose %d", bet + tie); war_settle(-bet - tie, bet + tie, m, -1, 0); }
	} else {
		/* a tie: the tie bet pays 10 to 1, then you choose */
		if (tie) cfg.chips += tie * 11;
		ui_set_focus(9705);
	}
}
static void war_go(int to_war)
{
	int tie_win = war_tie ? war_tie * 10 : -0;
	if (!to_war) {
		cfg.chips += war_bet / 2;
		char m[64]; snprintf(m, sizeof m, "You gave up. Half your bet back");
		war_settle(-(war_bet - war_bet / 2) + tie_win, war_bet + war_tie, m, -1, F_TIE);
		return;
	}
	if (war_bet > cfg.chips) { ui_toast("Not enough chips to go to war"); return; }
	cfg.chips -= war_bet; war_warbet = war_bet;
	/* three cards burned, then one each */
	next_t = plat_ms() + 300;
	for (int k = 0; k < 3; k++) music_sfx(SFX_CARD, 100 + k * 90);
	war_p[1].c = rnd() % 52; war_p[1].t = next_t; music_sfx(SFX_CARD, 300); next_t += speed_ms();
	war_d[1].c = rnd() % 52; war_d[1].t = next_t; music_sfx(SFX_CARD, 300 + speed_ms()); next_t += speed_ms();
	war_n = 2;
	int p = war_rank(war_p[1].c), d = war_rank(war_d[1].c);
	char m[64];
	if (p >= d) {
		/* win the war: the war bet pays even money, the first bet comes back */
		cfg.chips += war_bet * 3;
		snprintf(m, sizeof m, p == d ? "Another tie! War goes to you. You win %d" : "You win the war! You win %d", war_bet + tie_win);
		war_settle(war_bet + tie_win, war_bet * 2 + war_tie, m, 1, F_TIE | F_SPECIAL);
	} else {
		snprintf(m, sizeof m, "The dealer wins the war. You lose %d", war_bet * 2 - tie_win);
		war_settle(-war_bet * 2 + tie_win, war_bet * 2 + war_tie, m, -1, F_TIE);
	}
}
static void casino_war(int blocked)
{
	game_felt("Casino War", "Higher card wins. Tie? Go to war, or give up half.", RGB(30, 90, 130), RGB(12, 44, 70));
	text_c(F_SMALL, 430, 86, "DEALER", RGB(220, 230, 240), 200);
	text_c(F_SMALL, 430, 250, "YOU", RGB(220, 230, 240), 200);
	for (int i = 0; i < 2; i++) {
		int x = 300 + i * 150;
		if (i < war_n) { card_anim(&war_d[i], x + (i ? 0 : 98), 106, 0); card_anim(&war_p[i], x + (i ? 0 : 98), 150 + 120, 0); }
	}
	if (war_n == 0) { gfx_round(398, 106, CW, CH, 8, RGB(0, 0, 0), 60); gfx_round(398, 270, CW, CH, 8, RGB(0, 0, 0), 60); }
	if (war_n == 2) { text_c(F_BODYB, 350, 206, "WAR!", RGB(255, 90, 90), 255); }
	/* the tie bet */
	char tb[32]; snprintf(tb, sizeof tb, "Tie bet: %s", war_tie_on ? "On" : "Off");
	if ((ui_button(9701, 30, 260, 200, 30, tb, war_tie_on ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) && war_state != 1) war_tie_on = !war_tie_on;
	text(F_SMALL, 34, 296, "A tie pays 10 to 1", RGB(200, 210, 220), 200);
	int y = 330;
	if (war_state == 1 && war_n == 1) {
		text_c(F_BODYB, 430, 330, "A tie! Go to war?", RGB(255, 230, 190), 255);
		if ((ui_ibutton(9705, 290, 370, 150, 44, IC_PLAY, "Go to war", ST_NORMAL) & UI_CLICK) && !opt_open) war_go(1);
		if ((ui_ibutton(9706, 450, 370, 150, 44, IC_CLOSE, "Give up", ST_NORMAL) & UI_CLICK) && !opt_open) war_go(0);
	} else {
		chip_picker(250, y + 30, 9710);
		if ((ui_ibutton(9700, 470, y + 30, 134, 48, IC_PLAY, "Deal", ST_NORMAL) & UI_CLICK) && !opt_open) war_deal();
		if (msg[0] && war_state == 2) {
			int w = text_w(F_BODYB, msg) + 30; if (w > 360) w = 360;
			gfx_round(430 - w / 2, 214, w, 30, 15, msg_tone > 0 ? RGB(40, 150, 90) : RGB(150, 50, 50), 235);
			text_fit(F_BODYB, 430 - w / 2 + 15, 217, w - 30, msg, C_WHITE, 255);
		}
	}
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("1", war_state == 1 ? "War" : "Deal");
	ui_footer_hint("HOME", "Floor");
	if (!blocked && !opt_open && (in->down & BTN_1)) { if (war_state == 1 && war_n == 1) war_go(1); else war_deal(); }
	game_common(blocked, war_state == 1);
}

/* ================================================================== */
/* THE BIG WHEEL: bet on a symbol, swing the Wii Remote to spin it    */
/* ================================================================== */
#define BW_N 54
static const u8 BW_VAL[7] = { 1, 2, 5, 10, 20, 40, 40 };
static const char *const BW_NAME[7] = { "1", "2", "5", "10", "20", "Joker", "Star" };
static const u32 BW_COL[7] = { RGB(240, 236, 220), RGB(90, 170, 240), RGB(250, 200, 60), RGB(120, 220, 120), RGB(230, 90, 200), RGB(40, 40, 50), RGB(200, 30, 40) };
static u8 bw_seg[BW_N];
static int bw_ready, bw_bet[7], bw_last[7], bw_spinning, bw_result = -1, bw_hist[8], bw_nh;
static float bw_rot, bw_v; static int bw_last_seg;
static int bw_us[32], bw_uv[32], bw_nundo;
static void bw_build(void)
{
	/* 24 ones, 15 twos, 7 fives, 4 tens, 2 twenties, a joker and a star, spread around */
	static const u8 CNT[7] = { 24, 15, 7, 4, 2, 1, 1 };
	int n = 0; u8 tmp[BW_N];
	for (int k = 0; k < 7; k++) for (int i = 0; i < CNT[k]; i++) tmp[n++] = k;
	/* spread: take from the biggest pile that isn't the last one placed */
	int left[7]; for (int k = 0; k < 7; k++) left[k] = CNT[k];
	int prev = -1;
	for (int i = 0; i < BW_N; i++) {
		int best = -1;
		for (int k = 0; k < 7; k++) if (left[k] && k != prev && (best < 0 || left[k] > left[best] || (left[k] == left[best] && (i * 7 + k) % 3 == 0))) best = k;
		if (best < 0) best = prev;
		bw_seg[i] = best; left[best]--; prev = best;
	}
	(void)tmp;
	bw_ready = 1;
}
static void bigwheel_art(int cx, int cy, int r)
{
	if (!bw_ready) bw_build();
	float rot = plat_ms() / 3000.0f;
	float st = 6.2831853f / BW_N;
	gfx_circle(cx, cy, r + 3, RGB(200, 160, 80), 255);
	for (int i = 0; i < BW_N; i++) {
		float a0 = rot + i * st, a1 = a0 + st;
		gfx_tri(cx, cy, cx + (int)(cosf(a0) * r), cy + (int)(sinf(a0) * r), cx + (int)(cosf(a1) * r), cy + (int)(sinf(a1) * r), BW_COL[bw_seg[i]], 255);
	}
	gfx_circle(cx, cy, r / 4, RGB(230, 196, 110), 255);
}
static void bw_draw(int cx, int cy, int R)
{
	float st = 6.2831853f / BW_N;
	gfx_circle(cx + 3, cy + 5, R + 14, RGB(0, 0, 0), 110);
	gfx_circle(cx, cy, R + 14, RGB(90, 50, 20), 255);
	gfx_circle(cx, cy, R + 10, RGB(220, 180, 90), 255);
	for (int i = 0; i < BW_N; i++) {
		float a0 = bw_rot + i * st, a1 = a0 + st;
		int X0 = cx + (int)(cosf(a0) * R), Y0 = cy + (int)(sinf(a0) * R), X1 = cx + (int)(cosf(a1) * R), Y1 = cy + (int)(sinf(a1) * R);
		gfx_tri(cx, cy, X0, Y0, X1, Y1, BW_COL[bw_seg[i]], 255);
		gfx_line(cx + (int)(cosf(a0) * R * 0.35f), cy + (int)(sinf(a0) * R * 0.35f), X0, Y0, 1, RGB(120, 90, 40), 200);
		/* the value near the rim */
		float am = a0 + st / 2;
		int tx = cx + (int)(cosf(am) * R * 0.8f), ty = cy + (int)(sinf(am) * R * 0.8f);
		int k = bw_seg[i];
		if (k >= 5) gfx_circle(tx, ty, 4, k == 5 ? RGB(250, 250, 250) : RGB(255, 230, 120), 255);
		else if (R > 90) { char t[4]; snprintf(t, sizeof t, "%s", BW_NAME[k]); text_c(F_SMALL, tx, ty - 9, t, k == 0 ? RGB(40, 40, 40) : RGB(255, 255, 255), 230); }
	}
	/* pegs on the rim */
	for (int i = 0; i < BW_N; i++) { float a = bw_rot + i * st; gfx_circle(cx + (int)(cosf(a) * (R + 5)), cy + (int)(sinf(a) * (R + 5)), 2, RGB(250, 240, 210), 255); }
	gfx_circle(cx, cy, R * 30 / 100, RGB(40, 24, 12), 255);
	gfx_circle(cx, cy, R * 26 / 100, RGB(200, 160, 80), 255);
	text_c(F_BODYB, cx, cy - 12, "BIG", RGB(80, 40, 10), 255);
	/* the flapper at the top */
	int fl = 0;
	if (bw_spinning) { float seg = fmodf((-1.5708f - bw_rot) / st + BW_N * 100, 1.0f); fl = seg < 0.25f ? (int)((0.25f - seg) * 40) : 0; }
	gfx_tri(cx - 9, cy - R - 26, cx + 9, cy - R - 26, cx + fl / 2, cy - R + 4, RGB(200, 30, 40), 255);
	gfx_circle(cx, cy - R - 26, 6, RGB(230, 196, 110), 255);
}
static int bw_under(void)
{
	float st = 6.2831853f / BW_N;
	float a = -1.5708f - bw_rot;
	int i = (int)floorf(a / st);
	i %= BW_N; if (i < 0) i += BW_N;
	return i;
}
static void bigwheel_open(void) { if (!bw_ready) bw_build(); open_simple(PS_DUSTY, V_BIGWHEEL, 9730); }
static int bw_total(void) { int t = 0; for (int k = 0; k < 7; k++) t += bw_bet[k]; return t; }
static void bw_spin(float power);
static void bw_spin_def(void) { bw_spin(0.5f); }
static void bw_spin(float power)
{
	int t = bw_total();
	if (!t) { int again = 0; for (int k = 0; k < 7; k++) again += bw_last[k]; if (again && again <= cfg.chips) { memcpy(bw_bet, bw_last, sizeof bw_bet); t = again; } }
	if (!t) { ui_toast("Put chips on a symbol first"); return; }
	if (t > cfg.chips) { ui_toast("Not enough chips"); return; }
	if (!big_ok(t, bw_spin_def)) return;
	cfg.chips -= t; memcpy(bw_last, bw_bet, sizeof bw_bet);
	bw_spinning = 1; bw_v = 0.18f + power * 0.12f + (rnd() % 100) / 1500.0f;
	bw_last_seg = bw_under();
	plat_rumble(60); music_sfx(SFX_LEVER, 0);
	msg[0] = 0;
}
static void bw_finish(void)
{
	bw_spinning = 0;
	int k = bw_seg[bw_under()], stake = bw_total();
	int back = bw_bet[k] ? bw_bet[k] * (BW_VAL[k] + 1) : 0;
	cfg.chips += back;
	for (int i = 7; i > 0; i--) bw_hist[i] = bw_hist[i - 1];
	bw_hist[0] = k; if (bw_nh < 8) bw_nh++;
	char m[64];
	if (back) snprintf(m, sizeof m, "%s! You win %d", BW_NAME[k], back - stake); else snprintf(m, sizeof m, "%s. You lose %d", BW_NAME[k], stake);
	say(back > stake ? 1 : back < stake ? -1 : 0, m);
	memset(&ev, 0, sizeof ev); ev.game = 7; ev.num = BW_VAL[k];
	snprintf(ev.what, sizeof ev.what, "%s", BW_NAME[k]);
	if (k >= 5 && back) ev.flags |= F_SPECIAL;
	memset(bw_bet, 0, sizeof bw_bet); bw_nundo = 0;
	round_done(back - stake, stake);
}
static void big_wheel(int blocked)
{
	game_felt("The Big Wheel", "Bet on a symbol. It pays what it says.", RGB(110, 60, 20), RGB(50, 24, 8));
	/* spin physics: friction, and a click for every peg that passes the flapper */
	if (bw_spinning) {
		bw_rot += bw_v;
		bw_v *= bw_v > 0.05f ? 0.991f : 0.982f;
		if (anim_k() < 100) bw_v *= 0.96f;   /* skipping or B held: slow it down fast */
		int u = bw_under();
		if (u != bw_last_seg) { bw_last_seg = u; music_sfx(SFX_TICK, 0); }
		if (bw_v < 0.0012f) bw_finish();
	}
	bw_draw(136, 304, 98);
	/* the bets */
	for (int k = 0; k < 7; k++) {
		int x = 300 + (k % 4) * 78, y = 90 + (k / 4) * 70;
		int f = ui_item(9730 + k, x, y, 72, 62);
		gfx_round(x, y, 72, 62, 10, BW_COL[k], 255);
		gfx_round_line(x, y, 72, 62, 10, 2, RGB(230, 196, 110), 200);
		text_c(F_BODYB, x + 36, y + 4, BW_NAME[k], k == 0 ? RGB(30, 30, 30) : RGB(255, 255, 255), 255);
		char p[12]; snprintf(p, sizeof p, "%d to 1", BW_VAL[k]); text_c(F_SMALL, x + 36, y + 30, p, k == 0 ? RGB(60, 60, 60) : RGB(240, 240, 240), 230);
		if (bw_bet[k]) chip_draw(x + 60, y + 50, 10, bw_bet[k], 0);
		if (!bw_spinning && bw_nh && bw_hist[0] == k) gfx_round_line(x - 2, y - 2, 76, 66, 12, 3, RGB(255, 214, 90), 255);
		if (f & UI_FOCUS) ui_focus_ring(x, y, 72, 62, 10);
		if ((f & UI_CLICK) && !bw_spinning && !opt_open) {
			int v = chip_val();
			if (bw_total() + v > cfg.chips) ui_toast("Not enough chips");
			else if (bw_total() + v > SEC_MAX[sec] * 2) ui_toast("That's over the limit");
			else { bw_bet[k] += v; plat_rumble(8); music_sfx(SFX_CHIP, 0); if (bw_nundo < 32) { bw_us[bw_nundo] = k; bw_uv[bw_nundo] = v; bw_nundo++; } }
		}
	}
	/* history */
	for (int i = 0; i < bw_nh; i++) { gfx_circle(560 + (i % 2) * 22, 250 + (i / 2) * 22, 9, BW_COL[bw_hist[i]], 255); }
	int y = 330;
	chip_picker(300, y, 9740);
	char b[32]; snprintf(b, sizeof b, "Bet %d", bw_total()); text_c(F_BODYB, 400, y + 54, b, RGB(255, 214, 90), 255);
	if ((ui_ibutton(9750, 508, y, 96, 48, IC_PLAY, "Spin", bw_spinning ? ST_GHOST : ST_NORMAL) & UI_CLICK) && !bw_spinning && !opt_open) bw_spin(0.5f);
	if ((ui_button(9751, 508, y + 52, 96, 30, "Clear", ST_NORMAL) & UI_CLICK) && !bw_spinning) memset(bw_bet, 0, sizeof bw_bet);
	if (msg[0] && !bw_spinning) {
		int w = text_w(F_BODYB, msg) + 30;
		gfx_round(440 - w / 2, 244, w, 30, 15, msg_tone > 0 ? RGB(40, 150, 90) : RGB(150, 50, 50), 235);
		text_c(F_BODYB, 440, 247, msg, C_WHITE, 255);
	} else if (!bw_spinning) text_c(F_SMALL, 440, 250, "Swing the Wii Remote to spin it (or 1)", RGB(240, 220, 200), 200);
	ui_footer_begin();
	ui_footer_hint("A", "Place chip");
	ui_footer_hint("1", "Spin");
	ui_footer_hint("HOME", "Floor");
	if (!blocked && !opt_open && !bw_spinning) {
		if ((in->down & BTN_MINUS) && bw_nundo) { bw_nundo--; bw_bet[bw_us[bw_nundo]] -= bw_uv[bw_nundo]; if (bw_bet[bw_us[bw_nundo]] < 0) bw_bet[bw_us[bw_nundo]] = 0; in->down &= ~BTN_MINUS; music_sfx(SFX_CHIP, 0); }
		if (in->down & BTN_1) bw_spin(0.5f);
		else if (in->shake > 140 && bw_total()) bw_spin(in->shake > 300 ? 1.0f : (in->shake - 140) / 160.0f);   /* a harder swing spins it faster */
	}
	game_common(blocked, bw_spinning);
}

/* ================================================================== */
/* KENO: pick up to 10 numbers, 20 balls are drawn                    */
/* ================================================================== */
static u8 kn_pick[81], kn_drawn[81]; static int kn_npick, kn_state, kn_nd, kn_hits; static u8 kn_order[20]; static u64 kn_t;
static u8 kn_last_pick[81]; static int kn_last_n, kn_play_bet;
static int kn_lastp[10], kn_nl;   /* - un-picks the last number */
/* payout multipliers: [spots][hits] */
static const u16 KN_PAY[11][11] = {
	{ 0 }, { 0, 3 }, { 0, 0, 12 }, { 0, 0, 1, 40 }, { 0, 0, 1, 4, 100 }, { 0, 0, 0, 2, 15, 500 },
	{ 0, 0, 0, 1, 4, 70, 1500 }, { 0, 0, 0, 1, 2, 20, 300, 5000 }, { 0, 0, 0, 0, 1, 8, 80, 800, 8000 },
	{ 0, 0, 0, 0, 1, 5, 40, 300, 3000, 20000 }, { 2, 0, 0, 0, 0, 2, 20, 130, 1000, 5000, 50000 } };
static void keno_open(void) { kn_state = 0; open_simple(PS_LUPA, V_KENO, 9800); }
static void kn_play(void)
{
	if (!kn_npick) {
		if (kn_last_n) { memcpy(kn_pick, kn_last_pick, sizeof kn_pick); kn_npick = kn_last_n; }
		else { ui_toast("Pick some numbers first (or Quick pick)"); return; }
	}
	int bet = chip_val();
	if (bet > cfg.chips) { ui_toast("Not enough chips"); return; }
	if (!big_ok(bet, kn_play)) return;
	cfg.chips -= bet; S.bet_k[4] = chip_sel;
	memcpy(kn_last_pick, kn_pick, sizeof kn_pick); kn_last_n = kn_npick;
	memset(kn_drawn, 0, sizeof kn_drawn);
	for (int i = 0; i < 20; i++) { int n; do n = 1 + rnd() % 80; while (kn_drawn[n]); kn_drawn[n] = 1; kn_order[i] = n; }
	memset(kn_drawn, 0, sizeof kn_drawn);
	kn_nd = 0; kn_hits = 0; kn_state = 1; kn_t = plat_ms(); msg[0] = 0;
	kn_play_bet = bet;
}
static void kn_finish(void)
{
	kn_state = 2;
	int bet = kn_play_bet, m = KN_PAY[kn_npick][kn_hits], win = bet * m;
	cfg.chips += win;
	char t[64];
	if (win) snprintf(t, sizeof t, "%d of %d hit! %dx - you win %d", kn_hits, kn_npick, m, win - bet);
	else snprintf(t, sizeof t, "%d of %d hit. You lose %d", kn_hits, kn_npick, bet);
	say(win > bet ? 1 : win < bet ? -1 : 0, t);
	memset(&ev, 0, sizeof ev); ev.game = 8; ev.num = kn_hits; ev.run = kn_npick;
	if (m >= 20) ev.flags |= F_SPECIAL;
	if (!win && kn_hits >= kn_npick / 2 && kn_npick >= 4) ev.flags |= F_NEAR;
	round_done(win - bet, bet);
}
static void keno(int blocked)
{
	game_felt("Keno", "Pick up to 10 numbers. 20 balls are drawn.", RGB(40, 70, 90), RGB(14, 30, 44));
	u64 now = plat_ms();
	/* the draw: a ball every so often */
	if (kn_state == 1 && kn_nd < 20 && now > kn_t + (u64)(kn_nd ? 260 * speed_ms() / 260 + 120 : 400)) {
		int n = kn_order[kn_nd++]; kn_drawn[n] = 1; kn_t = now;
		if (kn_pick[n]) { kn_hits++; music_sfx(SFX_WIN, 0); plat_rumble(20); } else music_sfx(SFX_TICK, 0);
	}
	if (kn_state == 1 && kn_nd >= 20 && now > kn_t + 600) kn_finish();
	/* the board: 10 x 8 */
	int bx = 256, by = 80, cw = 34, ch = 27;
	for (int n = 1; n <= 80; n++) {
		int x = bx + ((n - 1) % 10) * cw, y = by + ((n - 1) / 10) * ch;
		int f = ui_item(9800 + n, x, y, cw - 2, ch - 2);
		int pk = kn_pick[n], dr = kn_drawn[n];
		u32 bg = pk && dr ? RGB(255, 200, 60) : dr ? RGB(200, 60, 60) : pk ? RGB(60, 140, 230) : RGB(236, 232, 222);
		gfx_round(x, y, cw - 2, ch - 2, 6, bg, 255);
		char t[4]; snprintf(t, sizeof t, "%d", n);
		text_c(F_SMALL, x + cw / 2 - 1, y + 3, t, pk || dr ? RGB(255, 255, 255) : RGB(40, 40, 50), 255);
		if (pk && dr) gfx_round_line(x - 1, y - 1, cw, ch, 7, 2, RGB(255, 255, 255), 255);
		if (f & UI_FOCUS) ui_focus_ring(x, y, cw - 2, ch - 2, 6);
		if ((f & UI_CLICK) && kn_state != 1 && !opt_open) {
			if (kn_state == 2) { memset(kn_drawn, 0, sizeof kn_drawn); kn_state = 0; msg[0] = 0; }
			if (pk) { kn_pick[n] = 0; kn_npick--; }
			else if (kn_npick < 10) { kn_pick[n] = 1; kn_npick++; music_sfx(SFX_CHIP, 0); kn_lastp[kn_nl < 10 ? kn_nl++ : 9] = n; }
			else ui_toast("10 numbers at most");
		}
	}
	/* the balls, as they come */
	for (int i = 0; i < kn_nd; i++) {
		int n = kn_order[i], x = 36 + (i % 5) * 40, y = 214 + (i / 5) * 32;
		int age = (int)(now - kn_t), pop = i == kn_nd - 1 && age < 200 ? 4 - age / 50 : 0;
		gfx_circle(x, y, 14 + pop, kn_pick[n] ? RGB(255, 200, 60) : RGB(240, 236, 226), 255);
		gfx_circle(x - 4, y - 5, 4, RGB(255, 255, 255), 160);
		char t[4]; snprintf(t, sizeof t, "%d", n); text_c(F_SMALL, x, y - 9, t, RGB(30, 30, 30), 255);
	}
	/* the pay table for how many you picked */
	if (kn_npick) {
		gfx_round(30, 346, 206, 70, 10, RGB(0, 0, 0), 120);
		char h[24]; snprintf(h, sizeof h, "%d spots pay:", kn_npick); text(F_SMALL, 38, 348, h, RGB(220, 230, 240), 255);
		int row = 0;
		for (int k = kn_npick; k >= 0 && row < 3; k--) if (KN_PAY[kn_npick][k]) { char p[32]; snprintf(p, sizeof p, "%d hit  %dx", k, KN_PAY[kn_npick][k]); text(F_SMALL, 40 + (row / 3) * 100, 366 + (row % 3) * 16, p, kn_state && k == kn_hits ? RGB(255, 214, 90) : RGB(230, 230, 230), 255); row++; }
	}
	int y = 306;
	chip_picker(256, y, 9890);
	char st[32]; snprintf(st, sizeof st, "%d picked,  %d hit", kn_npick, kn_hits); text(F_SMALL, 460, y - 8, st, RGB(230, 230, 230), 255);
	if ((ui_ibutton(9895, 460, y + 14, 144, 40, IC_PLAY, "Play", kn_state == 1 ? ST_GHOST : ST_NORMAL) & UI_CLICK) && kn_state != 1 && !opt_open) kn_play();
	if ((ui_button(9896, 256, y + 60, 110, 32, "Quick pick", ST_NORMAL) & UI_CLICK) && kn_state != 1) {
		memset(kn_pick, 0, sizeof kn_pick); memset(kn_drawn, 0, sizeof kn_drawn); kn_state = 0;
		int want = kn_npick ? kn_npick : 6; kn_npick = 0;
		while (kn_npick < want) { int n = 1 + rnd() % 80; if (!kn_pick[n]) { kn_pick[n] = 1; kn_npick++; } }
	}
	if ((ui_button(9897, 372, y + 60, 84, 32, "Clear", ST_NORMAL) & UI_CLICK) && kn_state != 1) { memset(kn_pick, 0, sizeof kn_pick); kn_npick = 0; memset(kn_drawn, 0, sizeof kn_drawn); kn_state = 0; }
	if (msg[0] && kn_state == 2) {
		int w = text_w(F_BODYB, msg) + 30; if (w > 344) w = 344;
		gfx_round(460 - 60, y + 60, 204, 32, 12, msg_tone > 0 ? RGB(40, 150, 90) : RGB(150, 50, 50), 235);
		text_fit(F_SMALL, 460 - 50, y + 66, 184, msg, C_WHITE, 255);
	}
	ui_footer_begin();
	ui_footer_hint("A", "Pick");
	ui_footer_hint("1", "Play");
	ui_footer_hint("HOME", "Floor");
	if (!blocked && !opt_open && kn_state != 1 && (in->down & BTN_1)) kn_play();
	if (!blocked && !opt_open && kn_state != 1 && (in->down & BTN_MINUS) && kn_nl) {
		int n = kn_lastp[--kn_nl]; if (kn_pick[n]) { kn_pick[n] = 0; kn_npick--; } in->down &= ~BTN_MINUS;
	}
	game_common(blocked, kn_state == 1);
}
