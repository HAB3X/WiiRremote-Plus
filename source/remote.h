#ifndef REMOTE_H
#define REMOTE_H
#include "common.h"

#define MAX_BTNS     72
#define MAX_REMOTES  24
#define NAME_LEN     20

/* physical controller buttons that can be mapped */
#define WM_COUNT 12   /* Wii Remote buttons + Nunchuk C and Z */
#define CC_COUNT 14
extern const u32  WM_BITS[WM_COUNT];
extern const char *WM_NAMES[WM_COUNT];
extern const u32  CC_BITS[CC_COUNT];
extern const char *CC_NAMES[CC_COUNT];

#define MAX_STEPS 16
#define MAX_NOTES 16
#define NOTE_LEN  40
#define MAX_CONS  8
#define MAX_SV    48
#define CONS_NOTE 80

typedef struct { u8 proto; u16 addr; u8 cmd; u16 wait; } Code;   /* wait = ms after this step, 0 = default gap */

typedef struct {
	char name[NAME_LEN];
	u8 proto, cmd; u16 addr;
	u8 fav;                  /* shown in the favourites bar */
	u8 dead;                 /* marked useless: hidden, and skipped when searching */
	u8 slot;                 /* spot on the on-screen remote (0 = none), see SLOT_* */
	u8 nsteps;               /* > 0 = macro: sends steps[] in order */
	Code steps[MAX_STEPS];
} RBtn;

typedef struct {
	char name[NAME_LEN];
	char file[40];
	u8 proto; u16 addr;
	int nbtn;
	RBtn btn[MAX_BTNS];
	s8 map_wm[WM_COUNT];   /* index into btn[], -1 = none */
	s8 map_cc[CC_COUNT];
	s8 map_wm2[WM_COUNT];  /* shift layer (while B is the shift button) */
	int nnotes;
	char notes[MAX_NOTES][NOTE_LEN];
	int nkeys;                  /* USB keyboard keys mapped to buttons */
	u8 key_code[32];
	s8 key_btn[32];
	int ncons;                  /* console profiles: one tap sets the TV up for a games console */
	char cons_name[MAX_CONS][NAME_LEN];
	s8 cons_btn[MAX_CONS];      /* button or macro to send, -1 = none */
	char cons_note[MAX_CONS][CONS_NOTE];
	int nsv;                    /* service menu safety net: setting, value before, value now */
	char sv_name[MAX_SV][NAME_LEN];
	char sv_before[MAX_SV][12], sv_after[MAX_SV][12];
} Remote;

typedef struct {
	int pointer;          /* keep sensor bar lit for the Wii Remote pointer */
	int open_last;        /* open the last remote at start */
	int seq_gap;          /* ms between codes when sending a selection */
	int repeat_ms;        /* ms between repeats while holding */
	int rumble;           /* rumble when a code is sent */
	int dim_secs;         /* dim the screen after this many idle seconds, 0 = never */
	int role_wm[MAX_PADS];/* job of each Wii Remote: ROLE_MENU / ROLE_TV / ROLE_OFF */
	int role_cc[MAX_PADS];/* job of the Classic Controller plugged into it */
	int auto_ms;          /* button finder auto step time */
	u32 help_seen;        /* which help pages have been shown once */
	int light;            /* 0 = dark look (default), 1 = light look */
	int shift_mode;       /* 0 = off, 1 = hold B, 2 = tap B to toggle */
	int role_gc[MAX_PADS];/* job of each GameCube controller */
	char profile[NAME_LEN];/* room profile, "" = Main */
	int role_bb, role_kb, role_mouse;   /* Balance Board, USB keyboard, USB mouse */
	int remote_grid;      /* 0 = remote-shaped on-screen remote, 1 = plain grid */
	int safe_hold;        /* 1 = no fast repeats while searching for codes */
	int corner;           /* screensaver always hits the corners */
	int auto_off;         /* minutes idle before the TV is switched off (0 = never) */
	int tilt;             /* hold 1 and tilt the Wii Remote for volume / channel */
	int chips;            /* casino chips (Settings > Extras > Casino) */
	int fakemote;         /* load cIOS 249 at start (for Fakemote controllers) */
	int speaker;          /* Wii Remote speaker click */
	int addr_first;       /* Find codes: find the TV's address first, then its codes */
	int group_test;       /* Find buttons: send a few codes at a time, split when the TV reacts */
	int group_size;       /* how many codes per group (4, 6 or 8) */
	char last[40];        /* file name of last remote */
} Config;

enum { ROLE_MENU, ROLE_TV, ROLE_OFF, ROLE_COUNT };
extern const char *ROLE_NAME[ROLE_COUNT];
extern Config cfg;
extern char data_dir[96];
extern char base_dir[64];
int  profiles_list(char names[][NAME_LEN], int max);
int  profile_create(const char *name);
void profile_use(const char *name);
void unique_name(Remote *r, const char *base, char *out);

void storage_init(void);
void config_load(void);
void config_save(void);

int  remotes_list(char files[][40], char names[][NAME_LEN], int max);
int  remote_load(Remote *r, const char *file);
int  remote_load_path(Remote *r, const char *path);   /* any remote .txt file */
int  ir_file_to_remote(const char *path, Remote *r, int *skipped);   /* Flipper .ir, LIRC .conf or remote .txt -> r; buttons read */
int  remote_save(Remote *r);        /* assigns a file name if empty */
int  remote_delete(const char *file);
void remote_new(Remote *r, const char *name, u8 proto, u16 addr);
int  remote_export(Remote *r, char *msg, int msglen);   /* .txt + Flipper .ir into wiirremote/export */
int  remote_add_btn(Remote *r, const char *name, u8 proto, u16 addr, u8 cmd);
void remote_del_btn(Remote *r, int i);
void remote_move_btn(Remote *r, int i, int dir);
int  remote_find_btn(Remote *r, const char *name);
void remote_automap(Remote *r);
const char *key_name(u8 code);
int  remote_add_macro(Remote *r, const char *name, const Code *steps, int n);
int  backup_make(void);        /* returns backup number, 0 on failure */
int  backup_restore(void);     /* restores newest backup, returns its number */
int  backup_latest(void);

#endif
