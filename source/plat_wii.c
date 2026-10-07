/*
   plat_wii.c - Wii platform layer: video, controllers, timing, sensor bar IR.
   WiiRremote Plus by Habex. Based on WiiRremote by Abdelali221. GPL-3.0.
*/
#ifndef HOST
#include "common.h"
#include "irlow.h"
#include <wiiuse/wpad.h>
#include <fat.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/usbmouse.h>
#include <wiikeyboard/keyboard.h>
#include <asndlib.h>
#include <math.h>
#include <stdio.h>

Usb usb;

extern void __wiiuse_sensorbar_enable(int enable);

static GXRModeObj *rmode, *pref_mode;
static int cur_mode;
static u32 *xfb[2];
static int cur_fb;
static int storage;
static int bar_idle;
int plat_rumble_chan;
static u64 rumble_until[MAX_PADS];
static int want_quit, want_poweroff;

static void reset_cb(u32 irq, void *ctx) { want_quit = 1; }
static void power_cb(void) { want_poweroff = 1; }
static void wpad_power_cb(s32 chan) { want_poweroff = 1; }

int plat_quit_requested(void) { return want_quit || want_poweroff; }
int plat_poweroff_requested(void) { return want_poweroff; }

/* ---- Fakemote: Switch / PlayStation / other USB+BT pads live inside cIOS 249 ---- */
static int cios249 = -1, fakemote_on;
int plat_has_cios249(void)
{
	if (cios249 < 0) {
		cios249 = 0;
		u32 sz = 0;
		if (ES_GetStoredTMDSize(0x00000001000000F9ULL, &sz) >= 0 && sz > 0 && sz <= 0x4000) {
			static u8 buf[0x4000] ATTRIBUTE_ALIGN(32);
			if (ES_GetStoredTMD(0x00000001000000F9ULL, (signed_blob *)buf, sz) >= 0) {
				tmd *t = (tmd *)SIGNATURE_PAYLOAD((signed_blob *)buf);
				cios249 = t->title_version < 0xFF00;   /* 0xFF00+ = empty stub, not a real cIOS */
			}
		}
	}
	return cios249;
}
static int fakemote_failed;
int plat_fakemote_failed(void) { return fakemote_failed; }
#define FM_TRY "sd:/wiirremote/fakemote.try"
/* called once the app has run a few seconds: IOS 249 worked, drop the crash guard */
void plat_boot_ok(void) { if (fakemote_on) remove(FM_TRY); }
int plat_fakemote_active(void) { return fakemote_on; }

/* read one number from the config before anything else starts */
static int early_cfg(const char *key)
{
	FILE *f = fopen("sd:/wiirremote/config.txt", "r");
	if (!f) return 0;
	char line[96]; int v = 0, kl = strlen(key);
	while (fgets(line, sizeof line, f))
		if (!strncmp(line, key, kl) && line[kl] == '=') { v = atoi(line + kl + 1); break; }
	fclose(f);
	return v;
}

void plat_init(void)
{
	VIDEO_Init();
	/* Fakemote wanted and installed? switch to IOS 249 before the controllers start */
	if (fatInitDefault()) {
		FILE *g = fopen(FM_TRY, "r");
		if (g) { fclose(g); remove(FM_TRY); fakemote_failed = 1; }   /* last try never finished: stay safe */
		else if (early_cfg("fakemote") && plat_has_cios249()) {
			g = fopen(FM_TRY, "w"); if (g) { fputs("1", g); fclose(g); }
			fatUnmount("sd:"); fatUnmount("usb:");
			if (IOS_ReloadIOS(249) >= 0) fakemote_on = 1;
		}
	}
	WPAD_Init();
	PAD_Init();
	KEYBOARD_Init(NULL);
	MOUSE_Init();
	usb.mx = 320; usb.my = 240;
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, SCR_W, SCR_H);
	SYS_SetResetCallback(reset_cb);
	SYS_SetPowerCallback(power_cb);
	WPAD_SetPowerButtonCallback(wpad_power_cb);

	/* whatever the Wii is set to: PAL 50, PAL 60, NTSC or 480p */
	rmode = VIDEO_GetPreferredMode(NULL);
	pref_mode = rmode;
	VIDEO_Configure(rmode);
	/* big enough for every mode we can switch to (up to 640x576) */
	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(&TVPal576IntDfScale));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(&TVPal576IntDfScale));
	VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer(xfb[0]);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	storage = fatInitDefault();
}

int plat_storage_ok(void) { return storage; }

u64 plat_ms(void) { return ticks_to_millisecs(gettime()); }

