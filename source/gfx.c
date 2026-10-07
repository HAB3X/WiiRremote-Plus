/*
   gfx.c - tiny software renderer (640x480 RGB canvas).
   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "gfx.h"

u32 canvas[SCR_W * SCR_H];

/* current draw target (the UI canvas, or a native-size picture buffer) */
static u32 *tgt = canvas;
static int TW = SCR_W, TH = SCR_H;
void gfx_target(u32 *buf, int w, int h) { tgt = buf ? buf : canvas; TW = buf ? w : SCR_W; TH = buf ? h : SCR_H; }
int gfx_w(void) { return TW; }
int gfx_h(void) { return TH; }
u32 *gfx_buf(void) { return tgt; }

static const Theme DARK = {
	RGB(12,16,28), RGB(24,31,50), RGB(30,39,62), RGB(44,56,88), RGB(58,72,110), RGB(236,241,250), RGB(146,160,190),
	RGB(10,14,26), RGB(10,14,26),
	RGB(56,68,104), RGB(38,47,74), RGB(64,82,128), RGB(46,60,96),
	RGB(20,80,56), RGB(110,40,50), RGB(70,56,24),
	RGB(0,0,0), RGB(40,50,80), RGB(28,36,60), RGB(4,6,12), RGB(0,0,0),
	RGB(236,241,250), RGB(12,16,28), RGB(44,56,88),
	110, 170, 40, 0,
};
static const Theme LIGHT = {
	RGB(222,226,233), RGB(247,248,250), RGB(244,246,249), RGB(255,255,255), RGB(208,214,224), RGB(44,48,56), RGB(122,130,144),
	RGB(234,237,242), RGB(255,255,255),
	RGB(255,255,255), RGB(236,239,244), RGB(240,250,255), RGB(222,240,252),
	RGB(222,245,230), RGB(253,228,226), RGB(255,243,222),
	RGB(236,239,244), RGB(255,255,255), RGB(246,247,250), RGB(40,46,58), RGB(60,70,90),
	RGB(44,48,56), RGB(255,255,255), RGB(255,255,255),
	255, 120, 22, 1,
};
Theme th = DARK;


static inline u32 blend(u32 d, u32 s, int a)
{
	if (a >= 255) return s;
	if (a <= 0) return d;
	u32 rb = ((((s & 0xFF00FF) * a) + ((d & 0xFF00FF) * (255 - a))) >> 8) & 0xFF00FF;
	u32 g  = ((((s & 0x00FF00) * a) + ((d & 0x00FF00) * (255 - a))) >> 8) & 0x00FF00;
	return rb | g;
}

static inline void px(int x, int y, u32 c, int a)
{
	if ((unsigned)x >= (unsigned)TW || (unsigned)y >= (unsigned)TH) return;
	u32 *p = &tgt[y * TW + x];
	*p = blend(*p, c, a);
}

static inline u32 lerp(u32 a, u32 b, int t) /* t 0..255 */
{
	return blend(a, b, t);
}

void gfx_clear(u32 c)
{
	for (int i = 0; i < TW * TH; i++) tgt[i] = c;
}

static u32 bg_cache[SCR_W * SCR_H];
static int bg_ready;

void theme_set(int light) { th = light ? LIGHT : DARK; bg_ready = 0; }

void gfx_bg(void)
{
	if (bg_ready) { memcpy(canvas, bg_cache, sizeof canvas); return; }
	for (int y = 0; y < SCR_H; y++) {
		u32 c = lerp(C_BG1, C_BG0, y * 255 / SCR_H);
		/* very faint horizontal lines, like the Wii Menu */
		if (th.stripes && (y & 3) == 0) c = blend(c, RGB(255, 255, 255), 90);
		u32 *row = &canvas[y * SCR_W];
		for (int x = 0; x < SCR_W; x++) row[x] = c;
	}
	if (!th.stripes)
		for (int y = 0; y < 220; y++)
			for (int x = 380; x < SCR_W; x++) {
				int dx = x - 600, dy = y + 40, d2 = dx * dx + dy * dy;
				if (d2 < 260 * 260) px(x, y, C_ACCENT2, (260 * 260 - d2) * 28 / (260 * 260));
			}
	memcpy(bg_cache, canvas, sizeof canvas);
	bg_ready = 1;
}

void gfx_rect(int x, int y, int w, int h, u32 c, int a)
{
	int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
	int x1 = x + w > TW ? TW : x + w, y1 = y + h > TH ? TH : y + h;
	for (int j = y0; j < y1; j++) {
		u32 *row = &tgt[j * TW];
		if (a >= 255) for (int i = x0; i < x1; i++) row[i] = c;
		else for (int i = x0; i < x1; i++) row[i] = blend(row[i], c, a);
	}
}

void gfx_vgrad(int x, int y, int w, int h, u32 top, u32 bot, int a)
{
	for (int j = 0; j < h; j++) gfx_rect(x, y + j, w, 1, lerp(top, bot, h > 1 ? j * 255 / (h - 1) : 0), a);
}

