/*
   casino_fx.inc - the casino's look: felt and carpet textures, wooden rails, 3D chips,
   card flips, coin showers, confetti, marquee bulbs and win banners.
   Included in casino.c after the card and chip drawing code.
   WiiRremote Plus by Habex. GPL-3.0.
*/

static u32 hash32(u32 x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static u32 mix(u32 a, u32 b, int t)   /* t 0..256 */
{
	int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255, br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
	return RGB(ar + ((br - ar) * t >> 8), ag + ((bg - ag) * t >> 8), ab + ((bb - ab) * t >> 8));
}
static u32 shade(u32 c, int d)   /* add d to each channel */
{
	int r = ((c >> 16) & 255) + d, g = ((c >> 8) & 255) + d, b = (c & 255) + d;
	if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0; if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
	return RGB(r, g, b);
}

/* ================================================================== */
/* felt: fibre noise, a faint diamond weave, light falling off at the edges */
/* ================================================================== */
#define FT_W 608
#define FT_H 400
/* one light map (how much lighter or darker each spot is), shared by every table colour */
static s8 *felt_map;
static s8 *felt_tex(void)
{
	if (felt_map) return felt_map;
	felt_map = malloc(FT_W * FT_H);
	if (!felt_map) return NULL;
	for (int y = 0; y < FT_H; y++)
		for (int x = 0; x < FT_W; x++) {
			u32 h = hash32(x * 7919 + y * 104729);
			int n = (int)(h & 15) - 8;                                     /* fibres */
			if (((x + y) % 26 == 0) || (((x - y) % 26 + 26) % 26 == 0)) n -= 4;   /* the weave */
			int dx = x - FT_W / 2, dy = y - FT_H / 2;
			int v = (dx * dx) / 900 + (dy * dy) / 400;                    /* edges darker */
			n -= v > 60 ? 60 : v;
			felt_map[y * FT_W + x] = (s8)(n < -127 ? -127 : n);
		}
	return felt_map;
}
static u8 clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
/* the textured felt inside a rounded box, then a padded wooden rail with a gold pinstripe */
static void felt_draw(int x, int y, int w, int h, int r, u32 top, u32 bot, u32 rail)
{
	/* rail: dark edge, wood, a highlight, then the felt inside */
	u32 wood = rail;
	gfx_round(x - 9, y - 9 + 4, w + 18, h + 18, r + 9, RGB(0, 0, 0), 120);
	gfx_round(x - 9, y - 9, w + 18, h + 18, r + 9, shade(wood, -70), 255);
	gfx_round_grad(x - 7, y - 7, w + 14, h + 14, r + 7, shade(wood, 30), shade(wood, -40), 255);
	gfx_round_line(x - 6, y - 6, w + 12, h + 12, r + 6, 1, shade(wood, 70), 150);
	s8 *tex = felt_tex(); u32 *dst = gfx_buf();
	int TWd = gfx_w(), THd = gfx_h();
	if (!tex) { gfx_round_grad(x, y, w, h, r, top, bot, 255); return; }
	for (int yy = 0; yy < h && yy < FT_H; yy++) {
		int sy = y + yy; if (sy < 0 || sy >= THd) continue;
		int in = 0;
		if (yy < r) { int d = r - yy; in = r - (int)sqrtf((float)(r * r - d * d)); }
		else if (yy >= h - r) { int d = yy - (h - r) + 1; in = r - (int)sqrtf((float)(r * r - d * d)); }
		int x0 = in, x1 = w - in; if (x1 > FT_W) x1 = FT_W;
		u32 base = mix(top, bot, yy * 256 / h);
		int br = (base >> 16) & 255, bg = (base >> 8) & 255, bb = base & 255;
		u32 *row = dst + sy * TWd; s8 *src = tex + yy * FT_W;
		for (int xx = x0; xx < x1; xx++) {
			int sx = x + xx; if (sx < 0 || sx >= TWd) continue;
			int n = src[xx];
			row[sx] = RGB(clamp8(br + n), clamp8(bg + n), clamp8(bb + n));
		}
	}
	/* inner shadow along the rail and a gold pinstripe */
	gfx_round_line(x, y, w, h, r, 3, RGB(0, 0, 0), 70);
	gfx_round_line(x + 8, y + 8, w - 16, h - 16, r > 8 ? r - 8 : r, 1, RGB(230, 196, 110), 70);
}

/* the casino carpet: a deep red with a gold-and-navy motif */
static u32 *carpet;
static void carpet_draw(void)
{
	if (!carpet) {
		carpet = malloc(SCR_W * SCR_H * 4);
		if (!carpet) { gfx_bg(); return; }
		for (int y = 0; y < SCR_H; y++)
			for (int x = 0; x < SCR_W; x++) {
				int tx = x % 48, ty = y % 48, cx = tx - 24, cy = ty - 24;
				int d = abs(cx) + abs(cy);
				u32 c = RGB(58, 10, 22);
				if (d == 18 || d == 19) c = RGB(150, 110, 40);               /* gold diamond */
				else if (d < 7) c = RGB(24, 22, 60);                         /* navy centre */
				else if (d < 9) c = RGB(150, 110, 40);
				else if ((tx == 0 || ty == 0)) c = RGB(40, 6, 16);
				int n = (int)(hash32(x * 31 + y * 977) & 7) - 4;
				int vy = abs(y - SCR_H / 2) / 8 + abs(x - SCR_W / 2) / 12;   /* darker to the edges */
				carpet[y * SCR_W + x] = shade(c, n - vy);
			}
	}
	memcpy(gfx_buf(), carpet, SCR_W * SCR_H * 4);
}

/* ================================================================== */
/* cards: gloss, and flipping over                                    */
/* ================================================================== */
static void card_gloss(int x, int y)
{
	gfx_tri(x + 3, y + 3, x + CW * 3 / 5, y + 3, x + 3, y + CH * 2 / 5, RGB(255, 255, 255), 34);
}
/* p: 0 = showing the back, 1 = showing the face. In between it turns over. */
static u32 flip_tmp[CW * CH];
static void card_flip_draw(int set, int x, int y, u8 c, float p)
{
	if (p >= 1) { card_draw(set, x, y, c, 0); card_gloss(x, y); return; }
	if (p <= 0) { card_draw(set, x, y, c, 1); card_gloss(x, y); return; }
	int face = p >= 0.5f;
	float k = face ? (p - 0.5f) * 2 : (0.5f - p) * 2;   /* 1 = full width */
	int w = (int)(CW * k); if (w < 2) w = 2;
	/* draw the side we see into a small buffer, then squeeze it */
	u32 *save = gfx_buf(); int sw = gfx_w(), sh = gfx_h();
	const u32 KEY = RGB(1, 2, 3);
	for (int i = 0; i < CW * CH; i++) flip_tmp[i] = KEY;
	gfx_target(flip_tmp, CW, CH);
	if (face) card_face(set, 0, 0, c); else card_back(set, 0, 0);
	gfx_target(save == canvas ? NULL : save, sw, sh);
	u32 *dst = gfx_buf();
	int x0 = x + (CW - w) / 2, lift = (int)((1 - k) * 6);
	gfx_round(x0 + 2, y + 4, w, CH, 6, RGB(0, 0, 0), 70);
	for (int yy = 0; yy < CH; yy++) {
		int sy = y + yy - lift; if (sy < 0 || sy >= sh) continue;
		for (int xx = 0; xx < w; xx++) {
			int sx = x0 + xx; if (sx < 0 || sx >= sw) continue;
			u32 v = flip_tmp[yy * CW + xx * CW / w];
			if (v != KEY) dst[sy * sw + sx] = v;
		}
	}
}
#define FLIP_MS 200
/* a dealt card: flies in from the shoe, then turns face up */
static void card_anim(const Card *k, int tx, int ty, int hidden)
{
	int x, y;
	if (!card_pos(k, tx, ty, &x, &y)) return;
	if (hidden) { card_draw(S.cardset, x, y, k->c, 1); card_gloss(x, y); return; }
	u64 now = plat_ms(), land = k->t + (u64)(speed_ms() * 3 / 4 + 60);
	float p = now < land ? 0 : (now - land) / (float)FLIP_MS;
	card_flip_draw(S.cardset, x, y, k->c, p);
}

/* ================================================================== */
/* chips: a stack for a bet                                           */
/* ================================================================== */
static void chip_stack(int cx, int cy, int amount, int r)
{
	static const int D[9] = { 5000, 1000, 500, 250, 100, 50, 25, 10, 5 };
	int n = 0, vals[8];
	for (int k = 0; k < 9 && n < 8; k++) while (amount >= D[k] && n < 8) { vals[n++] = D[k]; amount -= D[k]; }
	if (!n && amount > 0) vals[n++] = 5;
	for (int i = n - 1; i >= 0; i--) chip_draw(cx, cy - (n - 1 - i) * 3, r, vals[i], 0);
}

/* ================================================================== */
/* particles: coins, confetti, sparkles                               */
/* ================================================================== */
#define NP 150
typedef struct { float x, y, vx, vy, rot, vr; u32 col; u16 age, life; u8 kind, sz; } Part;
static Part parts[NP];
static void spawn(int kind, float x, float y, int n)
{
	static const u32 CONF[6] = { RGB(255, 80, 90), RGB(80, 200, 255), RGB(255, 214, 90), RGB(120, 240, 140), RGB(200, 120, 255), RGB(255, 255, 255) };
	for (int k = 0; k < n; k++) {
		for (int i = 0; i < NP; i++) {
			if (parts[i].life) continue;
			Part *p = &parts[i];
			float a = (rnd() % 628) / 100.0f, s = 1.5f + (rnd() % 100) / 25.0f;
			p->x = x; p->y = y; p->vx = cosf(a) * s; p->vy = sinf(a) * s - 4.0f;
			p->rot = (rnd() % 628) / 100.0f; p->vr = ((int)(rnd() % 40) - 20) / 100.0f;
			p->kind = kind; p->age = 0; p->life = 70 + rnd() % 50;
			p->sz = kind == 0 ? 6 + rnd() % 4 : kind == 1 ? 3 + rnd() % 3 : 2;
			p->col = kind == 1 ? CONF[rnd() % 6] : kind == 2 ? RGB(255, 250, 220) : RGB(250, 200, 60);
			break;
		}
	}
}
static void parts_draw(void)
{
	for (int i = 0; i < NP; i++) {
		Part *p = &parts[i];
		if (!p->life) continue;
		p->age++; if (p->age >= p->life) { p->life = 0; continue; }
		p->vy += p->kind == 1 ? 0.12f : 0.22f; p->vx *= p->kind == 1 ? 0.97f : 0.99f;
		p->x += p->vx; p->y += p->vy; p->rot += p->vr;
		if (p->y > 420) { p->life = 0; continue; }
		int a = p->life - p->age < 20 ? (p->life - p->age) * 12 : 255;
		int x = (int)p->x, y = (int)p->y;
		if (p->kind == 0) {         /* a spinning gold coin */
			int w = (int)(p->sz * fabsf(cosf(p->rot * 3))); if (w < 1) w = 1;
			gfx_round(x - w, y - p->sz, w * 2, p->sz * 2, w, RGB(170, 120, 20), a);
			if (w > 2) gfx_round(x - w + 1, y - p->sz + 1, w * 2 - 2, p->sz * 2 - 2, w - 1, RGB(255, 214, 90), a);
			if (w > 3) gfx_rect(x - w / 2, y - p->sz + 2, 1, p->sz, RGB(255, 250, 200), a);
		} else if (p->kind == 1) {  /* confetti: a little strip turning in the air */
			int w = (int)(p->sz * 2 * fabsf(cosf(p->rot * 2))) + 1;
			gfx_rect(x - w / 2, y - p->sz / 2, w, p->sz, p->col, a);
		} else {                    /* a sparkle */
			int s = 2 + (p->age / 4) % 3;
			gfx_rect(x - s, y, s * 2 + 1, 1, p->col, a); gfx_rect(x, y - s, 1, s * 2 + 1, p->col, a);
		}
	}
}

/* ================================================================== */
/* win banner with a count-up                                         */
/* ================================================================== */
static int wb_amt, wb_big; static u64 wb_at;
static void win_fx(int net, int stake)
{
	if (net <= 0) return;
	int big = stake > 0 && net >= stake * 5;
	music_sfx(big ? SFX_BIGWIN : SFX_WIN, 0);
	spawn(0, 320, 200, big ? 40 : 12);
	spawn(2, 320, 200, 10);
	if (big) { spawn(1, 160, 120, 30); spawn(1, 480, 120, 30); }
	wb_amt = net; wb_big = big; wb_at = plat_ms();
}
static void win_banner_draw(void)
{
	u64 now = plat_ms();
	if (!wb_at || now > wb_at + 2000) return;
	int t = (int)(now - wb_at);
	float p = t < 900 ? t / 900.0f : 1;
	int shown = (int)(wb_amt * (1 - (1 - p) * (1 - p)));
	int a = t > 1500 ? (2000 - t) * 255 / 500 : 255;
	/* the plate pops open, overshoots a little, settles */
	float s = t < 250 ? t / 250.0f * 1.12f : t < 400 ? 1.12f - (t - 250) / 150.0f * 0.12f : 1;
	int w = (int)((wb_big ? 300 : 210) * s), h = (int)((wb_big ? 76 : 50) * s), cy = 170;
	gfx_round(320 - w / 2 - 4, cy - h / 2 - 4, w + 8, h + 8, h / 2 + 4, RGB(255, 214, 90), a * 120 / 255);
	gfx_round_grad(320 - w / 2, cy - h / 2, w, h, h / 2, RGB(60, 40, 10), RGB(20, 12, 4), a * 230 / 255);
	gfx_round_line(320 - w / 2, cy - h / 2, w, h, h / 2, 2, RGB(255, 214, 90), a);
	if (s > 0.9f) {
		char tt[32];
		if (wb_big) { text_c(F_BODYB, 320, cy - 30, "BIG WIN", RGB(255, 240, 180), a); snprintf(tt, sizeof tt, "+%d", shown); text_c(F_TITLE, 320, cy - 8, tt, RGB(255, 214, 90), a); }
		else { snprintf(tt, sizeof tt, "+%d", shown); text_c(F_TITLE, 320, cy - 18, tt, RGB(255, 214, 90), a); }
	}
}

/* ================================================================== */
/* marquee bulbs around a box, chasing                                */
/* ================================================================== */
static void bulbs(int x, int y, int w, int h, int gap, int phase, u32 on, u32 off)
{
	int per = 2 * (w + h) / gap;
	for (int i = 0; i < per; i++) {
		int d = i * gap, bx, by;
		if (d < w) { bx = x + d; by = y; }
		else if (d < w + h) { bx = x + w; by = y + d - w; }
		else if (d < 2 * w + h) { bx = x + w - (d - w - h); by = y + h; }
		else { bx = x; by = y + h - (d - 2 * w - h); }
		int lit = ((i + phase) % 4) == 0;
		if (lit) gfx_circle(bx, by, 5, on, 70);
		gfx_circle(bx, by, 2, lit ? on : off, 255);
	}
}

/* a band of light sweeping across a box every few seconds */
static void shine(int x, int y, int w, int h, u64 period, u64 offset)
{
	u64 t = (plat_ms() + offset) % period;
	if (t > 700) return;
	int sx = x - 40 + (int)((w + 80) * t / 700);
	for (int k = 0; k < 18; k++) {
		int a = 40 - abs(k - 9) * 4; if (a <= 0) continue;
		int xx = sx + k;
		for (int yy = 0; yy < h; yy += 2) {
			int px = xx + yy / 3;
			if (px >= x + 6 && px < x + w - 6) gfx_rect(px, y + yy, 1, 2, RGB(255, 255, 255), a);
		}
	}
}