void plat_poll(Pad pads[MAX_PADS])
{
	WPAD_ScanPads();
	u32 gc_conn = PAD_ScanPads();
	u64 now = plat_ms();
	for (int c = 0; c < MAX_PADS; c++) {
		Pad *p = &pads[c];
		memset(p, 0, sizeof *p);
		p->battery = -1;
		{
			/* GameCube controller: turn it into Classic Controller buttons */
			static const u16 gcb[12] = { PAD_BUTTON_A, PAD_BUTTON_B, PAD_BUTTON_X, PAD_BUTTON_Y, PAD_TRIGGER_L, PAD_TRIGGER_R, PAD_TRIGGER_Z,
			                             PAD_BUTTON_START, PAD_BUTTON_UP, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT };
			static const u32 ccb[12] = { CC_A, CC_B, CC_X, CC_Y, CC_L, CC_R, CC_ZR, CC_PLUS, CC_UP, CC_DOWN, CC_LEFT, CC_RIGHT };
			u16 gd = PAD_ButtonsDown(c), gh = PAD_ButtonsHeld(c);
			p->gc_connected = (gc_conn & (PAD_CHAN0_BIT >> c)) != 0;
			for (int k = 0; k < 12; k++) { if (gd & gcb[k]) p->gc_down |= ccb[k]; if (gh & gcb[k]) p->gc_held |= ccb[k]; }
			/* Start + Z together = HOME */
			if ((gh & PAD_BUTTON_START) && (gh & PAD_TRIGGER_Z)) { p->gc_held |= CC_HOME; if (gd & (PAD_BUTTON_START | PAD_TRIGGER_Z)) p->gc_down |= CC_HOME; }
		}
		u32 type;
		if (WPAD_Probe(c, &type) != WPAD_ERR_NONE) { if (rumble_until[c]) { WPAD_Rumble(c, 0); rumble_until[c] = 0; } continue; }
		p->connected = 1;
		{ int b = WPAD_BatteryLevel(c); p->battery = b > 200 ? 100 : b / 2; }
		u32 d = WPAD_ButtonsDown(c), h = WPAD_ButtonsHeld(c);
		p->wm_down = d & 0xFFFF; p->wm_held = h & 0xFFFF;
		p->cc_down = d & 0xFFFF0000u; p->cc_held = h & 0xFFFF0000u;
		WPADData *wd = WPAD_Data(c);
		if (wd) {
			int et = wd->exp.type;
			p->has_classic = et == WPAD_EXP_CLASSIC;   /* also the Wii U Pro Controller */
			p->ext = et == WPAD_EXP_NUNCHUK ? 1 : et == WPAD_EXP_CLASSIC ? 2 : et == WPAD_EXP_GUITARHERO3 ? 3 : 0;
			if (wd->ir.valid) { p->ptr_valid = 1; p->px = (int)wd->ir.x; p->py = (int)wd->ir.y; }
			p->roll = (int)wd->orient.roll; p->pitch = (int)-wd->orient.pitch;
			{ float gx = wd->gforce.x, gy = wd->gforce.y, gz = wd->gforce.z; float m = sqrtf(gx * gx + gy * gy + gz * gz); p->shake = (int)(fabsf(m - 1.0f) * 100); }
			if (et == WPAD_EXP_NUNCHUK) {
				/* C / Z into spare Wii Remote bits, stick = D-pad */
				u32 cd = 0, ch = 0;
				if (d & WPAD_NUNCHUK_BUTTON_Z) cd |= BTN_NC_Z;
				if (d & WPAD_NUNCHUK_BUTTON_C) cd |= BTN_NC_C;
				if (h & WPAD_NUNCHUK_BUTTON_Z) ch |= BTN_NC_Z;
				if (h & WPAD_NUNCHUK_BUTTON_C) ch |= BTN_NC_C;
				static u32 stick_prev[MAX_PADS];
				u32 st = 0;
				float mag = wd->exp.nunchuk.js.mag, ang = wd->exp.nunchuk.js.ang;
				if (mag > 0.55f) {
					if (ang >= 315 || ang < 45) st = BTN_UP;
					else if (ang < 135) st = BTN_RIGHT;
					else if (ang < 225) st = BTN_DOWN;
					else st = BTN_LEFT;
				}
				cd |= st & ~stick_prev[c]; ch |= st;
				stick_prev[c] = st;
				p->wm_down |= cd; p->wm_held |= ch;
				p->cc_down = p->cc_held = 0;
			} else if (et == WPAD_EXP_GUITARHERO3) {
				/* guitar: frets / strum / +- become Classic buttons */
				static const u32 gb[9] = { WPAD_GUITAR_HERO_3_BUTTON_GREEN, WPAD_GUITAR_HERO_3_BUTTON_RED, WPAD_GUITAR_HERO_3_BUTTON_YELLOW,
				                           WPAD_GUITAR_HERO_3_BUTTON_BLUE, WPAD_GUITAR_HERO_3_BUTTON_ORANGE, WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP,
				                           WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN, WPAD_GUITAR_HERO_3_BUTTON_PLUS, WPAD_GUITAR_HERO_3_BUTTON_MINUS };
				static const u32 cb[9] = { CC_A, CC_B, CC_X, CC_Y, CC_L, CC_UP, CC_DOWN, CC_PLUS, CC_MINUS };
				u32 od = 0, oh = 0;
				for (int k = 0; k < 9; k++) { if (d & gb[k]) od |= cb[k]; if (h & gb[k]) oh |= cb[k]; }
				p->cc_down = od; p->cc_held = oh;
				p->has_classic = 1;
			} else if (et != WPAD_EXP_CLASSIC) {
				p->cc_down = p->cc_held = 0;
			}
		}
		if (rumble_until[c] && now >= rumble_until[c]) { WPAD_Rumble(c, 0); rumble_until[c] = 0; }
	}

	/* Balance Board: lean = direction */
	{
		u32 type;
		static u32 bb_prev;
		if (WPAD_Probe(WPAD_BALANCE_BOARD, &type) == WPAD_ERR_NONE) {
			WPADData *wd = WPAD_Data(WPAD_BALANCE_BOARD);
			if (wd && wd->exp.type == WPAD_EXP_WIIBOARD) {
				float tl = wd->exp.wb.tl, tr = wd->exp.wb.tr, bl = wd->exp.wb.bl, br = wd->exp.wb.br;
				float tot = tl + tr + bl + br;
				u32 st = 0;
				if (tot > 10.0f) {
					float x = (tr + br - tl - bl) / tot, y = (tl + tr - bl - br) / tot;
					if (y > 0.35f) st = CC_UP; else if (y < -0.35f) st = CC_DOWN;
					else if (x > 0.35f) st = CC_RIGHT; else if (x < -0.35f) st = CC_LEFT;
				}
				pads[0].bb_connected = 1;
				pads[0].bb_down = st & ~bb_prev; pads[0].bb_held = st;
				bb_prev = st;
			}
		}
	}

	/* USB keyboard */
	usb.nkeys = 0;
	keyboard_event ke;
	while (KEYBOARD_GetEvent(&ke) > 0) {
		if (ke.type == KEYBOARD_CONNECTED) usb.kb_connected = 1;
		else if (ke.type == KEYBOARD_DISCONNECTED) usb.kb_connected = 0;
		else if (ke.type == KEYBOARD_PRESSED && usb.nkeys < KB_MAX_EVENTS) {
			usb.kb_connected = 1;
			usb.keys[usb.nkeys] = ke.keycode;
			usb.syms[usb.nkeys] = ke.symbol;
			usb.nkeys++;
		}
	}

	/* USB mouse */
	usb.mouse_connected = MOUSE_IsConnected();
	u32 prev = usb.mouse_held;
	usb.wheel = 0;
	mouse_event me;
	while (MOUSE_GetEvent(&me)) {
		usb.mx += me.rx; usb.my += me.ry;
		if (usb.mx < 0) usb.mx = 0; if (usb.mx > 639) usb.mx = 639;
		if (usb.my < 0) usb.my = 0; if (usb.my > 479) usb.my = 479;
		if (me.rx || me.ry) usb.mouse_moved_ms = now;
		usb.mouse_held = me.button & 7;
		usb.wheel += me.rz;
	}
	usb.mouse_down = usb.mouse_held & ~prev;
}