/* Corner coverage lookup: cov_lut[r] holds r*r bytes for the top-left
   corner of a circle of radius r (4x4 supersampled), built on first use. */
#define MAX_R 320
static u8 *cov_lut[MAX_R + 1];

static const u8 *corner_lut(int r)
{
	if (r > MAX_R) r = MAX_R;
	if (cov_lut[r]) return cov_lut[r];
	u8 *t = malloc(r * r + 1);
	for (int j = 0; j < r; j++)
		for (int i = 0; i < r; i++) {
			int in = 0;
			for (int sy = 0; sy < 4; sy++)
				for (int sx = 0; sx < 4; sx++) {
					int fx = (i * 8 + sx * 2 + 1) - r * 8;
					int fy = (j * 8 + sy * 2 + 1) - r * 8;
					if (fx * fx + fy * fy <= r * r * 64) in++;
				}
			t[j * r + i] = in * 255 / 16;
		}
	cov_lut[r] = t;
	return t;
}

/* coverage (0..255) of pixel (i,j) inside a w x h rounded rect */
static inline int round_cov(int i, int j, int w, int h, int r)
{
	if (r <= 0) return 255;
	if (r > MAX_R) r = MAX_R;
	int ci, cj;
	if (i < r) ci = i; else if (i >= w - r) ci = w - 1 - i; else return 255;
	if (j < r) cj = j; else if (j >= h - r) cj = h - 1 - j; else return 255;
	return corner_lut(r)[cj * r + ci];
}

void gfx_round_grad(int x, int y, int w, int h, int r, u32 top, u32 bot, int a)
{
	if (r * 2 > h) r = h / 2;
	if (r * 2 > w) r = w / 2;
	for (int j = 0; j < h; j++) {
		u32 c = (top == bot) ? top : lerp(top, bot, h > 1 ? j * 255 / (h - 1) : 0);
		int corner = (j < r || j >= h - r);
		if (!corner) { gfx_rect(x, y + j, w, 1, c, a); continue; }
		/* only the r pixels at each end are curved; the middle of the row is solid */
		for (int i = 0; i < r; i++) {
			int cov = round_cov(i, j, w, h, r);
			if (cov) px(x + i, y + j, c, cov * a / 255);
			cov = round_cov(w - 1 - i, j, w, h, r);
			if (cov) px(x + w - 1 - i, y + j, c, cov * a / 255);
		}
		if (w > 2 * r) gfx_rect(x + r, y + j, w - 2 * r, 1, c, a);
	}
}

void gfx_round(int x, int y, int w, int h, int r, u32 c, int a)
{
	gfx_round_grad(x, y, w, h, r, c, c, a);
}

void gfx_round_line(int x, int y, int w, int h, int r, int t, u32 c, int a)
{
	/* outer minus inner coverage */
	if (r * 2 > h) r = h / 2;
	int ri = r - t; if (ri < 0) ri = 0;
	for (int j = 0; j < h; j++) {
		int mid = (j >= r && j < h - r && j >= t && j < h - t);
		if (mid) {
			gfx_rect(x, y + j, t, 1, c, a);
			gfx_rect(x + w - t, y + j, t, 1, c, a);
			continue;
		}
		/* curved ends: work them out; the straight middle is either the top/bottom edge or empty */
		int ends = r > t ? r : t; if (ends * 2 > w) ends = (w + 1) / 2;
		for (int k = 0; k < 2 * ends; k++) {
			int i = k < ends ? k : w - 1 - (k - ends);
			if (k >= ends && i < ends) break;
			int co = round_cov(i, j, w, h, r);
			int ci = 0;
			int ii = i - t, jj = j - t, wi = w - 2 * t, hi = h - 2 * t;
			if (ii >= 0 && jj >= 0 && ii < wi && jj < hi) ci = round_cov(ii, jj, wi, hi, ri);
			int cov = co - ci;
			if (cov > 0) px(x + i, y + j, c, cov * a / 255);
		}
		if (w > 2 * ends && (j < t || j >= h - t)) gfx_rect(x + ends, y + j, w - 2 * ends, 1, c, a);
	}
}

void gfx_circle(int cx, int cy, int r, u32 c, int a)
{
	gfx_round(cx - r, cy - r, r * 2, r * 2, r, c, a);
}

void gfx_ring(int cx, int cy, int r, int t, u32 c, int a)
{
	gfx_round_line(cx - r, cy - r, r * 2, r * 2, r, t, c, a);
}

void gfx_shadow(int x, int y, int w, int h, int r)
{
	gfx_round(x - 2, y + 2, w + 4, h + 4, r + 2, th.shadow, th.shadow_a * 2 / 3);
	gfx_round(x, y + 3, w, h, r, th.shadow, th.shadow_a);
}

/* ---------------- text ---------------- */

int text_w(const Font *f, const char *s)
{
	int w = 0;
	for (; *s; s++) {
		int c = (unsigned char)*s;
		if (c < 32 || c > 126) c = '?';
		w += f->g[c - 32].adv;
	}
	return w;
}

