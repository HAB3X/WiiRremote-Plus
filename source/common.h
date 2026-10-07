/*
   WiiRremote Plus by Habex.
   Based on WiiRremote by Abdelali221 (https://github.com/abdelali221/WiiRremote).
   GPL-3.0-or-later.
*/
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef HOST
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32;
#else
#include <gccore.h>
#endif

#define APP_NAME    "WiiRremote Plus"
#define APP_VER     "1.9.1"
#define APP_AUTHOR  "Habex"

#define SCR_W 640
#define SCR_H 480

/* ---------- controller buttons (same bit layout as libogc WPAD) ---------- */
#define BTN_2      0x0001u
#define BTN_1      0x0002u
#define BTN_B      0x0004u
#define BTN_A      0x0008u
#define BTN_MINUS  0x0010u
#define BTN_HOME   0x0080u
#define BTN_LEFT   0x0100u
#define BTN_RIGHT  0x0200u
#define BTN_DOWN   0x0400u
#define BTN_UP     0x0800u
#define BTN_PLUS   0x1000u
#define BTN_NC_Z   0x2000u   /* Nunchuk Z (moved into the Wii Remote word) */
#define BTN_NC_C   0x4000u   /* Nunchuk C */
#define CC_UP      (0x0001u<<16)
#define CC_LEFT    (0x0002u<<16)
#define CC_ZR      (0x0004u<<16)
#define CC_X       (0x0008u<<16)
#define CC_A       (0x0010u<<16)
#define CC_Y       (0x0020u<<16)
#define CC_B       (0x0040u<<16)
#define CC_ZL      (0x0080u<<16)
#define CC_R       (0x0200u<<16)
#define CC_PLUS    (0x0400u<<16)
#define CC_HOME    (0x0800u<<16)
#define CC_MINUS   (0x1000u<<16)
#define CC_L       (0x2000u<<16)
#define CC_DOWN    (0x4000u<<16)
#define CC_RIGHT   (0x8000u<<16)

#define MAX_PADS 4
typedef struct {
	u32 wm_down, wm_held;     /* Wii Remote buttons (low 16 bits) */
	u32 cc_down, cc_held;     /* Classic Controller buttons (high 16 bits) */
	int ptr_valid, px, py;
	int connected, has_classic;
	int battery;              /* 0-100, -1 unknown */
	int gc_connected;         /* GameCube controller in port c */
	u32 gc_down, gc_held;     /* already converted to Classic Controller bits */
	int ext;                  /* 0 none, 1 Nunchuk, 2 Classic / Wii U Pro, 3 Guitar */
	int bb_connected;         /* Balance Board (only in pads[0]) */
	u32 bb_down, bb_held;     /* leaning = Classic D-pad bits */
	int roll, pitch;          /* Wii Remote tilt in degrees (roll: right = +, pitch: up = +) */
	int shake;                /* how hard the Wii Remote is being shaken right now (0 = still) */
} Pad;

/* USB keyboard and mouse (global, there is only one of each) */
#define KB_MAX_EVENTS 16
typedef struct {
	int kb_connected;
	int nkeys;                      /* keys pressed this frame */
	u8  keys[KB_MAX_EVENTS];        /* USB HID key codes */
	u16 syms[KB_MAX_EVENTS];        /* typed characters (0 if none) */
	int mouse_connected;
	int mx, my;                     /* mouse cursor, 640x480 */
	u32 mouse_down, mouse_held;     /* bit0 left, bit1 right, bit2 middle */
	int wheel;                      /* +/- this frame */
	u64 mouse_moved_ms;
} Usb;
extern Usb usb;

typedef struct {
	u32 down, held, up;       /* Wii Remote + Classic buttons */
	int ptr_valid;
	int px, py;               /* pointer position, 640x480 space */
	int has_classic;
	int connected;
	int roll, pitch, tilt_ok;  /* tilt of the Wii Remote driving the menus */
	int shake;                 /* shaking the Wii Remote (dice cups) */
} Input;