/* ---- TV beep (for the audio sync and lag tests) ---- */
#define BEEP_SAMPLES 2400   /* 50 ms at 48 kHz */
static s16 beep_lo[BEEP_SAMPLES * 2] ATTRIBUTE_ALIGN(32);
static s16 beep_hi[BEEP_SAMPLES * 2] ATTRIBUTE_ALIGN(32);
static int snd_ready;
static void make_tone(s16 *b, int hz)
{
	for (int i = 0; i < BEEP_SAMPLES; i++) {
		float env = i < 48 ? i / 48.0f : (i > BEEP_SAMPLES - 240 ? (BEEP_SAMPLES - i) / 240.0f : 1.0f);
		s16 v = (s16)(sinf(i * 2.0f * 3.14159265f * hz / 48000.0f) * 20000 * env);
		b[i * 2] = b[i * 2 + 1] = v;
	}
	DCFlushRange(b, sizeof beep_lo);
}
void plat_beep(int high)
{
	if (!snd_ready) { ASND_Init(); ASND_Pause(0); make_tone(beep_lo, 1000); make_tone(beep_hi, 2000); snd_ready = 1; }
	s32 v = ASND_GetFirstUnusedVoice();
	if (v < 0) v = 0;
	ASND_SetVoice(v, VOICE_STEREO_16BIT, 48000, 0, high ? beep_hi : beep_lo, sizeof beep_lo, 255, 255, NULL);
}

/* left / middle / right beep for the speaker test */
void plat_beep_pan(int pan)
{
	if (!snd_ready) { ASND_Init(); ASND_Pause(0); make_tone(beep_lo, 1000); make_tone(beep_hi, 2000); snd_ready = 1; }
	s32 v = ASND_GetFirstUnusedVoice();
	if (v < 0) v = 0;
	int l = pan == 2 ? 0 : 255, r = pan == 0 ? 0 : 255;
	ASND_SetVoice(v, VOICE_STEREO_16BIT, 48000, 0, beep_lo, sizeof beep_lo, l, r, NULL);
}

/* ---- steady tone for the rattle tester: two small buffers refilled by ASND ---- */
#define TONE_N 1024
static s16 tone_buf[2][TONE_N * 2] ATTRIBUTE_ALIGN(32);
static volatile int tone_hz;
static int tone_voice = -1, tone_k;
static float tone_phase;
static void tone_fill(s16 *b)
{
	int hz = tone_hz; /* read once: main thread may change it mid-fill */
	float step = 2.0f * 3.14159265f * hz / 48000.0f;
	for (int i = 0; i < TONE_N; i++) {
		s16 v = (s16)(sinf(tone_phase) * 14000);
		b[i * 2] = b[i * 2 + 1] = v;
		tone_phase += step; if (tone_phase > 6.2831853f) tone_phase -= 6.2831853f;
	}
	DCFlushRange(b, sizeof tone_buf[0]);
}
static void tone_cb(s32 voice)
{
	if (!tone_hz) return;
	tone_k ^= 1;
	tone_fill(tone_buf[tone_k]);
	ASND_AddVoice(voice, tone_buf[tone_k], sizeof tone_buf[0]);
}
void plat_tone(int hz)
{
	if (!snd_ready) { if (!hz) return; ASND_Init(); ASND_Pause(0); make_tone(beep_lo, 1000); make_tone(beep_hi, 2000); snd_ready = 1; }
	tone_hz = hz;
	if (!hz) { if (tone_voice >= 0) { ASND_StopVoice(tone_voice); tone_voice = -1; } return; }
	if (tone_voice < 0) {
		tone_voice = ASND_GetFirstUnusedVoice();
		if (tone_voice < 0) return;
		tone_k = 0; tone_fill(tone_buf[0]);
		ASND_SetVoice(tone_voice, VOICE_STEREO_16BIT, 48000, 0, tone_buf[0], sizeof tone_buf[0], 200, 200, tone_cb);
	}
}

