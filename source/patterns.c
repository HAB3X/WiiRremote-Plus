/*
   patterns.c - test pictures for TV calibration, in the spirit of the
   240p Test Suite by Artemio Urbina, redrawn from scratch for the Wii's
   own output (480i / 480p / 576i, component or AV), plus your own pictures
   loaded from the SD card.
   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "patterns.h"
#include "gfx.h"
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <strings.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#include "stb_image.h"
void health_add(const char *what);   /* extras.c */

static int W, H;   /* size of the current target */

static void rect(int x, int y, int w, int h, u32 c) { gfx_rect(x, y, w, h, c, 255); }
static u32 grey(int v) { if (v < 0) v = 0; if (v > 255) v = 255; return RGB(v, v, v); }
static int sx(int x320) { return x320 * W / 320; }   /* 240p-style layout helpers */
static int sy(int y240) { return y240 * H / 240; }

/* ------------------------------------------------------------------ */
/* options for patterns that have them                                 */
/* ------------------------------------------------------------------ */
static int o_bars100;               /* EBU / SMPTE: 75% or 100% */
static int o_bleed = 1;             /* colour bleed checkerboard on/off */
static int o_pluge;                 /* 0 = normal, 1 = high contrast */
static int o_field_col, o_field_lvl = 255;
static int o_ire = 100;
static int o_mono = 100;
static int o_grid;                  /* 0 = 32px, 1 = 16px */
static int o_sharp;
static int o_ov[4] = { 16, 16, 16, 16 }, o_ov_side;   /* top bottom left right */
static int o_conv;
static int o_stripes, o_stripe_w = 1, o_stripe_phase;
static int o_fill;                  /* your pictures: 0 = pixel perfect, 1 = fill */
static u64 warm_start;              /* warm-up drift check: when the timer started */
static int o_ramp_dir;
static int o_bloom_hz = 1, o_bloom_box;
static const int BLOOM_HZ[3] = { 1, 2, 5 };
static int o_scroll_spd = 2, o_scroll_vert, o_scroll_pause, o_gs_dir, o_gs_spd = 1, o_spr_mode, o_diag_spin = 1, o_clip_bg, o_gamma_cmp;
static int scroll_pos, gs_x, gs_y, diag_ang;
int pat_animated(int i) { return i == PAT_SCROLL || i == PAT_GRIDSCROLL || i == PAT_SPRITE || i == PAT_DIAG || i == PAT_BLOOM || i == PAT_AVSYNC || i == PAT_LAG || i == PAT_WASHER || i == PAT_SWEEP || i == PAT_SPEAKERS || i == PAT_EDGE || i == PAT_WARMUP; }
int pat_grabs_input(int i) { return i == PAT_LAG || i == PAT_SWEEP || i == PAT_SPEAKERS || i == PAT_WASHER || i == PAT_BLOOM || i == PAT_EDGE; }
/* point-to-measure overscan: the Wii Remote pointer, in picture pixels */
static int ptr_v, ptr_x, ptr_y, edge_step, edge_v[4];
void pat_pointer(int valid, int x, int y) { ptr_v = valid; ptr_x = x; ptr_y = y; }
static int o_chan, o_wash_mode, o_spk;
void pat_set_chan(int c) { o_chan = c % 3; }
void pat_set_field(int col, int lvl) { o_field_col = col % 5; o_field_lvl = lvl < 0 ? 0 : lvl > 255 ? 255 : lvl; }
static int flash_ok;                 /* you said OK to flashing pictures this session */
static int o_sweep_auto = 1, o_sweep_on = 1;
static float o_sweep_hz = 20;
static u32 frame_no;                 /* counts drawn frames of the moving tests */
static int o_sync_snd = 1, o_lag_snd = 1;
#define LAG_TRIES 10
static int lag_res[LAG_TRIES], lag_n;
static int hz(void) { return (H == 574 || H == 264 || H == 288 || H == 576) ? 50 : 60; }

static const char *NAMES[PAT_BUILTIN] = {
	"Picture scene", "Colour chart", "Red Green Blue White bars", "EBU colour bars", "SMPTE colour bars",
	"Referenced bars", "Bars with grey scale", "Colour bleed", "PLUGE", "Grey steps", "Grey ramp",
	"White & RGB screens", "IRE level", "Monoscope", "Grid", "Sharpness", "Overscan",
	"Convergence", "Stripes & checkerboard", "Skin tones", "CRT bloom flasher", "Audio sync", "Lag test",
	"Red only", "Green only", "Blue only", "50% grey", "Colour channel check", "Frame edge", "Screen washer", "Rattle tester (tone sweep)", "Left / right speakers",
	"Scroll test", "Grid scroll", "Flicker & drop shadow", "Diagonal lines", "White clipping", "Gamma check", "Linearity", "Warm-up drift check",
};

static const char *TIPS[PAT_BUILTIN] = {
	"Sky, grass, sand and skin should all look natural.",
	"Every patch different. Bottom greys must have no tint.",
	"Each ramp should rise smoothly from 0 to F. No colour in the white one.",
	"Yellow must not look green. Cyan must not look green.",
	"Standard SMPTE bars. Check colour and tint.",
	"Each bar next to its 100% reference: brightness should match.",
	"Colour bars above, neutral grey steps below.",
	"Colours should not smear into the black and white lines.",
	"Black level: the dimmest bars should only just show.",
	"All steps neutral grey - no green or pink.",
	"Smooth ramp - look for coloured bands creeping in.",
	"Full screen colour. Adjust gains until white is white.",
	"A box at a set IRE level on black.",
	"Circles should be round, lines straight and sharp.",
	"Lines should be straight and evenly spaced to the edges.",
	"Fine lines should be crisp but with no bright halos.",
	"Move each edge until it just shows = how much the TV cuts off.",
	"Red, green and blue lines should sit on top of each other.",
	"Fine lines and dots. Good for sharpness and composite shimmer.",
	"Skin tones from light to dark. No green or grey cast.",
	"FLASHING! Tests the power supply: the frame must not grow.",
	"Beep should play as the ball hits the line. Late = sound delay.",
	"Press A each time the box hits the line (go with the rhythm). 10 tries = your lag.",
	"Pure red. Any other colour mixed in = a cable or TV fault.",
	"Pure green. Should be even everywhere, no patches.",
	"Pure blue. Great for spotting purity (colour blotches).",
	"Middle grey. Green or pink tint here = check the cable / colour settings.",
	"One colour at a time (A changes it): each should come through clean.",
	"White line = picture edge. Stretch until it touches the glass.",
	"FLASHING! Clears image burn and shows purity. Look for coloured blotches.",
	"A rising tone. When the TV case buzzes, pause: that's the rattle.",
	"A beep on the left, then the right, each from its own side.",
	"Should glide smoothly. Stutter or smear = TV or scaler blur.",
	"Grid should move smoothly and stay sharp. Blur = motion processing.",
	"Shadow shows every other frame. 240p: see-through. 480i: flicker.",
	"Lines should stay smooth. Stair steps = a poor deinterlacer.",
	"Every block should show. Merging into white = contrast too high.",
	"Step back. The box that blends in best shows your TV's gamma.",
	"Circles round, dots evenly spaced. Squashed edges = fix linearity.",
	"Run it from switch-on. Colour, size and focus settle in 20-30 min.",
};

static const char *CONTROLS[PAT_BUILTIN] = {
	"", "", "A: flip left/right", "A: 75% / 100%", "A: 75% / 100%", "", "",
	"A: checkerboard on/off", "A: normal / high contrast", "", "A: flip direction",
	"A: colour   <>: brightness", "<>: IRE level", "<>: line brightness", "A: 32 px / 16 px",
	"A: change chart", "Up/Down: pick edge   <>: move it", "A: change pattern",
	"A: type   <>: line width   B: shift", "",
	"<>: speed   A: full screen / box",
	"A: sound on / off",
	"A: press when the box hits the line   B: start again",
	"", "", "", "", "A: red / green / blue", "Point at each edge you can see + A   B: again",
	"A: strobe / noise / bars / all   B: stop",
	"A: auto / by hand   <>: change   B: sound on/off",
	"A: left / right / both",
	"<>: speed   A: sideways / up-down   B: pause",
	"<>: speed   A: direction",
	"A: shadow / striped / both",
	"A: spin on / off",
	"A: white / black background",
	"A: lines / checkerboard",
	"",
	"A: restart the timer",
};

/* ------------------------------------------------------------------ */
/* your pictures                                                       */
/* ------------------------------------------------------------------ */
#define MAX_USER 48
static char user_path[MAX_USER][128];
static char user_name[MAX_USER][40];
static int nuser;
static char user_folder[96];

static int has_ext(const char *n)
{
	const char *d = strrchr(n, '.');
	if (!d) return 0;
	char e[8]; int i = 0;
	for (d++; *d && i < 7; d++) e[i++] = tolower((unsigned char)*d);
	e[i] = 0;
	return !strcmp(e, "png") || !strcmp(e, "jpg") || !strcmp(e, "jpeg") || !strcmp(e, "bmp") || !strcmp(e, "gif");
}

