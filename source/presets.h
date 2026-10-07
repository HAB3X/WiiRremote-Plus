#ifndef PRESETS_H
#define PRESETS_H
#include "common.h"

/* TV code library (generated into presets_db.c by tools/gen_presets.py) */
enum {
	FN_POWER, FN_MUTE, FN_VOLUP, FN_VOLDN, FN_CHUP, FN_CHDN, FN_MENU, FN_OK, FN_UP, FN_DOWN, FN_LEFT, FN_RIGHT,
	FN_BACK, FN_INPUT, FN_INFO, FN_SLEEP, FN_D0, FN_D1, FN_D2, FN_D3, FN_D4, FN_D5, FN_D6, FN_D7, FN_D8, FN_D9,
	FN_PICTURE, FN_PREVCH,
	/* discrete codes (v1.9.1): one job each, the reliable ones for macros */
	FN_PWRON, FN_PWROFF, FN_AV1, FN_AV2, FN_AV3, FN_COMP, FN_HDMI1, FN_HDMI2, FN_TVTUNER, FN_COUNT
};
typedef struct { u8 fid, cmd; } PCode;
typedef struct { const char *brands; u8 proto; u16 addr; u8 n; u16 off; u8 seen; } PSet;
/* the library Smart match uses = the built-in one + your own remotes + files in wiirremote/library */
extern const PCode *PRESET_CODES;
extern const PSet *PRESETS_DB;
extern int PRESETS_DB_N;
extern const PCode BUILTIN_CODES[];
extern const PSet BUILTIN_DB[];
extern const int BUILTIN_N, BUILTIN_CODES_N;
void smart_library_reload(void);              /* read your remotes and the library folder again */
void smart_library_exclude(const char *file); /* don't match a remote against itself */
int  smart_library_user(void);                /* how many sets came from your SD card */
extern const char *FUNC_NAME[FN_COUNT];

/* what the person saw after a code was sent */
enum { R_NONE, R_POWER, R_VOLUME, R_MUTE, R_CHANNEL, R_INPUT, R_MENU, R_OTHER, R_COUNT };

void smart_init(void);
void smart_note(u8 proto, u16 addr, u8 cmd, int resp, float weight);   /* weight 1 = you said so, <1 = guessed */
void smart_forget_last(void);
void smart_reset(void);
int  smart_answers(void);                    /* how many things you told it */
int  smart_top(int *idx, float *pct, int max);   /* best presets, most likely first */
float smart_none_pct(void);                  /* chance it's not in the library at all */
int  smart_suggest(u8 *proto, u16 *addr, u8 *cmd, int *fid);   /* the most useful code to try next */
int  smart_find_fid(int preset, u8 cmd);     /* -1 if the preset doesn't have this command */
int  fid_from_name(const char *name);        /* "Vol up" -> FN_VOLUP, -1 if unknown */
int  resp_for_fid(int fid);
int  smart_has_group(u8 proto, u16 addr);    /* any preset uses this protocol + address */
void smart_note_fid(u8 proto, u16 addr, u8 cmd, int resp, int fid, float weight);   /* fid = the button you said it was, -1 = none */
void smart_claim(u8 proto, u16 addr, u8 cmd, int resp, int fid);   /* a saved button: believed, not blindly */
int  smart_status(u8 proto, u16 addr, u8 cmd); /* 0 unknown, 1 guessed, 2 claimed, 3 answered */
int  smart_evidence(void);
void smart_skip(u8 proto, u16 addr, u8 cmd);  /* don't ask about this code again */
void smart_write(FILE *f);                     /* save what it knows (pause and come back) */
int  smart_read_line(const char *line);        /* ...and read it back, one line at a time */
int  smart_home(u8 *proto, u16 *addr);        /* your TV's own protocol + address, if known */
int  smart_home_confirmed(u8 *proto, u16 *addr);   /* only from codes you said worked */
void smart_want(int fid, int on);             /* a button we must find (yours was wrong): try for it first */
int  smart_wanted(int *fids, int max);
int  smart_suggest2(u8 *proto, u16 *addr, u8 *cmd, int *fid, float *conf, int use_library);
int  resp_from_name(const char *name);        /* "HDMI 1" -> R_INPUT */
int  smart_answer_fid(u8 proto, u16 addr, u8 cmd);   /* >= 0 button, -1 reacted, -2 nothing, -3 not answered */

/* presets sharing one protocol + address = one kind of TV */
#define SMART_MAXG 200
typedef struct { int best; float pct; int count; } SGroup;
int  smart_groups(SGroup *g, int max);       /* g must hold SMART_MAXG */
int  smart_build(int best, u8 *cmd_for_fid); /* fills FN_COUNT slots, 0xFF = none; returns how many */
#endif
