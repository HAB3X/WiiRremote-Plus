/*
   casino_games.inc - the rest of the casino floor: Roulette, Slots, Video Poker, Hi-Lo, Baccarat,
   plus the floor itself with its two areas (Main Floor and the High Roller Suite).
   Included at the end of casino.c so it shares the cards, chips, stats and Phantom Moth.
   WiiRremote Plus by Habex. GPL-3.0.
*/

/* ================================================================== */
/* areas of the casino                                                */
/* ================================================================== */
#define SUITE_NEED 2500
static const int SEC_CHIP[2][4] = { { 5, 25, 100, 500 }, { 100, 500, 1000, 5000 } };
static const int SEC_MAX[2] = { 500, 5000 };
/* every game on the floor */
static int r_type;   /* roulette wheel: 0 European, 1 American, 2 Triple Zero */
enum { G_BJ, G_ROUL, G_SLOTS, G_VPOKER, G_HILO, G_BACC, G_WAR, G_BIGWHEEL, G_KENO, G_ROUL_US, G_ROUL_000, G_HOLDEM, G_LIAR, G_N };
static int chip_sel;              /* which chip you bet with */

/* shared look: a felt table with a rail, the game name and your chips */
static void game_felt(const char *name, const char *sub, u32 top, u32 bot)
{
	if (sec) { top = RGB(118, 22, 34); bot = RGB(60, 8, 16); }
	felt_draw(16, 14, 608, 400, 26, top, bot, sec ? RGB(150, 110, 50) : RGB(120, 70, 36));
	text(F_BODYB, 37, 25, name, RGB(0, 0, 0), 120);
	text(F_BODYB, 36, 24, name, RGB(250, 244, 226), 255);
	if (sub) text_fit(F_SMALL, 36, 50, 280, sub, RGB(200, 220, 210), 255);
	char cs[32]; snprintf(cs, sizeof cs, "%d", cfg.chips);
	gfx_round(390, 24, 146, 30, 15, RGB(0, 0, 0), 110);
	chip_draw(408, 39, 11, SEC_CHIP[sec][1], 0);
	text(F_BODYB, 426, 27, cs, RGB(255, 214, 90), 255);
	delta_draw(463, 58);
}

/* a row of 4 chips to pick your bet size with; returns 1 if one was picked */
static int chip_picker(int x, int y, int idbase)
{
	int picked = 0;
	for (int k = 0; k < 4; k++) {
		int cx = x + 23 + k * 50;
		int f = ui_item(idbase + k, cx - 23, y, 46, 48);
		chip_draw(cx, y + 24, 21, SEC_CHIP[sec][k], (f & UI_FOCUS) != 0);
		if (k == chip_sel) gfx_round_line(cx - 25, y - 2, 50, 52, 12, 2, RGB(255, 214, 90), 255);
		if ((f & UI_CLICK) && !opt_open) { chip_sel = k; picked = 1; plat_rumble(8); music_sfx(SFX_CHIP, 0); }
	}
	return picked;
}
static int chip_val(void) { return SEC_CHIP[sec][chip_sel & 3]; }

/* ================================================================== */
/* the dealers: each game has its own. Phantom Moth runs the floor and     */
/* the blackjack tables; the others work for him.                     */
/* ================================================================== */
#if 0   /* old fixed lines, replaced by talk.inc */
typedef struct {
	const char *name;
	const char *intro[3], *win[4], *lose[4], *idle[5], *angry[3], *taunt[3];
} Persona;
#endif
enum { PS_DUSTY, PS_CORVUS, PS_MIMIC, PS_LUPA, PS_TALON, PS_COUNT };
#if 0
static const Persona PERSONA[PS_COUNT] = {
	{ "Trigger Lynx",   /* Trigger Lynx, roulette: a gunslinger who can't stop spinning things */
	  { "They call me Trigger Lynx. I spin the wheel. Some folks say I spin it a little too much.", "Thirty-seven chambers in this wheel, %s. Only one of 'em's loaded with your number.", "Step right up. The wheel's hungry and my trigger finger's itchy." },
	  { "Hah! Nice shot, %s. You're quicker than you look.", "Well I'll be. Beginner's aim.", "You win this round. Don't let it go to your head.", "Lucky draw, partner." },
	  { "Too slow on the draw, %s.", "Bang. Right between the chips.", "The wheel never misses. Neither do I.", "Should've picked red. Or black. Or anything else." },
	  { "Six shots or twelve? ...Sorry. Old habit. Place your bet.", "You hear that click? That's the wheel. Or my revolver. Hard to tell.", "I've been spinning since before you were born, %s.", "Take your time. I like a slow standoff.", "Hold still. I'm practising my spin." },
	  { "Alright, that's enough. Nobody's THAT lucky, %s.", "Somebody's loading the wheel and it ain't me!", "Draw again, %s. I dare you." },
	  { "Out of ammo already, %s?", "The house always wins the showdown.", "Reload. I'll wait." } },
	{ "Thunder Crow",       /* Thunder Crow, slots: a giant who talks to ravens */
	  { "The ravens told me you would come, %s.", "Sit. The machines are my birds. They sing for me.", "I am Thunder Crow. The ravens and I will watch you pull." },
	  { "The ravens are... surprised.", "A good pull. The birds cry out.", "Fortune flies your way. For now.", "Hmph. The birds did not see that coming." },
	  { "The ravens laugh at you, %s.", "Another feather for my nest.", "The reels have fed my birds well tonight.", "As the ravens foretold." },
	  { "Listen. The ravens are circling your chips.", "The machines are cold tonight. So am I.", "Pull the lever, %s. The birds grow hungry.", "I have seen a thousand players at this machine. I remember none.", "One of my ravens is on your shoulder. Don't look." },
	  { "My birds grow restless, %s. You take too much.", "The ravens will pick you clean for this!", "Enough! The machine is MINE." },
	  { "The ravens are feasting tonight.", "Keep feeding the machine, %s. Keep feeding my birds.", "Down you go. Like the others." } },
	{ "Mirror Squid",        /* Mirror Squid, video poker: a master of disguise who is never who he seems */
	  { "Hi %s! It's me, your old friend! ...You don't remember me? Strange.", "I could be anyone, %s. Tonight I'm your dealer.", "Relax. I'm definitely not Phantom Moth in a mask. Ha. Ha." },
	  { "You win! Or did I let you? I forget which face I'm wearing.", "Well played... is what your friend would say. So I'm saying it.", "Nice hand. Mine was nicer. I just swapped it.", "Good cards. I should know. I dealt them." },
	  { "Oh dear. Would you like me to be someone who cares?", "That hand didn't suit you. I have spares.", "Unlucky! Says the voice of your mother. Very convincing, no?", "You lose. I'm told it builds character. I have many." },
	  { "Hold the good cards. I'll hold my face on.", "Is that really you, %s? Hard to tell these days.", "I once pretended to be a Wii Remote. Nobody noticed.", "Everyone in this casino is me. Probably.", "Psst. Phantom Moth can't hear us. ...Or can he." },
	  { "Who ARE you, %s? Nobody wins like this! Nobody I've been, anyway!", "I'll disguise myself as a winner. Watch me.", "Stop it! You're ruining my face!" },
	  { "Don't worry. I'll be someone kinder next hand.", "Another loss. I'd cry, but that's not my face.", "You look defeated. I could wear that look." } },
	{ "Silent Vixen",         /* Silent Vixen, hi-lo: a patient sharpshooter */
	  { "I don't miss, %s. Let's see if you do.", "Patience is everything. One card. One shot.", "Sit. Breathe slow. Take your shot when it's clean." },
	  { "A clean shot. Good.", "You have the eyes of a hunter, %s.", "Hm. You didn't flinch.", "Well aimed." },
	  { "You rushed the shot.", "Missed. Everyone misses eventually.", "Your hands were shaking, %s. I saw it.", "One wrong breath and it's over." },
	  { "Wolves hunt in packs. Cards hunt alone.", "I once waited three days for the right shot. You can wait for one card.", "Steady, %s. Steady.", "The next card is out there. Waiting. Like me.", "Don't blink." },
	  { "How are you seeing these, %s? What scope are you using?!", "No one reads the cards like this. No one but me!", "Enough. You're in my sights now." },
	  { "Another one down.", "Stay down, %s.", "The hunt is over for you." } },
	{ "Crimson Viper",        /* Crimson Viper, baccarat, the Suite: the boss's ambitious right hand */
	  { "Welcome to the top, %s. Only the best of us make it up here.", "So you're the one the Moth keeps talking about. We'll see.", "Baccarat. A game for kings. Or for people who pretend to be." },
	  { "You win. Enjoy it, brother. It won't happen twice.", "Hmph. Not bad for someone from the Main Floor.", "You have some talent, %s. Wasted, but some.", "Fine. Take the pot. I'll take the war." },
	  { "Did you really think you could beat me up here?", "This is my table. You're just visiting.", "The Suite eats people like you, %s.", "Down you go. Back to the Main Floor." },
	  { "I was built to win, %s. You were built to watch.", "The Moth reads minds. I read markets. Guess who owns the Suite.", "They call this the High Roller Suite. I call it my throne.", "Look around. Every chip up here answers to me.", "Pick a side. Player or Banker. Me or you." },
	  { "ENOUGH! You don't belong up here, %s!", "Is the Moth feeding you cards? I KNEW it!", "You think you're better than me?!" },
	  { "Kneel, %s. The Suite has a king.", "Is that all? I expected more.", "Keep losing. I'm enjoying the view." } },
};
static void persona_say(const char *fmt)
{
	char t[220]; snprintf(t, sizeof t, fmt, nm());
	quip(t);
	if (persona >= 0) q_who = PERSONA[persona].name;
}
static const char *pick3(const char *const *l, int n) { return l[rnd() % n]; }
/* how each dealer feels about you over time: [0] Phantom Moth, then the others. HIGH = you keep beating them, LOW = they keep beating you */
static const char *const REL_HIGH[6] = {
	"You again, %s. You've taken too much from me. I don't forget. I don't forgive.",
	"Well, if it ain't the fastest hand in the casino. I've been practising, %s.",
	"The ravens speak your name with fear now, %s. I do not like it.",
	"Oh, it's YOU. I tried being you once. I couldn't keep up.",
	"You've earned my respect, %s. Few do. Don't waste it.",
	"YOU. I've thought about you every night, %s. Tonight, you lose." };
static const char *const REL_LOW[6] = {
	"Back for more, %s? I almost pity you. Almost.",
	"Easy money walks in again. Howdy, %s.",
	"The ravens are happy to see you, %s. You feed them so well.",
	"My favourite customer! I'd hug you, but whose arms would I use?",
	"You again. Try to aim this time, %s.",
	"Ah, my loyal donor. The Suite thanks you, %s." };
#endif
static int rel_tier(int who) { int r = S.rel[who]; return r >= 15 ? 1 : r <= -15 ? -1 : 0; }
const char *casino_rel_word(int who)
{
	static const char *HI[6] = { "Holds a grudge", "Wants a rematch", "Fears you", "Admires you", "Respects you", "Hates you" };
	static const char *LO[6] = { "Pities you", "Thinks you're easy", "Likes feeding on you", "Loves you (as a wallet)", "Unimpressed", "Thinks you're a donor" };
	int t = rel_tier(who);
	return t > 0 ? HI[who] : t < 0 ? LO[who] : "Neutral";
}
#if 0
static void persona_after(int res)
{
	if (!S.quips || persona < 0) return;
	if (S.quips == 1) { quiet_line(res); q_who = PERSONA[persona].name; return; }
	const Persona *P = &PERSONA[persona];
	dealer_read();   /* for the name */
	char st[220];
	if (style_line(st, sizeof st)) { quip(st); q_who = P->name; return; }
	if (pw_streak >= 3) { anger = pw_streak >= 5 ? 2 : 1; persona_say(pick3(P->angry, 3)); if (anger == 2) plat_rumble(200); return; }
	anger = 0;
	if (dw_streak >= 3) { persona_say(pick3(P->taunt, 3)); return; }
	if ((rnd() % 10) < 6) persona_say(res > 0 ? pick3(P->win, 4) : res < 0 ? pick3(P->lose, 4) : pick3(P->idle, 5));
}
static void persona_idle(void)
{
	if (S.quips < 2 || persona < 0) return;
	u64 now = plat_ms();
	static u64 next;
	if (!next || now < q_at) next = now + 15000;
	if (now < next || now < q_at + (u64)strlen(q_text) * 28 + 9000) return;
	next = now + 17000 + rnd() % 12000;
	dealer_read();
	/* now and then Phantom Moth cuts in from somewhere on the floor */
	if ((rnd() % 5) == 0) { static const char *const V[] = { "(Phantom Moth, from somewhere:) I'm still watching you, %s.", "(Phantom Moth, from somewhere:) Don't get too comfortable.", "(Phantom Moth, from somewhere:) Having fun with my staff?" };
		char t[200]; snprintf(t, sizeof t, pick3(V, 3), nm()); quip(t); return; }
	persona_say(pick3(PERSONA[persona].idle, 5));
}

#endif