/* ---- Wii Remote speaker click (experimental) ---- */
static int spk_want;
void plat_speaker(int on)
{
	spk_want = on;
	for (int c = 0; c < MAX_PADS; c++) {
		u32 t; if (WPAD_Probe(c, &t) != WPAD_ERR_NONE) continue;
		WPAD_ControlSpeaker(c, on);
	}
}
/* ---- casino music: the synth fills a ring of buffers in the main loop, ASND plays them in order ---- */
#define MB_N 5
#define MB_F 1024
static s16 mbuf[MB_N][MB_F * 2] ATTRIBUTE_ALIGN(32);
static s16 msil[MB_F * 2] ATTRIBUTE_ALIGN(32);
static volatile u8 mb_full[MB_N];
static volatile int mb_rd, mb_old1 = -1, mb_old2 = -1;
static int mb_wr, mb_voice = -1, mb_on;
static void mus_cb(s32 voice)
{
	/* the buffer queued two turns ago has finished: it can be filled again */
	if (mb_old2 >= 0) mb_full[mb_old2] = 0;
	mb_old2 = mb_old1;
	if (mb_full[mb_rd]) { mb_old1 = mb_rd; ASND_AddVoice(voice, mbuf[mb_rd], sizeof mbuf[0]); mb_rd = (mb_rd + 1) % MB_N; }
	else { mb_old1 = -1; ASND_AddVoice(voice, msil, sizeof msil); }
}
void plat_music_pump(void)
{
	if (!mb_on) return;
	for (int k = 0; k < MB_N; k++) {
		if (mb_full[mb_wr]) break;
		music_render(mbuf[mb_wr], MB_F, 48000);
		DCFlushRange(mbuf[mb_wr], sizeof mbuf[0]);
		mb_full[mb_wr] = 1;
		mb_wr = (mb_wr + 1) % MB_N;
	}
}
void plat_music(int on)
{
	if (on == mb_on) return;
	if (!snd_ready) { if (!on) return; ASND_Init(); ASND_Pause(0); make_tone(beep_lo, 1000); make_tone(beep_hi, 2000); snd_ready = 1; }
	if (!on) {
		if (mb_voice >= 0) ASND_StopVoice(mb_voice);
		mb_voice = -1; mb_on = 0;
		return;
	}
	memset((void *)mb_full, 0, sizeof mb_full); memset(msil, 0, sizeof msil); DCFlushRange(msil, sizeof msil);
	mb_rd = mb_wr = 0; mb_old1 = mb_old2 = -1;
	mb_on = 1;
	plat_music_pump();
	mb_voice = ASND_GetFirstUnusedVoice();
	if (mb_voice < 0) { mb_on = 0; return; }
	mb_old1 = 0; mb_rd = 1;
	ASND_SetVoice(mb_voice, VOICE_STEREO_16BIT, 48000, 0, mbuf[0], sizeof mbuf[0], 255, 255, mus_cb);
}

void plat_click(void)
{
	if (!spk_want) return;
	int c = plat_rumble_chan;
	if (c < 0 || c >= MAX_PADS) c = 0;
	if (WPAD_IsSpeakerEnabled(c) <= 0) {
		static u64 asked[MAX_PADS];
		u64 now = plat_ms();
		if (now - asked[c] > 2000) { asked[c] = now; WPAD_ControlSpeaker(c, 1); }   /* wakes up; a later press clicks */
		return;
	}
	static s16 pcm[80];
	for (int i = 0; i < 80; i++) pcm[i] = (i < 40) ? (s16)((i & 4 ? 1 : -1) * 16000 * (40 - i) / 40) : 0;
	static WPADEncStatus st;
	static u8 enc[40];
	memset(&st, 0, sizeof st);
	WPAD_EncodeData(&st, 0, pcm, 40, enc);
	WPAD_EncodeData(&st, 1, pcm + 40, 40, enc + 20);
	WPAD_SendStreamData(c, enc, 20);
	WPAD_SendStreamData(c, enc + 20, 20);
}

void plat_rumble(int ms)
{
	int c = plat_rumble_chan;
	if (c < 0 || c >= MAX_PADS) c = 0;
	WPAD_Rumble(c, 1);
	rumble_until[c] = plat_ms() + ms;
}

/* RGB buffer -> YCbCr 4:2:2 framebuffer (BT.601, studio range 16-235) */
int plat_deflicker;
static inline u32 blend121(u32 a, u32 b, u32 c)
{
	u32 rb = ((a & 0xFF00FF) + 2 * (b & 0xFF00FF) + (c & 0xFF00FF)) >> 2;
	u32 g = ((a & 0xFF00) + 2 * (b & 0xFF00) + (c & 0xFF00)) >> 2;
	return (rb & 0xFF00FF) | (g & 0xFF00);
}

static void present_buf(const u32 *cv, int sw, int sh)
{
	cur_fb ^= 1;
	u32 *dst = xfb[cur_fb];
	int W = rmode->fbWidth, H = rmode->xfbHeight;
	for (int y = 0; y < H; y++) {
		int sy = sh == H ? y : y * sh / H;
		const u32 *row = cv + sy * sw;
		/* flicker filter = the Wii's standard 8,8,10,12,10,8,8 taps = lines 1:2:1 */
		const u32 *up = cv + (sy > 0 ? sy - 1 : 0) * sw, *dn = cv + (sy < sh - 1 ? sy + 1 : sh - 1) * sw;
		u32 *out = dst + y * (W / 2);
		for (int x = 0; x < W; x += 2) {
			u32 p1, p2;
			int i1 = W == sw ? x : (x * sw) / W, i2 = W == sw ? x + 1 : ((x + 1) * sw) / W;
			if (plat_deflicker) { p1 = blend121(up[i1], row[i1], dn[i1]); p2 = blend121(up[i2], row[i2], dn[i2]); }
			else { p1 = row[i1]; p2 = row[i2]; }
			int r1 = p1 >> 16, g1 = (p1 >> 8) & 255, b1 = p1 & 255;
			int r2 = p2 >> 16, g2 = (p2 >> 8) & 255, b2 = p2 & 255;
			int y1 = ((66 * r1 + 129 * g1 + 25 * b1 + 128) >> 8) + 16;
			int y2 = ((66 * r2 + 129 * g2 + 25 * b2 + 128) >> 8) + 16;
			int r = (r1 + r2) >> 1, g = (g1 + g2) >> 1, b = (b1 + b2) >> 1;
			int cb = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
			int cr = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
			out[x / 2] = ((u32)y1 << 24) | ((u32)cb << 16) | ((u32)y2 << 8) | (u32)cr;
		}
	}
	DCFlushRange(dst, W * H * 2);
	VIDEO_SetNextFramebuffer(dst);
	VIDEO_Flush();
}

