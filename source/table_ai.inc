/*
   table_ai.inc - shared by the games where several dealers sit and play with you
   (Liar's Dice, Texas Hold'em): the seats around the table, a speech bubble for each seat,
   and the talk queue that lets them speak in turn, answer each other and argue.
   Included in casino_games.inc. WiiRremote Plus by Habex. GPL-3.0.
*/
#define TB_MAX 6
static int tb_n;                        /* seats, you are seat 0 */
static int tb_who[TB_MAX];              /* -1 = you, else which dealer */
static int tb_x[TB_MAX], tb_y[TB_MAX];  /* where each seat is */
static const u32 TB_ACC[W_N] = { RGB(170, 90, 220), RGB(230, 170, 70), RGB(120, 150, 185), RGB(60, 200, 190), RGB(170, 220, 250), RGB(220, 60, 70) };
static const u8 TB_SPD[W_N] = { 34, 24, 40, 19, 33, 26 };
static const char *const SHORT_NAME[W_N] = { "Moth", "Lynx", "Crow", "Squid", "Vixen", "Viper" };

typedef struct { char t[200]; u64 at; int speed, shout, creepy, blip; } Bub;
static Bub tb_bub[TB_MAX];
typedef struct { int seat; char t[200]; int gap, creepy; } TQ;
static TQ tb_q[16]; static int tb_qn;
static u64 tb_next_ok;

/* seats around the oval table: you at the bottom, the others spread around the top */
static void tb_layout(void)
{
	static const int PX[5] = { 88, 172, 320, 468, 552 }, PY[5] = { 206, 118, 92, 118, 206 };
	static const u8 PICK[6][5] = { { 0 }, { 2 }, { 1, 3 }, { 0, 2, 4 }, { 0, 1, 3, 4 }, { 0, 1, 2, 3, 4 } };
	int ai = tb_n - 1; if (ai < 1) ai = 1; if (ai > 5) ai = 5;
	tb_x[0] = 320; tb_y[0] = 330;
	for (int i = 0; i < ai; i++) { tb_x[i + 1] = PX[PICK[ai][i]]; tb_y[i + 1] = PY[PICK[ai][i]]; }
}
/* pass the remote: Player 2 sits in seat 1 (tb_who -2) and the remote goes back and forth */
static int tb_p2;              /* two people at the table this game */
static int tb_view;            /* whose turn it is with the remote (seat 0 or 1) */
static int tb_hand = -1;       /* waiting for the remote to be handed to this seat */
#define ME tb_view                  /* whoever has the remote right now */
#define YOU (tb_p2 ? -99 : 0)        /* the seat that reads as "You" (nobody when two people play) */
static int is_human(int s) { return s >= 0 && s < tb_n && tb_who[s] < 0; }
static int is_ai(int s) { return s >= 0 && s < tb_n && tb_who[s] >= 0; }
static const char *tb_name(int seat) { return seat < 0 || seat >= tb_n ? "someone" : tb_who[seat] == -2 ? "Player 2" : tb_who[seat] < 0 ? (tb_p2 ? "Player 1" : nm()) : SHORT_NAME[tb_who[seat]]; }
/* the whole screen goes dark so nobody sees the other cards/dice, until the new person presses A */
static int tb_handoff(int blocked)
{
	if (tb_hand < 0) return 0;
	gfx_rect(0, 0, 640, 480, RGB(6, 4, 10), 250);
	char t[64]; snprintf(t, sizeof t, "Pass the Wii Remote to %s", tb_name(tb_hand));
	text_c(F_TITLE, 320, 170, t, RGB(255, 240, 200), 255);
	text_c(F_BODY, 320, 222, "No peeking! Press A when you have it.", RGB(220, 210, 190), 230);
	ui_footer_begin(); ui_footer_hint("A", "I have it");
	if (!blocked && (in->down & BTN_A)) { tb_view = tb_hand; tb_hand = -1; in->down = 0; plat_rumble(30); }
	return 1;
}
/* it's a person's turn: if it's the other person, the remote has to change hands first */
static int tb_need(int s) { if (!tb_p2 || !is_human(s) || s == tb_view) return 0; if (tb_hand < 0) tb_hand = s; return 1; }