/* every round of every game counts for the stats and for the dealers */
static int view_game(void) { return view == V_ROUL ? 1 : view == V_SLOTS ? 2 : view == V_POKER ? 3 : view == V_HILO ? 4 : view == V_BACC ? 5 : 0; }
/* per-game stats: which game is this screen, and note a finished round */
static int view_gid(void)
{
	switch (view) {
	case V_TABLE: return G_BJ; case V_ROUL: return r_type == 1 ? G_ROUL_US : r_type == 2 ? G_ROUL_000 : G_ROUL;
	case V_SLOTS: return G_SLOTS; case V_POKER: return G_VPOKER; case V_HILO: return G_HILO; case V_BACC: return G_BACC;
	case V_WAR: return G_WAR; case V_BIGWHEEL: return G_BIGWHEEL; case V_KENO: return G_KENO; case V_LIAR: return G_LIAR; case V_HOLDEM: return G_HOLDEM;
	default: return -1;
	}
}
static void gstat(int net)
{
	int g = view_gid(); if (g < 0 || g >= 16) return;
	static u64 last; u64 now = plat_ms();
	S.gs_p[g]++; if (net > 0) S.gs_w[g]++; if (net > S.gs_best[g]) S.gs_best[g] = net;
	if (last && now > last && now - last < 600000) S.gs_sec[g] += (int)((now - last) / 1000);
	last = now;
}
static void round_done(int net, int stake)
{
	gstat(net);
	style_note(stake, net > 0 ? 1 : net < 0 ? -1 : 0);
	track_round(net);
	int res = net > 0 ? 1 : net < 0 ? -1 : 0;
	S.played++;
	if (res > 0) S.won++; else if (res < 0) S.lost++; else S.pushed++;
	if (net > S.best_win) S.best_win = net;
	if (res > 0) { S.streak++; if (S.streak > S.best_streak) S.best_streak = S.streak; achieve(AC_FIRST_WIN); } else if (res < 0) S.streak = 0;
	if (S.streak >= 5) achieve(AC_STREAK5);
	if (cfg.chips > S.max_chips) S.max_chips = cfg.chips;
	if (cfg.chips >= 10000) achieve(AC_10K);
	stats_save(); config_save();
	talk_round(res, net, stake);
	if (net > 0) { if (!S.snd) plat_beep(1); plat_rumble(50); }
}

/* the standard end of every game screen: Vex, the achievement pop, leaving */
static void game_common(int blocked, int busy)
{
	if (!busy) talk_idle();
	creep_shade();
	quip_draw();
	quip_tick();
	u64 now = plat_ms();
	if (now < ach_until) {
		int w = text_w(F_BODYB, ach_msg) + 50;
		gfx_round(320 - w / 2, 150, w, 40, 20, RGB(30, 30, 40), 235);
		gfx_round_line(320 - w / 2, 150, w, 40, 20, 2, RGB(255, 214, 90), 255);
		icon(IC_STAR, 320 - w / 2 + 22, 170, 18, RGB(255, 214, 90));
		text(F_BODYB, 320 - w / 2 + 38, 158, ach_msg, C_WHITE, 255);
	}
	if (blocked || opt_open) return;
	if (in->down & BTN_PLUS) { opt_open = 1; ui_set_focus(9000); return; }
	if (in->down & (BTN_HOME | BTN_B)) {
		if (busy) { if (in->down & BTN_HOME) ui_toast("Wait for this round to finish (hold B to speed it up)"); }
		else { int from = persona + 1; view = V_FLOOR; S.last_game = -1; stats_save(); ui_set_focus(8200); talk_floor_return(from); }
	}
}

/* ================================================================== */
/* ROULETTE (European, single zero)                                   */
/* ================================================================== */
static const u8 WHEEL[37] = { 0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10, 5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26 };
static int is_red(int n) { static const u8 R[18] = { 1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36 }; for (int i = 0; i < 18; i++) if (R[i] == n) return 1; return 0; }
#define RB_N 48   /* 0-36 numbers, 37 1-18, 38 even, 39 red, 40 black, 41 odd, 42 19-36, 43-45 dozens, 46 = 00, 47 = 000 */
/* three wheels: European (one zero), American (0 and 00), Triple Zero (0, 00, 000). 37 = 00, 38 = 000 */
static const u8 WHEEL_US[38] = { 0, 28, 9, 26, 30, 11, 7, 20, 32, 17, 5, 22, 34, 15, 3, 24, 36, 13, 1, 37, 27, 10, 25, 29, 12, 8, 19, 31, 18, 6, 21, 33, 16, 4, 23, 35, 14, 2 };
static const u8 WHEEL_3Z[39] = { 0, 38, 37, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10, 5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26 };
static int r_np(void) { return r_type == 1 ? 38 : r_type == 2 ? 39 : 37; }
static const u8 *r_wheel(void) { return r_type == 1 ? WHEEL_US : r_type == 2 ? WHEEL_3Z : WHEEL; }
static int r_spot(int n) { return n <= 36 ? n : n == 37 ? 46 : 47; }
static const char *r_label(int n) { static char b[4][6]; static int k; k = (k + 1) & 3; if (n == 37) return "00"; if (n == 38) return "000"; snprintf(b[k], 6, "%d", n); return b[k]; }
static int r_green(int n) { return n == 0 || n >= 37; }
static int rb[RB_N], rb_last[RB_N], r_spinning, r_result = -1, r_hist[10], r_nh, r_auto;
static u64 r_done_at;
static u64 r_t0; static float r_wheel0, r_ball0; static int r_dur = 4200;
static int r_undo_s[64], r_undo_v[64], r_nundo;   /* - takes back the last chip */
static float r_ball_r, r_ball_v;   /* the ball's distance out (x wheel size) and speed, for drawing */
static int r_total(void) { int t = 0; for (int i = 0; i < RB_N; i++) t += rb[i]; return t; }
static int r_wins(int spot, int n)
{
	if (spot <= 36) return spot == n ? 36 : 0;
	if (spot >= 46) return r_spot(n) == spot ? 36 : 0;
	if (r_green(n)) return 0;
	switch (spot) {
	case 37: return n <= 18 ? 2 : 0;
	case 38: return !(n & 1) ? 2 : 0;
	case 39: return is_red(n) ? 2 : 0;
	case 40: return !is_red(n) ? 2 : 0;
	case 41: return (n & 1) ? 2 : 0;
	case 42: return n >= 19 ? 2 : 0;
	default: { int d = spot - 43; return (n > d * 12 && n <= d * 12 + 12) ? 3 : 0; }
	}
}
static void r_place(int spot)
{
	if (r_spinning) return;
	int v = chip_val();
	if (r_total() + v > SEC_MAX[sec]) { ui_toast("That's over the table limit"); return; }
	if (r_total() + v > cfg.chips) { ui_toast("Not enough chips"); return; }
	rb[spot] += v; plat_rumble(8); music_sfx(SFX_CHIP, 0);
	if (r_nundo < 64) { r_undo_s[r_nundo] = spot; r_undo_v[r_nundo] = v; r_nundo++; }
}
static void r_spin(void)
{
	int t = r_total();
	if (!t) { ui_toast("Put some chips on the table first"); return; }
	if (!big_ok(t, r_spin)) return;
	cfg.chips -= t;
	S.bet_k[1] = chip_sel;
	memcpy(rb_last, rb, sizeof rb);
	r_result = r_wheel()[rnd() % r_np()];
	r_spinning = 1; r_t0 = plat_ms();
	music_sfx(SFX_TICK, 0);
	r_dur = 4200 * anim_k() / 100; if (r_dur < 1100) r_dur = 1100;
	{ static const int TK[8] = { 2600, 2870, 2950, 3400, 3470, 3935, 3985, 4150 }; for (int k = 0; k < 8; k++) music_sfx(SFX_TICK, TK[k] * r_dur / 4200); }
	r_wheel0 = (rnd() % 360) * 0.01745f; r_ball0 = (rnd() % 360) * 0.01745f;
	plat_rumble(20);
}
static void r_finish(void)
{
	r_spinning = 0;
	int n = r_result, back = 0, stake = r_total();
	for (int i = 0; i < RB_N; i++) if (rb[i]) back += rb[i] * r_wins(i, n);
	cfg.chips += back;
	if (rb[r_spot(n)] && back) achieve(AC_ROUL);
	for (int i = 9; i > 0; i--) r_hist[i] = r_hist[i - 1];
	r_hist[0] = n; if (r_nh < 10) r_nh++;
	char m[64];
	if (back) snprintf(m, sizeof m, "%s %s. You win %d", r_label(n), r_green(n) ? "green" : is_red(n) ? "red" : "black", back - stake);
	else snprintf(m, sizeof m, "%s %s. You lose %d", r_label(n), r_green(n) ? "green" : is_red(n) ? "red" : "black", stake);
	say(back > stake ? 1 : back < stake ? -1 : 0, m);
	{	/* what Trigger Lynx saw */
		memset(&ev, 0, sizeof ev); ev.game = 1; ev.num = n;
		snprintf(ev.what, sizeof ev.what, "%s %s", r_label(n), r_green(n) ? "green" : is_red(n) ? "red" : "black");
		if (rb[r_spot(n)] && back && n <= 36) ev.flags |= F_SPECIAL;
		if (r_green(n) && back < stake) ev.flags |= F_ZERO;
		if (back < stake && n) {
			const u8 *W = r_wheel(); int np = r_np();
			int idx = 0; for (int i = 0; i < np; i++) if (W[i] == n) idx = i;
			int a = W[(idx + 1) % np], b = W[(idx + np - 1) % np];
			if (n <= 36 && ((a <= 36 && rb[a]) || (b <= 36 && rb[b]))) ev.flags |= F_CLOSE;
		}
	}
	memset(rb, 0, sizeof rb); r_nundo = 0;
	round_done(back - stake, stake);
	r_done_at = plat_ms();
	if (r_auto > 0) r_auto--;
	if (goal_open) r_auto = 0;
}
/* the wheel: wooden bowl, a ball track, the pockets with gold frets, a turned cone and a turret.
   br = how far out the ball is, bv = how fast it is going (for its blur trail) */
