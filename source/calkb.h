/*
   calkb.h - what the Calibration Suite knows about CRT TVs: the settings (with the names
   different makers give them), the faults, the questions that tell faults apart, and how
   to fix each one. Used by every guided mode. WiiRremote Plus by Habex. GPL-3.0.
*/
#ifndef CALKB_H
#define CALKB_H
#include "common.h"

/* the four areas */
enum { AR_COLOUR, AR_GREY, AR_GEOM, AR_SHARP, AR_SIGNAL, AR_COUNT, AR_ALL = AR_COUNT };
extern const char *AREA_NAME[AR_COUNT];

/* where a setting lives */
enum { MN_USER, MN_SERVICE, MN_HARDWARE, MN_SOURCE };   /* normal menu, service menu, a knob / a technician, the Wii or cable */
extern const char *MENU_NAME[4];

/* ---------- settings ---------- */
typedef struct {
	const char *name;      /* what we call it */
	const char *aliases;   /* other names on real menus, "|" separated, upper case, no spaces or dots */
	u8 area, menu;
	const char *what;      /* what it does */
	const char *up;        /* what going up does */
	u8 risk;               /* 0 safe, 1 careful, 2 dangerous (write the number down first) */
} CalSetting;
extern const CalSetting CAL_SET[];
extern const int CAL_NSET;
int cal_setting_find(const char *menu_name);   /* your menu's name -> setting, -1 if unknown */
const char *cal_setting_code(int s);           /* the label menus usually show: "G-BIA (G CUT)" */
const char *cal_setting_look(int s);           /* what to watch while changing it: "the dark grey" */
const char *cal_setting_until(int s);          /* when to stop: "the dark grey is plain grey" */

/* ---------- test pictures used by the guides ---------- */
typedef struct { u8 pat, a, b; } CalPic;     /* pattern number and two options (see cal_pic_show) */
CalPic cal_setting_pic(int s);                 /* the picture to watch it on (pat 255 = none in particular) */

/* ---------- faults ---------- */
#define MAX_FIX 8
typedef struct { s16 set; s8 dir; const char *how; } CalStep;   /* set = CAL_SET index, dir +1 up, -1 down, 0 check */
typedef struct {
	const char *name;       /* short name */
	u8 area;
	const char *looks;      /* what you see */
	const char *why;        /* what causes it */
	CalPic pic;             /* the picture that shows it best */
	CalStep fix[MAX_FIX];   /* in order: first thing to try first */
	u8 nfix;
	u8 prior;               /* how common, 1 rare .. 10 very common */
	u8 hw;                  /* 0 settings, 1 may be a part wearing out, 2 SWITCH OFF NOW, 3 normal: not a fault */
} CalFault;
extern const CalFault CAL_FAULT[];
extern const int CAL_NFAULT;

/* ---------- questions ---------- */
/* every question has 2-8 answers (yes / no is just the simplest). For each fault, how likely each
   answer is; answers not listed share what's left, like a TV without that fault would answer */
#define MAX_QP 40
#define MAX_OPT 8
typedef struct { s16 f; u8 o, p; } CalQP;   /* fault, answer number, chance (%) */
typedef struct {
	const char *text;
	u8 area;                /* AR_ALL = a general question */
	CalPic pic;
	const char *opt[MAX_OPT];   /* NULL-terminated */
	u8 base[MAX_OPT];       /* chance (%) of each answer when none of the listed faults is the problem */
	CalQP qp[MAX_QP];
	u8 nqp;
	u8 gate;                /* 1 = an opening question ("Is it a colour problem?") */
} CalQuestion;
extern const CalQuestion CAL_Q[];
extern const int CAL_NQ;
int cal_q_nopt(int q);
const char *cal_q_text(int q, char *buf, int n);   /* q >= CAL_NQ: "Is this it?" for fault q - CAL_NQ */
const char *cal_q_opt(int q, int o);
CalPic cal_q_pic(int q);
int cal_q_area(int q);

/* ---------- guided steps (solo mode) ---------- */
typedef struct {
	u8 area;
	const char *title, *text, *good;   /* what this step is, what to change, what "right" looks like */
	CalPic pic;
	s16 set[4];                        /* the settings that matter here (-1 = none) */
} CalGuide;
extern const CalGuide CAL_GUIDE[];
extern const int CAL_NGUIDE;

/* ---------- troubleshooting notes (the left tab) ---------- */
typedef struct { u8 area; const char *seen, *try_this; } CalNote;
extern const CalNote CAL_NOTE[];
extern const int CAL_NNOTE;

/* settings in an area, most relevant first (for the right tab). rel = settings to put on top */
int cal_settings_for(int area, const s16 *rel, int nrel, s16 *out, int max);

/* ---------- the diagnosis engine ---------- */
enum { ANS_YES = 0, ANS_NO = 1, ANS_UNSURE = 99 };   /* or any answer number 0..7 */
void cal_dx_start(int area);                /* AR_ALL = everything */
int  cal_dx_next(void);                     /* next question to ask, -1 = done */
void cal_dx_answer(int q, int ans);
void cal_dx_undo(void);
int  cal_dx_top(int *f, float *p, int max); /* most likely faults */
float cal_dx_none(void);                    /* chance nothing listed is wrong */
int  cal_dx_asked(void);
int  cal_dx_confident(void);                /* 1 = sure enough to suggest a fix */
void cal_dx_exclude(int f);                 /* fixed (or ruled out): look for the next problem */
int  cal_dx_area(void);
#endif