void pat_scan_user(void)
{
	nuser = 0;
#ifdef HOST
	const char *dirs[] = { "data/pictures" };
#else
	const char *dirs[] = { "sd:/apps/WiiRremotePlus/pictures", "sd:/wiirremote/pictures" };
#endif
	strcpy(user_folder, dirs[0]);
	for (size_t k = 0; k < sizeof dirs / sizeof dirs[0]; k++) {
		DIR *d = opendir(dirs[k]);
		if (!d) continue;
		struct dirent *e;
		while ((e = readdir(d)) && nuser < MAX_USER) {
			if (e->d_name[0] == '.' || !has_ext(e->d_name)) continue;
			snprintf(user_path[nuser], sizeof user_path[0], "%s/%s", dirs[k], e->d_name);
			snprintf(user_name[nuser], sizeof user_name[0], "%s", e->d_name);
			char *dot = strrchr(user_name[nuser], '.'); if (dot) *dot = 0;
			nuser++;
		}
		closedir(d);
	}
	/* alphabetical */
	for (int i = 0; i < nuser; i++)
		for (int j = i + 1; j < nuser; j++)
			if (strcasecmp(user_name[i], user_name[j]) > 0) {
				char t[128];
				strcpy(t, user_path[i]); strcpy(user_path[i], user_path[j]); strcpy(user_path[j], t);
				strcpy(t, user_name[i]); strcpy(user_name[i], user_name[j]); strcpy(user_name[j], t);
			}
}

const char *pat_user_folder(void) { return user_folder; }
int pat_user_count(void) { return nuser; }
int pat_count(void) { return PAT_BUILTIN + nuser; }

const char *pat_name(int i)
{
	if (i < PAT_BUILTIN) return NAMES[i];
	return user_name[i - PAT_BUILTIN];
}
const char *pat_tip(int i)
{
	if (i < PAT_BUILTIN) return TIPS[i];
	return "Your own picture from the SD card.";
}
const char *pat_controls(int i)
{
	if (i < PAT_BUILTIN) return CONTROLS[i];
	return "A: pixel perfect / fill screen";
}

static char optbuf[64];
const char *pat_option(int i)
{
	optbuf[0] = 0;
	static const char *fc[5] = { "White", "Red", "Green", "Blue", "Black" };
	static const char *side[4] = { "Top", "Bottom", "Left", "Right" };
	static const char *conv[4] = { "Grid", "Dots", "Crosshatch", "RGB lines" };
	static const char *st[3] = { "Horizontal", "Vertical", "Checkerboard" };
	static const char *sh[3] = { "Lines", "Bricks", "Wedges" };
	switch (i) {
	case PAT_EBU: case PAT_SMPTE: snprintf(optbuf, sizeof optbuf, "%d%%", o_bars100 ? 100 : 75); break;
	case PAT_BLEED: snprintf(optbuf, sizeof optbuf, "Checkerboard %s", o_bleed ? "on" : "off"); break;
	case PAT_PLUGE: snprintf(optbuf, sizeof optbuf, o_pluge ? "High contrast" : "Normal"); break;
	case PAT_FIELD: snprintf(optbuf, sizeof optbuf, "%s  %d/255  (%d%%)", fc[o_field_col], o_field_lvl, o_field_lvl * 100 / 255); break;
	case PAT_IRE: snprintf(optbuf, sizeof optbuf, "%d IRE", o_ire); break;
	case PAT_MONO: snprintf(optbuf, sizeof optbuf, "Lines %d%%", o_mono); break;
	case PAT_GRID: snprintf(optbuf, sizeof optbuf, "%d px squares  -  %dx%d", o_grid ? 16 : 32, W, H); break;
	case PAT_SHARP: snprintf(optbuf, sizeof optbuf, "%s", sh[o_sharp]); break;
	case PAT_OVERSCAN: {
		int v = o_ov[o_ov_side], full = (o_ov_side < 2) ? H : W;
		snprintf(optbuf, sizeof optbuf, "%s edge: %d px (%d.%d%%)   T%d B%d L%d R%d", side[o_ov_side], v,
		         v * 100 / full, (v * 1000 / full) % 10, o_ov[0], o_ov[1], o_ov[2], o_ov[3]);
		break;
	}
	case PAT_CONVERGE: snprintf(optbuf, sizeof optbuf, "%s", conv[o_conv]); break;
	case PAT_STRIPES: snprintf(optbuf, sizeof optbuf, "%s  %d px", st[o_stripes], o_stripe_w); break;
	case PAT_BLOOM: snprintf(optbuf, sizeof optbuf, "%d Hz  %s", BLOOM_HZ[o_bloom_hz], o_bloom_box ? "box" : "full"); break;
	case PAT_AVSYNC: snprintf(optbuf, sizeof optbuf, "Sound %s", o_sync_snd ? "on" : "off"); break;
	case PAT_LAG: snprintf(optbuf, sizeof optbuf, "%d / %d tries", lag_n, LAG_TRIES); break;
	case PAT_CHANNEL: { static const char *cn[3] = { "Red only", "Green only", "Blue only" }; snprintf(optbuf, sizeof optbuf, "%s", cn[o_chan]); break; }
	case PAT_WASHER: { static const char *wm[4] = { "RGB strobe", "Noise", "Inverting bars", "All, in turn" }; snprintf(optbuf, sizeof optbuf, "%s", wm[o_wash_mode]); break; }
	case PAT_SWEEP: snprintf(optbuf, sizeof optbuf, "%d Hz  %s%s", (int)o_sweep_hz, o_sweep_auto ? "auto" : "by hand", o_sweep_on ? "" : "  (muted)"); break;
	case PAT_SCROLL: snprintf(optbuf, sizeof optbuf, "%s  speed %d%s", o_scroll_vert ? "Up-down" : "Sideways", o_scroll_spd, o_scroll_pause ? "  (paused)" : ""); break;
	case PAT_GRIDSCROLL: { static const char *gd[4] = { "Right", "Down", "Diagonal", "Left" }; snprintf(optbuf, sizeof optbuf, "%s  speed %d", gd[o_gs_dir], o_gs_spd); break; }
	case PAT_SPRITE: { static const char *sm[3] = { "Shadow", "Striped", "Both" }; snprintf(optbuf, sizeof optbuf, "%s", sm[o_spr_mode]); break; }
	case PAT_DIAG: snprintf(optbuf, sizeof optbuf, "%s", o_diag_spin ? "Spinning" : "Still"); break;
	case PAT_WHITECLIP: snprintf(optbuf, sizeof optbuf, "%s background", o_clip_bg ? "Black" : "White"); break;
	case PAT_GAMMA: snprintf(optbuf, sizeof optbuf, "%s", o_gamma_cmp ? "Checkerboard" : "Lines"); break;
	case PAT_SPEAKERS: { static const char *sp[3] = { "Left", "Right", "Both" }; snprintf(optbuf, sizeof optbuf, "%s", sp[o_spk]); break; }
	default:
		if (i >= PAT_BUILTIN) snprintf(optbuf, sizeof optbuf, o_fill ? "Fill screen" : "Pixel perfect");
	}
	return optbuf;
}