static void wheel_draw(int cx, int cy, int R, float rot, float ball, int show_n)
{
	float br = r_ball_r > 0 ? r_ball_r : 0.82f, bv = r_ball_v;
	gfx_circle(cx + 2, cy + 5, R + 14, RGB(0, 0, 0), 110);
	gfx_circle(cx, cy, R + 14, RGB(56, 30, 14), 255);                 /* the bowl */
	gfx_circle(cx, cy, R + 12, RGB(120, 70, 32), 255);
	gfx_circle(cx - 2, cy - 2, R + 10, RGB(150, 92, 44), 255);
	gfx_circle(cx, cy, R + 8, RGB(40, 24, 14), 255);                  /* the ball track */
	gfx_ring(cx, cy, R + 6, 1, RGB(90, 60, 36), 200);
	gfx_circle(cx, cy, R + 2, RGB(210, 170, 90), 255);                /* gold edge */
	const u8 *W = r_wheel(); int np = r_np();
	float step = 6.2831853f / np;
	for (int i = 0; i < np; i++) {
		float a0 = rot + i * step, a1 = a0 + step;
		int n = W[i];
		u32 c = r_green(n) ? RGB(20, 140, 60) : is_red(n) ? RGB(196, 28, 38) : RGB(22, 22, 26);
		int X0 = cx + (int)(cosf(a0) * R), Y0 = cy + (int)(sinf(a0) * R), X1 = cx + (int)(cosf(a1) * R), Y1 = cy + (int)(sinf(a1) * R);
		gfx_tri(cx, cy, X0, Y0, X1, Y1, c, 255);
		/* light from the top left: pockets facing it are brighter */
		float m = cosf(a0 + step / 2 + 2.356f);
		if (m > 0) gfx_tri(cx, cy, X0, Y0, X1, Y1, RGB(255, 255, 255), (int)(m * 40));
	}
	/* the inner pocket ring (where the ball lands) */
	gfx_ring(cx, cy, R * 76 / 100, R * 8 / 100, RGB(0, 0, 0), 90);
	for (int i = 0; i < np; i++) {   /* gold frets */
		float a = rot + i * step;
		gfx_line(cx + (int)(cosf(a) * R * 0.62f), cy + (int)(sinf(a) * R * 0.62f), cx + (int)(cosf(a) * R), cy + (int)(sinf(a) * R), 1, RGB(236, 200, 110), 230);
	}
	gfx_ring(cx, cy, R * 80 / 100, 1, RGB(236, 200, 110), 200);
	/* the cone */
	gfx_circle(cx, cy, R * 63 / 100, RGB(236, 200, 110), 255);
	gfx_circle(cx, cy, R * 60 / 100, RGB(90, 52, 22), 255);
	gfx_circle(cx - R / 20, cy - R / 20, R * 52 / 100, RGB(136, 82, 36), 255);
	gfx_circle(cx, cy, R * 34 / 100, RGB(100, 60, 26), 255);
	gfx_ring(cx, cy, R * 44 / 100, 1, RGB(170, 110, 50), 160);
	/* the turret: four arms and a knob */
	for (int k = 0; k < 4; k++) {
		float a = rot + k * 1.5708f;
		int ex = cx + (int)(cosf(a) * R * 0.46f), ey = cy + (int)(sinf(a) * R * 0.46f);
		gfx_line(cx + 1, cy + 2, ex + 1, ey + 2, 3, RGB(0, 0, 0), 90);
		gfx_line(cx, cy, ex, ey, 3, RGB(236, 206, 130), 255);
		gfx_circle(ex, ey, R > 50 ? 4 : 2, RGB(250, 226, 160), 255);
	}
	gfx_circle(cx, cy, R > 50 ? 9 : 5, RGB(250, 226, 160), 255);
	gfx_circle(cx - 2, cy - 2, R > 50 ? 3 : 2, RGB(255, 255, 240), 255);
	/* the ball, with a blur trail when it's fast */
	float bR = R * br;
	int tr = bv > 0.004f ? 6 : 0;
	for (int k = tr; k >= 1; k--) {
		float a = ball + bv * k * 14;
		gfx_circle(cx + (int)(cosf(a) * bR), cy + (int)(sinf(a) * bR), 4, RGB(250, 250, 250), 70 - k * 10);
	}
	int bx = cx + (int)(cosf(ball) * bR), by = cy + (int)(sinf(ball) * bR);
	gfx_circle(bx + 1, by + 2, R > 50 ? 5 : 3, RGB(0, 0, 0), 120);
	gfx_circle(bx, by, R > 50 ? 5 : 3, RGB(236, 236, 240), 255);
	gfx_circle(bx - 1, by - 1, R > 50 ? 2 : 1, RGB(255, 255, 255), 255);
	if (show_n >= 0) {
		const char *t = r_label(show_n);
		u32 c = r_green(show_n) ? RGB(20, 140, 60) : is_red(show_n) ? RGB(200, 30, 40) : RGB(24, 24, 28);
		gfx_circle(cx, cy + 2, 21, RGB(0, 0, 0), 110);
		gfx_circle(cx, cy, 20, c, 255); gfx_ring(cx, cy, 20, 2, RGB(240, 220, 160), 255);
		text_c(F_BODYB, cx, cy - 11, t, C_WHITE, 255);
	}
}
static void roulette(int blocked)
{
	static const char *const RT[3] = { "Roulette", "US Roulette", "Triple Zero Roulette" }, *const RS[3] = { "Single zero. Straight up pays 35 to 1.", "0 and 00. Straight up pays 35 to 1.", "0, 00 and 000. The house loves this one." };
	game_felt(sec ? "VIP Roulette" : RT[r_type], RS[r_type], RGB(22, 110, 64), RGB(10, 64, 36));
	u64 now = plat_ms();
	/* the wheel (bottom left) */
	int wx = 132, wy = 316, WR = 84;
	float rot = r_wheel0, ball = r_ball0;
	int shown = r_spinning ? -1 : r_nh ? r_hist[0] : -1;
	if (r_spinning) {
		float p = (now - r_t0) / (float)r_dur; if (p > 1) p = 1;
		float e = 1 - (1 - p) * (1 - p) * (1 - p);
		/* end with the result's pocket under the ball at the top */
		const u8 *W = r_wheel(); int np = r_np();
		int idx = 0; for (int i = 0; i < np; i++) if (W[i] == r_result) idx = i;
		float step = 6.2831853f / np, target = -1.5708f - (idx + 0.5f) * step;
		float wend = target + 6.2831853f * 3;
		rot = r_wheel0 + (wend - r_wheel0) * e;
		float total = 6.2831853f * 6 + (r_ball0 + 1.5708f);
		ball = r_ball0 - total * e;
		r_ball_v = total * 3 * (1 - p) * (1 - p) / (float)r_dur;   /* radians per ms */
		/* it rides the track, then drops in and bounces between the frets */
		if (p < 0.62f) r_ball_r = 1.07f;
		else { float q = (p - 0.62f) / 0.38f; r_ball_r = 0.78f + 0.29f * (1 - q) * fabsf(cosf(q * 9.4f)); }
		if (p >= 1) { r_wheel0 = target; r_ball0 = -1.5708f; r_finish(); }
	} else if (r_nh) { ball = -1.5708f; r_ball_r = 0.78f; r_ball_v = 0; }
	else { r_ball_r = 1.07f; r_ball_v = 0; }
	wheel_draw(wx, wy, WR, rot, ball, shown);
	r_ball_r = 0; r_ball_v = 0;
	gfx_tri(wx - 7, wy - WR - 16, wx + 7, wy - WR - 16, wx, wy - WR - 4, RGB(255, 214, 90), 255);
	/* the board: 0, then 3 rows of 12 */
	int bx = 286, by = 82, cw = 26, chh = 30;
	{	/* the zeros: one, two or three green boxes */
		int nz = r_type + 1, zh = chh * 3 / nz;
		for (int z = 0; z < nz; z++) {
			int n = z == 0 ? 0 : 36 + z, spot = r_spot(n), zy = by + z * zh;
			int f = ui_item(z == 0 ? 8700 : 8746 + z - 1, 256, zy, 28, zh - 1);
			gfx_round(256, zy, 28, zh - 1, 6, RGB(20, 140, 60), 255);
			gfx_round_line(256, zy, 28, zh - 1, 6, 1, RGB(240, 230, 200), 200);
			text_c(F_SMALL, 270, zy + zh / 2 - 10, r_label(n), C_WHITE, 255);
			if (rb[spot]) chip_draw(270, zy + zh - 12, 8, rb[spot], 0);
			if (!r_spinning && r_nh && r_hist[0] == n) gfx_round_line(255, zy - 1, 30, zh + 1, 6, 2, RGB(255, 214, 90), 255);
			if (f & UI_FOCUS) ui_focus_ring(256, zy, 28, zh - 1, 6);
			if (f & UI_CLICK) r_place(spot);
		}
	}
	for (int n = 1; n <= 36; n++) {
		int col = (n - 1) / 3, row = 2 - (n - 1) % 3;
		int x = bx + col * cw, y = by + row * chh;
		int f = ui_item(8700 + n, x, y, cw - 1, chh - 1);
		gfx_rect(x, y, cw - 1, chh - 1, is_red(n) ? RGB(190, 30, 40) : RGB(24, 24, 28), 255);
		char t[4]; snprintf(t, sizeof t, "%d", n);
		text_c(F_SMALL, x + cw / 2, y + 5, t, C_WHITE, 255);
		if (rb[n]) chip_draw(x + cw / 2, y + chh / 2 + 2, 9, rb[n], 0);
		if (!r_spinning && r_nh && r_hist[0] == n) gfx_round_line(x - 1, y - 1, cw + 1, chh + 1, 4, 2, RGB(255, 214, 90), 255);
		if (f & UI_FOCUS) ui_focus_ring(x, y, cw - 1, chh - 1, 4);
		if (f & UI_CLICK) r_place(n);
	}
	/* outside bets */
	static const char *OL[6] = { "1-18", "Even", "Red", "Black", "Odd", "19-36" };
	for (int k = 0; k < 6; k++) {
		int x = 256 + k * 58, y = by + chh * 3 + 6, spot = 37 + k;
		int f = ui_item(8700 + spot, x, y, 56, 28);
		u32 c = k == 2 ? RGB(190, 30, 40) : k == 3 ? RGB(24, 24, 28) : RGB(10, 70, 40);
		gfx_round(x, y, 56, 28, 6, c, 255); gfx_round_line(x, y, 56, 28, 6, 1, RGB(240, 230, 200), 160);
		text_c(F_SMALL, x + 28, y + 4, OL[k], C_WHITE, 255);
		if (rb[spot]) chip_draw(x + 50, y + 5, 6, rb[spot], 0);
		if (f & UI_FOCUS) ui_focus_ring(x, y, 56, 28, 6);
		if (f & UI_CLICK) r_place(spot);
	}
	static const char *DL[3] = { "1st 12", "2nd 12", "3rd 12" };
	for (int k = 0; k < 3; k++) {
		int x = 256 + k * 116, y = by + chh * 3 + 38, spot = 43 + k;
		int f = ui_item(8700 + spot, x, y, 114, 28);
		gfx_round(x, y, 114, 28, 6, RGB(10, 70, 40), 255); gfx_round_line(x, y, 114, 28, 6, 1, RGB(240, 230, 200), 160);
		text_c(F_SMALL, x + 57, y + 4, DL[k], C_WHITE, 255);
		if (rb[spot]) chip_draw(x + 100, y + 14, 9, rb[spot], 0);
		if (f & UI_FOCUS) ui_focus_ring(x, y, 114, 28, 6);
		if (f & UI_CLICK) r_place(spot);
	}
	/* history */
	int hy = by + chh * 3 + 74;
	text(F_SMALL, 256, hy, "Last:", RGB(200, 220, 210), 255);
	for (int i = 0; i < r_nh && i < 9; i++) {
		int n = r_hist[i];
		gfx_circle(310 + i * 30, hy + 10, 12, r_green(n) ? RGB(20, 140, 60) : is_red(n) ? RGB(190, 30, 40) : RGB(24, 24, 28), 255);
		text_c(F_SMALL, 310 + i * 30, hy + 1, r_label(n), C_WHITE, 255);
	}
	/* chips and buttons */
	int y = 330;
	chip_picker(250, y, 8750);
	char b[32]; snprintf(b, sizeof b, "Bet %d", r_total());
	text_c(F_BODYB, 350, y + 54, b, RGB(255, 214, 90), 255);
	char al[16]; if (r_auto) snprintf(al, sizeof al, "Auto %d", r_auto); else snprintf(al, sizeof al, "Auto");
	if ((ui_tile(8763, 548, y, 56, 44, -1, al, r_auto ? ST_PRIMARY : ST_NORMAL, 0) & UI_CLICK) && !opt_open) {
		if (r_auto) r_auto = 0;
		else { int t = 0; for (int i = 0; i < RB_N; i++) t += rb[i] ? rb[i] : 0; if (!t) for (int i = 0; i < RB_N; i++) t += rb_last[i];
		       if (!t) ui_toast("Place a bet first, then Auto repeats it 10 times"); else { r_auto = 10; if (!r_total()) memcpy(rb, rb_last, sizeof rb); if (!r_spinning) r_spin(); } }
	}
	if (r_auto && !r_spinning && plat_ms() > r_done_at + 1600 && !opt_open && !goal_open) {
		int t = 0; for (int i = 0; i < RB_N; i++) t += rb_last[i];
		if (t > cfg.chips) { r_auto = 0; ui_toast("Auto stopped: not enough chips"); } else { memcpy(rb, rb_last, sizeof rb); r_spin(); }
	}
	if ((ui_tile(8760, 456, y, 86, 44, -1, "Spin", r_spinning ? ST_GHOST : ST_NORMAL, 0) & UI_CLICK) && !r_spinning && !opt_open) {
		if (!r_total() && r_total() == 0) {
			int again = 0; for (int i = 0; i < RB_N; i++) again += rb_last[i];
			if (again && again <= cfg.chips) memcpy(rb, rb_last, sizeof rb);
		}
		r_spin();
	}
	if ((ui_tile(8761, 450, y + 50, 54, 30, -1, "Clear", ST_NORMAL, 0) & UI_CLICK) && !r_spinning) { memset(rb, 0, sizeof rb); r_nundo = 0; }
	if ((ui_tile(8762, 507, y + 50, 48, 30, -1, "Redo", ST_NORMAL, 0) & UI_CLICK) && !r_spinning) {
		int t = 0; for (int i = 0; i < RB_N; i++) t += rb_last[i];
		if (t && t <= cfg.chips) { memcpy(rb, rb_last, sizeof rb); r_nundo = 0; } else ui_toast("Nothing to rebet");
	}
	/* your saved layout ("my numbers"): 2 saves the chips on the table, Mine puts them back */
	if ((ui_tile(8764, 558, y + 50, 46, 30, -1, "Mine", ST_NORMAL, 0) & UI_CLICK) && !r_spinning) {
		int t = 0; for (int i = 0; i < RB_N; i++) t += S.srb[r_type][i];
		if (!t) ui_toast("No saved bet yet: place chips, then press 2 to save them");
		else if (t > cfg.chips) ui_toast("Not enough chips for your saved bet");
		else { for (int i = 0; i < RB_N; i++) rb[i] = S.srb[r_type][i]; r_nundo = 0; ui_toast("Your saved bet is on the table"); }
	}
	if (msg[0] && !r_spinning && r_nh) {
		int w = text_w(F_BODYB, msg) + 30;
		gfx_round(430 - w / 2, 276, w, 30, 15, msg_tone > 0 ? RGB(40, 150, 90) : msg_tone < 0 ? RGB(150, 50, 50) : RGB(60, 70, 90), 235);
		text_c(F_BODYB, 430, 279, msg, C_WHITE, 255);
	}
	ui_footer_begin();
	ui_footer_hint("A", "Place chip");
	ui_footer_hint("1", "Spin");
	ui_footer_hint("HOME", "Floor");
	if (!blocked && !opt_open && (in->down & BTN_1) && !r_spinning) r_spin();
	if (!blocked && !opt_open && !r_spinning && (in->down & BTN_2)) {
		if (!r_total()) ui_toast("Place some chips first, then press 2 to save them");
		else { for (int i = 0; i < RB_N; i++) S.srb[r_type][i] = rb[i]; stats_save(); ui_toast("Saved. Press Mine to put this bet down again"); }
	}
	if (!blocked && !opt_open && !r_spinning && (in->down & BTN_MINUS) && r_nundo) {
		r_nundo--; rb[r_undo_s[r_nundo]] -= r_undo_v[r_nundo]; if (rb[r_undo_s[r_nundo]] < 0) rb[r_undo_s[r_nundo]] = 0;
		in->down &= ~BTN_MINUS; music_sfx(SFX_CHIP, 0);
	}
	game_common(blocked, r_spinning);
}

