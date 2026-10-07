/* Web (browser) platform layer: the real app code, compiled to WebAssembly,
   so the menus can be tried on a PC. No IR is sent - a light on the page shows when it would be. */
#include "common.h"
#include "irlow.h"
#include "gfx.h"
#include "app.h"
#include <time.h>

#define IMPORT(n) __attribute__((import_module("env"), import_name(n)))
IMPORT("js_now") double js_now(void);
IMPORT("js_beep") void js_beep(int hz, int pan);
IMPORT("js_tone") void js_tone(int hz);
IMPORT("js_ir") void js_ir(int hz);
IMPORT("js_rumble") void js_rumble(int ms);
IMPORT("js_clock") void js_clock(char *buf, int n);
IMPORT("js_music") void js_music(int on);

int plat_rumble_chan;
u64 plat_ms(void) { return (u64)js_now(); }
void plat_init(void) {}
void plat_present(const u32 *c) { (void)c; }
void plat_wait_frame(void) {}
void plat_sensorbar_idle(int on) { (void)on; }
int  plat_storage_ok(void) { return 1; }
void plat_rumble(int ms) { if (cfg.rumble) js_rumble(ms); }
void plat_beep(int high) { js_beep(high ? 2000 : 1000, 1); }
void plat_speaker(int on) { (void)on; }
void plat_beep_pan(int pan) { js_beep(1000, pan); }
void plat_tone(int hz) { js_tone(hz); }
void plat_click(void) {}
int plat_has_cios249(void) { return 0; }
int plat_fakemote_active(void) { return 0; }
int plat_fakemote_failed(void) { return 0; }
void plat_boot_ok(void) {}
int plat_deflicker;
void irlow_begin(u32 hz) { js_ir((int)hz); }
void irlow_mark(u32 us) { (void)us; }
void irlow_space(u32 us) { (void)us; }
void irlow_end(void) {}
void irlow_idle(void) {}
u32  irlow_lock(void) { return 0; }
void irlow_unlock(u32 l) { (void)l; }
void plat_clock(char *b, int n) { js_clock(b, n); }
/* casino music: the page pulls audio from here when it needs more */
void plat_music(int on) { js_music(on); }
void plat_music_pump(void) {}
static s16 mus_out[8192 * 2];
__attribute__((export_name("web_music"))) s16 *web_music(int frames, int rate)
{
	if (frames > 8192) frames = 8192;
	music_render(mus_out, frames, rate);
	return mus_out;
}

static Pad web_pads[MAX_PADS];
void plat_poll(Pad pads[MAX_PADS]) { memcpy(pads, web_pads, sizeof web_pads); }

/* ---- picture output: the menus (640x480) or a test picture at its real line count ---- */
static int web_h = 480, web_mode;
static u32 native[640 * 576];
void plat_present_native(const u32 *b) { memcpy(native, b, 640 * web_h * 4); }
int plat_native_w(void) { return 640; }
int plat_native_h(void) { return web_h; }
static const char *MODE_N[8] = { "Wii setting", "480i 60Hz", "480p 60Hz", "240p 60Hz", "480i PAL60", "240p PAL60", "576i 50Hz", "288p 50Hz" };
const char *plat_video_mode(void) { return web_mode == 0 ? "PC preview" : MODE_N[web_mode]; }
int plat_video_modes(const char **names, int max) { for (int i = 0; i < 8 && i < max; i++) names[i] = MODE_N[i]; return 8; }
int plat_video_set(int i) { web_mode = i; web_h = (i == 3 || i == 5) ? 240 : i == 7 ? 264 : i == 6 ? 574 : 480; return 1; }
int plat_video_current(void) { return web_mode; }
int plat_video_info(char L[][72], int max)
{
	int n = 0;
	if (n < max) snprintf(L[n++], 72, "Output: %s (PC preview)", MODE_N[web_mode]);
	if (n < max) snprintf(L[n++], 72, "Picture: 640 x %d", web_h);
	if (n < max) snprintf(L[n++], 72, "This is the PC preview. No real video or IR here.");
	return n;
}

Usb usb;

/* ---- called from the page ---- */
static u32 prev_held;
static int started;
extern u32 *app_native_buf;

__attribute__((export_name("web_init"))) void web_init(void)
{
	web_pads[0].connected = 1; web_pads[0].battery = 80;
	app_init();
	started = 1;
}

/* the Wii Remote: buttons held, pointer (-1 = off the screen) */
__attribute__((export_name("web_pad"))) void web_pad(u32 held, int px, int py)
{
	Pad *p = &web_pads[0];
	p->wm_held = held & 0xFFFF;
	p->wm_down = held & ~prev_held & 0xFFFF;
	prev_held = held;
	p->ptr_valid = px >= 0;
	p->px = px; p->py = py;
}

/* a typed key from the PC keyboard (USB HID code + character) */
__attribute__((export_name("web_key"))) void web_key(int code, int sym)
{
	usb.kb_connected = 1;
	if (usb.nkeys < KB_MAX_EVENTS) { usb.keys[usb.nkeys] = (u8)code; usb.syms[usb.nkeys] = (u16)sym; usb.nkeys++; }
}

__attribute__((export_name("web_typing"))) int web_typing(void) { return screen == S_KEYBOARD; }
__attribute__((export_name("web_screen"))) int web_screen(void) { return screen; }

/* run one frame. returns 0 when the app quits */
__attribute__((export_name("web_frame"))) int web_frame(void)
{
	Pad p[MAX_PADS]; plat_poll(p);
	int r = app_frame(p);
	web_pads[0].wm_down = 0;
	usb.nkeys = 0;
	if (app_native_buf) plat_present_native(app_native_buf);
	return r;
}
/* what to draw: the test picture if one is up, else the menus */
__attribute__((export_name("web_buf"))) u32 *web_buf(void) { return app_native_buf ? native : canvas; }
__attribute__((export_name("web_bufh"))) int web_bufh(void) { return app_native_buf ? web_h : 480; }



/* test / PC preview: pretend data (nothing real is read) */
#define NICKNAME "Player"
void plat_dossier(Dossier *d)
{
	memset(d, 0, sizeof *d);
	snprintf(d->nick, sizeof d->nick, "%s", NICKNAME);
	snprintf(d->region, sizeof d->region, "Australian");
	d->miis = 6; d->mii_found = 1; d->mii_female = 0; d->mii_height = 90; d->mii_bmonth = 7; d->mii_bday = 14; snprintf(d->mii_name, 24, "%s", NICKNAME);
	d->fit_found = 1; d->fit_height_cm = 181; d->fit_age = 27; d->fit_profiles = 3; snprintf(d->fit_name, 24, "%s", NICKNAME);
	static const char *S[] = { "Mario Kart Wii", "Super Smash Bros. Brawl", "Wii Sports", "Super Mario Galaxy" };
	d->wii_saves = 9; d->nsave_named = 4; for (int i = 0; i < 4; i++) snprintf(d->save_names[i], 40, "%s", S[i]);
	static const char *B[] = { "Xenoblade Chronicles", "The Last Story", "Super Mario Sunshine" };
	d->wii_backups = 2; d->gc_backups = 1; d->nbackup = 3; for (int i = 0; i < 3; i++) snprintf(d->backup_names[i], 40, "%s", B[i]);
	d->gc_saves = 2; d->roms = 23; d->cios = 1; d->online = 1; snprintf(d->ssid, sizeof d->ssid, "HomeWiFi");
	snprintf(d->last_played, sizeof d->last_played, "The Homebrew Channel"); d->last_minutes = 47;
}
