/*
   icons.c - simple vector icons drawn with the software renderer.
   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "icons.h"
#include <ctype.h>

static void arrow(int cx, int cy, int s, int dir, u32 c)
{
	/* dir 0 up, 1 down, 2 left, 3 right */
	int h = s / 2;
	switch (dir) {
		case 0: gfx_tri(cx, cy - h, cx - h, cy + h / 2, cx + h, cy + h / 2, c, 255); break;
		case 1: gfx_tri(cx, cy + h, cx + h, cy - h / 2, cx - h, cy - h / 2, c, 255); break;
		case 2: gfx_tri(cx - h, cy, cx + h / 2, cy + h, cx + h / 2, cy - h, c, 255); break;
		case 3: gfx_tri(cx + h, cy, cx - h / 2, cy - h, cx - h / 2, cy + h, c, 255); break;
	}
}

static void speaker(int cx, int cy, int s, u32 c)
{
	int u = s / 8;
	gfx_rect(cx - 4 * u, cy - 2 * u, 3 * u, 4 * u, c, 255);
	gfx_tri(cx - 2 * u, cy - 2 * u, cx + u, cy - 4 * u - u / 2, cx + u, cy + 4 * u + u / 2, c, 255);
	gfx_tri(cx - 2 * u, cy - 2 * u, cx + u, cy + 4 * u + u / 2, cx - 2 * u, cy + 2 * u, c, 255);
}