/* ================================================================== */
/* SLOTS                                                              */
/* ================================================================== */
enum { SY_CHERRY, SY_LEMON, SY_BELL, SY_BAR, SY_STAR, SY_DIAMOND, SY_SEVEN, SY_COUNT };
static const u8 SY_W[SY_COUNT] = { 7, 7, 6, 5, 3, 2, 2 };
static u8 strip[3][32];
static int s_ready, s_spin, s_pos[3], s_bet_k = 1, s_auto, s_k = 100;
static u64 s_t0;
static const int S_PAY[SY_COUNT] = { 8, 8, 12, 20, 40, 75, 150 };
static void s_build(void)
{
	for (int r = 0; r < 3; r++) {
		int n = 0;
		for (int s = 0; s < SY_COUNT; s++) for (int k = 0; k < SY_W[s]; k++) strip[r][n++] = s;
		for (int i = 31; i > 0; i--) { int j = rnd() % (i + 1); u8 t = strip[r][i]; strip[r][i] = strip[r][j]; strip[r][j] = t; }
	}
	s_ready = 1;
}
static void sym_draw(int s, int cx, int cy, int a)
{
	switch (s) {
	case SY_CHERRY:
		gfx_line(cx - 8, cy + 4, cx + 2, cy - 16, 2, RGB(40, 140, 40), a); gfx_line(cx + 10, cy + 6, cx + 2, cy - 16, 2, RGB(40, 140, 40), a);
		gfx_circle(cx - 9, cy + 8, 10, RGB(210, 20, 40), a); gfx_circle(cx + 10, cy + 10, 10, RGB(210, 20, 40), a);
		gfx_circle(cx - 12, cy + 5, 3, RGB(255, 150, 160), a); gfx_circle(cx + 7, cy + 7, 3, RGB(255, 150, 160), a);
		break;
	case SY_LEMON:
		gfx_circle(cx - 6, cy, 14, RGB(250, 220, 40), a); gfx_circle(cx + 6, cy, 14, RGB(250, 220, 40), a);
		gfx_circle(cx - 18, cy, 4, RGB(250, 220, 40), a); gfx_circle(cx + 18, cy, 4, RGB(250, 220, 40), a);
		gfx_circle(cx - 6, cy - 5, 4, RGB(255, 250, 180), a);
		break;
	case SY_BELL:
		gfx_circle(cx, cy - 6, 12, RGB(240, 190, 40), a);
		gfx_tri(cx - 12, cy - 6, cx + 12, cy - 6, cx + 18, cy + 12, RGB(240, 190, 40), a);
		gfx_tri(cx - 12, cy - 6, cx + 18, cy + 12, cx - 18, cy + 12, RGB(240, 190, 40), a);
		gfx_circle(cx, cy + 15, 5, RGB(180, 120, 20), a);
		break;
	case SY_BAR:
		gfx_round(cx - 24, cy - 12, 48, 24, 5, RGB(24, 24, 30), a);
		gfx_round_line(cx - 24, cy - 12, 48, 24, 5, 2, RGB(240, 240, 240), a);
		text_c(F_SMALL, cx, cy - 9, "BAR", RGB(250, 250, 250), a);
		break;
	case SY_STAR:
		for (int k = 0; k < 5; k++) {
			float a0 = -1.5708f + k * 1.2566f, a1 = a0 + 0.6283f, a2 = a0 - 0.6283f;
			gfx_tri(cx + (int)(cosf(a0) * 22), cy + (int)(sinf(a0) * 22), cx + (int)(cosf(a1) * 9), cy + (int)(sinf(a1) * 9), cx + (int)(cosf(a2) * 9), cy + (int)(sinf(a2) * 9), RGB(255, 210, 40), a);
		}
		gfx_circle(cx, cy, 10, RGB(255, 210, 40), a);
		break;
	case SY_DIAMOND:
		gfx_tri(cx - 18, cy - 6, cx + 18, cy - 6, cx, cy + 20, RGB(60, 200, 240), a);
		gfx_tri(cx - 18, cy - 6, cx + 18, cy - 6, cx, cy - 18, RGB(150, 235, 255), a);
		break;
	default:
		text_c(F_TITLE, cx + 2, cy - 18, "7", RGB(120, 0, 10), a);
		text_c(F_TITLE, cx, cy - 20, "7", RGB(230, 30, 40), a);
		break;
	}
}
static int s_pay(int a, int b, int c, int *jack)
{
	*jack = 0;
	int prem = (a >= SY_STAR) && (b >= SY_STAR) && (c >= SY_STAR);
	if (a == b && b == c) { if (a == SY_SEVEN) *jack = 1; return a == SY_SEVEN && sec ? 500 : S_PAY[a]; }
	if (prem) return 10;
	if (a == SY_CHERRY && b == SY_CHERRY) return 4;
	if (a == SY_CHERRY) return sec ? 1 : 2;
	return 0;
}
static void s_start(void)
{
	int bet = SEC_CHIP[sec][s_bet_k];
	if (bet > cfg.chips) { ui_toast("Not enough chips for that bet"); s_auto = 0; return; }
	if (!big_ok(bet, s_start)) { s_auto = 0; return; }
	cfg.chips -= bet;
	S.bet_k[2] = s_bet_k;
	for (int r = 0; r < 3; r++) s_pos[r] = rnd() % 32;
	s_spin = 1; s_t0 = plat_ms(); msg[0] = 0;
	s_k = anim_k();
	music_sfx(SFX_LEVER, 0); music_sfx(SFX_REEL, 1000 * s_k / 100); music_sfx(SFX_REEL, 1450 * s_k / 100); music_sfx(SFX_REEL, 1900 * s_k / 100);
	plat_rumble(15);
}
static void s_end(void)
{
	s_spin = 0;
	int bet = SEC_CHIP[sec][s_bet_k], jack;
	int m = s_pay(strip[0][s_pos[0]], strip[1][s_pos[1]], strip[2][s_pos[2]], &jack);
	int win = bet * m;
	cfg.chips += win;
	if (jack) achieve(AC_777);
	char t[64];
	if (m >= 40) snprintf(t, sizeof t, "JACKPOT! %dx - you win %d", m, win);
	else if (m) snprintf(t, sizeof t, "%dx - you win %d", m, win);
	else snprintf(t, sizeof t, "No win");
	say(win > bet ? 1 : win < bet ? -1 : 0, t);
	{	/* what Thunder Crow saw */
		static const char *const SYP[SY_COUNT] = { "cherries", "lemons", "bells", "bars", "stars", "diamonds", "sevens" };
		int a = strip[0][s_pos[0]], b = strip[1][s_pos[1]], c = strip[2][s_pos[2]];
		memset(&ev, 0, sizeof ev); ev.game = 2; ev.num = m;
		if (a == b && b == c) snprintf(ev.what, sizeof ev.what, "three %s", SYP[a]);
		else if (a == SY_CHERRY && b == SY_CHERRY) snprintf(ev.what, sizeof ev.what, "two cherries");
		else if (a == SY_CHERRY) snprintf(ev.what, sizeof ev.what, "one cherry");
		else snprintf(ev.what, sizeof ev.what, "a mixed line");
		if (jack) ev.flags |= F_SPECIAL;
		if (m && m <= 4) ev.flags |= F_MINOR;
		if (!m && (a == b || b == c)) { ev.flags |= F_NEAR; snprintf(ev.sym, sizeof ev.sym, "%s", SYP[b]); }
	}
	round_done(win - bet, bet);
	if (m >= 40) { plat_rumble(400); if (!S.snd) for (int k = 0; k < 3; k++) plat_beep(1); }
}
static void slots(int blocked)
{
	if (!s_ready) s_build();
	game_felt(sec ? "Gold Slots" : "Slots", sec ? "Three sevens pay 500x" : "Line up three of a kind", RGB(90, 30, 120), RGB(40, 10, 60));
	u64 now = plat_ms();
	/* the machine: a chrome cabinet with chasing lights */
	int mx = 256, my = 82, mw = 330, mh = 196;
	int win_now = !s_spin && msg_tone > 0 && msg[0];
	u32 m_top = sec ? RGB(250, 214, 120) : RGB(236, 236, 246), m_bot = sec ? RGB(130, 90, 30) : RGB(96, 96, 116);
	gfx_round(mx + 3, my + 6, mw, mh, 18, RGB(0, 0, 0), 120);
	gfx_round_grad(mx, my, mw, mh, 18, m_top, m_bot, 255);
	gfx_round_grad(mx + 5, my + 5, mw - 10, mh - 10, 14, m_bot, m_top, 255);          /* the bevel */
	gfx_round_grad(mx + 9, my + 9, mw - 18, mh - 18, 11, shade(m_top, -20), shade(m_bot, 10), 255);
	gfx_round(mx + 12, my + 10, mw - 24, 3, 2, RGB(255, 255, 255), 90);                /* chrome highlight */
	bulbs(mx + 4, my + 4, mw - 8, mh - 8, 18, (int)(now / (s_spin ? 70 : win_now ? 50 : 220)), sec ? RGB(255, 240, 180) : RGB(255, 214, 90), RGB(110, 80, 40));
	int rw = 96, rh = 156, gap = 9, rx0 = mx + (mw - 3 * rw - 2 * gap) / 2, ry = my + 20;
	int stop_t[3] = { 1000 * s_k / 100, 1450 * s_k / 100, 1900 * s_k / 100 };
	gfx_round(rx0 - 6, ry - 6, 3 * rw + 2 * gap + 12, rh + 12, 12, RGB(16, 12, 20), 255);   /* the window */
	for (int r = 0; r < 3; r++) {
		int rx = rx0 + r * (rw + gap);
		/* the drum: brighter in the middle, darker where it curves away */
		for (int yy = 0; yy < rh; yy += 4) {
			int d = abs(yy + 2 - rh / 2) * 256 / (rh / 2);
			gfx_rect(rx, ry + yy, rw, 4, mix(RGB(252, 252, 250), RGB(150, 150, 160), d * d / 256), 255);
		}
		float off;
		int moving = s_spin && now < s_t0 + (u64)stop_t[r];
		float speed = 0;
		if (moving) {
			float p = (now - s_t0) / (float)stop_t[r];
			off = s_pos[r] - 24 * (1 - p) * (1 - p) - 1e-3f;   /* slows into place */
			speed = 48 * (1 - p) / stop_t[r] * 16;              /* symbols per frame */
		} else off = s_pos[r];
		/* a little kick back when it stops */
		int kick = 0;
		if (s_spin || now < s_t0 + (u64)stop_t[2] + 400) {
			int since = (int)(now - s_t0) - stop_t[r];
			if (since >= 0 && since < 160) kick = (int)(sinf(since / 160.0f * 3.14159f) * 6);
		}
		int base = (int)floorf(off); float frac = off - base;
		for (int k = -2; k <= 2; k++) {
			int idx = ((base + k) % 32 + 32) % 32;
			int cy = ry + rh / 2 + (int)((k - frac) * 54) + kick;
			if (cy < ry + 10 || cy > ry + rh - 10) continue;
			int d = abs(cy - (ry + rh / 2)), a = d > 50 ? 255 - (d - 50) * 5 : 255;
			if (a < 40) a = 40;
			if (moving && speed > 0.15f) {   /* motion blur: smeared copies */
				for (int m = 1; m <= 3; m++) sym_draw(strip[r][idx], rx + rw / 2, cy - m * (int)(speed * 9), a / (m + 2));
				sym_draw(strip[r][idx], rx + rw / 2, cy, a / 2);
			} else sym_draw(strip[r][idx], rx + rw / 2, cy, a);
		}
		/* glass: shadow bands top and bottom, and a sheen */
		for (int k = 0; k < 5; k++) { gfx_rect(rx, ry + k * 4, rw, 4, RGB(0, 0, 0), 90 - k * 18); gfx_rect(rx, ry + rh - 4 - k * 4, rw, 4, RGB(0, 0, 0), 90 - k * 18); }
		gfx_rect(rx + 8, ry + 6, 10, rh - 12, RGB(255, 255, 255), 30);
		gfx_rect(rx + 20, ry + 6, 4, rh - 12, RGB(255, 255, 255), 18);
	}
	/* the pay line: glows when you win */
	int lw = 3 * rw + 2 * gap + 16;
	if (win_now) {
		int pulse = 120 + (int)(sinf(now / 90.0f) * 80);
		gfx_round(rx0 - 8, ry + rh / 2 - 7, lw, 14, 7, RGB(255, 214, 90), pulse / 3);
		gfx_round(rx0 - 8, ry + rh / 2 - 3, lw, 6, 3, RGB(255, 214, 90), pulse);
		gfx_rect(rx0 - 8, ry + rh / 2 - 1, lw, 2, RGB(255, 250, 220), 255);
		if ((now / 120) % 6 == 0) spawn(2, rx0 + rnd() % lw, ry + rh / 2, 1);
	} else gfx_rect(rx0 - 8, ry + rh / 2 - 1, lw, 3, RGB(220, 40, 60), 200);
	gfx_tri(rx0 - 12, ry + rh / 2 - 7, rx0 - 12, ry + rh / 2 + 7, rx0 - 3, ry + rh / 2, RGB(220, 40, 60), 255);
	gfx_tri(rx0 + lw - 4, ry + rh / 2 - 7, rx0 + lw - 4, ry + rh / 2 + 7, rx0 + lw - 13, ry + rh / 2, RGB(220, 40, 60), 255);
	/* the lever: pulled down when you spin */
	{
		int lx = mx + mw + 14, pivot = my + 132, t = s_spin ? (int)(now - s_t0) : 9999;
		float pull = t < 420 ? sinf(t / 420.0f * 3.14159f) : 0;
		int ky = my + 36 + (int)(pull * 110);
		gfx_round(mx + mw - 2, pivot - 16, 18, 32, 6, shade(m_bot, -30), 255);
		gfx_line(lx + 1, pivot + 2, lx + 1, ky + 2, 5, RGB(0, 0, 0), 90);
		gfx_line(lx, pivot, lx, ky, 5, RGB(210, 210, 220), 255);
		gfx_line(lx - 1, pivot, lx - 1, ky, 1, RGB(255, 255, 255), 200);
		gfx_circle(lx + 1, ky + 3, 12, RGB(0, 0, 0), 100);
		gfx_circle(lx, ky, 12, RGB(200, 20, 36), 255);
		gfx_circle(lx - 4, ky - 4, 4, RGB(255, 150, 160), 255);
	}
	if (s_spin && now >= s_t0 + (u64)stop_t[2] + 150) s_end();
	/* paytable, bottom left */
	{
		int px = 30, py = 212;
		gfx_round(px - 4, py - 6, 222, 196, 12, RGB(0, 0, 0), 90);
		static const u8 ORD[7] = { SY_SEVEN, SY_DIAMOND, SY_STAR, SY_BAR, SY_BELL, SY_LEMON, SY_CHERRY };
		for (int i = 0; i < 7; i++) {
			int s = ORD[i], y = py + i * 22;
			for (int k = 0; k < 3; k++) {
				/* tiny symbols */
				int cx = px + 14 + k * 26, cy = y + 9;
				if (s == SY_SEVEN) text_c(F_SMALL, cx, cy - 9, "7", RGB(230, 30, 40), 255);
				else if (s == SY_BAR) { gfx_round(cx - 10, cy - 5, 20, 10, 3, RGB(24, 24, 30), 255); }
				else if (s == SY_CHERRY) gfx_circle(cx, cy, 5, RGB(210, 20, 40), 255);
				else if (s == SY_LEMON) gfx_circle(cx, cy, 6, RGB(250, 220, 40), 255);
				else if (s == SY_BELL) gfx_circle(cx, cy, 6, RGB(240, 190, 40), 255);
				else if (s == SY_STAR) gfx_circle(cx, cy, 6, RGB(255, 210, 40), 255);
				else { gfx_tri(cx - 6, cy - 2, cx + 6, cy - 2, cx, cy + 7, RGB(60, 200, 240), 255); gfx_tri(cx - 6, cy - 2, cx + 6, cy - 2, cx, cy - 6, RGB(150, 235, 255), 255); }
			}
			char t[16]; snprintf(t, sizeof t, "%dx", s == SY_SEVEN && sec ? 500 : S_PAY[s]);
			text_r(F_SMALL, px + 208, y, t, RGB(255, 214, 90), 255);
		}
		text(F_SMALL, px, py + 156, sec ? "3 mixed 7/diamond/star 10x" : "Any 3 of 7, diamond, star: 10x", RGB(220, 220, 230), 255);
		text(F_SMALL, px, py + 172, sec ? "2 cherries 4x   1 cherry 1x" : "2 cherries 4x   1 cherry 2x", RGB(220, 220, 230), 255);
	}
	/* bet + spin */
	int y = 292;
	text(F_SMALL, 256, y, "Bet per spin", RGB(220, 210, 240), 255);
	for (int k = 0; k < 4; k++) {
		int cx = 279 + k * 50;
		int f = ui_item(8800 + k, cx - 23, y + 18, 46, 48);
		chip_draw(cx, y + 42, 21, SEC_CHIP[sec][k], (f & UI_FOCUS) != 0);
		if (k == s_bet_k) gfx_round_line(cx - 25, y + 16, 50, 52, 12, 2, RGB(255, 214, 90), 255);
		if ((f & UI_CLICK) && !s_spin) s_bet_k = k;
	}
	if ((ui_ibutton(8810, 460, y + 16, 144, 50, IC_PLAY, s_spin ? "..." : "Spin", ST_NORMAL) & UI_CLICK) && !s_spin && !opt_open) s_start();
	if (ui_tile(8811, 460, y + 72, 144, 34, -1, s_auto ? "Auto: On" : "Auto: Off", s_auto ? ST_PRIMARY : ST_NORMAL, 0) & UI_CLICK) s_auto = !s_auto;
	if (goal_open) s_auto = 0;
	if (s_auto && !s_spin && now > s_t0 + 3200 && !opt_open) s_start();
	if (msg[0] && !s_spin) {
		int w = text_w(F_BODYB, msg) + 30;
		gfx_round(360 - w / 2, 368, w, 30, 15, msg_tone > 0 ? RGB(40, 150, 90) : RGB(80, 60, 100), 235);
		text_c(F_BODYB, 360, 371, msg, C_WHITE, 255);
	}
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	ui_footer_hint("1", "Spin");
	ui_footer_hint("HOME", "Floor");
	if (!blocked && !opt_open && (in->down & BTN_1) && !s_spin) s_start();
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) s_auto = 0;
	game_common(blocked, s_spin);
}

