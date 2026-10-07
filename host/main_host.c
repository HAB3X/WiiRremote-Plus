/* Host test driver: runs the app with scripted input and saves screenshots. */
#include "app.h"
#include "patterns.h"

extern Pad host_pads[MAX_PADS];
extern u64 sim_ms;
extern int host_ir_frames;
void save_png(const char *name);
void finder_start(void);

static void frame(int n)
{
	for (int i = 0; i < n; i++) {
		Pad p[MAX_PADS]; memcpy(p, host_pads, sizeof p);
		app_frame(p);
		host_pads[0].wm_down = 0; host_pads[0].cc_down = 0;
		sim_ms += 16;
	}
}
static void press(u32 b) { host_pads[0].wm_down = b & 0xFFFF; host_pads[0].cc_down = b & 0xFFFF0000u; frame(1); }
static void point(int x, int y) { host_pads[0].ptr_valid = 1; host_pads[0].px = x; host_pads[0].py = y; frame(2); }
static void nopoint(void) { host_pads[0].ptr_valid = 0; frame(1); }
static void shot(const char *n) { save_png(n); }

int main(void)
{
	host_pads[0].connected = 1;
	host_pads[0].has_classic = 1;
	host_pads[1].connected = 1;
	app_init();
	cfg.help_seen = 0xFFFFFFFF;
	frame(2);
	shot("01_remote_start");

	/* selection */
	press(BTN_B); press(BTN_RIGHT); press(BTN_B); press(BTN_RIGHT); press(BTN_B);
	frame(1); shot("02_remote_selected");
	press(BTN_A); frame(3); shot("03_remote_sent");
	remote_screen_reset(); frame(1);

	point(530, 270); shot("04_remote_pointer");
	press(BTN_A); frame(1); shot("05_more_popup");
	press(BTN_HOME); nopoint();

	go(S_HOME); frame(2); shot("06_home");
	point(600 - 26, 50); press(BTN_A); frame(1); shot("07_help"); press(BTN_HOME); nopoint();

	open_remote_file(cfg.last); frame(1);
	finder_start(); frame(2); shot("08_finder");
	press(BTN_PLUS); frame(1); shot("09_name");
	kb_open(K_RENAME_REMOTE, "Transonic", "Rename remote", NAME_LEN - 1); frame(2); shot("10_keyboard");
	go(S_REMOTE);
	map_start(); frame(2); shot("11_map_wm");
	press(BTN_RIGHT); press(BTN_RIGHT); frame(1);
	point(400, 120); nopoint();
	go(S_MAP); frame(1);
	/* switch to classic tab */
	point(300, 116); press(BTN_A); nopoint(); frame(1); shot("12_map_cc");
	point(250, 170); press(BTN_A); frame(1); shot("13_map_picker"); press(BTN_HOME); nopoint();
	direct_start(); frame(1); press(BTN_PLUS); frame(2); shot("14_direct");
	cal_start(); frame(2); shot("15_cal_start");
	{ extern int host_native_h; extern void save_native(const char *);
	  press(BTN_A); frame(2);
	  for (int p = 0; p < 22; p++) { char n[32]; snprintf(n, sizeof n, "n480_%02d", p); frame(1); extern u32 *app_native_buf; plat_present_native(app_native_buf); save_native(n); press(BTN_2); }
	  press(BTN_MINUS); frame(1); { extern u32 *app_native_buf; plat_present_native(app_native_buf); } save_native("n480_mode");
	  host_native_h = 574; press(BTN_1); frame(2); { extern u32 *app_native_buf; plat_present_native(app_native_buf); } save_native("n576_x");
	  press(BTN_HOME); host_native_h = 480; frame(1); }
	press(BTN_HOME);
	scan_start(); frame(2); shot("20_scan");
	press(BTN_A); frame(40); shot("21_scan_running");
	press(BTN_A); press(BTN_PLUS); frame(1); shot("22_scan_worked"); press(BTN_HOME); press(BTN_HOME);
	go(S_SETTINGS); frame(2); shot("23_settings");
	go(S_PADS); frame(2); shot("24_pads");
	go(S_NEWREMOTE); frame(2); shot("25_newremote");
	go(S_NOTES); frame(2); shot("26_notes");
	go(S_SLEEP); frame(2); shot("27_sleep");
	code_start(-1); frame(2); shot("28_code");
	go(S_REMOTE); frame(1);
	cfg.help_seen = 0; go(S_FINDER); frame(2); shot("29_first_help"); press(BTN_A); frame(1); shot("30_first_help_p2");
	cfg.help_seen = 0xFFFFFFFF; cal_start(); frame(1); point(395, 140); press(BTN_A); nopoint(); frame(2); shot("31_picker");
	printf("ir frames sent: %d\n", host_ir_frames);
	return 0;
}