/* how long a line takes to say at that speed (with the pauses) */
static u64 tb_type_ms(const char *t, int spd)
{
	u64 s = 0;
	if (!spd) return 0;
	for (int i = 0; t[i]; i++) { s += spd; char c = t[i]; if (c == '.' || c == '!' || c == '?') s += t[i + 1] == '.' ? 70 : t[i + 1] ? 260 : 0; else if (c == ',') s += 120; }
	return s;
}
static int tb_typed(const char *t, int spd, u64 el)
{
	u64 s = 0; int i;
	if (!spd) return (int)strlen(t);
	for (i = 0; t[i]; i++) { s += spd; char c = t[i]; if (c == '.' || c == '!' || c == '?') s += t[i + 1] == '.' ? 70 : t[i + 1] ? 260 : 0; else if (c == ',') s += 120; if (s > el) break; }
	return i;
}

/* put a line in the queue: they speak in order, each waits for the one before */
static void tb_push(int seat, const char *t, int gap, int creepy)
{
	if (!t || !t[0] || seat < 0 || seat >= tb_n || tb_who[seat] < 0) return;
	if (tb_qn == 16) { memmove(tb_q, tb_q + 1, 15 * sizeof(TQ)); tb_qn = 15; }
	TQ *q = &tb_q[tb_qn++];
	q->seat = seat; snprintf(q->t, sizeof q->t, "%s", t); q->gap = gap; q->creepy = creepy;
}
/* build a line for a seat (with the talk engine) and queue it. target = the seat they're talking to, or -1 */
static int tb_target = -1;
static int tb_say(int seat, int intent, u32 flags, int target, int gap)
{
	if (!S.quips || !is_ai(seat)) return 0;
	int w = tb_who[seat];
	tonight_start(w);
	tb_target = target;
	ctx_tgt = target >= 0 && target < tb_n ? tb_name(target) : "";
	if (target >= 0 && target < tb_n) flags |= tb_who[target] < 0 ? TG_YOU : (u32)(TG_MOTH << tb_who[target]);
	char t[240];
	int ok = compose(t, sizeof t, w, intent, flags, S.quips >= 2);
	if (!ok && (flags & TG_ALL)) ok = compose(t, sizeof t, w, intent, flags & ~TG_ALL, S.quips >= 2);
	if (ok) tb_push(seat, t, gap, 0);
	return ok;
}
static int tb_busy(void) { return tb_qn > 0 || plat_ms() < tb_next_ok; }
static void tb_pump(void)
{
	u64 now = plat_ms();
	if (!tb_qn || now < tb_next_ok) return;
	TQ q = tb_q[0];
	memmove(tb_q, tb_q + 1, (tb_qn - 1) * sizeof(TQ)); tb_qn--;
	/* one speaker at a time: the last bubble goes when the next person talks, so they never pile up */
	for (int k = 0; k < TB_MAX; k++) tb_bub[k].t[0] = 0;
	Bub *b = &tb_bub[q.seat];
	int w = tb_who[q.seat];
	snprintf(b->t, sizeof b->t, "%s", q.t);
	b->at = now; b->blip = 0; b->creepy = q.creepy;
	b->shout = talk_mood(w) == -2;
	b->speed = (TB_SPD[w] * (b->shout ? 7 : 10) / 10 + (q.creepy ? 8 : 0)) * tspd_k() / 100;
	chatlog_add(w, b->t);
	if (b->shout) plat_rumble(120);
	tb_next_ok = now + tb_type_ms(b->t, b->speed) + 350 + q.gap;
}
static void tb_clear(void) { tb_hand = -1; tb_qn = 0; tb_next_ok = 0; memset(tb_bub, 0, sizeof tb_bub); }