/* ================================================================== */
/* VIDEO POKER (Jacks or Better; Double Bonus in the Suite)           */
/* ================================================================== */
static u8 vp_deck[52]; static int vp_dp, vp_state, vp_hold[5], vp_bet_k = 1, vp_last = -1;   /* state 0 bet, 1 holding, 2 shown */
static Card vp_c[5];
static const char *VP_NAME[10] = { "Nothing", "Jacks or better", "Two pair", "Three of a kind", "Straight", "Flush", "Full house", "Four of a kind", "Straight flush", "Royal flush" };
static const int VP_JOB[10] = { 0, 1, 2, 3, 4, 6, 9, 25, 50, 250 };
static const int VP_DB[10] = { 0, 1, 1, 3, 5, 7, 10, 50, 50, 250 };   /* four of a kind: 50 (5-K), 80 (2-4), 160 (Aces) */
static int vp_rank(const Card *h, int *four_kind_rank)
{
	int cnt[13] = { 0 }, suit[4] = { 0 };
	for (int i = 0; i < 5; i++) { cnt[h[i].c % 13]++; suit[h[i].c / 13]++; }
	int flush = 0; for (int s = 0; s < 4; s++) if (suit[s] == 5) flush = 1;
	int straight = 0, high = -1;
	/* ranks with Ace high: index r=0 is Ace */
	int bits = 0; for (int r = 0; r < 13; r++) if (cnt[r]) bits |= 1 << r;
	for (int lo = 1; lo <= 8; lo++) { int m = 0x1F << lo; if ((bits & m) == m) { straight = 1; high = lo + 4; } }
	if ((bits & 0x1E01) == 0x1E01) { straight = 1; high = 13; }   /* 10 J Q K A */
	if ((bits & 0x1F) == 0x1F) straight = 1;                       /* A 2 3 4 5 */
	int pairs = 0, three = 0, four = -1;
	for (int r = 0; r < 13; r++) { if (cnt[r] == 2) pairs++; if (cnt[r] == 3) three = 1; if (cnt[r] == 4) four = r; }
	if (four_kind_rank) *four_kind_rank = four;
	if (straight && flush) return high == 13 ? 9 : 8;
	if (four >= 0) return 7;
	if (three && pairs) return 6;
	if (flush) return 5;
	if (straight) return 4;
	if (three) return 3;
	if (pairs == 2) return 2;
	if (pairs == 1) { if (cnt[0] == 2 || cnt[10] == 2 || cnt[11] == 2 || cnt[12] == 2) return 1; }
	return 0;
}
static int vp_pay(int rank, int four)
{
	if (!sec) return VP_JOB[rank];
	if (rank == 7) return four == 0 ? 160 : (four >= 1 && four <= 3) ? 80 : 50;
	return VP_DB[rank];
}
static void vp_deal(void)
{
	int bet = SEC_CHIP[sec][vp_bet_k];
	if (bet > cfg.chips) { ui_toast("Not enough chips for that bet"); return; }
	if (!big_ok(bet, vp_deal)) return;
	cfg.chips -= bet;
	S.bet_k[3] = vp_bet_k;
	for (int i = 0; i < 52; i++) vp_deck[i] = i;
	for (int i = 51; i > 0; i--) { int j = rnd() % (i + 1); u8 t = vp_deck[i]; vp_deck[i] = vp_deck[j]; vp_deck[j] = t; }
	vp_dp = 0; next_t = plat_ms();
	for (int i = 0; i < 5; i++) { vp_c[i].c = vp_deck[vp_dp++]; vp_c[i].t = next_t; music_sfx(SFX_CARD, (int)(next_t - plat_ms())); next_t += 120; vp_hold[i] = 0; }
	vp_state = 1; vp_last = -1; msg[0] = 0;
	ui_set_focus(8900);
}
static void vp_draw_cards(void)
{
	next_t = plat_ms();
	for (int i = 0; i < 5; i++) if (!vp_hold[i]) { vp_c[i].c = vp_deck[vp_dp++]; vp_c[i].t = next_t; music_sfx(SFX_CARD, (int)(next_t - plat_ms())); next_t += 140; }
	vp_state = 2;
	int four, r = vp_rank(vp_c, &four), bet = SEC_CHIP[sec][vp_bet_k];
	int m = vp_pay(r, four), win = bet * m;
	cfg.chips += win; vp_last = r;
	if (r == 9) achieve(AC_ROYAL);
	char t[64];
	if (m) snprintf(t, sizeof t, "%s! %dx - you win %d", VP_NAME[r], m, win); else snprintf(t, sizeof t, "Nothing this time");
	say(win > bet ? 1 : win < bet ? -1 : 0, t);
	{	/* what Mirror Squid saw */
		memset(&ev, 0, sizeof ev); ev.game = 3; ev.num = r;
		snprintf(ev.hand, sizeof ev.hand, "%s", VP_NAME[r]);
		if (ev.hand[0] >= 'A' && ev.hand[0] <= 'Z') ev.hand[0] += 32;
		if (r >= 3) ev.flags |= F_SPECIAL;
		if (r >= 7) ev.flags |= F_HUGE;
		if (r == 2) ev.flags |= F_MINOR;
	}
	round_done(win - bet, bet);
	ui_set_focus(8920);
}
static void poker(int blocked)
{
	game_felt(sec ? "Double Bonus Poker" : "Video Poker", sec ? "Four Aces pay 160x" : "Jacks or Better", RGB(20, 70, 130), RGB(8, 36, 76));
	/* pay table, bottom left */
	int px = 30, py = 212;
	gfx_round(px - 4, py - 6, 222, 196, 12, RGB(0, 0, 0), 90);
	for (int r = 9; r >= 1; r--) {
		int y = py + (9 - r) * 20;
		int on = vp_state == 2 && vp_last == r;
		if (on) gfx_round(px - 2, y - 1, 218, 19, 6, RGB(255, 214, 90), 90);
		text(F_SMALL, px + 4, y, VP_NAME[r], on ? C_WHITE : RGB(220, 225, 240), 255);
		char t[24];
		if (sec && r == 7) snprintf(t, sizeof t, "50-160x"); else snprintf(t, sizeof t, "%dx", sec ? VP_DB[r] : VP_JOB[r]);
		text_r(F_SMALL, px + 210, y, t, RGB(255, 214, 90), 255);
	}
	/* the five cards */
	int cy = 96;
	for (int i = 0; i < 5; i++) {
		int x = 262 + i * 70;
		int f = ui_item(8900 + i, x, cy, CW, CH + 26);
		if (vp_state) {
			if (vp_hold[i]) gfx_round(x - 5, cy - 11, CW + 10, CH + 10, 10, RGB(255, 214, 90), 60 + (int)(sinf(plat_ms() / 200.0f) * 30));
			card_anim(&vp_c[i], x, cy - (vp_hold[i] ? 6 : 0), 0);
		} else { card_draw(S.cardset, x, cy, 0, 1); card_gloss(x, cy); }
		if (vp_hold[i]) { gfx_round(x + 4, cy + CH + 4, CW - 8, 20, 10, RGB(255, 214, 90), 255); text_c(F_SMALL, x + CW / 2, cy + CH + 5, "HELD", RGB(40, 30, 0), 255); }
		if (f & UI_FOCUS) ui_focus_ring(x - 3, cy - 3, CW + 6, CH + 30, 10);
		if ((f & UI_CLICK) && vp_state == 1) { vp_hold[i] = !vp_hold[i]; plat_rumble(8); music_sfx(SFX_CHIP, 0); }
	}
	/* bet + deal/draw */
	int y = 292;
	text(F_SMALL, 256, y, "Bet", RGB(210, 220, 240), 255);
	for (int k = 0; k < 4; k++) {
		int cx = 279 + k * 50;
		int f = ui_item(8910 + k, cx - 23, y + 18, 46, 48);
		chip_draw(cx, y + 42, 21, SEC_CHIP[sec][k], (f & UI_FOCUS) != 0);
		if (k == vp_bet_k) gfx_round_line(cx - 25, y + 16, 50, 52, 12, 2, RGB(255, 214, 90), 255);
		if ((f & UI_CLICK) && vp_state != 1) vp_bet_k = k;
	}
	const char *bl = vp_state == 1 ? "Draw" : "Deal";
	if ((ui_ibutton(8920, 460, y + 16, 144, 50, IC_PLAY, bl, ST_NORMAL) & UI_CLICK) && !opt_open) { if (vp_state == 1) vp_draw_cards(); else vp_deal(); }
	if (vp_state == 1) text_c(F_SMALL, 432, 244, "Pick the cards to keep, then Draw", RGB(210, 220, 240), 255);
	if (msg[0] && vp_state == 2) {
		int w = text_w(F_BODYB, msg) + 30;
		gfx_round(432 - w / 2, 240, w, 30, 15, msg_tone > 0 ? RGB(40, 150, 90) : RGB(80, 60, 100), 235);
		text_c(F_BODYB, 432, 243, msg, C_WHITE, 255);
	}
	ui_footer_begin();
	ui_footer_hint("A", vp_state == 1 ? "Hold" : "Choose");
	ui_footer_hint("1", vp_state == 1 ? "Draw" : "Deal");
	ui_footer_hint("HOME", "Floor");
	if (!blocked && !opt_open && (in->down & BTN_1)) { if (vp_state == 1) vp_draw_cards(); else vp_deal(); }
	game_common(blocked, vp_state == 1);
}