void icon(int id, int cx, int cy, int s, u32 c)
{
	int u = s / 8; if (u < 1) u = 1;
	int t = s / 9; if (t < 2) t = 2;
	switch (id) {
	case IC_POWER:
		{
			/* ring with a gap at the top, anti-aliased by 4x supersampling */
			int r = s * 3 / 8, ccy = cy + u / 2;
			for (int y = -r - 1; y <= r + 1; y++)
				for (int x = -r - 1; x <= r + 1; x++) {
					int cov = 0;
					for (int k = 0; k < 4; k++) {
						int fx = x * 4 + (k & 1) * 2 - 1, fy = y * 4 + (k >> 1) * 2 - 1;
						int d2 = fx * fx + fy * fy;
						if (d2 > r * r * 16 || d2 < (r - t) * (r - t) * 16) continue;
						if (fy < 0 && abs(fx) * 10 < -fy * 7) continue;   /* top gap */
						cov++;
					}
					if (cov) gfx_rect(cx + x, ccy + y, 1, 1, c, 255 * cov / 4);
				}
			gfx_round(cx - t / 2 - 1, cy - s / 2, t + 2, s / 2, t, c, 255);
		}
		break;
	case IC_PLUS:
		gfx_round(cx - s / 2 + u, cy - t, s - 2 * u, t * 2, t, c, 255);
		gfx_round(cx - t, cy - s / 2 + u, t * 2, s - 2 * u, t, c, 255);
		break;
	case IC_MINUS:
		gfx_round(cx - s / 2 + u, cy - t, s - 2 * u, t * 2, t, c, 255);
		break;
	case IC_SEARCH:
		gfx_ring(cx - u, cy - u, s * 5 / 16, t, c, 255);
		gfx_line(cx + u, cy + u, cx + s / 2 - u, cy + s / 2 - u, t + 1, c, 255);
		break;
	case IC_GAMEPAD:
		gfx_round(cx - s / 2, cy - s / 4, s, s / 2 + u, s / 4, c, 255);
		gfx_rect(cx - s / 3, cy - u / 2 - 1, s / 5, t, C_BG0, 255);
		gfx_rect(cx - s / 3 + s / 10 - t / 2, cy - s / 10 - 1, t, s / 5, C_BG0, 255);
		gfx_circle(cx + s / 5, cy - u / 2, t, C_BG0, 255);
		gfx_circle(cx + s / 3, cy + u, t, C_BG0, 255);
		break;
	case IC_PALETTE:
		gfx_circle(cx, cy, s / 2 - u / 2, c, 255);
		gfx_circle(cx - s / 5, cy - s / 6, u + 1, RGB(255, 90, 90), 255);
		gfx_circle(cx + s / 8, cy - s / 4, u + 1, RGB(90, 220, 110), 255);
		gfx_circle(cx + s / 4, cy + u / 2, u + 1, RGB(80, 150, 255), 255);
		gfx_circle(cx - s / 8, cy + s / 5, u + 1, RGB(255, 210, 60), 255);
		break;
	case IC_GEAR:
		for (int k = 0; k < 8; k++) {
			static const int dx[8] = { 0, 7, 10, 7, 0, -7, -10, -7 }, dy[8] = { -10, -7, 0, 7, 10, 7, 0, -7 };
			gfx_circle(cx + dx[k] * s / 26, cy + dy[k] * s / 26, s / 9 + 1, c, 255);
		}
		gfx_circle(cx, cy, s * 5 / 16, c, 255);
		gfx_circle(cx, cy, s / 8 + 1, C_BG0, 255);
		break;
	case IC_RADAR:
		gfx_ring(cx, cy, s / 2 - 1, t, c, 255);
		gfx_ring(cx, cy, s / 4, t, c, 200);
		gfx_line(cx, cy, cx + s / 3, cy - s / 3, t, c, 255);
		gfx_circle(cx, cy, t, c, 255);
		break;
	case IC_TRASH:
		gfx_round(cx - s / 3, cy - s / 5, s * 2 / 3, s * 3 / 5, u, c, 255);
		gfx_round(cx - s * 2 / 5, cy - s / 3, s * 4 / 5, t, t / 2, c, 255);
		gfx_round(cx - s / 10, cy - s / 2 + u / 2, s / 5, t + 1, t / 2, c, 255);
		break;
	case IC_PENCIL:
		gfx_line(cx - s / 3, cy + s / 3, cx + s / 4, cy - s / 4, t * 2, c, 255);
		gfx_tri(cx - s / 2 + u / 2, cy + s / 2 - u / 2, cx - s / 3 - t, cy + s / 5, cx - s / 5, cy + s / 3 + t, c, 255);
		break;
	case IC_BACK:
		arrow(cx - s / 6, cy, s * 5 / 8, 2, c);
		gfx_round(cx - s / 8, cy - t, s / 2, t * 2, t, c, 255);
		break;
	case IC_HELP:
		gfx_ring(cx, cy, s / 2 - 1, t, c, 255);
		text_c(s >= 30 ? F_BODYB : F_SMALL, cx, cy - (s >= 30 ? F_BODYB : F_SMALL)->height / 2, "?", c, 255);
		break;
	case IC_PLAY:
		arrow(cx + u / 2, cy, s * 3 / 4, 3, c);
		break;
	case IC_CHECK:
		gfx_line(cx - s / 3, cy, cx - s / 10, cy + s / 4, t + 1, c, 255);
		gfx_line(cx - s / 10, cy + s / 4, cx + s / 3, cy - s / 4, t + 1, c, 255);
		break;
	case IC_CLOSE:
		gfx_line(cx - s / 4, cy - s / 4, cx + s / 4, cy + s / 4, t + 1, c, 255);
		gfx_line(cx + s / 4, cy - s / 4, cx - s / 4, cy + s / 4, t + 1, c, 255);
		break;
	case IC_REMOTE:
		gfx_round(cx - s / 5, cy - s / 2, s * 2 / 5, s, s / 7, c, 255);
		gfx_circle(cx, cy - s / 4, u + 1, C_BG0, 255);
		for (int k = 0; k < 3; k++) gfx_rect(cx - s / 10, cy + k * u * 2 - u / 2, s / 5, u, C_BG0, 255);
		break;
	case IC_MORE:
		for (int k = -1; k <= 1; k++) gfx_circle(cx + k * s / 3, cy, u + 1, c, 255);
		break;
	case IC_UP: arrow(cx, cy, s * 3 / 4, 0, c); break;
	case IC_DOWN: arrow(cx, cy, s * 3 / 4, 1, c); break;
	case IC_LEFT: arrow(cx, cy, s * 3 / 4, 2, c); break;
	case IC_RIGHT: arrow(cx, cy, s * 3 / 4, 3, c); break;
	case IC_VOLUP:
		speaker(cx - u, cy, s, c);
		gfx_round(cx + s / 4, cy - t / 2, s / 4, t, t / 2, c, 255);
		gfx_round(cx + s / 4 + s / 8 - t / 2, cy - s / 8, t, s / 4, t / 2, c, 255);
		break;
	case IC_VOLDN:
		speaker(cx - u, cy, s, c);
		gfx_round(cx + s / 4, cy - t / 2, s / 4, t, t / 2, c, 255);
		break;
	case IC_MUTE:
		speaker(cx - u, cy, s, c);
		gfx_line(cx + s / 5, cy - s / 7, cx + s / 2 - u, cy + s / 7, t, c, 255);
		gfx_line(cx + s / 2 - u, cy - s / 7, cx + s / 5, cy + s / 7, t, c, 255);
		break;
	case IC_TV:
		gfx_round(cx - s / 2, cy - s / 3, s, s * 5 / 8, u, c, 255);
		gfx_round(cx - s / 2 + t, cy - s / 3 + t, s - 2 * t, s * 5 / 8 - 2 * t, u / 2, C_BG0, 255);
		gfx_rect(cx - s / 6, cy + s / 3 + 1, s / 3, t, c, 255);
		break;
	case IC_MENU:
		for (int k = -1; k <= 1; k++) gfx_round(cx - s * 3 / 8, cy + k * s / 4 - t / 2, s * 3 / 4, t, t / 2, c, 255);
		break;
	case IC_OK:
		gfx_ring(cx, cy, s / 2 - 1, t, c, 255);
		gfx_circle(cx, cy, s / 5, c, 255);
		break;
	case IC_SEQ:
		for (int k = 0; k < 3; k++) gfx_round(cx - s / 2 + k * s / 3, cy - s / 6, s / 4, s / 3, u, c, 255 - k * 50);
		break;
	case IC_STAR:
		gfx_tri(cx, cy - s / 2, cx - s / 7, cy - s / 10, cx + s / 7, cy - s / 10, c, 255);
		gfx_tri(cx - s / 2, cy - s / 10, cx + s / 2, cy - s / 10, cx, cy + s / 6, c, 255);
		gfx_tri(cx, cy + s / 8, cx - s / 3, cy + s / 2, cx - s / 7, cy, c, 255);
		gfx_tri(cx, cy + s / 8, cx + s / 3, cy + s / 2, cx + s / 7, cy, c, 255);
		break;
	case IC_CH_UP:
		arrow(cx, cy - s / 8, s * 5 / 8, 0, c);
		gfx_round(cx - s / 4, cy + s / 4, s / 2, t, t / 2, c, 255);
		break;
	case IC_CH_DN:
		arrow(cx, cy + s / 8, s * 5 / 8, 1, c);
		gfx_round(cx - s / 4, cy - s / 4 - t, s / 2, t, t / 2, c, 255);
		break;
	case IC_INPUT:
		gfx_round(cx - s / 2, cy - s / 3, s * 3 / 4, s * 5 / 8, u, c, 255);
		gfx_round(cx - s / 2 + t, cy - s / 3 + t, s * 3 / 4 - 2 * t, s * 5 / 8 - 2 * t, u / 2, C_BG0, 255);
		arrow(cx + s / 4, cy, s / 2, 3, c);
		gfx_round(cx - s / 8, cy - t / 2, s / 3, t, t / 2, c, 255);
		break;
	case IC_WRENCH:
		gfx_line(cx - s / 3, cy + s / 3, cx + s / 8, cy - s / 8, t * 2, c, 255);
		gfx_ring(cx + s / 5, cy - s / 5, s / 5, t + 1, c, 255);
		break;
	case IC_MOON: {
		int r1 = s * 3 / 8, r2 = s * 5 / 16, ox = s / 5, oy = -s / 6;
		for (int y = -r1 - 1; y <= r1 + 1; y++)
			for (int x = -r1 - 1; x <= r1 + 1; x++) {
				int cov = 0;
				for (int k = 0; k < 4; k++) {
					int fx = x * 4 + (k & 1) * 2 - 1, fy = y * 4 + (k >> 1) * 2 - 1;
					int in1 = fx * fx + fy * fy <= r1 * r1 * 16;
					int dx = fx - ox * 4, dy = fy - oy * 4;
					int in2 = dx * dx + dy * dy <= r2 * r2 * 16;
					if (in1 && !in2) cov++;
				}
				if (cov) gfx_rect(cx + x, cy + y, 1, 1, c, 255 * cov / 4);
			}
		break;
	}
	case IC_NOTE:
		gfx_round(cx - s * 3 / 8, cy - s / 2, s * 3 / 4, s, u, c, 255);
		for (int k = 0; k < 3; k++) gfx_rect(cx - s / 4, cy - s / 4 + k * s / 5, s / 2, t, C_BG0, 255);
		break;
	case IC_SAVE:
		gfx_round(cx - s * 3 / 8, cy - s * 3 / 8, s * 3 / 4, s * 3 / 4, u, c, 255);
		gfx_rect(cx - s / 5, cy - s * 3 / 8, s * 2 / 5, s / 4, C_BG0, 255);
		gfx_round(cx - s / 4, cy + s / 16, s / 2, s / 4, u / 2, C_BG0, 255);
		break;
	case IC_SWAP:
		arrow(cx + s / 4, cy - s / 5, s / 2, 3, c);
		gfx_round(cx - s / 3, cy - s / 5 - t / 2, s / 2, t, t / 2, c, 255);
		arrow(cx - s / 4, cy + s / 5, s / 2, 2, c);
		gfx_round(cx - s / 6, cy + s / 5 - t / 2, s / 2, t, t / 2, c, 255);
		break;
	case IC_PAUSE:
		gfx_round(cx - s / 4, cy - s / 3, s / 6, s * 2 / 3, u / 2, c, 255);
		gfx_round(cx + s / 12, cy - s / 3, s / 6, s * 2 / 3, u / 2, c, 255);
		break;
	}
}