void plat_present(const u32 *cv) { present_buf(cv, SCR_W, SCR_H); }
void plat_present_native(const u32 *buf) { present_buf(buf, rmode->fbWidth, rmode->xfbHeight); }
int plat_native_w(void) { return rmode->fbWidth; }
int plat_native_h(void) { return rmode->xfbHeight; }

static const char *MODE_NAMES[] = {
	"Wii setting", "480i 60Hz", "480p 60Hz", "240p 60Hz", "480i PAL60", "240p PAL60", "576i 50Hz", "288p 50Hz",
};
static GXRModeObj *mode_obj(int i)
{
	switch (i) {
		case 1: return &TVNtsc480IntDf;
		case 2: return &TVNtsc480Prog;
		case 3: return &TVNtsc240Ds;
		case 4: return &TVEurgb60Hz480IntDf;
		case 5: return &TVEurgb60Hz240Ds;
		case 6: return &TVPal576IntDfScale;
		case 7: return &TVPal264Ds;
	}
	return pref_mode;
}

int plat_video_modes(const char **names, int max)
{
	int n = sizeof MODE_NAMES / sizeof MODE_NAMES[0];
	for (int i = 0; i < n && i < max; i++) names[i] = MODE_NAMES[i];
	return n;
}

int plat_video_current(void) { return cur_mode; }

int plat_video_set(int i)
{
	GXRModeObj *m = mode_obj(i);
	if (!m) return 0;
	if (i == 2 && !VIDEO_HaveComponentCable()) return 0;   /* 480p needs component */
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	rmode = m;
	VIDEO_Configure(rmode);
	VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer(xfb[cur_fb]);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
	cur_mode = i;
	return 1;
}

int plat_video_info(char L[][72], int max)
{
	int n = 0;
	int fmt = rmode->viTVMode >> 2, mode = rmode->viTVMode & 3;
	const char *scan = mode == VI_PROGRESSIVE ? "progressive" : mode == VI_NON_INTERLACE ? "non-interlaced (240p/288p style)" : "interlaced";
	int hz50 = fmt == VI_PAL;
	if (n < max) snprintf(L[n++], 72, "Output: %s  -  %s", plat_video_mode(), scan);
	if (n < max) snprintf(L[n++], 72, "Picture: %d x %d, %s", rmode->fbWidth, rmode->xfbHeight, hz50 ? "50 Hz" : "60 Hz (59.94)");
	if (n < max) snprintf(L[n++], 72, "Cable: %s", VIDEO_HaveComponentCable() ? "Component (YPbPr)" : "AV: composite, S-Video or RGB SCART");
	int v = CONF_GetVideo();
	if (n < max) snprintf(L[n++], 72, "Wii setting: %s, PAL60 %s, 480p %s, %s", v == CONF_VIDEO_PAL ? "PAL" : v == CONF_VIDEO_MPAL ? "PAL-M" : "NTSC",
	                      CONF_GetEuRGB60() > 0 ? "on" : "off", CONF_GetProgressiveScan() > 0 ? "on" : "off",
	                      CONF_GetAspectRatio() == CONF_ASPECT_16_9 ? "16:9" : "4:3");
	if (n < max) snprintf(L[n++], 72, plat_deflicker ? "Flicker filter: ON - soft, like Wii games" : "Flicker filter: OFF - pixels go to the TV untouched");
	if (n < max) snprintf(L[n++], 72, "Colour: YCbCr, black 16 / white 235 (BT.601)");
	return n;
}

const char *plat_video_mode(void)
{
	static char m[48];
	int fmt = rmode->viTVMode >> 2, mode = rmode->viTVMode & 3;
	const char *cable = VIDEO_HaveComponentCable() ? "Component" : "AV";
	const char *hz = fmt == VI_PAL ? "50Hz" : "60Hz";
	if (mode == VI_PROGRESSIVE) snprintf(m, sizeof m, "%dp %s  -  %s", rmode->xfbHeight, hz, cable);
	else if (mode == VI_NON_INTERLACE) snprintf(m, sizeof m, "%dp %s  -  %s", rmode->xfbHeight, hz, cable);
	else if (fmt == VI_PAL) snprintf(m, sizeof m, "576i PAL 50Hz  -  %s", cable);
	else if (fmt == VI_EURGB60) snprintf(m, sizeof m, "480i PAL 60Hz  -  %s", cable);
	else if (fmt == VI_MPAL) snprintf(m, sizeof m, "480i PAL-M  -  %s", cable);
	else snprintf(m, sizeof m, "480i NTSC  -  %s", cable);
	return m;
}

void plat_wait_frame(void) { VIDEO_WaitVSync(); }