/* ================================================================== */
/* HI-LO: higher or lower, cash out any time                          */
/* ================================================================== */
static int hl_state, hl_bet_k = 1, hl_n, hl_stake;   /* state 0 bet, 1 playing */
static float hl_pot;
static Card hl_c[16];
static int hl_rank(u8 c) { int r = c % 13; return r == 0 ? 12 : r - 1; }   /* 2 low ... Ace high */
static float hl_mult(int r, int up) { int ways = up ? 12 - r : r; return ways ? 0.97f * 13.0f / ways : 0; }
static void hl_start(void)
{
	int bet = SEC_CHIP[sec][hl_bet_k];
	if (bet > cfg.chips) { ui_toast("Not enough chips for that bet"); return; }
	if (!big_ok(bet, hl_start)) return;
	cfg.chips -= bet; hl_stake = bet; hl_pot = bet;
	S.bet_k[4] = hl_bet_k;
	hl_n = 0; next_t = plat_ms();
	hl_c[hl_n].c = rnd() % 52; hl_c[hl_n].t = next_t; music_sfx(SFX_CARD, (int)(next_t - plat_ms())); hl_n++;
	hl_state = 1; msg[0] = 0;
	ui_set_focus(8950);
}
static void hl_end(int won)
{
	int back = won ? (int)hl_pot : 0;
	cfg.chips += back;
	char t[64];
	if (won) snprintf(t, sizeof t, "Cashed out %d (%d cards)", back, hl_n - 1); else snprintf(t, sizeof t, "Wrong! You lose %d", hl_stake);
	say(back > hl_stake ? 1 : back < hl_stake ? -1 : 0, t);
	{	/* what Silent Vixen saw */
		memset(&ev, 0, sizeof ev); ev.game = 4;
		ev.run = won ? hl_n - 1 : hl_n - 2; if (ev.run < 0) ev.run = 0;
		if (hl_n) card_name(ev.card, sizeof ev.card, hl_c[hl_n - 1].c);
		if (won) { ev.flags |= F_CASH; if (ev.run >= 5) ev.flags |= F_LONG; }
		else if (ev.run >= 3) ev.flags |= F_NEAR;
	}
	hl_state = 0;
	round_done(back - hl_stake, hl_stake);
	ui_set_focus(8960);
}
static void hl_guess(int up)
{
	int r = hl_rank(hl_c[hl_n - 1].c);
	float m = hl_mult(r, up);
	if (m <= 0) { ui_toast(up ? "Nothing is higher than an Ace" : "Nothing is lower than a 2"); return; }
	if (hl_n >= 16) { hl_end(1); return; }
	next_t = plat_ms();
	u8 c = rnd() % 52; hl_c[hl_n].c = c; hl_c[hl_n].t = next_t; music_sfx(SFX_CARD, (int)(next_t - plat_ms())); hl_n++;
	int nr = hl_rank(c);
	if (nr == r) { ui_toast("Same card. No change"); return; }
	if ((up && nr > r) || (!up && nr < r)) {
		hl_pot *= m; plat_rumble(15);
		if (hl_n - 1 >= 10) achieve(AC_HILO10);
		if (hl_n >= 16) hl_end(1);
	} else hl_end(0);
}
static void hilo_game(int blocked)
{
	game_felt(sec ? "VIP Hi-Lo" : "Hi-Lo", "Higher or lower? Cash out when you like.", RGB(150, 90, 20), RGB(90, 50, 8));
	/* rules, bottom left */
	gfx_round(26, 206, 222, 120, 12, RGB(0, 0, 0), 90);
	text(F_SMALL, 36, 214, "Aces are high, 2s are low.", RGB(240, 230, 210), 255);
	text(F_SMALL, 36, 234, "Right guesses grow the pot.", RGB(240, 230, 210), 255);
	text(F_SMALL, 36, 254, "Long odds pay more.", RGB(240, 230, 210), 255);
	text(F_SMALL, 36, 274, "Same card = no change.", RGB(240, 230, 210), 255);
	text(F_SMALL, 36, 294, "A wrong guess loses it all.", RGB(240, 230, 210), 255);
	if (hl_state == 1 || hl_n) {
		/* the trail of cards, the newest big on the right */
		int shown = hl_n > 5 ? 5 : hl_n;
		for (int i = 0; i < shown; i++) {
			int idx = hl_n - shown + i;
			int x = 262 + i * 70, dx, dy;
			(void)dx; (void)dy; card_anim(&hl_c[idx], x, 90, 0);
		}
	} else { card_draw(S.cardset, 400, 90, 0, 1); card_gloss(400, 90); }
	if (hl_state == 1) {
		int r = hl_rank(hl_c[hl_n - 1].c);
		char p[64]; snprintf(p, sizeof p, "Pot %d   (%d right)", (int)hl_pot, hl_n - 1);
		text_c(F_BTN, 432, 196, p, RGB(255, 214, 90), 255);
		char hi[32], lo[32];
		float mh = hl_mult(r, 1), ml = hl_mult(r, 0);
		if (mh > 0) snprintf(hi, sizeof hi, "Higher  x%.2f", mh); else snprintf(hi, sizeof hi, "Higher  -");
		if (ml > 0) snprintf(lo, sizeof lo, "Lower  x%.2f", ml); else snprintf(lo, sizeof lo, "Lower  -");
		int y = 240;
		if ((ui_ibutton(8950, 256, y, 170, 48, IC_UP, hi, mh > 0 ? ST_NORMAL : ST_GHOST) & UI_CLICK) && !opt_open) hl_guess(1);
		if ((ui_ibutton(8951, 434, y, 170, 48, IC_DOWN, lo, ml > 0 ? ST_NORMAL : ST_GHOST) & UI_CLICK) && !opt_open && hl_state == 1) hl_guess(0);
		if (hl_state == 1 && hl_n > 1) {
			char co[32]; snprintf(co, sizeof co, "Cash out %d", (int)hl_pot);
			if ((ui_ibutton(8952, 256, y + 56, 348, 44, IC_CHECK, co, ST_NORMAL) & UI_CLICK) && !opt_open) hl_end(1);
		}
	} else {
		int y = 292;
		text(F_SMALL, 256, y, "Stake", RGB(240, 230, 210), 255);
		for (int k = 0; k < 4; k++) {
			int cx = 279 + k * 50;
			int f = ui_item(8955 + k, cx - 23, y + 18, 46, 48);
			chip_draw(cx, y + 42, 21, SEC_CHIP[sec][k], (f & UI_FOCUS) != 0);
			if (k == hl_bet_k) gfx_round_line(cx - 25, y + 16, 50, 52, 12, 2, RGB(255, 214, 90), 255);
			if (f & UI_CLICK) hl_bet_k = k;
		}
		if ((ui_ibutton(8960, 460, y + 16, 144, 50, IC_PLAY, "Start", ST_NORMAL) & UI_CLICK) && !opt_open) hl_start();
		if (msg[0]) {
			int w = text_w(F_BODYB, msg) + 30;
			gfx_round(432 - w / 2, 204, w, 30, 15, msg_tone > 0 ? RGB(40, 150, 90) : RGB(150, 50, 50), 235);
			text_c(F_BODYB, 432, 207, msg, C_WHITE, 255);
		}
	}
	ui_footer_begin();
	if (hl_state == 1) { ui_footer_hint("<>", "Choose"); ui_footer_hint("1", "Higher"); ui_footer_hint("2", "Lower"); }
	else { ui_footer_hint("A", "Choose"); ui_footer_hint("1", "Start"); }
	ui_footer_hint("HOME", "Floor");
	if (!blocked && !opt_open) {
		if (hl_state == 1 && (in->down & BTN_1)) hl_guess(1);
		else if (hl_state == 1 && (in->down & BTN_2)) hl_guess(0);
		else if (hl_state == 0 && (in->down & BTN_1)) hl_start();
	}
	game_common(blocked, hl_state == 1);
}

/* ================================================================== */
/* BACCARAT (Suite only)                                              */
/* ================================================================== */
static int bc_bet[3], bc_state;   /* bets: player, banker, tie. state 0 bet, 1 dealing, 2 done */
static Card bc_p[3], bc_b[3]; static int bc_np, bc_nb;
static u64 bc_end;
static int bc_val(const Card *k, int n) { int t = 0; for (int i = 0; i < n; i++) { int r = k[i].c % 13; t += r >= 9 ? 0 : r + 1; } return t % 10; }
static void bc_deal(void)
{
	int t = bc_bet[0] + bc_bet[1] + bc_bet[2];
	if (!t) { ui_toast("Bet on Player, Banker or Tie first"); return; }
	if (t > cfg.chips) { ui_toast("Not enough chips"); return; }
	if (!big_ok(t, bc_deal)) return;
	cfg.chips -= t;
	S.bet_k[5] = chip_sel;
	next_t = plat_ms(); bc_np = bc_nb = 0; msg[0] = 0;
#define BC_DEAL(h, n) do { h[n].c = rnd() % 52; h[n].t = next_t; music_sfx(SFX_CARD, (int)(next_t - plat_ms())); next_t += speed_ms(); n++; } while (0)
	BC_DEAL(bc_p, bc_np); BC_DEAL(bc_b, bc_nb); BC_DEAL(bc_p, bc_np); BC_DEAL(bc_b, bc_nb);
	int pv = bc_val(bc_p, 2), bv = bc_val(bc_b, 2);
	if (pv < 8 && bv < 8) {
		int p3 = -1;
		if (pv <= 5) { BC_DEAL(bc_p, bc_np); int r = bc_p[2].c % 13; p3 = r >= 9 ? 0 : r + 1; }
		int draw;
		if (p3 < 0) draw = bv <= 5;
		else draw = bv <= 2 || (bv == 3 && p3 != 8) || (bv == 4 && p3 >= 2 && p3 <= 7) || (bv == 5 && p3 >= 4 && p3 <= 7) || (bv == 6 && (p3 == 6 || p3 == 7));
		if (draw) BC_DEAL(bc_b, bc_nb);
	}
#undef BC_DEAL
	bc_state = 1; bc_end = next_t + 500;
}
static void bc_settle(void)
{
	int pv = bc_val(bc_p, bc_np), bv = bc_val(bc_b, bc_nb);
	int stake = bc_bet[0] + bc_bet[1] + bc_bet[2], back = 0;
	if (pv > bv) back += bc_bet[0] * 2;
	else if (bv > pv) back += bc_bet[1] + bc_bet[1] * 95 / 100;
	else { back += bc_bet[2] * 9 + bc_bet[0] + bc_bet[1]; if (bc_bet[2]) achieve(AC_BACC_TIE); }
	cfg.chips += back;
	char t[64];
	snprintf(t, sizeof t, "%s %d to %d.  %s %d", pv > bv ? "Player wins" : bv > pv ? "Banker wins" : "Tie", pv > bv ? pv : bv, pv > bv ? bv : pv,
	         back >= stake ? "You win" : "You lose", back >= stake ? back - stake : stake - back);
	say(back > stake ? 1 : back < stake ? -1 : 0, t);
	{	/* what Crimson Viper saw */
		memset(&ev, 0, sizeof ev); ev.game = 5; ev.pv = pv; ev.dv = bv;
		if (pv == bv && bc_bet[2]) ev.flags |= F_TIE;
		int wv = pv > bv ? pv : bv, wn = pv > bv ? bc_np : bc_nb;
		if (pv != bv && wv >= 8 && wn == 2) ev.flags |= F_SPECIAL;
		if (abs(pv - bv) == 1) ev.flags |= F_CLOSE;
	}
	bc_state = 2;
	round_done(back - stake, stake);
	memset(bc_bet, 0, sizeof bc_bet);
}
static void baccarat(int blocked)
{
	game_felt("Baccarat", "Banker pays 0.95 to 1. Tie pays 8 to 1.", 0, 0);
	u64 now = plat_ms();
	/* hands */
	const char *who[2] = { "PLAYER", "BANKER" };
	for (int s = 0; s < 2; s++) {
		Card *h = s ? bc_b : bc_p; int n = s ? bc_nb : bc_np;
		int cx = s ? 520 : 344;
		text_c(F_BODYB, cx, 82, who[s], RGB(250, 230, 180), 255);
		if (bc_state) {
			int w = CW + (n - 1) * 24, x0 = cx - w / 2;
			for (int i = 0; i < n; i++) card_anim(&h[i], x0 + i * 24, 108, 0);
			Card v[3]; int m = 0; for (int i = 0; i < n; i++) if (now >= h[i].t) v[m++] = h[i];
			if (m) { char t[8]; snprintf(t, sizeof t, "%d", bc_val(v, m)); gfx_circle(cx, 214, 14, RGB(0, 0, 0), 140); text_c(F_BODYB, cx, 203, t, C_WHITE, 255); }
		}
	}
	if (bc_state == 1 && now >= bc_end) bc_settle();
	/* bet spots */
	static const char *BL[3] = { "Player  1:1", "Banker  0.95:1", "Tie  8:1" };
	for (int k = 0; k < 3; k++) {
		int x = 256 + k * 118, y = 240;
		int f = ui_item(8980 + k, x, y, 112, 50);
		gfx_round(x, y, 112, 50, 10, RGB(0, 0, 0), 100); gfx_round_line(x, y, 112, 50, 10, 1, RGB(230, 196, 110), 200);
		text_c(F_SMALL, x + 56, y + 4, BL[k], RGB(250, 230, 180), 255);
		if (bc_bet[k]) { char b[16]; snprintf(b, sizeof b, "%d", bc_bet[k]); chip_draw(x + 24, y + 34, 10, bc_bet[k], 0); text(F_SMALL, x + 40, y + 25, b, RGB(255, 214, 90), 255); }
		if (f & UI_FOCUS) ui_focus_ring(x, y, 112, 50, 10);
		if ((f & UI_CLICK) && bc_state != 1 && !opt_open) {
			if (bc_state == 2) { bc_state = 0; bc_np = bc_nb = 0; msg[0] = 0; }
			int v = chip_val(), t = bc_bet[0] + bc_bet[1] + bc_bet[2];
			if (t + v > SEC_MAX[sec]) ui_toast("That's over the table limit");
			else if (t + v > cfg.chips) ui_toast("Not enough chips");
			else { bc_bet[k] += v; plat_rumble(8); music_sfx(SFX_CHIP, 0); }
		}
	}
	chip_picker(250, 300, 8990);
	if ((ui_ibutton(8995, 460, 300, 144, 48, IC_PLAY, "Deal", bc_state == 1 ? ST_GHOST : ST_NORMAL) & UI_CLICK) && bc_state != 1 && !opt_open) { if (bc_state == 2) bc_state = 0; bc_deal(); }
	if ((ui_tile(8996, 460, 354, 144, 30, -1, "Clear bets", ST_NORMAL, 0) & UI_CLICK) && bc_state != 1) memset(bc_bet, 0, sizeof bc_bet);
	if (msg[0] && bc_state == 2) {
		int w = text_w(F_BODYB, msg) + 30; if (w > 360) w = 360;
		gfx_round(432 - w / 2, 386, w, 26, 13, msg_tone > 0 ? RGB(40, 150, 90) : msg_tone < 0 ? RGB(150, 50, 50) : RGB(60, 70, 90), 240);
		text_fit(F_SMALL, 432 - w / 2 + 15, 389, w - 30, msg, C_WHITE, 255);
	}
	/* how it works, bottom left */
	gfx_round(26, 206, 222, 120, 12, RGB(0, 0, 0), 90);
	text(F_SMALL, 36, 214, "Bet on the hand closest to 9.", RGB(240, 230, 210), 255);
	text(F_SMALL, 36, 234, "10s and faces count 0.", RGB(240, 230, 210), 255);
	text(F_SMALL, 36, 254, "The cards draw by the rules.", RGB(240, 230, 210), 255);
	text(F_SMALL, 36, 274, "You just watch.", RGB(240, 230, 210), 255);
	ui_footer_begin();
	ui_footer_hint("A", "Bet / Choose");
	ui_footer_hint("1", "Deal");
	ui_footer_hint("HOME", "Floor");
	if (!blocked && !opt_open && (in->down & BTN_1) && bc_state != 1) { if (bc_state == 2) bc_state = 0; bc_deal(); }
	game_common(blocked, bc_state == 1);
}