static int starts(const char *s, const char *p)
{
	while (*p) { if (toupper((unsigned char)*s) != *p) return 0; s++; p++; }
	return 1;
}

int icon_for_name(const char *n)
{
	if (starts(n, "POWER") || starts(n, "PWR")) return IC_POWER;
	if (starts(n, "VOL+") || starts(n, "VOL UP")) return IC_VOLUP;
	if (starts(n, "VOL-") || starts(n, "VOL DOWN")) return IC_VOLDN;
	if (starts(n, "MUTE")) return IC_MUTE;
	if (starts(n, "CH+") || starts(n, "CH UP") || !strcmp(n, "P+") || !strcmp(n, "PR+")) return IC_CH_UP;
	if (starts(n, "CH-") || starts(n, "CH DOWN") || !strcmp(n, "P-") || !strcmp(n, "PR-")) return IC_CH_DN;
	if (starts(n, "UP")) return IC_UP;
	if (starts(n, "DOWN")) return IC_DOWN;
	if (starts(n, "LEFT")) return IC_LEFT;
	if (starts(n, "RIGHT")) return IC_RIGHT;
	if (starts(n, "MENU")) return IC_MENU;
	if (starts(n, "OK") || starts(n, "ENTER") || starts(n, "SELECT")) return IC_OK;
	if (starts(n, "EXIT") || starts(n, "BACK") || starts(n, "RETURN")) return IC_BACK;
	if (starts(n, "INPUT") || starts(n, "TV/AV") || starts(n, "AV") || starts(n, "SOURCE") || starts(n, "HDMI") || starts(n, "COMP") || starts(n, "SCART") || starts(n, "S-VIDEO")) return IC_INPUT;
	if (!strcmp(n, "TV")) return IC_TV;
	if (starts(n, "FACTORY") || starts(n, "SERVICE")) return IC_WRENCH;
	if (starts(n, "PICTURE")) return IC_PALETTE;
	return -1;
}