int pat_input(int i, u32 d)
{
	int L = (d & BTN_LEFT) != 0, R = (d & BTN_RIGHT) != 0, U = (d & BTN_UP) != 0, D = (d & BTN_DOWN) != 0;
	int A = (d & BTN_A) != 0, B = (d & BTN_B) != 0;
	int ch = 0;
	switch (i) {
	case PAT_RAMPS: case PAT_RAMP: if (A) { o_ramp_dir ^= 1; ch = 1; } break;
	case PAT_EBU: case PAT_SMPTE: if (A) { o_bars100 ^= 1; ch = 1; } break;
	case PAT_BLEED: if (A) { o_bleed ^= 1; ch = 1; } break;
	case PAT_WARMUP: if (A) { warm_start = 0; ch = 1; } break;
	case PAT_PLUGE: if (A) { o_pluge ^= 1; ch = 1; } break;
	case PAT_FIELD:
		if (A) { o_field_col = (o_field_col + 1) % 5; ch = 1; }
		if (L || R) { o_field_lvl += R ? 5 : -5; if (o_field_lvl < 0) o_field_lvl = 0; if (o_field_lvl > 255) o_field_lvl = 255; ch = 1; }
		break;
	case PAT_IRE:
		if (L || R) { o_ire += R ? 10 : -10; if (o_ire < 0) o_ire = 0; if (o_ire > 100) o_ire = 100; ch = 1; }
		break;
	case PAT_MONO:
		if (L || R) { o_mono += R ? 25 : -25; if (o_mono < 25) o_mono = 25; if (o_mono > 100) o_mono = 100; ch = 1; }
		break;
	case PAT_GRID: if (A) { o_grid ^= 1; ch = 1; } break;
	case PAT_SHARP: if (A) { o_sharp = (o_sharp + 1) % 3; ch = 1; } break;
	case PAT_OVERSCAN:
		if (U) { o_ov_side = (o_ov_side + 3) % 4; ch = 1; }
		if (D) { o_ov_side = (o_ov_side + 1) % 4; ch = 1; }
		if (L || R) { o_ov[o_ov_side] += R ? 1 : -1; if (o_ov[o_ov_side] < 0) o_ov[o_ov_side] = 0; if (o_ov[o_ov_side] > 80) o_ov[o_ov_side] = 80; ch = 1; }
		if (B) { for (int k = 0; k < 4; k++) o_ov[k] = 16; ch = 1; }
		break;
	case PAT_CONVERGE: if (A) { o_conv = (o_conv + 1) % 4; ch = 1; } break;
	case PAT_STRIPES:
		if (A) { o_stripes = (o_stripes + 1) % 3; ch = 1; }
		if (L || R) { o_stripe_w += R ? 1 : -1; if (o_stripe_w < 1) o_stripe_w = 1; if (o_stripe_w > 4) o_stripe_w = 4; ch = 1; }
		if (B) { o_stripe_phase ^= 1; ch = 1; }
		break;
	case PAT_AVSYNC: if (A) { o_sync_snd ^= 1; ch = 1; } break;
	case PAT_CHANNEL: if (A) { o_chan = (o_chan + 1) % 3; ch = 1; } break;
	case PAT_WASHER: case PAT_BLOOM:
		if (!flash_ok) { if (A) { flash_ok = 1; ch = 1; } return ch; }
		if (i == PAT_WASHER) { if (A) { o_wash_mode = (o_wash_mode + 1) % 4; ch = 1; } if (B) { flash_ok = 0; ch = 1; } break; }
		if (L && o_bloom_hz > 0) { o_bloom_hz--; ch = 1; }
		if (R && o_bloom_hz < 2) { o_bloom_hz++; ch = 1; }
		if (A) { o_bloom_box ^= 1; ch = 1; }
		if (B) { flash_ok = 0; ch = 1; }
		break;
	case PAT_SWEEP:
		if (A) { o_sweep_auto ^= 1; ch = 1; }
		if (B) { o_sweep_on ^= 1; ch = 1; }
		if (L || R) { o_sweep_auto = 0; o_sweep_hz *= R ? 1.0595f : 1 / 1.0595f; if (o_sweep_hz < 20) o_sweep_hz = 20; if (o_sweep_hz > 20000) o_sweep_hz = 20000; ch = 1; }
		if (U || D) { o_sweep_auto = 0; o_sweep_hz *= U ? 2.0f : 0.5f; if (o_sweep_hz < 20) o_sweep_hz = 20; if (o_sweep_hz > 20000) o_sweep_hz = 20000; ch = 1; }
		break;
	case PAT_SPEAKERS: if (A) { o_spk = (o_spk + 1) % 3; ch = 1; } break;
	case PAT_SCROLL:
		if (L && o_scroll_spd > 1) { o_scroll_spd--; ch = 1; }
		if (R && o_scroll_spd < 16) { o_scroll_spd++; ch = 1; }
		if (A) { o_scroll_vert ^= 1; ch = 1; }
		if (B) { o_scroll_pause ^= 1; ch = 1; }
		break;
	case PAT_GRIDSCROLL:
		if (L && o_gs_spd > 1) { o_gs_spd--; ch = 1; }
		if (R && o_gs_spd < 8) { o_gs_spd++; ch = 1; }
		if (A) { o_gs_dir = (o_gs_dir + 1) % 4; ch = 1; }
		break;
	case PAT_SPRITE: if (A) { o_spr_mode = (o_spr_mode + 1) % 3; ch = 1; } break;
	case PAT_DIAG: if (A) { o_diag_spin ^= 1; ch = 1; } break;
	case PAT_WHITECLIP: if (A) { o_clip_bg ^= 1; ch = 1; } break;
	case PAT_GAMMA: if (A) { o_gamma_cmp ^= 1; ch = 1; } break;
	case PAT_EDGE:
		if (B) { edge_step = 0; ch = 1; }
		if (A && ptr_v && edge_step < 4) {
			edge_v[edge_step] = edge_step == 0 ? ptr_y : edge_step == 1 ? H - ptr_y : edge_step == 2 ? ptr_x : W - ptr_x;
			if (edge_v[edge_step] < 0) edge_v[edge_step] = 0;
			edge_step++; ch = 1;
			if (edge_step == 4) {
				char m[64]; snprintf(m, sizeof m, "Overscan: top %d.%d%% bottom %d.%d%% left %d.%d%% right %d.%d%%",
					edge_v[0] * 100 / H, edge_v[0] * 1000 / H % 10, edge_v[1] * 100 / H, edge_v[1] * 1000 / H % 10,
					edge_v[2] * 100 / W, edge_v[2] * 1000 / W % 10, edge_v[3] * 100 / W, edge_v[3] * 1000 / W % 10);
				health_add(m);
			}
		}
		break;
	case PAT_LAG:
		if (B) { lag_n = 0; ch = 1; }
		if (A && lag_n < LAG_TRIES) {
			/* distance in frames from the nearest hit (hit = every 60 frames) */
			int ph = (int)(frame_no % 60), off = ph <= 30 ? ph : ph - 60;
			lag_res[lag_n++] = off;
			ch = 1;
			if (lag_n == LAG_TRIES) {
				int sum = 0; for (int k = 0; k < LAG_TRIES; k++) sum += lag_res[k];
				char m[48]; snprintf(m, sizeof m, "Lag test: about %d ms", sum * (1000 / hz()) / LAG_TRIES);
				if (sum >= 0) health_add(m);
			}
		} else if (A) { lag_n = 0; ch = 1; }
		break;
	default:
		if (i >= PAT_BUILTIN && A) { o_fill ^= 1; ch = 1; }
	}
	return ch;
}

/* ------------------------------------------------------------------ */
/* the patterns                                                        */
/* ------------------------------------------------------------------ */

/* 240p Suite style RGB ramps with 0-F markers */
static void ramps(void)
{
	rect(0, 0, W, H, 0);
	static const char *lbl[4] = { "RED", "GREEN", "BLUE", "WHITE" };
	static const u32 lc[4] = { RGB(255, 40, 40), RGB(40, 255, 40), RGB(60, 90, 255), RGB(255, 255, 255) };
	int x0 = sx(58), x1 = sx(310);
	for (int k = 0; k < 16; k++) {
		char d[2] = { "0123456789ABCDEF"[o_ramp_dir ? 15 - k : k], 0 };
		int cx = x0 + (x1 - x0) * (2 * k + 1) / 32;
		text_c(F_BODYB, cx, sy(26) - F_BODYB->height / 2, d, RGB(255, 170, 30), 255);
	}
	for (int r = 0; r < 4; r++) {
		int y = sy(40 + r * 40), h = sy(72 + r * 40) - y;
		text_r(F_BODYB, x0 - 10, y + h / 2 - F_BODYB->height / 2, lbl[r], lc[r], 255);
		for (int s = 0; s < 32; s++) {
			int v = (o_ramp_dir ? 31 - s : s) * 255 / 31;
			u32 c = r == 0 ? RGB(v, 0, 0) : r == 1 ? RGB(0, v, 0) : r == 2 ? RGB(0, 0, v) : RGB(v, v, v);
			int a = x0 + (x1 - x0) * s / 32, b = x0 + (x1 - x0) * (s + 1) / 32;
			rect(a, y, b - a, h, c);
		}
	}
}

static void bars7(int y, int h, int v)
{
	u32 cols[7] = { RGB(v, v, v), RGB(v, v, 0), RGB(0, v, v), RGB(0, v, 0), RGB(v, 0, v), RGB(v, 0, 0), RGB(0, 0, v) };
	for (int i = 0; i < 7; i++) rect(i * W / 7, y, (i + 1) * W / 7 - i * W / 7, h, cols[i]);
}

static void ebu(void)
{
	int v = o_bars100 ? 255 : 191;
	u32 cols[8] = { RGB(255, 255, 255), RGB(v, v, 0), RGB(0, v, v), RGB(0, v, 0), RGB(v, 0, v), RGB(v, 0, 0), RGB(0, 0, v), 0 };
	for (int i = 0; i < 8; i++) rect(i * W / 8, 0, (i + 1) * W / 8 - i * W / 8, H, cols[i]);
}

static void smpte(void)
{
	int v = o_bars100 ? 255 : 191;
	int h1 = H * 67 / 100, h2 = H * 8 / 100, y3 = h1 + h2, h3 = H - y3;
	bars7(0, h1, v);
	u32 rev[7] = { RGB(0, 0, v), RGB(19, 19, 19), RGB(v, 0, v), RGB(19, 19, 19), RGB(0, v, v), RGB(19, 19, 19), RGB(v, v, v) };
	for (int i = 0; i < 7; i++) rect(i * W / 7, h1, (i + 1) * W / 7 - i * W / 7, h2, rev[i]);
	int w4 = W * 5 / 28;
	rect(0, y3, w4, h3, RGB(0, 33, 76));
	rect(w4, y3, w4, h3, RGB(255, 255, 255));
	rect(w4 * 2, y3, w4, h3, RGB(50, 0, 106));
	rect(w4 * 3, y3, W - w4 * 3, h3, RGB(19, 19, 19));
	int bw = W / 7, px0 = bw * 5, pw = bw / 3;
	rect(px0, y3, pw, h3, RGB(9, 9, 9));
	rect(px0 + pw, y3, pw, h3, RGB(19, 19, 19));
	rect(px0 + pw * 2, y3, pw, h3, RGB(29, 29, 29));
}

static void refbars(void)
{
	rect(0, 0, W, H, 0);
	static const u32 full[6] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 0), RGB(0, 255, 255), RGB(255, 0, 255) };
	static const char *nm[6] = { "R", "G", "B", "Y", "C", "M" };
	int bh = (H - 60) / 6;
	for (int i = 0; i < 6; i++) {
		int y = 30 + i * bh;
		u32 c = full[i];
		text(F_BODYB, 16, y + bh / 2 - 12, nm[i], c, 255);
		/* reference 100% on the left, then 75 / 50 / 25 % */
		for (int k = 0; k < 4; k++) {
			int pct = 100 - k * 25;
			int r = ((c >> 16) & 255) * pct / 100, g = ((c >> 8) & 255) * pct / 100, b = (c & 255) * pct / 100;
			int x = 50 + k * (W - 70) / 4;
			rect(x, y + 4, (W - 70) / 4 - 6, bh - 8, RGB(r, g, b));
			if (i == 0) { char t[8]; snprintf(t, sizeof t, "%d%%", pct); text_c(F_SMALL, x + (W - 70) / 8, 8, t, grey(200), 255); }
		}
	}
}