#include <time.h>
void plat_clock(char *buf, int n)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	if (!tm || t < 100000) { buf[0] = 0; return; }
	int h = tm->tm_hour % 12; if (!h) h = 12;
	snprintf(buf, n, "%d:%02d %s", h, tm->tm_min, tm->tm_hour < 12 ? "AM" : "PM");
}

/* ---------------- sensor bar IR ---------------- */

static u64 cursor, carrier_period, carrier_on;

static inline u64 ns_to_ticks(u64 ns) { return (ns * (u64)PPC_TIMER_CLOCK) / 1000000000ULL; }
static inline void wait_until(u64 t) { while (gettime() < t) { } }

void plat_sensorbar_idle(int on) { bar_idle = on; __wiiuse_sensorbar_enable(on); }
void irlow_idle(void) { __wiiuse_sensorbar_enable(bar_idle); }
u32  irlow_lock(void) { return IRQ_Disable(); }
void irlow_unlock(u32 lvl) { IRQ_Restore(lvl); }

void irlow_begin(u32 hz)
{
	carrier_period = ns_to_ticks(1000000000ULL / hz);
	carrier_on = carrier_period / 3;
	__wiiuse_sensorbar_enable(0);
	/* let the TV's receiver settle if the bar was lit for the pointer */
	cursor = gettime() + ns_to_ticks(bar_idle ? 2000000 : 20000);
	wait_until(cursor);
}

void irlow_mark(u32 us)
{
	u64 end = cursor + ns_to_ticks((u64)us * 1000ULL), t = cursor;
	while (t < end) {
		wait_until(t);
		__wiiuse_sensorbar_enable(1);
		u64 off = t + carrier_on; if (off > end) off = end;
		wait_until(off);
		__wiiuse_sensorbar_enable(0);
		t += carrier_period;
	}
	cursor = end;
}

void irlow_space(u32 us)
{
	__wiiuse_sensorbar_enable(0);
	cursor += ns_to_ticks((u64)us * 1000ULL);
	wait_until(cursor);
}

void irlow_end(void) { __wiiuse_sensorbar_enable(0); }

/* ---- clean exit: stop everything that runs in the background before the Homebrew Channel
   loads over our memory. A sound callback, the USB keyboard/mouse threads or the Wii Remote
   speaker left running = the Wii freezes on exit. ---- */
void plat_shutdown(void)
{
	if (snd_ready) {
		tone_hz = 0;
		if (tone_voice >= 0) { ASND_StopVoice(tone_voice); tone_voice = -1; }
		if (mb_voice >= 0) { ASND_StopVoice(mb_voice); mb_voice = -1; mb_on = 0; }
		ASND_Pause(1);
		ASND_End();
		snd_ready = 0;
	}
	for (int c = 0; c < MAX_PADS; c++) {
		u32 t;
		if (WPAD_Probe(c, &t) != WPAD_ERR_NONE) continue;
		WPAD_Rumble(c, 0);
		if (spk_want) WPAD_ControlSpeaker(c, 0);
	}
	PAD_ControlMotor(PAD_CHAN0, PAD_MOTOR_STOP); PAD_ControlMotor(PAD_CHAN1, PAD_MOTOR_STOP);
	PAD_ControlMotor(PAD_CHAN2, PAD_MOTOR_STOP); PAD_ControlMotor(PAD_CHAN3, PAD_MOTOR_STOP);
	KEYBOARD_Deinit();
	MOUSE_Deinit();
	if (fakemote_on) remove(FM_TRY);
	fatUnmount("sd:"); fatUnmount("usb:");
	/* back to the picture mode the Wii was in, blanked, so the loader starts clean */
	VIDEO_SetBlack(true);
	if (pref_mode) VIDEO_Configure(pref_mode);
	VIDEO_Flush();
	VIDEO_WaitVSync(); VIDEO_WaitVSync();
	extern void __wiiuse_sensorbar_enable(int enable);
	__wiiuse_sensorbar_enable(1);   /* leave the bar on for the Homebrew Channel pointer */
	WPAD_Flush(WPAD_CHAN_ALL);
	WPAD_Shutdown();
}

/* ---- for the casino dealer: your Wii's name, its save files, and the games on your SD card ---- */
#include <ogc/isfs.h>
#include <ogc/conf.h>
#include <dirent.h>
#include <malloc.h>
#include <strings.h>

static void utf16_ascii(const u8 *b, int chars, char *out, int n)
{
	int k = 0;
	for (int c = 0; c < chars && k < n - 1; c++) {
		u16 ch = (b[c * 2] << 8) | b[c * 2 + 1];
		if (!ch) break;
		if (ch >= 32 && ch < 127) out[k++] = (char)ch;
	}
	out[k] = 0;
}

/* read part of a NAND file (0 = worked) */
static int nand_read(const char *path, int off, void *dst, int len)
{
	s32 fd = ISFS_Open(path, ISFS_OPEN_READ);
	if (fd < 0) return -1;
	int ok = -1;
	u8 *b = memalign(32, (len + 31) & ~31);
	if (b && (off == 0 || ISFS_Seek(fd, off, 0) >= 0) && ISFS_Read(fd, b, len) == len) { memcpy(dst, b, len); ok = 0; }
	if (b) free(b);
	ISFS_Close(fd);
	return ok;
}