#include "table_ai.inc"
#include "liar.inc"
#include "holdem.inc"
#include "backroom.inc"
#include "phase1.inc"

/* ================================================================== */
/* THE FLOOR                                                          */
/* ================================================================== */
enum { GM_BJ, GM_ROUL, GM_SLOTS, GM_POKER, GM_HILO, GM_BACC, GM_COUNT };
static const char *GM_NAME[2][GM_COUNT] = {
	{ "Blackjack", "Roulette", "Slots", "Video Poker", "Hi-Lo", "Baccarat" },
	{ "Blackjack", "VIP Roulette", "Gold Slots", "Double Bonus", "VIP Hi-Lo", "Baccarat" },
};
static int bj_pick;    /* blackjack mode menu open */

/* ---- the rooms, and which games are in each ---- */
enum { R_MAIN, R_TABLES, R_DICE, R_SLOTHALL, R_LOUNGE, R_POKERROOM, R_BACK, R_MINE, R_SUITE, R_N };
static const char *const ROOM_NAME[R_N] = { "Main Floor", "Card Tables", "Dice & Wheels", "Slot Hall", "The Lounge", "Poker Room", "The Back Room", "My Games", "High Roller Suite" };
static const u32 ROOM_TOP[R_N] = { RGB(26, 104, 66), RGB(22, 84, 130), RGB(110, 60, 20), RGB(90, 30, 120), RGB(40, 70, 90), RGB(20, 70, 100), RGB(22, 86, 64), RGB(70, 70, 90), RGB(118, 22, 34) };
static const s8 ROOM_GAMES[R_N][6] = {
	{ G_BJ, G_ROUL, G_SLOTS, G_VPOKER, G_HILO, G_WAR },
	{ G_WAR, G_BJ, -1, -1, -1, -1 },
	{ G_BIGWHEEL, G_ROUL_US, G_ROUL_000, G_ROUL, -1, -1 },
	{ G_SLOTS, G_VPOKER, -1, -1, -1, -1 },
	{ G_KENO, -1, -1, -1, -1, -1 },
	{ G_HOLDEM, -1, -1, -1, -1, -1 },
	{ G_LIAR, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1 },   /* My Games: filled in from your favourites and recent games */
	{ G_BJ, G_ROUL, G_SLOTS, G_VPOKER, G_HILO, G_BACC },
};

static void open_any(int g);
static int gl_open;              /* the list of all games */
static int cur_game = -1; static u64 game_t0;   /* for per-game stats */
static const char *game_name(int g);


static void bigwheel_art(int cx, int cy, int r);
static void cup_draw(int cx, int base, int lift, int shake, int a);
static void die_draw(int cx, int cy, int sz, int face, int a, int hl);
/* a little picture for each game tile */
static void game_art(int g, int cx, int cy)
{
	switch (g) {
	case GM_BJ: card_draw(S.cardset, cx - 42, cy - 42, 0 + 13 * 3, 0); card_draw(S.cardset, cx - 18, cy - 38, 12 + 13 * 0, 0); break;
	case GM_ROUL: wheel_draw(cx, cy + 6, 30, plat_ms() / 1500.0f, -1.5708f, -1); break;
	case GM_SLOTS: gfx_round(cx - 44, cy - 20, 88, 48, 8, RGB(250, 250, 252), 255); sym_draw(SY_SEVEN, cx - 26, cy + 4, 255); sym_draw(SY_SEVEN, cx, cy + 4, 255); sym_draw(SY_SEVEN, cx + 26, cy + 4, 255); break;
	case GM_POKER: for (int i = 0; i < 3; i++) card_draw(S.cardset, cx - 52 + i * 20, cy - 42, 9 + i + 13 * 1, 0); break;
	case GM_HILO: card_draw(S.cardset, cx - 38, cy - 42, 6, 0); icon(IC_UP, cx + 22, cy - 12, 20, RGB(255, 214, 90)); icon(IC_DOWN, cx + 22, cy + 16, 20, RGB(255, 214, 90)); break;
	case G_BACC: gfx_circle(cx - 18, cy + 6, 20, RGB(200, 40, 50), 255); gfx_circle(cx + 18, cy + 6, 20, RGB(30, 90, 200), 255); text_c(F_SMALL, cx - 18, cy - 3, "P", C_WHITE, 255); text_c(F_SMALL, cx + 18, cy - 3, "B", C_WHITE, 255); break;
	case G_WAR: card_draw(S.cardset, cx - 50, cy - 40, 12 + 13 * 3, 0); card_draw(S.cardset, cx - 12, cy - 36, 11 + 13 * 1, 0); break;
	case G_BIGWHEEL: bigwheel_art(cx, cy + 4, 34); break;
	case G_KENO: for (int i = 0; i < 12; i++) { int bx = cx - 40 + (i % 4) * 27, by = cy - 26 + (i / 4) * 24; gfx_circle(bx, by, 10, (i * 7) % 5 == 1 ? RGB(255, 214, 90) : RGB(240, 236, 226), 255); char n[4]; snprintf(n, sizeof n, "%d", 3 + i * 6); text_c(F_SMALL, bx, by - 9, n, RGB(30, 30, 30), 255); } break;
	case G_ROUL_US: case G_ROUL_000: wheel_draw(cx, cy + 6, 30, plat_ms() / 1700.0f, -1.5708f, -1); text_c(F_SMALL, cx, cy - 6, g == G_ROUL_US ? "00" : "000", RGB(255, 255, 255), 255); break;
	case G_HOLDEM: card_draw(S.cardset, cx - 46, cy - 42, 0 + 13 * 3, 0); card_draw(S.cardset, cx - 22, cy - 38, 0 + 13 * 0, 0); chip_stack(cx + 36, cy + 20, 1250, 13); break;
	case G_LIAR: cup_draw(cx - 22, cy + 22, 0, 0, 255); die_draw(cx + 18, cy + 8, 22, 5, 255, 0); die_draw(cx + 36, cy - 12, 18, 1, 255, 0); break;
	default: break;
	}
}

static void open_game(int gm)
{
	msg[0] = 0;
	static const int PS_OF[GM_COUNT] = { -1, PS_DUSTY, PS_CORVUS, PS_MIMIC, PS_LUPA, PS_TALON };
	persona = PS_OF[gm];
	anger = 0; pw_streak = dw_streak = 0;
	if (S.quips) { if (persona < 0) talk_bj_menu(); else talk_sit(persona + 1); }
	S.last_game = gm; S.last_sec = sec; stats_save();
	chip_sel = S.bet_k[gm] & 3; s_bet_k = S.bet_k[2] & 3; vp_bet_k = S.bet_k[3] & 3; hl_bet_k = S.bet_k[4] & 3;
	switch (gm) {
	case GM_BJ: if (sec) sit(3); else { bj_pick = 1; ui_set_focus(9000); } return;
	case GM_ROUL: view = V_ROUL; ui_set_focus(8700); break;
	case GM_SLOTS: view = V_SLOTS; ui_set_focus(8810); break;
	case GM_POKER: vp_state = 0; view = V_POKER; ui_set_focus(8920); break;
	case GM_HILO: hl_state = 0; hl_n = 0; view = V_HILO; ui_set_focus(8960); break;
	default: bc_state = 0; bc_np = bc_nb = 0; view = V_BACC; ui_set_focus(8980); break;
	}
}

static const char *game_name(int g)
{
	static const char *const N[G_N] = { "Blackjack", "Roulette", "Slots", "Video Poker", "Hi-Lo", "Baccarat", "Casino War", "Big Wheel", "Keno",
		"US Roulette", "Triple Zero", "Texas Hold'em", "Liar's Dice" };
	if (sec && g <= G_BACC) return GM_NAME[1][g];
	return g >= 0 && g < G_N ? N[g] : "";
}
static void war_open(void);
static void bigwheel_open(void);
static void keno_open(void);
static void open_any(int g)
{
	/* remember it as a recent game */
	{ int k = 0; while (k < 2 && S.recent[k] != g) k++; for (; k > 0; k--) S.recent[k] = S.recent[k - 1]; S.recent[0] = g; }
	cur_game = g; game_t0 = plat_ms();
	switch (g) {
	case G_BJ: case G_ROUL: case G_SLOTS: case G_VPOKER: case G_HILO: case G_BACC: if (g == G_ROUL && r_type) { r_type = 0; r_nh = 0; } open_game(g); break;
	case G_ROUL_US: if (r_type != 1) { r_type = 1; r_nh = 0; } open_game(GM_ROUL); break;
	case G_ROUL_000: if (r_type != 2) { r_type = 2; r_nh = 0; } open_game(GM_ROUL); break;
	case G_WAR: war_open(); break;
	case G_BIGWHEEL: bigwheel_open(); break;
	case G_KENO: keno_open(); break;
	case G_HOLDEM: br_game = 1; setup_open = 1; ui_set_focus(8245); break;
	case G_LIAR: br_game = 0; setup_open = 1; ui_set_focus(8245); break;
	}
}

