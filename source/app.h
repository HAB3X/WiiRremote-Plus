#ifndef APP_H
#define APP_H
#include "common.h"
#include "gfx.h"
#include "ui.h"
#include "icons.h"
#include "remote.h"

enum {
	S_HOME, S_REMOTE, S_FINDER, S_NAME, S_KEYBOARD, S_MAP, S_DIRECT, S_CAL,
	S_SCAN, S_SETTINGS, S_CODE, S_NEWREMOTE, S_NOTES, S_SLEEP, S_PADS, S_MACRO, S_PROFILES, S_VIDEO, S_PRESETS, S_IMPORT, S_KEYMAP, S_WIZARD, S_SMART, S_CONSOLES, S_SETUP, S_INPUTMAC, S_HEALTH, S_SVALS, S_VREMOTE, S_CASINO, S_CALFLOW, S_QUIT
};

/* why the keyboard / name picker was opened */
enum { K_NEW_REMOTE, K_RENAME_REMOTE, K_NEW_BTN, K_RENAME_BTN, K_SCAN_REMOTE, K_CODE_BTN,
       K_MACRO, K_NOTE_ADD, K_NOTE_EDIT, K_PROFILE, K_CONS_NAME, K_CONS_NOTE, K_HEALTH, K_SV_NAME, K_SV_BEFORE, K_SV_AFTER, K_GUIDE_NOTE };

extern int screen;
extern Remote cur;
extern int have_cur;

void go(int s);
void go_back(void);            /* HOME / Back: to the screen you came from */
void app_init(void);
int  app_frame(Pad *pads);   /* returns 0 to quit */
extern Input menu_in, tv_in;
extern u32 flash_bit;
extern u64 flash_ms;

/* sending */
void send_code_press(u8 proto, u16 addr, u8 cmd, u32 hold_bit);
void press_btn(int i, u32 bit);
int  handle_mapped(Input *src, u32 wm_mask);
void queue_codes(const Code *c, int n);
int  expand_btn(int i, Code *out, int max);
extern char last_sent[NAME_LEN + 8];
extern u64 last_sent_ms;

/* sleep timer */
extern u64 sleep_at;        /* 0 = off */
extern int sleep_btn;

/* text entry */
void kb_open(int purpose, const char *initial, const char *title, int maxlen);
void name_open(int purpose, const char *title);
extern char kb_text[NOTE_LEN];

/* screens */
void scr_home(void);
void scr_remote(void);
void scr_finder(void);
void scr_name(void);
void scr_keyboard(void);
void scr_map(void);
void scr_direct(void);
void scr_cal(void);
void scr_scan(void);
void scr_settings(void);
void scr_code(void);
void scr_newremote(void);
void scr_notes(void);
void scr_sleep(void);
void scr_pads(void);
void scr_macro(void);
void scr_profiles(void);
void scr_video(void);
void scr_presets(void);
void scr_import(void);
void scr_keymap(void);
void scr_wizard(void);
void scr_smart(void);
void scr_consoles(void);
void scr_setup(void);
void scr_inputmac(void);
void scr_health(void);
void scr_svals(void);
void scr_casino(void);
void casino_start(void);
void casino_leave(void);
void health_start(void);
void health_add(const char *what);
void svals_text_done(int purpose, const char *txt);
void auto_off_check(u64 idle_ms);
int  tilt_frame(void);
void input_macro_start(void);
void game_night_start(int cons);
void cons_text_done(int purpose, const char *txt);
void cal_service_start(void);
void doctor_start(void);
void guide_start(void);
void smart_start(void);
void wizard_start(void);
void unique_remote_name(const char *base, char *out);
void macro_edit_start(int idx);

void text_done(int purpose, const char *txt);

/* shared bits used between screens */
extern int edit_btn;                          /* button being renamed / re-coded */
extern int edit_note;
extern u8  pend_proto, pend_cmd;
extern u16 pend_addr;   /* code waiting for a name */
extern Code pend_macro[MAX_STEPS];
extern int  pend_macro_n;
void save_cur(void);
void open_remote_file(const char *file);
int  menu_popup(int *open, const char **items, const int *icons, int n, const char *title);
void home_refresh(void);
int  value_row(int id, int y, const char *label, const char *val, int *delta);
void finder_start(void);
void code_start(int idx);
void map_start(void);
void cal_start(void);
void scan_start(void);
void direct_start(void);
void remote_screen_reset(void);
int  cal_in_pictures(void);
/* calui.c: the guided modes */
void calui_start(int area);
void calui_suite_enter(void);
int  calui_remote_is_open(void);
int  calui_in_picture(void);
int  calui_remote_overlay(int W, int H, int pv, int px, int py);
void scr_calflow(void);
void cal_open_group(int cat);   /* a test picture group (0 colour .. 3 sharpness) */
extern Pad pads_now[MAX_PADS];
int  service_btn(void);
void draw_last_sent(int x, int y);
void undo_save(const char *what);
int  undo_available(void);
int  undo_do(void);
void draw_undo_chip(void);
int  shift_active(Input *src);
extern int ui_battery[MAX_PADS];
void draw_sleep_chip(int x, int y);
void controller_picture(int which, int x, int y, int interactive, int idbase, int *clicked, u32 flash_bit);

#endif