static void barsgrey(void)
{
	rect(0, 0, W, H, 0);
	bars7(0, H / 2, 191);
	for (int i = 0; i < 11; i++) rect(i * W / 11, H / 2, (i + 1) * W / 11 - i * W / 11, H / 2, grey(i * 255 / 10));
}

static void bleed(void)
{
	rect(0, 0, W, H, 0);
	static const u32 c[4] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 255) };
	int bh = H / 9;
	for (int i = 0; i < 4; i++) {
		int y = bh + i * 2 * bh;
		rect(40, y, W - 80, bh, c[i]);
		if (o_bleed)
			for (int yy = y; yy < y + bh; yy++)
				for (int xx = 40; xx < W - 40; xx++)
					if (((xx + yy) & 1) == 0) gfx_rect(xx, yy, 1, 1, i == 3 ? 0 : RGB(255, 255, 255), 255);
	}
}

static void pluge(void)
{
	rect(0, 0, W, H, 0);
	int hi = o_pluge;
	/* bars just above black: 2%, 4%, 7.5% (NTSC setup), 10% */
	static const int lv[4] = { 5, 10, 19, 26 };
	for (int i = 0; i < 4; i++) {
		int v = hi ? lv[i] * 3 : lv[i];
		rect(W / 10 + i * W / 10, H / 6, W / 20, H * 2 / 3, grey(v));
		rect(W - W / 10 - W / 20 - i * W / 10, H / 6, W / 20, H * 2 / 3, grey(v));
	}
	/* centre reference patches */
	int cw = W / 5, ch = H / 8;
	static const int ref[4] = { 255, 191, 128, 64 };
	for (int i = 0; i < 4; i++) rect(W / 2 - cw / 2, H / 6 + i * ch, cw, ch, grey(ref[i]));
}

static void steps(void) { for (int i = 0; i < 11; i++) rect(i * W / 11, 0, (i + 1) * W / 11 - i * W / 11, H, grey(i * 255 / 10)); }

static void ramp(void)
{
	for (int x = 0; x < W; x++) {
		int v = (o_ramp_dir ? W - 1 - x : x) * 255 / (W - 1);
		rect(x, 0, 1, H / 2, grey(v));
		rect(x, H / 2, 1, H / 6, RGB(v, 0, 0));
		rect(x, H * 2 / 3, 1, H / 6, RGB(0, v, 0));
		rect(x, H * 5 / 6, 1, H - H * 5 / 6, RGB(0, 0, v));
	}
}

static void field(void)
{
	int v = o_field_lvl;
	u32 c = o_field_col == 0 ? grey(v) : o_field_col == 1 ? RGB(v, 0, 0) : o_field_col == 2 ? RGB(0, v, 0) : o_field_col == 3 ? RGB(0, 0, v) : 0;
	rect(0, 0, W, H, c);
}

static void ire(void)
{
	rect(0, 0, W, H, 0);
	rect(W / 4, H / 4, W / 2, H / 2, grey(o_ire * 255 / 100));
}

static void mono(void)
{
	rect(0, 0, W, H, 0);
	u32 c = grey(o_mono * 255 / 100);
	int step = 40;
	for (int x = W / 2 % step; x < W; x += step) rect(x - 1, 0, 2, H, grey(o_mono * 110 / 100));
	for (int y = H / 2 % step; y < H; y += step) rect(0, y - 1, W, 2, grey(o_mono * 110 / 100));
	gfx_ring(W / 2, H / 2, H / 2 - 6, 3, c, 255);
	gfx_ring(W / 2, H / 2, H / 4, 3, c, 255);
	int r = H / 8;
	gfx_ring(r + 10, r + 10, r, 3, c, 255); gfx_ring(W - r - 10, r + 10, r, 3, c, 255);
	gfx_ring(r + 10, H - r - 10, r, 3, c, 255); gfx_ring(W - r - 10, H - r - 10, r, 3, c, 255);
	rect(W / 2 - 1, 0, 2, H, c); rect(0, H / 2 - 1, W, 2, c);
	rect(0, 0, W, 2, c); rect(0, H - 2, W, 2, c); rect(0, 0, 2, H, c); rect(W - 2, 0, 2, H, c);
}

static void grid(void)
{
	rect(0, 0, W, H, 0);
	int s = o_grid ? 16 : 32;
	for (int x = 0; x < W; x += s) rect(x, 0, 2, H, grey(230));
	rect(W - 2, 0, 2, H, grey(230));
	for (int y = 0; y < H; y += s) rect(0, y, W, 2, grey(230));
	rect(0, H - 2, W, 2, grey(230));
	rect(W / 2 - 1, 0, 2, H, RGB(255, 60, 60));
	rect(0, H / 2 - 1, W, 2, RGB(255, 60, 60));
	/* safe areas: 90% action, 80% title */
	gfx_round_line(W / 20, H / 20, W * 9 / 10, H * 9 / 10, 0, 2, RGB(60, 200, 255), 255);
	gfx_round_line(W / 10, H / 10, W * 8 / 10, H * 8 / 10, 0, 2, RGB(255, 200, 40), 255);
}

static void sharp(void)
{
	rect(0, 0, W, H, grey(128));
	if (o_sharp == 0) {
		/* groups of vertical lines, 1-4 px, and horizontal groups */
		for (int g = 0; g < 4; g++) {
			int wdt = g + 1, x0 = 40 + g * (W - 80) / 4;
			for (int x = 0; x < (W - 80) / 4 - 20; x++) rect(x0 + x, 40, 1, H / 2 - 60, ((x / wdt) & 1) ? grey(255) : 0);
			for (int y = 0; y < H / 2 - 60; y++) rect(x0, H / 2 + 20 + y, (W - 80) / 4 - 20, 1, ((y / wdt) & 1) ? grey(255) : 0);
		}
	} else if (o_sharp == 1) {
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x += 1) {
				int row = y / 8, off = (row & 1) * 8;
				int edge = (y % 8 == 0) || ((x + off) % 16 == 0);
				if (edge) rect(x, y, 1, 1, 0);
			}
	} else {
		/* converging wedges */
		for (int k = 0; k < 24; k++) {
			int cx = W / 2, cy = H / 2;
			double a = k * 3.14159265 / 12;
			int x2 = cx + (int)(W * 0.6 * cos(a)), y2 = cy + (int)(W * 0.6 * sin(a));
			double a2 = (k + 0.5) * 3.14159265 / 12;
			int x3 = cx + (int)(W * 0.6 * cos(a2)), y3 = cy + (int)(W * 0.6 * sin(a2));
			gfx_tri(cx, cy, x2, y2, x3, y3, 0, 255);
		}
		gfx_ring(W / 2, H / 2, H / 3, 2, RGB(255, 60, 60), 255);
	}
}

static void overscan(void)
{
	rect(0, 0, W, H, RGB(255, 255, 255));
	int t = o_ov[0], b = o_ov[1], l = o_ov[2], r = o_ov[3];
	rect(l, t, W - l - r, H - t - b, grey(60));
	/* highlight the edge being moved */
	u32 hl = RGB(255, 60, 60);
	if (o_ov_side == 0) rect(l, t, W - l - r, 3, hl);
	if (o_ov_side == 1) rect(l, H - b - 3, W - l - r, 3, hl);
	if (o_ov_side == 2) rect(l, t, 3, H - t - b, hl);
	if (o_ov_side == 3) rect(W - r - 3, t, 3, H - t - b, hl);
	char m[40]; snprintf(m, sizeof m, "%d x %d", W, H);
	text_c(F_TITLE, W / 2, H / 2 - 20, m, grey(200), 255);
}

static void converge(void)
{
	rect(0, 0, W, H, 0);
	int s = 32;
	if (o_conv == 0 || o_conv == 2) {
		for (int x = 0; x < W; x += s) rect(x, 0, o_conv == 2 ? 1 : 2, H, grey(255));
		for (int y = 0; y < H; y += s) rect(0, y, W, o_conv == 2 ? 1 : 2, grey(255));
	}
	if (o_conv == 1)
		for (int y = s / 2; y < H; y += s)
			for (int x = s / 2; x < W; x += s) rect(x - 1, y - 1, 2, 2, grey(255));
	if (o_conv == 3) {
		for (int x = 0; x < W; x += s) { rect(x, 0, 2, H, RGB(255, 0, 0)); rect(x + 10, 0, 2, H, RGB(0, 255, 0)); rect(x + 20, 0, 2, H, RGB(0, 0, 255)); }
		for (int y = 0; y < H; y += s) rect(0, y, W, 2, RGB(255, 255, 255));
	}
}

static void stripes(void)
{
	int w = o_stripe_w, ph = o_stripe_phase;
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			int on;
			if (o_stripes == 0) on = ((y / w) + ph) & 1;
			else if (o_stripes == 1) on = ((x / w) + ph) & 1;
			else on = ((x / w) + (y / w) + ph) & 1;
			gfx_rect(x, y, 1, 1, on ? grey(255) : 0, 255);
		}
}

static void checker(void)
{
	static const u32 cc[24] = {
		RGB(115,82,68), RGB(194,150,130), RGB(98,122,157), RGB(87,108,67), RGB(133,128,177), RGB(103,189,170),
		RGB(214,126,44), RGB(80,91,166), RGB(193,90,99), RGB(94,60,108), RGB(157,188,64), RGB(224,163,46),
		RGB(56,61,150), RGB(70,148,73), RGB(175,54,60), RGB(231,199,31), RGB(187,86,149), RGB(8,133,161),
		RGB(243,243,242), RGB(200,200,200), RGB(160,160,160), RGB(122,122,121), RGB(85,85,85), RGB(52,52,52),
	};
	rect(0, 0, W, H, RGB(24, 24, 24));
	int mx = 50, my = 50, gap = 10;
	int pw = (W - 2 * mx - 5 * gap) / 6, ph = (H - 2 * my - 3 * gap) / 4;
	for (int i = 0; i < 24; i++) gfx_round(mx + (i % 6) * (pw + gap), my + (i / 6) * (ph + gap), pw, ph, 8, cc[i], 255);
}