static void floor_screen(int blocked)
{
	persona = -1;
	if (sec && cfg.chips < SUITE_NEED && cfg.chips < 100) sec = 0;
	/* the marquee sign */
	{
		u64 now = plat_ms();
		/* centred on the header row (y 50): star, title, chips and the ? button all sit on that line */
		gfx_round(22, 22, 604, 64, 14, RGB(0, 0, 0), 110);
		gfx_round_grad(20, 18, 604, 64, 14, RGB(40, 14, 22), RGB(14, 4, 8), 240);
		gfx_round_line(20, 18, 604, 64, 14, 2, RGB(230, 196, 110), 255);
		bulbs(26, 23, 592, 54, 16, (int)(now / 160), RGB(255, 226, 140), RGB(90, 60, 30));
		shine(20, 18, 604, 64, 5200, 0);
	}
	header("Casino", NULL);
	text_fit(F_SMALL, 196, 38, 200, sec ? "High Roller Suite" : "Main Floor. Just for fun.", RGB(230, 196, 110), 255);
	/* which room: < name > */
	int ty = 84;
	int prev_room = floor_room;
	if (ui_button(8190, 256, ty, 44, 34, "<", ST_NORMAL) & UI_CLICK) { floor_room = (floor_room + R_N - 1) % R_N; setup_open = 0; }
	if (ui_button(8191, 560, ty, 44, 34, ">", ST_NORMAL) & UI_CLICK) { floor_room = (floor_room + 1) % R_N; setup_open = 0; }
	if (floor_room == R_SUITE && prev_room != R_SUITE && cfg.chips < SUITE_NEED && !(S.ach & (1 << AC_SUITE))) {
		ui_toast("The Suite needs 2,500 chips at the door");
		floor_room = prev_room == R_SUITE - 1 ? 0 : R_SUITE - 1;
	}
	if (floor_room != prev_room) {
		sec = floor_room == R_SUITE; chip_sel = 1;
		if (floor_room == R_SUITE) {
			achieve(AC_SUITE);
			if (S.quips) { char t[160]; snprintf(t, sizeof t, "Welcome to the Suite, %s. The chips are bigger up here. So are the losses.", nm()); dealer_read(); quip(t); }
		} else if (floor_room == R_BACK && S.quips >= 2 && chance(70)) { dealer_read(); char t[240]; if (compose(t, sizeof t, W_MOTH, I_LD_HELLO, TG_YOU, 0)) talk_push(t, W_MOTH, 0, 0); }
	}
	sec = floor_room == R_SUITE;
	gfx_round(304, ty, 252, 34, 12, RGB(0, 0, 0), 150);
	gfx_round_line(304, ty, 252, 34, 12, 1, RGB(230, 196, 110), 160);
	text_c(F_BODYB, 430, ty + 3, ROOM_NAME[floor_room], RGB(255, 240, 200), 255);
	for (int k = 0; k < R_N; k++) gfx_circle(430 - (R_N - 1) * 5 + k * 10, ty + 40, 2, k == floor_room ? RGB(255, 214, 90) : RGB(120, 110, 100), 255);
	if (setup_open) { if (back_room(blocked)) return; }
	/* My Games: your favourites first, then what you played last */
	int mine[6] = { -1, -1, -1, -1, -1, -1 }, nmine = 0;
	for (int g = 0; g < G_N && nmine < 6; g++) if (S.fav >> g & 1) mine[nmine++] = g;
	for (int k = 0; k < 3 && nmine < 6; k++) { int g = S.recent[k], dup = 0; if (g < 0 || g >= G_N) continue; for (int j = 0; j < nmine; j++) dup |= mine[j] == g; if (!dup) mine[nmine++] = g; }
	if (floor_room == R_MINE && !nmine && !setup_open) text_c(F_SMALL, 430, 220, "Play a few games, or press 1 on a game to star it", RGB(220, 210, 190), 230);
	/* game tiles: 3 x 2 */
	for (int slot = 0; !setup_open && slot < 6; slot++) {
		int gm = floor_room == R_MINE ? mine[slot] : ROOM_GAMES[floor_room][slot];
		if (gm < 0) continue;
		int x = 256 + (slot % 3) * 118, y = 130 + (slot / 3) * 130, w = 112, h = 122;
		int f = ui_item(8200 + slot, x, y, w, h);
		u32 top = ROOM_TOP[floor_room], bot = shade(ROOM_TOP[floor_room], -60);
		gfx_shadow(x, y, w, h, 14);
		/* a little table: wooden rail, felt, the game on it */
		u32 rail = sec ? RGB(150, 110, 50) : RGB(120, 70, 36);
		gfx_round(x, y, w, h, 14, shade(rail, -60), 255);
		gfx_round_grad(x + 1, y + 1, w - 2, h - 2, 13, shade(rail, 30), shade(rail, -30), 255);
		gfx_round_grad(x + 5, y + 5, w - 10, h - 10, 10, top, bot, 255);
		gfx_round_line(x + 5, y + 5, w - 10, h - 10, 10, 2, RGB(0, 0, 0), 60);
		gfx_round_line(x + 9, y + 9, w - 18, h - 18, 7, 1, RGB(230, 196, 110), 60);
		game_art(gm, x + w / 2, y + 48);
		shine(x, y, w, h - 26, 6000, (u64)slot * 900);
		if (f & UI_FOCUS) bulbs(x - 3, y - 3, w + 6, h + 6, 14, (int)(plat_ms() / 110), RGB(255, 226, 140), RGB(90, 60, 30));
		gfx_round(x + 4, y + h - 26, w - 8, 22, 11, RGB(0, 0, 0), 150);
		text_fit(F_SMALL, x + 10, y + h - 25, w - 20, game_name(gm), RGB(250, 244, 226), 255);
		if (S.fav >> gm & 1) icon(IC_STAR, x + w - 16, y + 16, 14, RGB(255, 214, 90));
		if (f & UI_FOCUS) {
			ui_focus_ring(x, y, w, h, 14);
			if (!blocked && !opt_open && !bj_pick && (in->down & BTN_1)) {   /* 1 = star it for My Games */
				S.fav ^= 1 << gm; stats_save(); in->down &= ~BTN_1;
				ui_toast(S.fav >> gm & 1 ? "Starred: it's in My Games now" : "Unstarred");
			}
		}
		if ((f & UI_CLICK) && !opt_open && !bj_pick) {
			if (cfg.chips < SEC_CHIP[sec][0]) ui_toast("Not enough chips for this game");
			else { open_any(gm); if (view != V_FLOOR) return; }
		}
	}
	/* left side: buttons under Vex's corner */
	int by = 212;
	if (ui_ibutton(8210, 26, by, 218, 40, IC_STAR, "Card sets", ST_NORMAL) & UI_CLICK) { open_view(V_DECKS, 8500 + S.cardset); return; }
	if (ui_ibutton(8211, 26, by + 46, 218, 40, IC_MENU, "Stats", ST_NORMAL) & UI_CLICK) { open_view(V_STATS, 8600); return; }
	if (ui_ibutton(8212, 26, by + 92, 218, 40, IC_GEAR, "Options", ST_NORMAL) & UI_CLICK) { opt_open = 1; ui_set_focus(9000); }
	char pl[32]; snprintf(pl, sizeof pl, "Playing: Player %d", S.player + 1);
	if (ui_ibutton(8214, 26, by + 138, 218, 40, IC_GAMEPAD, pl, ST_NORMAL) & UI_CLICK) {
		int t = cfg.chips; cfg.chips = S.chips2; S.chips2 = t; S.player ^= 1;
		if (cfg.chips <= 0) cfg.chips = 1000;
		config_save(); stats_save();
		char m[48]; snprintf(m, sizeof m, "Player %d: %d chips", S.player + 1, cfg.chips); ui_toast(m);
		if (S.quips >= 2) { dealer_read(); quip(S.player ? "Oh? Someone else holds the remote now. The other one. I see you too." : "The first one returns. I knew you'd come back to me."); }
	}
	if (cfg.chips < 5 && ui_ibutton(8213, 26, by + 184, 218, 30, IC_PLUS, "Get 1,000 free chips", ST_NORMAL) & UI_CLICK) { cfg.chips = 1000; config_save(); ui_toast("1,000 chips added"); }
	creep_shade();
	quip_draw();
	quip_tick();
	/* blackjack: pick a mode */
	if (bj_pick) {
		static const char *M[6] = { "Single Deck", "Classic (6 decks)", "Vegas (6:5)", "Spanish 21", "Free Bet", "Switch" };
		static const int IC3[6] = { IC_STAR, IC_STAR, IC_STAR, IC_PLAY, IC_PLAY, IC_SWAP };
		static const int TB6[6] = { 0, 1, 2, 4, 5, 6 };
		int k = menu_popup(&bj_pick, M, IC3, 6, "Blackjack: pick a table");
		if (k >= 0) {
			if (cfg.chips < TABLES[TB6[k]].min) ui_toast("Not enough chips for this table");
			else { sit(TB6[k]); ui_toast(TABLES[TB6[k]].blurb); return; }
		}
	}
	ui_footer_begin();
	ui_footer_hint("A", "Play");
	ui_footer_hint("1", "Star");
	ui_footer_hint("2", "All games");
	ui_footer_hint("+", "Options");
	ui_footer_hint("HOME", "Leave");
	/* 2 = a list of every game */
	if (gl_open) {
		const char *IT[G_N]; int IC[G_N];
		for (int g = 0; g < G_N; g++) { IT[g] = game_name(g); IC[g] = S.fav >> g & 1 ? IC_STAR : IC_PLAY; }
		int k = menu_popup(&gl_open, IT, IC, G_N, "All games");
		if (k >= 0) {
			if (k == G_BACC) { if (cfg.chips < SUITE_NEED && !(S.ach & (1 << AC_SUITE))) { ui_toast("Baccarat is in the High Roller Suite (2,500 chips)"); return; } floor_room = R_SUITE; sec = 1; }
			else if (sec) { floor_room = R_MAIN; sec = 0; }
			open_any(k); return;
		}
	}
	if (blocked || opt_open || bj_pick || gl_open) return;
	if (in->down & BTN_PLUS) { opt_open = 1; ui_set_focus(9000); }
	else if ((in->down & BTN_2) && !setup_open) { gl_open = 1; ui_set_focus(9000); }
	else if (in->down & (BTN_HOME | BTN_B)) leave_casino();
}

/* busy = in the middle of a round (you can't leave until it's done) */
static int cas_busy(void)
{
	if (view == V_TABLE) return g == G_PLAY || g == G_DEAL || g == G_INSURE || g == G_DEALER;
	if (view == V_ROUL) return r_spinning;
	if (view == V_SLOTS) return s_spin;
	if (view == V_POKER) return vp_state == 1;
	if (view == V_HILO) return hl_state == 1;
	if (view == V_BACC) return bc_state == 1;
	if (view == V_LIAR) return LD.state != LS_OVER && ld_alive(0);
	if (view == V_WAR) return war_state == 1;
	if (view == V_BIGWHEEL) return bw_spinning;
	if (view == V_KENO) return kn_state == 1;
	if (view == V_HOLDEM) return holdem_busy();
	return 0;
}

/* the summary when you leave */
static void summary_screen(int blocked)
{
	header("Leaving so soon?", "Your night at the casino");
	int mins = (int)((plat_ms() - sess_t0) / 60000), net = cfg.chips - sess_start_chips;
	gfx_round(40, 92, 560, 214, 18, C_CARD, 255);
	const char *L[5] = { "Time played", "Rounds", "Chips when you came in", "Chips now", "Biggest win" };
	char V[5][32];
	snprintf(V[0], 32, "%d min", mins < 1 ? 1 : mins); snprintf(V[1], 32, "%d", sess_rounds);
	snprintf(V[2], 32, "%d", sess_start_chips); snprintf(V[3], 32, "%d  (%s%d)", cfg.chips, net >= 0 ? "+" : "", net);
	snprintf(V[4], 32, "%d", sess_best);
	for (int i = 0; i < 5; i++) {
		text(F_BODYB, 64, 106 + i * 38, L[i], C_DIM, 255);
		text_r(F_BTN, 576, 102 + i * 38, V[i], i == 3 ? (net > 0 ? C_GOOD : net < 0 ? C_BAD : C_TEXT) : C_TEXT, 255);
	}
	/* a parting shot */
	static char bye[160]; static u64 made;
	if (!made || plat_ms() - made > 600000) {
		made = plat_ms();
		dealer_read();
		if (net > 500) snprintf(bye, sizeof bye, "Phantom Moth: Go, then, %s. Take my chips. I'll be here when you come back. You always come back.", nm());
		else if (net < -500) snprintf(bye, sizeof bye, "Phantom Moth: Leaving %d chips lighter, %s. Thank you for your... donation.", -net, nm());
		else snprintf(bye, sizeof bye, "Phantom Moth: Even, more or less. How dull. Come back when you're braver, %s.", nm());
	}
	if (S.quips) {
		char L2[3][64]; int n = wrap_lines(bye, 400, L2, 3);
		gfx_round(40, 314, 560, 26 + n * 18, 14, RGB(16, 10, 26), 230);
		for (int i = 0; i < n; i++) text(F_SMALL, 56, 318 + i * 18, L2[i], RGB(230, 210, 250), 255);
	}
	if (ui_ibutton(8650, 400, 380, 200, 40, IC_CHECK, "Leave", ST_NORMAL) & UI_CLICK) { made = 0; go_back(); return; }
	if (ui_ibutton(8651, 190, 380, 200, 40, IC_BACK, "Stay", ST_NORMAL) & UI_CLICK) { made = 0; view = V_FLOOR; ui_set_focus(8200); return; }
	ui_footer_begin();
	ui_footer_hint("A", "Choose");
	if (!blocked && (in->down & (BTN_HOME | BTN_B))) { made = 0; go_back(); }
}

static int floor_backroom(void) { return floor_room == R_POKERROOM || floor_room == R_BACK; }
