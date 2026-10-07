/* Host (PC) platform layer: used only to test the app and render screenshots. */
#include "common.h"
#include "irlow.h"
#include "gfx.h"
#include <time.h>

int plat_rumble_chan;
u64 sim_ms = 1000;
int host_ir_frames;
u64 plat_ms(void) { return sim_ms; }
void plat_init(void) {}
void plat_present(const u32 *c) { (void)c; }
void plat_wait_frame(void) { sim_ms += 16; }
void plat_sensorbar_idle(int on) { (void)on; }
int  plat_storage_ok(void) { return 1; }
void plat_rumble(int ms) { (void)ms; }
int host_beeps;
void plat_beep(int high) { (void)high; host_beeps++; }
void plat_speaker(int on) { (void)on; }
void plat_beep_pan(int pan) { (void)pan; host_beeps++; }
int host_tone; void plat_tone(int hz) { host_tone = hz; }
void plat_click(void) {}
int host_music; void plat_music(int on) { host_music = on; }
void plat_music_pump(void) { static s16 b[800 * 2]; if (host_music) music_render(b, 800, 48000); }
int plat_has_cios249(void) { return 1; }
int plat_fakemote_active(void) { return 0; }
int plat_fakemote_failed(void) { return 0; }
void plat_boot_ok(void) {}
int plat_deflicker;
void irlow_begin(u32 hz) { (void)hz; host_ir_frames++; }
void irlow_mark(u32 us) { sim_ms += 0; (void)us; }
void irlow_space(u32 us) { (void)us; }
void irlow_end(void) {}
void irlow_idle(void) {}
u32  irlow_lock(void) { return 0; }
void irlow_unlock(u32 l) { (void)l; }

Pad host_pads[MAX_PADS];
void plat_poll(Pad pads[MAX_PADS]) { memcpy(pads, host_pads, sizeof host_pads); }

void save_png(const char *name)
{
	char p[256]; snprintf(p, sizeof p, "shots4/%s.ppm", name);
	FILE *f = fopen(p, "wb");
	fprintf(f, "P6 %d %d 255\n", SCR_W, SCR_H);
	for (int i = 0; i < SCR_W * SCR_H; i++) { u32 c = canvas[i]; fputc(c >> 16, f); fputc((c >> 8) & 255, f); fputc(c & 255, f); }
	fclose(f);
}

void plat_clock(char *b, int n) { snprintf(b, n, "7:42 PM"); }

int host_native_h = 480;
u32 host_native[640 * 576];
void plat_present_native(const u32 *b) { memcpy(host_native, b, 640 * host_native_h * 4); }
int plat_native_w(void) { return 640; }
int plat_native_h(void) { return host_native_h; }
const char *plat_video_mode(void) { return host_native_h == 480 ? "480p  -  Component" : "576i PAL 50Hz  -  AV"; }
void save_native(const char *name)
{
	char p[256]; snprintf(p, sizeof p, "shots4/%s.ppm", name);
	FILE *f = fopen(p, "wb");
	fprintf(f, "P6 %d %d 255\n", 640, host_native_h);
	for (int i = 0; i < 640 * host_native_h; i++) { u32 c = host_native[i]; fputc(c >> 16, f); fputc((c >> 8) & 255, f); fputc(c & 255, f); }
	fclose(f);
}

static int host_mode;
int plat_video_modes(const char **names, int max) { static const char *N[] = { "Wii setting", "480i 60Hz", "480p 60Hz", "240p 60Hz", "480i PAL60", "240p PAL60", "576i 50Hz", "288p 50Hz" }; for (int i = 0; i < 8 && i < max; i++) names[i] = N[i]; return 8; }
int plat_video_set(int i) { host_mode = i; host_native_h = (i == 3 || i == 5) ? 240 : i == 7 ? 264 : i == 6 ? 574 : 480; return 1; }
int plat_video_current(void) { return host_mode; }
int plat_video_info(char L[][72], int max) { int n = 0; snprintf(L[n++], 72, "Output: %s", plat_video_mode()); snprintf(L[n++], 72, "Picture: 640 x %d, 60 Hz", host_native_h); snprintf(L[n++], 72, "Cable: Component (YPbPr)"); snprintf(L[n++], 72, "Wii setting: PAL, PAL60 on, 480p on, 4:3"); snprintf(L[n++], 72, "Deflicker: OFF - pixels go to the TV untouched"); snprintf(L[n++], 72, "Colour: YCbCr, black 16 / white 235 (BT.601)"); return n; }

Usb usb;
/* test / PC preview: pretend data (nothing real is read) */
#define NICKNAME "Habex"
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