void text(const Font *f, int x, int y, const char *s, u32 c, int a)
{
	for (; *s; s++) {
		int ch = (unsigned char)*s;
		if (ch < 32 || ch > 126) ch = '?';
		const Glyph *g = &f->g[ch - 32];
		const unsigned char *p = f->px + g->off;
		for (int j = 0; j < g->h; j++)
			for (int i = 0; i < g->w; i++) {
				int v = p[j * g->w + i];
				if (v) px(x + g->x0 + i, y + g->y0 + j, c, v * a / 255);
			}
		x += g->adv;
	}
}

void text_c(const Font *f, int cx, int y, const char *s, u32 c, int a)
{
	text(f, cx - text_w(f, s) / 2, y, s, c, a);
}

void text_r(const Font *f, int rx, int y, const char *s, u32 c, int a)
{
	text(f, rx - text_w(f, s), y, s, c, a);
}

#ifdef HOST
/* layout audit (tests only): every string that had to be cut short */
int gfx_audit; char gfx_audit_where[48];
static char au_s[4000][120]; static int au_n;
void gfx_audit_note(const char *kind, const char *s)
{
	if (!gfx_audit) return;
	char k[120]; snprintf(k, sizeof k, "%s | %s | %.70s", gfx_audit_where, kind, s);
	for (int i = 0; i < au_n; i++) if (!strcmp(au_s[i], k)) return;
	if (au_n < 4000) snprintf(au_s[au_n++], 120, "%s", k);
}
void gfx_audit_dump(void) { for (int i = 0; i < au_n; i++) printf("  %s\n", au_s[i]); printf("%d layout problems\n", au_n); }
#endif
void text_fit(const Font *f, int x, int y, int maxw, const char *s, u32 c, int a)
{
	if (text_w(f, s) <= maxw) { text(f, x, y, s, c, a); return; }
#ifdef HOST
	gfx_audit_note("cut", s);
#endif
	char buf[128];
	int n = strlen(s); if (n > 120) n = 120;
	while (n > 0) {
		memcpy(buf, s, n); strcpy(buf + n, "...");
		if (text_w(f, buf) <= maxw) break;
		n--;
	}
	text(f, x, y, buf, c, a);
}

int badge(int x, int y, const char *label, u32 c)
{
	int w = text_w(F_SMALL, label) + 14;
	if (w < 24) w = 24;
	(void)c;
	gfx_round(x, y, w, 24, 12, th.badge_bg, 255);
	text_c(F_SMALL, x + w / 2, y + 1, label, th.badge_fg, 255);
	return w;
}

int hint(int x, int y, const char *key, const char *label)
{
	int w = badge(x, y, key, C_TEXT);
	text(F_SMALL, x + w + 6, y + 1, label, C_DIM, 255);
	return w + 6 + text_w(F_SMALL, label) + 18;
}

void gfx_cursor(int x, int y)
{
	gfx_circle(x + 2, y + 3, 14, RGB(0, 0, 0), 60);
	gfx_circle(x, y, 13, C_ACCENT, 110);
	gfx_ring(x, y, 14, 2, C_TEXT, 255);
	gfx_ring(x, y, 12, 3, C_WHITE, 255);
	gfx_circle(x, y, 4, C_ACCENT, 255);
}

/* filled triangle (edge-function, 2x2 supersampled) */
void gfx_tri(int x0, int y0, int x1, int y1, int x2, int y2, u32 c, int a)
{
	int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
	int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
	int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
	int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
	long area = (long)(x1 - x0) * (y2 - y0) - (long)(y1 - y0) * (x2 - x0);
	if (!area) return;
	int s = area > 0 ? 1 : -1;
	for (int y = miny; y <= maxy; y++)
		for (int x = minx; x <= maxx; x++) {
			int cov = 0;
			for (int k = 0; k < 4; k++) {
				/* sample points in 1/4 pixel units */
				long px4 = x * 4 + (k & 1) * 2 + 1, py4 = y * 4 + (k >> 1) * 2 + 1;
				long w0 = ((long)(x2 - x1) * (py4 - y1 * 4) - (long)(y2 - y1) * (px4 - x1 * 4)) * s;
				long w1 = ((long)(x0 - x2) * (py4 - y2 * 4) - (long)(y0 - y2) * (px4 - x2 * 4)) * s;
				long w2 = ((long)(x1 - x0) * (py4 - y0 * 4) - (long)(y1 - y0) * (px4 - x0 * 4)) * s;
				if (w0 >= 0 && w1 >= 0 && w2 >= 0) cov++;
			}
			if (cov) px(x, y, c, a * cov / 4);
		}
}

/* thick line */
void gfx_line(int x0, int y0, int x1, int y1, int t, u32 c, int a)
{
	int dx = x1 - x0, dy = y1 - y0;
	int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
	if (!steps) steps = 1;
	for (int i = 0; i <= steps; i++) {
		int x = x0 + dx * i / steps, y = y0 + dy * i / steps;
		gfx_round(x - t / 2, y - t / 2, t, t, t / 2, c, a);
	}
}