static void skin(void)
{
	static const u32 s[10] = {
		RGB(255,224,196), RGB(241,194,125), RGB(224,172,105), RGB(198,134,66), RGB(141,85,36),
		RGB(255,219,172), RGB(234,192,134), RGB(194,145,110), RGB(122,79,56), RGB(84,52,40),
	};
	rect(0, 0, W, H, RGB(120, 120, 120));
	int w = (W - 100) / 5, h = (H - 110) / 2;
	for (int i = 0; i < 10; i++) gfx_round(50 + (i % 5) * w + 5, 50 + (i / 5) * (h + 10), w - 10, h, 60, s[i], 255);
}

static void scene(void)
{
	u32 *buf = gfx_buf();
	int horizon = H * 52 / 100;
	for (int y = 0; y < horizon; y++) {
		int t = y * 255 / horizon;
		u32 c = (t < 128) ? RGB(60 + t / 3, 120 + t / 3, 205 + t / 10) : RGB(102 + (t - 128), 162 + (t - 128) / 3, 218 - (t - 128) / 4);
		rect(0, y, W, 1, c);
	}
	gfx_circle(W * 78 / 100, H * 18 / 100, 60, RGB(255, 236, 170), 60);
	gfx_circle(W * 78 / 100, H * 18 / 100, 40, RGB(255, 244, 200), 255);
	u32 cl = RGB(250, 250, 252);
	gfx_circle(120, 80, 26, cl, 235); gfx_circle(150, 70, 32, cl, 235); gfx_circle(185, 82, 24, cl, 235);
	gfx_round(100, 82, 110, 26, 13, cl, 235);
	gfx_circle(360, 110, 18, cl, 220); gfx_circle(382, 102, 24, cl, 220); gfx_round(346, 112, 64, 18, 9, cl, 220);
	for (int x = 0; x < W; x++) {
		int m1 = horizon - 90 + abs((x % 260) - 130) * 70 / 130;
		int m2 = horizon - 50 + abs(((x + 90) % 200) - 100) * 45 / 100;
		for (int y = m1; y < horizon; y++) buf[y * W + x] = (y < m1 + 12) ? RGB(236, 238, 245) : RGB(96, 104, 132);
		for (int y = m2; y < horizon; y++) buf[y * W + x] = RGB(62, 110, 76);
	}
	int split = W * 45 / 100;
	for (int y = horizon; y < H; y++) {
		int t = (y - horizon) * 255 / (H - horizon);
		rect(0, y, split, 1, RGB(70 + t / 6, 150 - t / 5, 60 - t / 8));
		rect(split, y, W - split, 1, RGB(20 + t / 10, 110 + t / 5, 150 + t / 6));
	}
	for (int y = H * 80 / 100; y < H; y++) rect(split - 40, y, W - split + 40, 1, RGB(232, 206, 150));
	for (int i = 0; i < 4; i++) {
		int tx = 40 + i * 70, ty = horizon + 30 + (i % 2) * 20;
		rect(tx - 5, ty, 10, 50, RGB(110, 72, 40));
		gfx_circle(tx, ty - 10, 30, RGB(40 + i * 8, 120 + i * 10, 50), 255);
	}
	gfx_circle(W * 62 / 100, H * 84 / 100, 26, RGB(220, 40, 50), 255);
	gfx_circle(W * 74 / 100, H * 86 / 100, 22, RGB(250, 205, 40), 255);
	gfx_circle(W * 86 / 100, H * 84 / 100, 24, RGB(40, 80, 210), 255);
	static const u32 sk[4] = { RGB(245,208,180), RGB(214,160,120), RGB(160,110,76), RGB(96,62,44) };
	for (int i = 0; i < 4; i++) {
		int fx = 60 + i * 64, fy = H - 60;
		gfx_circle(fx, fy, 24, sk[i], 255);
		gfx_circle(fx - 8, fy - 4, 3, RGB(30, 30, 30), 255);
		gfx_circle(fx + 8, fy - 4, 3, RGB(30, 30, 30), 255);
		gfx_round(fx - 8, fy + 8, 16, 4, 2, RGB(150, 60, 60), 255);
	}
}

/* ---- your pictures ---- */
static void user_pic(int k)
{
	rect(0, 0, W, H, 0);
	FILE *f = fopen(user_path[k], "rb");
	if (!f) { text_c(F_BODYB, W / 2, H / 2, "Can't open this picture", RGB(255, 120, 120), 255); return; }
	int iw, ih, n;
	unsigned char *img = stbi_load_from_file(f, &iw, &ih, &n, 4);
	fclose(f);
	if (!img) { text_c(F_BODYB, W / 2, H / 2, "This picture can't be read (try PNG or JPG)", RGB(255, 120, 120), 255); return; }
	/* crop see-through borders (e.g. a 320x240 picture inside a 512x256 texture) */
	int cx0 = iw, cy0 = ih, cx1 = -1, cy1 = -1;
	for (int y = 0; y < ih; y++)
		for (int x = 0; x < iw; x++)
			if (img[((size_t)y * iw + x) * 4 + 3]) {
				if (x < cx0) cx0 = x; if (x > cx1) cx1 = x;
				if (y < cy0) cy0 = y; if (y > cy1) cy1 = y;
			}
	if (cx1 < 0) { cx0 = cy0 = 0; cx1 = iw - 1; cy1 = ih - 1; }
	int stride = iw;
	unsigned char *base = img + ((size_t)cy0 * iw + cx0) * 4;
	iw = cx1 - cx0 + 1; ih = cy1 - cy0 + 1;
	int dw, dh;
	if (o_fill) { dw = W; dh = H; }
	else {
		/* pixel perfect: biggest whole-number zoom that fits, else shrink to fit */
		int zx = W / iw, zy = H / ih, z = zx < zy ? zx : zy;
		if (z >= 1) { dw = iw * z; dh = ih * z; }
		else if (iw * H > ih * W) { dw = W; dh = ih * W / iw; }
		else { dh = H; dw = iw * H / ih; }
	}
	int ox = (W - dw) / 2, oy = (H - dh) / 2;
	u32 *buf = gfx_buf();
	for (int y = 0; y < dh; y++) {
		int syy = y * ih / dh;
		u32 *row = buf + (oy + y) * W + ox;
		const unsigned char *src = base + (size_t)syy * stride * 4;
		for (int x = 0; x < dw; x++) {
			const unsigned char *p = src + (x * iw / dw) * 4;
			int a = p[3];
			row[x] = RGB(p[0] * a / 255, p[1] * a / 255, p[2] * a / 255);
		}
	}
	stbi_image_free(img);
}

static void bloom(void)
{
	int period = 1000 / BLOOM_HZ[o_bloom_hz];
	int white = (plat_ms() % period) < (u64)(period / 2);
	if (!o_bloom_box) { rect(0, 0, W, H, white ? grey(255) : 0); return; }
	/* grey frame stays put; the centre flips - watch the frame edges */
	rect(0, 0, W, H, grey(40));
	gfx_round_line(20, 20, W - 40, H - 40, 0, 3, grey(200), 255);
	rect(W / 6, H / 6, W * 2 / 3, H * 2 / 3, white ? grey(255) : 0);
}

/* ---- audio sync: ball drops onto a line every 60 frames, beep on that exact frame ---- */
static void avsync(void)
{
	frame_no++;
	rect(0, 0, W, H, 0);
	int ph = frame_no % 60;
	int floor_y = H * 58 / 100, r = H / 24;
	/* bounce: height = parabola that is 0 at ph == 0 */
	float t = (ph < 30 ? ph : 60 - ph) / 30.0f;
	int by = floor_y - r - (int)(t * t * (H * 2 / 5));
	int hit = ph == 0;
	rect(W / 8, floor_y, W * 3 / 4, 4, hit ? RGB(255, 255, 255) : grey(110));
	gfx_round(W / 2 - r, by, r * 2, r * 2, r, hit ? RGB(255, 255, 255) : RGB(60, 190, 250), 255);
	/* frame counter strip: 60 ticks, the current one lit */
	for (int k = 0; k < 60; k++)
		rect(W / 8 + k * (W * 3 / 4) / 60, floor_y + 30, (W * 3 / 4) / 60 - 2, 10, k == ph ? RGB(255, 255, 255) : grey(40));
	if (hit) {
		if (o_sync_snd) plat_beep(0);
		rect(0, 0, W, 8, RGB(255, 255, 255));
	}
}