/* a few well known disc IDs, for saves without a readable banner */
static const char *known_id(const char *id)
{
	static const char *T[][2] = {
		{ "RMC", "Mario Kart Wii" }, { "RSB", "Super Smash Bros. Brawl" }, { "SMN", "New Super Mario Bros. Wii" },
		{ "RMG", "Super Mario Galaxy" }, { "SB4", "Super Mario Galaxy 2" }, { "RZD", "Twilight Princess" },
		{ "SOU", "Skyward Sword" }, { "RSP", "Wii Sports" }, { "RZT", "Wii Sports Resort" }, { "RHA", "Wii Play" },
		{ "SF8", "Donkey Kong Country Returns" }, { "R8P", "Super Paper Mario" }, { "RFN", "Wii Fit" }, { "RFP", "Wii Fit Plus" },
		{ "RMH", "Monster Hunter Tri" }, { "SX4", "Xenoblade Chronicles" }, { "R3M", "Metroid Prime Trilogy" },
		{ "RM3", "Metroid Prime 3" }, { "SUK", "Kirby's Return to Dream Land" }, { "RUU", "Animal Crossing: City Folk" },
		{ "RGH", "Guitar Hero III" }, { "SMG", "Mario Party 9" }, { "RM8", "Mario Party 8" }, { "SJB", "Just Dance" },
		{ "RB4", "Resident Evil 4" }, { "SMO", "Metroid: Other M" }, { "RPB", "Pokemon Battle Revolution" },
		{ "SOT", "Punch-Out!!" }, { "RWS", "Mario & Sonic at the Olympic Games" }, { "RPJ", "PokePark Wii" },
	};
	for (size_t k = 0; k < sizeof T / sizeof T[0]; k++) if (!strncmp(id, T[k][0], 3)) return T[k][1];
	return NULL;
}

static int ext_is(const char *n, const char *const *list)
{
	const char *d = strrchr(n, '.'); if (!d) return 0;
	for (int i = 0; list[i]; i++) if (!strcasecmp(d + 1, list[i])) return 1;
	return 0;
}
static void clean_title(const char *in, char *out, int n)
{
	snprintf(out, n, "%s", in);
	char *b = strchr(out, '['); if (b) *b = 0;
	char *d = strrchr(out, '.'); if (d && d != out) *d = 0;
	int l = strlen(out); while (l && (out[l - 1] == ' ' || out[l - 1] == '_')) out[--l] = 0;
	for (char *c = out; *c; c++) if (*c == '_') *c = ' ';
}
static int bcd(u8 v) { return (v >> 4) * 10 + (v & 15); }