/* a speech bubble for one seat, pointing at them */
static void tb_bubble(int seat)
{
	Bub *b = &tb_bub[seat];
	if (!b->t[0] || !is_ai(seat)) return;
	u64 now = plat_ms();
	u64 full = tb_type_ms(b->t, b->speed);
	if (now > b->at + full + 3200) { b->t[0] = 0; return; }
	int shown = now > b->at ? tb_typed(b->t, b->speed, now - b->at) : 0;
	int len = strlen(b->t); if (shown > len) shown = len;
	int w = tb_who[seat];
	for (; b->blip < shown; b->blip++) {
		char c = b->t[b->blip];
		if (((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) && !(b->blip & 1)) music_voice(w, c, b->creepy);
	}
	char part[200]; snprintf(part, sizeof part, "%.*s", shown, b->t);
	int big = S.big_text, lh = big ? 21 : 16;
	int bw = big ? 236 : 176;
	char L[6][64]; int n = wrap_lines(part, bw - 18, L, 6);
	char Lf[6][64]; int nf = wrap_lines(b->t, bw - 18, Lf, 6);   /* the size of the whole line, so the bubble doesn't grow */
	int bh = 26 + (nf ? nf : 1) * lh;
	/* sit it toward the middle of the table from the seat */
	int sx = tb_x[seat], sy = tb_y[seat];
	int x = sx < 200 ? sx + 26 : sx > 440 ? sx - 26 - bw : sx - bw / 2;
	int y = sy < 150 ? sy + 44 : sy - bh - 34;
	if (sx == 320 && sy < 150) { x = sx + 36; y = sy - 34; }   /* the top seat talks off to the side */
	if (sx < 200 && sy > 150) { x = sx - 70; y = sy - bh - 44; }   /* side seats talk above their heads */
	if (sx > 440 && sy > 150) { x = sx + 70 - bw; y = sy - bh - 44; }
	if (x < 6) x = 6; if (x + bw > 634) x = 634 - bw; if (y < 6) y = 6;
	int age = (int)(now - b->at), fa = age < 140 ? age * 255 / 140 : 255;
	u64 gone = b->at + full + 3200; int fo = now + 500 > gone ? (int)((gone - now) * 255 / 500) : 255;
	if (fo < fa) fa = fo;
	int md = talk_mood(w);
	u32 acc = md == -2 ? RGB(255, 60, 60) : TB_ACC[w];
	int jx = b->shout && shown < len ? (int)(rnd() % 3) - 1 : 0;
	gfx_round(x + 2 + jx, y + 3, bw, bh, 12, RGB(0, 0, 0), 90 * fa / 255);
	gfx_round(x + jx, y, bw, bh, 12, md == -2 ? RGB(44, 8, 12) : RGB(18, 12, 28), 228 * fa / 255);
	gfx_round_line(x + jx, y, bw, bh, 12, md <= -1 ? 2 : 1, acc, 230 * fa / 255);
	/* the little tail toward the speaker */
	int tx = sx < x ? x + 14 : sx > x + bw ? x + bw - 14 : sx;
	if (y > sy) gfx_tri(tx - 6, y + 1, tx + 6, y + 1, sx + (sx - tx) / 3, y - 10, md == -2 ? RGB(44, 8, 12) : RGB(18, 12, 28), 228 * fa / 255);
	else gfx_tri(tx - 6, y + bh - 1, tx + 6, y + bh - 1, sx + (sx - tx) / 3, y + bh + 10, md == -2 ? RGB(44, 8, 12) : RGB(18, 12, 28), 228 * fa / 255);
	text(F_SMALL, x + 10 + jx, y + 3, WHO_NAME[w], shade(acc, 30), fa);
	for (int i = 0; i < n; i++) {
		if (b->creepy) text(bub_font(), x + 10 + jx, y + 22 + i * lh, L[i], RGB(255, 30, 70), 80 * fa / 255);
		text(bub_font(), x + 8 + jx, y + 22 + i * lh, L[i], RGB(240, 234, 250), fa);
	}
}