/* ---- manual lag test (240p Test Suite style: tap in rhythm, reaction time cancels out) ---- */
static void lagtest(void)
{
	frame_no++;
	rect(0, 0, W, H, 0);
	int ph = frame_no % 60;
	int line_x = W / 2, sz = H / 12, top = H * 35 / 100, lh = H / 4;
	float t = (ph < 30 ? ph : 60 - ph) / 30.0f;     /* 0 on the hit */
	int bx = line_x - sz / 2 + (int)(t * (W / 3));
	int hit = ph == 0;
	rect(line_x - 1, top, 3, lh, hit ? RGB(255, 255, 255) : grey(110));
	gfx_round(bx, top + lh / 2 - sz / 2, sz, sz, 6, hit ? RGB(255, 255, 255) : RGB(250, 200, 60), 255);
	if (hit && o_lag_snd) plat_beep(1);
	/* results */
	int fms = 1000 / hz();
	int y = H / 12 + F_TITLE->height + 6;   /* results sit above the box, clear of the info bar */
	char b[64];
	for (int k = 0; k < lag_n; k++) {
		int ms = lag_res[k] * fms;
		snprintf(b, sizeof b, "%+d ms", ms);
		int x = W / 10 + (k % 5) * (W / 6), yy = y + (k / 5) * (F_BODYB->height + 4);
		text(F_BODYB, x, yy, b, lag_res[k] < 0 ? RGB(250, 200, 60) : RGB(230, 232, 240), 255);
	}
	if (lag_n == LAG_TRIES) {
		int sum = 0; for (int k = 0; k < LAG_TRIES; k++) sum += lag_res[k];
		if (sum < 0) snprintf(b, sizeof b, "Too early - press B and try again");
		else snprintf(b, sizeof b, "Lag: about %d ms  (%d frames)", sum * fms / LAG_TRIES, (sum + LAG_TRIES / 2) / LAG_TRIES);
		text_c(F_TITLE, W / 2, H / 12, b, sum < 0 ? RGB(250, 200, 60) : RGB(80, 220, 120), 255);
	} else {
		snprintf(b, sizeof b, "Press A on the hit  -  %d left", LAG_TRIES - lag_n);
		text_c(F_BODYB, W / 2, H / 12, b, RGB(230, 232, 240), 255);
	}
}

/* ---- warning before anything that flashes ---- */
static void flash_warning(void)
{
	rect(0, 0, W, H, grey(16));
	text_c(F_TITLE, W / 2, H / 3 - 30, "Flashing pictures", RGB(255, 190, 60), 255);
	text_c(F_BODY, W / 2, H / 3 + 20, "This test flashes quickly. Skip it if flashing", RGB(230, 232, 240), 255);
	text_c(F_BODY, W / 2, H / 3 + 46, "lights bother you.", RGB(230, 232, 240), 255);
	text_c(F_BODYB, W / 2, H / 3 + 96, "Press A to start      2 = skip", RGB(80, 200, 250), 255);
}

/* ---- colour isolation ---- */
static void channel_bars(void)
{
	/* 75% bars, showing only one colour channel: the bars that should match are side by side */
	static const u32 B7[7] = { RGB(191,191,191), RGB(191,191,0), RGB(0,191,191), RGB(0,191,0), RGB(191,0,191), RGB(191,0,0), RGB(0,0,191) };
	u32 mask = o_chan == 0 ? 0xFF0000 : o_chan == 1 ? 0x00FF00 : 0x0000FF;
	for (int k = 0; k < 7; k++) rect(k * W / 7, 0, (k + 1) * W / 7 - k * W / 7, H * 3 / 4, B7[k] & mask);
	/* a reference strip underneath: pure level of that channel */
	rect(0, H * 3 / 4, W, H / 4, RGB(191, 191, 191) & mask);
	for (int k = 0; k < 7; k += 2) rect(k * W / 7, H * 3 / 4, W / 14, H / 4, 0);
}

/* ---- the exact edge of the Wii's picture ---- */
static void frame_edge(void)
{
	rect(0, 0, W, H, grey(24));
	/* safe-area rings: 2.5% / 5% / 10% in */
	static const int PCT[3] = { 25, 50, 100 };   /* tenths of a percent */
	static const u32 COL[3] = { RGB(90, 200, 90), RGB(230, 200, 60), RGB(230, 90, 90) };
	for (int k = 0; k < 3; k++) {
		int mx = W * PCT[k] / 1000, my = H * PCT[k] / 1000;
		gfx_round_line(mx, my, W - 2 * mx, H - 2 * my, 0, 1, COL[k], 255);
		char t[12]; snprintf(t, sizeof t, "%d.%d%%", PCT[k] / 10, PCT[k] % 10);
		text(F_SMALL, W * (30 + 14 * k) / 100, my + 2, t, COL[k], 255);   /* spread out so they never overlap */
	}
	/* the very edge: 2 px white, corner ticks, centre cross */
	rect(0, 0, W, 2, grey(255)); rect(0, H - 2, W, 2, grey(255));
	rect(0, 0, 2, H, grey(255)); rect(W - 2, 0, 2, H, grey(255));
	for (int k = 1; k < 8; k++) { rect(k * W / 8, 0, 1, H / 30, grey(200)); rect(k * W / 8, H - H / 30, 1, H / 30, grey(200)); rect(0, k * H / 8, W / 40, 1, grey(200)); rect(W - W / 40, k * H / 8, W / 40, 1, grey(200)); }
	rect(W / 2 - 20, H / 2, 40, 1, grey(255)); rect(W / 2, H / 2 - 20, 1, 40, grey(255));
	char sz[32]; snprintf(sz, sizeof sz, "%d x %d", W, H);
	const Font *ef = H < 400 ? F_SMALL : F_BODYB;   /* 240p lines are twice as tall */
	text_c(ef, W / 2, H / 2 + (H < 400 ? 12 : 28), sz, grey(200), 255);
	/* point-to-measure: aim at the edge you can just see, press A (top, bottom, left, right) */
	static const char *SIDE[4] = { "TOP", "BOTTOM", "LEFT", "RIGHT" };
	char m[64];
	if (edge_step < 4) snprintf(m, sizeof m, ptr_v ? "Point at the %s edge you can see, press A" : "Point the Wii Remote at the TV to measure", SIDE[edge_step]);
	else snprintf(m, sizeof m, "Cut off: T %d%%  B %d%%  L %d%%  R %d%%  (B = again)", edge_v[0] * 100 / H, edge_v[1] * 100 / H, edge_v[2] * 100 / W, edge_v[3] * 100 / W);
	text_c(ef, W / 2, H / 2 + (H < 400 ? 30 : 56), m, RGB(80, 200, 250), 255);
	for (int k = 0; k < edge_step && k < 4; k++) {
		if (k == 0) rect(0, edge_v[0], W, 2, RGB(250, 200, 60));
		if (k == 1) rect(0, H - edge_v[1], W, 2, RGB(250, 200, 60));
		if (k == 2) rect(edge_v[2], 0, 2, H, RGB(250, 200, 60));
		if (k == 3) rect(W - edge_v[3], 0, 2, H, RGB(250, 200, 60));
	}
	if (ptr_v && edge_step < 4) {
		if (edge_step < 2) rect(0, ptr_y, W, 1, RGB(80, 200, 250)); else rect(ptr_x, 0, 1, H, RGB(80, 200, 250));
		gfx_circle(ptr_x, ptr_y, 5, RGB(80, 200, 250), 255);
	}
}

/* ---- warm-up drift: a timer, and the pictures that show drift, 15 s each ---- */
static void warmup(void)
{
	u64 now = plat_ms();
	if (!warm_start || now < warm_start) warm_start = now;
	u32 s = (u32)((now - warm_start) / 1000);
	int which = (s / 15) % 3;
	if (which == 0) steps();
	else if (which == 1) { rect(0, 0, W, H, grey(0)); for (int x = 0; x <= 16; x++) rect(x * (W - 2) / 16, 0, 2, H, grey(200)); for (int y = 0; y <= 12; y++) rect(0, y * (H - 2) / 12, W, 2, grey(200)); }
	else { rect(0, 0, W, H, grey(128)); }
	char t[64]; snprintf(t, sizeof t, "On for %u:%02u%s", s / 60, s % 60, s >= 1500 ? "  -  warm: good to calibrate" : "  -  still warming up");
	int w = text_w(F_BODYB, t) + 24;
	rect(W / 2 - w / 2, H / 2 - 18, w, 36, grey(20));
	text_c(F_BODYB, W / 2, H / 2 - 12, t, s >= 1500 ? RGB(120, 230, 140) : RGB(250, 210, 90), 255);
}

/* ---- screen washer: strobe, noise and inverting bars ---- */
static u32 rng = 12345;
static void washer(void)
{
	if (!flash_ok) { flash_warning(); return; }
	frame_no++;
	int mode = o_wash_mode == 3 ? (int)((frame_no / 600) % 3) : o_wash_mode;   /* all: 10 s each */
	if (mode == 0) {
		static const u32 C[4] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 255) };
		rect(0, 0, W, H, C[(frame_no / 6) % 4]);                 /* 10 changes a second */
	} else if (mode == 1) {
		u32 *b = gfx_buf();
		for (int y = 0; y < H; y += 2) {
			u32 *r = b + y * W;
			for (int x = 0; x < W; x += 2) {
				rng = rng * 1103515245u + 12345u;
				u32 v = (rng >> 24) > 127 ? 0xFFFFFF : 0;
				r[x] = r[x + 1] = v;
			}
			memcpy(r + W, r, W * 4);
		}
	} else {
		int bw = W / 8, off = (int)(frame_no * 4) % (bw * 2);
		int inv = (frame_no / 30) & 1;
		for (int x = -bw * 2; x < W; x += bw * 2) {
			int x0 = x + off;
			int a0 = x0 < 0 ? 0 : x0, a1 = x0 + bw > W ? W : x0 + bw;
			int b0 = x0 + bw < 0 ? 0 : x0 + bw, b1 = x0 + 2 * bw > W ? W : x0 + 2 * bw;
			if (a1 > a0) rect(a0, 0, a1 - a0, H, inv ? 0 : grey(255));
			if (b1 > b0) rect(b0, 0, b1 - b0, H, inv ? grey(255) : 0);
		}
	}
}