/* ---------- platform layer ---------- */
void plat_init(void);
void plat_poll(Pad pads[MAX_PADS]);
void plat_present(const u32 *canvas);   /* 640x480 0xRRGGBB */
void plat_present_native(const u32 *buf);  /* plat_native_w() x plat_native_h(), no scaling */
int  plat_native_w(void);
int  plat_native_h(void);
const char *plat_video_mode(void);      /* e.g. "480p", "480i", "576i (PAL 50)" */
int  plat_video_modes(const char **names, int max);   /* list of output modes, 0 = Wii setting */
int  plat_video_set(int i);             /* 1 = ok, 0 = not possible with this cable/console */
int  plat_video_current(void);
int  plat_video_info(char lines[][72], int max);
void plat_wait_frame(void);
u64  plat_ms(void);
void plat_sensorbar_idle(int on);       /* LED state between IR frames */
int  plat_storage_ok(void);
void plat_clock(char *buf, int n);   /* "7:42 PM" style, empty if unknown */
void plat_rumble(int ms);   /* rumbles the Wii Remote that was used last */
extern int plat_rumble_chan;
void plat_beep(int high);    /* short beep from the TV speakers */
void plat_beep_pan(int pan);  /* 0 = left, 1 = middle, 2 = right */
void plat_tone(int hz);       /* steady sine tone, 0 = off */
void plat_speaker(int on);   /* Wii Remote speaker on/off */
void plat_click(void);       /* tiny click from the Wii Remote speaker (if on) */
/* casino music and sounds (music.c): a small synth the platform pulls audio from */
enum { SFX_CARD, SFX_FLIP, SFX_CHIP, SFX_WIN, SFX_BIGWIN, SFX_ACH, SFX_LEVER, SFX_REEL, SFX_TICK, SFX_LOSE, SFX_ROLL, SFX_SLAM, SFX_LIAR };
void music_render(s16 *out, int frames, int rate);   /* stereo, interleaved */
void music_play(int tune, int volume, int effects);  /* tune -1 = no music, 0 floor, 1 suite. volume 1 quiet, 2 normal */
void music_mood(float creep);                        /* 0..1: the band slows down and goes out of tune */
void music_sfx(int kind, int delay_ms);
int  music_busy(void);
void music_voice(int who, int ch, int creepy);
void music_levels(int music, int voices, int effects);   /* 0 off .. 3 full */         /* a dealer's voice: one blip per syllable */                               /* still making sound? */
void plat_music(int on);                             /* start / stop the audio stream */
void plat_music_pump(void);                          /* keep the stream fed (every frame) */
int  plat_has_cios249(void); /* cIOS 249 installed (where Fakemote lives) */
int  plat_fakemote_active(void);
int  plat_fakemote_failed(void);   /* last start with Fakemote crashed, so it was skipped */
void plat_boot_ok(void);
/* Everything the casino dealer "knows" about you. Read only from this Wii and its SD / USB,
   kept in memory only: never saved, never sent anywhere. */
typedef struct {
	char nick[24];                     /* console nickname */
	char region[20];
	int miis, mii_found, mii_female, mii_height, mii_bmonth, mii_bday;   /* height 0-127 slider */
	char mii_name[24];
	int fit_found, fit_height_cm, fit_age, fit_profiles;
	char fit_name[24];
	int wii_saves, nsave_named; char save_names[8][40];
	int wii_backups, gc_backups, nbackup; char backup_names[8][40];
	int gc_saves, roms, cios;
	int online; char ssid[33];
	char last_played[48]; int last_minutes;
} Dossier;
void plat_dossier(Dossier *d);
extern int plat_deflicker;   /* Wii-game style 1:2:1 vertical soften on output */

/* ---------- IR ---------- */
enum { P_NEC, P_SAMSUNG, P_RC5, P_SONY, P_JVC, P_PANA, P_SHARP, P_RCA, P_MITSU, P_NECX, P_RC6, P_SONY15, P_RCA38, P_COUNT };
#define P_SCANNABLE 9   /* the scanner walks the first 9 (the rest have too many addresses) */
extern const char *PROTO_NAME[P_COUNT];
void ir_send(u8 proto, u16 addr, u8 cmd);
void ir_send_repeat(u8 proto, u16 addr, u8 cmd); /* while a button is held */
u16  addr_wrap(u8 proto, int addr);   /* keep an address in range for its protocol */
int  proto_from_name(const char *s);

#endif