void plat_dossier(Dossier *d)
{
	/* read fresh each time, straight into the caller's copy: nothing is kept here, nothing is written, no network */
	Dossier D;
	memset(&D, 0, sizeof D);
	/* console name and region (SYSCONF) */
	{
		u8 raw[0x40] ATTRIBUTE_ALIGN(32); memset(raw, 0, sizeof raw);
		if (CONF_GetNickName(raw) > 0) utf16_ascii(raw, 10, D.nick, sizeof D.nick);
		static const char *AR[] = { "Japanese", "American", "European", "Australian", "Brazilian", "Taiwanese", "Taiwanese", "Korean",
			"Hong Kong", "Asian", "Latin American", "South African", "Chinese" };
		s32 a = CONF_GetArea(); if (a >= 0 && a < 13) snprintf(D.region, sizeof D.region, "%s", AR[a]);
	}
	D.cios = plat_has_cios249();
	if (ISFS_Initialize() >= 0) {
		/* Miis: /shared2/menu/FaceLib/RFL_DB.dat = "RNOD" + 100 Miis of 0x4A bytes */
		{
			u8 *m = memalign(32, 4 + 100 * 0x4A + 32);
			if (m && nand_read("/shared2/menu/FaceLib/RFL_DB.dat", 0, m, 4 + 100 * 0x4A) == 0 && !memcmp(m, "RNOD", 4)) {
				int best = -1;
				for (int i = 0; i < 100; i++) {
					u8 *e = m + 4 + i * 0x4A;
					if (!e[2] && !e[3]) continue;   /* empty slot */
					D.miis++;
					char nm[24]; utf16_ascii(e + 2, 10, nm, sizeof nm);
					if (best < 0 || (D.nick[0] && !strcasecmp(nm, D.nick))) best = i;
				}
				if (best >= 0) {
					u8 *e = m + 4 + best * 0x4A;
					u16 f = (e[0] << 8) | e[1];
					D.mii_found = 1; D.mii_female = (f >> 14) & 1; D.mii_bmonth = (f >> 10) & 15; D.mii_bday = (f >> 5) & 31;
					D.mii_height = e[0x16];
					utf16_ascii(e + 2, 10, D.mii_name, sizeof D.mii_name);
				}
			}
			if (m) free(m);
		}
		/* internet: /shared2/sys/net/02/config.dat, 3 connections of 0x91C bytes after an 8 byte header */
		{
			u8 *c = memalign(32, 8 + 3 * 0x91C + 32);
			if (c && nand_read("/shared2/sys/net/02/config.dat", 0, c, 8 + 3 * 0x91C) == 0) {
				for (int i = 0; i < 3; i++) {
					u8 *e = c + 8 + i * 0x91C;
					if (!e[0]) continue;
					D.online = 1;
					int l = e[0x7E4]; if (l > 32) l = 32;
					if (l && (e[0] & 0x80)) { memcpy(D.ssid, e + 0x7C4, l); D.ssid[l] = 0; for (int k = 0; k < l; k++) if (D.ssid[k] < 32 || D.ssid[k] > 126) D.ssid[k] = '?'; }
				}
			}
			if (c) free(c);
		}
		/* the last thing played: Wii Menu's play_rec.dat (name, start, last update) */
		{
			u8 r[0x70];
			if (nand_read("/title/00000001/00000002/data/play_rec.dat", 0, r, 0x70) == 0) {
				utf16_ascii(r + 4, 0x28, D.last_played, sizeof D.last_played);
				u64 t0 = 0, t1 = 0; for (int k = 0; k < 8; k++) { t0 = (t0 << 8) | r[0x54 + k]; t1 = (t1 << 8) | r[0x5C + k]; }
				if (t1 > t0) { u64 mins = (t1 - t0) / 60750000ULL / 60; if (mins < 100000) D.last_minutes = (int)mins; }
			}
		}
		/* saved games: /title/00010000/<id>/data/banner.bin */
		u32 num = 0;
		if (ISFS_ReadDir("/title/00010000", NULL, &num) >= 0 && num > 0 && num < 400) {
			char *list = memalign(32, (num * 13 + 31) & ~31);
			if (list && ISFS_ReadDir("/title/00010000", list, &num) >= 0) {
				char *e = list;
				for (u32 i = 0; i < num; i++, e += strlen(e) + 1) {
					char id[5] = { 0 };
					for (int b = 0; b < 4; b++) { unsigned v = 0; sscanf(e + b * 2, "%2x", &v); id[b] = (v >= 32 && v < 127) ? (char)v : '?'; }
					char path[96]; snprintf(path, sizeof path, "/title/00010000/%s/data/banner.bin", e);
					u8 b[0xA0]; char name[40] = "";
					if (nand_read(path, 0, b, 0xA0) != 0) continue;   /* no save data */
					if (!memcmp(b, "WIBN", 4)) utf16_ascii(b + 0x20, 32, name, sizeof name);
					if (!name[0]) { const char *kn = known_id(id); if (kn) snprintf(name, sizeof name, "%s", kn); }
					D.wii_saves++;
					if (name[0] && D.nsave_named < 8) snprintf(D.save_names[D.nsave_named++], 40, "%s", name);
					/* Wii Fit profiles: name, height (cm), birthday */
					if (!D.fit_found && (!strncmp(id, "RFN", 3) || !strncmp(id, "RFP", 3))) {
						static const char *FF[] = { "FitPlus0.dat", "RPWiiFit0.dat" };
						for (int ff = 0; ff < 2 && !D.fit_found; ff++) {
							snprintf(path, sizeof path, "/title/00010000/%s/data/%s", e, FF[ff]);
							for (int pr = 0; pr < 8; pr++) {
								u8 h[0x28];
								if (nand_read(path, 8 + pr * 0x9281A, h, 0x28) != 0) break;
								char fn[24]; utf16_ascii(h + 8, 11, fn, sizeof fn);
								int cm = h[0x1F], yr = bcd(h[0x20]) * 100 + bcd(h[0x21]);
								if (!fn[0] || cm < 90 || cm > 230 || yr < 1920 || yr > 2025) continue;
								D.fit_profiles++;
								if (!D.fit_found || (D.nick[0] && !strcasecmp(fn, D.nick))) {
									D.fit_found = 1; D.fit_height_cm = cm; snprintf(D.fit_name, sizeof D.fit_name, "%s", fn);
									int mo = bcd(h[0x22]), dy = bcd(h[0x23]);
									time_t t = time(NULL); struct tm *tm = localtime(&t);
									if (tm && tm->tm_year > 100) { int age = tm->tm_year + 1900 - yr; if (tm->tm_mon + 1 < mo || (tm->tm_mon + 1 == mo && tm->tm_mday < dy)) age--; if (age > 0 && age < 110) D.fit_age = age; }
								}
							}
						}
					}
				}
			}
			if (list) free(list);
		}
		ISFS_Deinitialize();
	}
	/* SD / USB: Wii backups (wbfs), GameCube games (Nintendont games/), memory card saves, old console ROMs */
	{
		static const char *const WG[] = { "wbfs", "iso", "ciso", "rvz", "wia", NULL };
		static const char *const GG[] = { "iso", "gcm", "ciso", "rvz", "gcz", NULL };
		static const char *const ROM[] = { "nes", "sfc", "smc", "gb", "gbc", "gba", "n64", "z64", "v64", "sms", "gen", "md", "pce", NULL };
		static const char *const ROOTS[] = { "sd:", "usb:" };
		for (int ro = 0; ro < 2; ro++) {
			for (int g = 0; g < 2; g++) {
				char p[64]; snprintf(p, sizeof p, "%s/%s", ROOTS[ro], g ? "games" : "wbfs");
				DIR *dd = opendir(p); if (!dd) continue;
				struct dirent *e;
				while ((e = readdir(dd))) {
					if (e->d_name[0] == '.') continue;
					if (!(ext_is(e->d_name, g ? GG : WG) || strchr(e->d_name, '['))) continue;
					if (g) D.gc_backups++; else D.wii_backups++;
					if (D.nbackup < 8) clean_title(e->d_name, D.backup_names[D.nbackup++], 40);
				}
				closedir(dd);
			}
			char p[64]; snprintf(p, sizeof p, "%s/saves", ROOTS[ro]);   /* Nintendont memory cards */
			DIR *dd = opendir(p);
			if (dd) { struct dirent *e; while ((e = readdir(dd))) if (e->d_name[0] != '.') D.gc_saves++; closedir(dd); }
			snprintf(p, sizeof p, "%s/roms", ROOTS[ro]);
			dd = opendir(p);
			if (dd) {
				struct dirent *e;
				while ((e = readdir(dd))) {
					if (e->d_name[0] == '.') continue;
					if (ext_is(e->d_name, ROM)) { D.roms++; continue; }
					char sub[160]; snprintf(sub, sizeof sub, "%s/%s", p, e->d_name);
					DIR *d2 = opendir(sub);
					if (d2) { struct dirent *e2; while ((e2 = readdir(d2))) if (ext_is(e2->d_name, ROM)) D.roms++; closedir(d2); }
				}
				closedir(dd);
			}
		}
	}
	*d = D;
	memset(&D, 0, sizeof D);
}
#endif