/* ---- rattle tester: sine sweep 20 Hz to 20 kHz ---- */
static void sweep(void)
{
	frame_no++;
	if (o_sweep_auto) { o_sweep_hz *= 1.0012f; if (o_sweep_hz > 20000) o_sweep_hz = 20; }   /* about 100 s per sweep */
	plat_tone(o_sweep_on ? (int)o_sweep_hz : 0);
	rect(0, 0, W, H, grey(12));
	char t[24]; snprintf(t, sizeof t, "%d Hz", (int)o_sweep_hz);
	text_c(F_BIG, W / 2, H / 4, t, RGB(240, 240, 240), 255);
	/* log scale bar */
	int bx = W / 10, bw = W * 8 / 10, by = H / 2;
	rect(bx, by, bw, 10, grey(60));
	float pos = logf(o_sweep_hz / 20.0f) / logf(1000.0f);
	rect(bx, by, (int)(bw * pos), 10, RGB(60, 190, 250));
	static const int MARK[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
	for (int k = 0; k < 10; k++) {
		int x = bx + (int)(bw * logf(MARK[k] / 20.0f) / logf(1000.0f));
		rect(x, by + 14, 1, 8, grey(160));
		char m[8]; if (MARK[k] >= 1000) snprintf(m, sizeof m, "%dk", MARK[k] / 1000); else snprintf(m, sizeof m, "%d", MARK[k]);
		text_c(F_SMALL, x, by + 24, m, grey(160), 255);
	}
	text_c(F_SMALL, W / 2, by + 60, o_sweep_on ? "Turn the TV up slowly. Pause when the case buzzes." : "Sound off (B)", grey(170), 255);
}

/* ---- left / right speakers ---- */
static void speakers(void)
{
	frame_no++;
	rect(0, 0, W, H, grey(14));
	int ph = frame_no % 90;           /* a beep every 1.5 s, alternating when "both" */
	static int side;
	if (ph == 0) { side = o_spk == 2 ? !side : o_spk; plat_beep_pan(side ? 2 : 0); }
	int lit = ph < 15;
	int l = lit && side == 0, r = lit && side == 1;
	gfx_round(W / 10, H / 3, W / 4, H / 3, 20, l ? RGB(60, 190, 250) : grey(50), 255);
	gfx_round(W - W / 10 - W / 4, H / 3, W / 4, H / 3, 20, r ? RGB(60, 190, 250) : grey(50), 255);
	text_c(F_TITLE, W / 10 + W / 8, H / 2 - 16, "LEFT", grey(240), 255);
	text_c(F_TITLE, W - W / 10 - W / 8, H / 2 - 16, "RIGHT", grey(240), 255);
}

void pat_leave(void) { plat_tone(0); }

const char *CAT_NAME[CAT_COUNT] = { "Colour", "Grey & black level", "Geometry & overscan", "Sharpness", "Motion & lag", "Sound", "Screen care", "Your pictures" };
const char *CAT_TIP[CAT_COUNT] = {
	"Bars, pure red/green/blue, tint checks",
	"Black level, white level, grey scale",
	"Grid, circles, edges, convergence",
	"Fine detail and shimmer",
	"Lag test and power supply bloom",
	"Audio sync, speakers, rattle sweep",
	"Burn-in and purity washer",
	"Pictures from your SD card",
};

/* ================================================================== */
/* motion and processing tests                                        */
/* ================================================================== */

/* fast fill of one row span straight into the picture */
static void hspan(u32 *b, int y, int x0, int x1, u32 c)
{
	if (y < 0 || y >= H) return;
	if (x0 < 0) x0 = 0; if (x1 > W) x1 = W;
	for (int x = x0; x < x1; x++) b[y * W + x] = c;
}

/* ---- scroll test: a striped city skyline that moves at a set speed ---- */
static void scroll_test(void)
{
	frame_no++;
	if (!o_scroll_pause) scroll_pos += o_scroll_spd;
	u32 *b = gfx_buf();
	int len = o_scroll_vert ? H : W;
	int p = scroll_pos % (len * 4);
	rect(0, 0, W, H, RGB(30, 40, 70));
	if (!o_scroll_vert) {
		/* 3 layers: sky stripes, buildings, a road with dashes: each moves at the same speed */
		for (int y = 0; y < H; y++) {
			u32 *row = b + y * W;
			for (int x = 0; x < W; x++) {
				int u = x + p;
				u32 c;
				if (y < H * 45 / 100) c = ((u / 16 + y / 16) & 1) ? RGB(40, 60, 110) : RGB(30, 45, 90);
				else if (y < H * 75 / 100) {
					int bw = 48, bi = (u / bw) & 7, top = H * 45 / 100 + (bi * 37 % 5) * H / 40;
					c = y < top ? RGB(30, 45, 90) : (((u % bw) < 4) ? RGB(20, 20, 30) : ((u / 8 + y / 12) & 1) && (u % bw) > 10 && (u % bw) < bw - 10 ? RGB(250, 220, 120) : RGB(70, 76, 96));
				} else c = (y > H * 86 / 100 && y < H * 89 / 100 && ((u / 40) & 1)) ? RGB(255, 255, 255) : RGB(50, 50, 56);
				row[x] = c;
			}
		}
	} else {
		for (int y = 0; y < H; y++) {
			int v = y + p;
			u32 *row = b + y * W;
			for (int x = 0; x < W; x++) {
				u32 c;
				if (x < W / 3) c = ((v / 16 + x / 16) & 1) ? RGB(40, 60, 110) : RGB(30, 45, 90);
				else if (x < W * 2 / 3) c = ((v / 32) & 1) ? RGB(230, 230, 230) : RGB(30, 30, 30);
				else c = ((v % 64) < 2) ? RGB(255, 200, 60) : RGB(50, 50, 56);
				row[x] = c;
			}
		}
	}
}

/* ---- grid scroll: a fine grid moving one way ---- */
static void grid_scroll(void)
{
	frame_no++;
	static const int DX[4] = { 1, 0, 1, -1 }, DY[4] = { 0, 1, 1, 0 };
	gs_x += DX[o_gs_dir] * o_gs_spd; gs_y += DY[o_gs_dir] * o_gs_spd;
	u32 *b = gfx_buf();
	int ox = ((gs_x % 32) + 32) % 32, oy = ((gs_y % 32) + 32) % 32;
	for (int y = 0; y < H; y++) {
		u32 *row = b + y * W;
		int line = ((y + 32 - oy) % 32) == 0;
		for (int x = 0; x < W; x++) row[x] = (line || ((x + 32 - ox) % 32) == 0) ? RGB(255, 255, 255) : 0;
	}
}

/* ---- flicker and drop shadow (240p Test Suite style) ---- */
static void sprite_test(void)
{
	frame_no++;
	u32 *b = gfx_buf();
	/* busy background: diagonal colour stripes */
	for (int y = 0; y < H; y++) { u32 *row = b + y * W; for (int x = 0; x < W; x++) row[x] = (((x + y) / 12) & 1) ? RGB(60, 110, 60) : RGB(40, 80, 150); }
	float t = frame_no * 0.03f;
	int cx = W / 2 + (int)(sinf(t) * W / 3), cy = H / 2 + (int)(sinf(t * 1.7f) * H / 4);
	int r = H / 10;
	if (o_spr_mode != 1 && (frame_no & 1)) gfx_circle(cx + r / 2, cy + r / 2, r, RGB(0, 0, 0), 255);   /* the shadow: every other frame */
	if (o_spr_mode != 1) gfx_circle(cx, cy, r, RGB(250, 200, 60), 255);
	if (o_spr_mode != 0) {
		/* striped sprite: 1-line stripes, a 480i deinterlacer shows combing on it */
		int sx0 = W - cx - r, sy0 = H - cy - r;
		for (int y = 0; y < 2 * r; y++) if (!(y & 1)) hspan(b, sy0 + y, sx0, sx0 + 2 * r, RGB(255, 255, 255));
	}
}

/* ---- diagonal lines: for deinterlacers and scalers ---- */
static void diag_test(void)
{
	frame_no++;
	rect(0, 0, W, H, grey(20));
	/* a fan of lines from one corner, so they never cross each other */
	static const int ANG[6] = { 5, 15, 30, 45, 60, 80 };
	int ox = W / 12, oy = H - H / 10, len = H * 7 / 10;
	for (int k = 0; k < 6; k++) {
		float a = ANG[k] * 3.14159265f / 180.0f;
		int ex = ox + (int)(cosf(a) * len), ey = oy - (int)(sinf(a) * len);
		gfx_line(ox, oy, ex, ey, 2, grey(230), 255);
		char t[8]; snprintf(t, sizeof t, "%d", ANG[k]);
		text(F_SMALL, ex + 6, ey - 10, t, grey(140), 255);
	}
	/* a spinning bar in the corner */
	if (o_diag_spin) diag_ang = (diag_ang + 1) % 360;
	float a = diag_ang * 3.14159265f / 180.0f;
	int cx = W * 4 / 5, cy = H / 4, l = H / 6;
	gfx_line(cx - (int)(cosf(a) * l), cy - (int)(sinf(a) * l), cx + (int)(cosf(a) * l), cy + (int)(sinf(a) * l), 3, RGB(250, 200, 60), 255);
	gfx_circle(cx, cy, 4, grey(255), 255);
}

/* ---- white clipping: the contrast twin of PLUGE ---- */
static void white_clip(void)
{
	rect(0, 0, W, H, o_clip_bg ? 0 : grey(255));
	static const int LV[8] = { 200, 215, 225, 230, 235, 240, 245, 250 };
	int bw = W / 10, bh = H / 3, y = H / 4;
	for (int k = 0; k < 8; k++) {
		int x = W / 10 + k * bw;
		rect(x + 4, y, bw - 8, bh, grey(LV[k]));
		char t[8]; snprintf(t, sizeof t, "%d", LV[k]);
		text_c(F_SMALL, x + bw / 2, y + bh + 8, t, o_clip_bg ? grey(200) : grey(90), 255);
	}
	/* a 255 strip under them to compare with */
	rect(W / 10, y + bh + 36, bw * 8, H / 10, grey(255));
	rect(W / 10, y + bh + 36, bw * 8, 1, grey(120));
}

/* ---- gamma check: lined box (true 50% light) next to solid greys ---- */
static void gamma_check(void)
{
	rect(0, 0, W, H, grey(60));
	u32 *b = gfx_buf();
	/* solid greys for gamma 1.8 .. 2.6: level that gives 50% light */
	static const int G10[5] = { 18, 20, 22, 24, 26 };
	int bw = W / 6, bh = H / 3, y = H / 3;
	for (int k = 0; k < 5; k++) {
		int lvl = (int)(255.0f * powf(0.5f, 10.0f / G10[k]) + 0.5f);
		int x = W / 12 + k * bw;
		/* middle: 1-line black/white lines (or checkerboard) */
		for (int yy = y; yy < y + bh; yy++) {
			if (yy < 0 || yy >= H) continue;
			for (int xx = x + bw / 4; xx < x + bw * 3 / 4; xx++)
				b[yy * W + xx] = (o_gamma_cmp ? ((xx + yy) & 1) : (yy & 1)) ? grey(255) : 0;
		}
		/* sides: the solid grey */
		rect(x + 4, y, bw / 4 - 4, bh, grey(lvl)); rect(x + bw * 3 / 4, y, bw / 4 - 4, bh, grey(lvl));
		char t[8]; snprintf(t, sizeof t, "%d.%d", G10[k] / 10, G10[k] % 10);
		text_c(F_SMALL, x + bw / 2, y + bh + 8, t, grey(220), 255);
	}
	text_c(F_SMALL, W / 2, y - 30, "Which box blends in best? That is your gamma (2.2 - 2.5 is normal for a CRT)", grey(220), 255);
}

/* ---- linearity: equal circles and an even grid of dots ---- */
static void linearity(void)
{
	rect(0, 0, W, H, 0);
	int step = W / 16;
	for (int y = step / 2; y < H; y += step) for (int x = step / 2; x < W; x += step) rect(x - 1, y - 1, 3, 3, grey(120));
	int r = H / 7;
	int cxs[5] = { W / 2, r + 8, W - r - 8, r + 8, W - r - 8 }, cys[5] = { H / 2, r + 8, r + 8, H - r - 8, H - r - 8 };
	for (int k = 0; k < 5; k++) {
		int rr = k ? r : H * 2 / 5;
		gfx_round_line(cxs[k] - rr, cys[k] - rr, rr * 2, rr * 2, rr, 2, grey(255), 255);
	}
	/* rulers along the middle: ticks every 1/10 */
	for (int k = 0; k <= 10; k++) {
		rect(k * (W - 1) / 10, H / 2 - 8, 1, 16, RGB(250, 200, 60));
		rect(W / 2 - 8, k * (H - 1) / 10, 16, 1, RGB(250, 200, 60));
	}
}

int pat_in_cat(int cat, int *out, int max)
{
	/* each area lists every picture useful for it (some appear in more than one area) */
	static const signed char L[CAT_COUNT][16] = {
		{ PAT_SMPTE, PAT_EBU, PAT_REFBARS, PAT_BARSGREY, PAT_CHANNEL, PAT_FIELD, PAT_RED, PAT_GREEN, PAT_BLUE, PAT_GREY50, PAT_CHECKER, PAT_RAMPS, PAT_SKIN, PAT_BLEED, PAT_CONVERGE, -1 },
		{ PAT_PLUGE, PAT_WHITECLIP, PAT_STEPS, PAT_GREY50, PAT_GAMMA, PAT_RAMP, PAT_RAMPS, PAT_IRE, PAT_FIELD, PAT_BARSGREY, PAT_REFBARS, PAT_BLOOM, PAT_WARMUP, -1 },
		{ PAT_GRID, PAT_OVERSCAN, PAT_EDGE, PAT_MONO, PAT_LINEARITY, PAT_CONVERGE, PAT_GRIDSCROLL, PAT_BLOOM, PAT_WARMUP, -1 },
		{ PAT_SHARP, PAT_MONO, PAT_STRIPES, PAT_CHECKER, PAT_BLEED, PAT_CONVERGE, PAT_DIAG, PAT_SPRITE, PAT_SCENE, -1 },
		{ PAT_SCROLL, PAT_GRIDSCROLL, PAT_SPRITE, PAT_DIAG, PAT_LAG, PAT_BLOOM, -1 },
		{ PAT_AVSYNC, PAT_SPEAKERS, PAT_SWEEP, -1 },
		{ PAT_WARMUP, PAT_WASHER, PAT_FIELD, -1 },
		{ -1 },
	};
	int n = 0;
	if (cat == CAT_USER) { for (int k = 0; k < nuser && n < max; k++) out[n++] = PAT_BUILTIN + k; return n; }
	if (cat >= CAT_COUNT) { for (int k = 0; k < PAT_BUILTIN + nuser && n < max; k++) if (k != PAT_RED && k != PAT_GREEN && k != PAT_BLUE) out[n++] = k; return n; }   /* every picture */
	for (int k = 0; k < 16 && L[cat][k] >= 0 && n < max; k++) out[n++] = L[cat][k];
	return n;
}

static void pat_draw1(int i)
{
	W = gfx_w(); H = gfx_h();
	switch (i) {
	case PAT_SCENE: scene(); break;
	case PAT_CHECKER: checker(); break;
	case PAT_RAMPS: ramps(); break;
	case PAT_EBU: ebu(); break;
	case PAT_SMPTE: smpte(); break;
	case PAT_REFBARS: refbars(); break;
	case PAT_BARSGREY: barsgrey(); break;
	case PAT_BLEED: bleed(); break;
	case PAT_PLUGE: pluge(); break;
	case PAT_STEPS: steps(); break;
	case PAT_RAMP: ramp(); break;
	case PAT_FIELD: field(); break;
	case PAT_WARMUP: warmup(); break;
	case PAT_IRE: ire(); break;
	case PAT_MONO: mono(); break;
	case PAT_GRID: grid(); break;
	case PAT_SHARP: sharp(); break;
	case PAT_OVERSCAN: overscan(); break;
	case PAT_CONVERGE: converge(); break;
	case PAT_STRIPES: stripes(); break;
	case PAT_SKIN: skin(); break;
	case PAT_BLOOM: if (!flash_ok) flash_warning(); else bloom(); break;
	case PAT_RED: rect(0, 0, W, H, RGB(255, 0, 0)); break;
	case PAT_GREEN: rect(0, 0, W, H, RGB(0, 255, 0)); break;
	case PAT_BLUE: rect(0, 0, W, H, RGB(0, 0, 255)); break;
	case PAT_GREY50: rect(0, 0, W, H, grey(128)); break;
	case PAT_CHANNEL: channel_bars(); break;
	case PAT_EDGE: frame_edge(); break;
	case PAT_WASHER: washer(); break;
	case PAT_SWEEP: sweep(); break;
	case PAT_SPEAKERS: speakers(); break;
	case PAT_AVSYNC: avsync(); break;
	case PAT_LAG: lagtest(); break;
	case PAT_SCROLL: scroll_test(); break;
	case PAT_GRIDSCROLL: grid_scroll(); break;
	case PAT_SPRITE: sprite_test(); break;
	case PAT_DIAG: diag_test(); break;
	case PAT_WHITECLIP: white_clip(); break;
	case PAT_GAMMA: gamma_check(); break;
	case PAT_LINEARITY: linearity(); break;
	default:
		if (i >= PAT_BUILTIN && i - PAT_BUILTIN < nuser) user_pic(i - PAT_BUILTIN);
	}
}

/* pictures that are mostly words: on 240p / 288p draw them at double height and squash,
   so the words keep their shape on the TV (line tests stay at the real lines) */
static int mostly_words(int i)
{
	return i == PAT_SWEEP || i == PAT_SPEAKERS || i == PAT_LAG || i == PAT_AVSYNC ||
	       ((i == PAT_BLOOM || i == PAT_WASHER) && !flash_ok);
}

void pat_draw(int i)
{
	int w = gfx_w(), h = gfx_h();
	u32 *dst = gfx_buf();
	static u32 *tall;
	if (h >= 400 || !dst || !mostly_words(i)) { pat_draw1(i); return; }
	if (!tall) tall = malloc(640 * 576 * 4);
	if (!tall) { pat_draw1(i); return; }
	gfx_target(tall, w, h * 2);
	pat_draw1(i);
	for (int y = 0; y < h; y++) {
		u32 *a0 = tall + 2 * y * w, *a1 = a0 + w, *o = dst + y * w;
		for (int x = 0; x < w; x++) { u32 p0 = a0[x], p1 = a1[x]; o[x] = p0 == p1 ? p0 : (((p0 >> 1) & 0x7F7F7F7F) + ((p1 >> 1) & 0x7F7F7F7F)); }
	}
	gfx_target(dst, w, h);
	W = w; H = h;
}
