/* fuzz the Calibration Suite v1.9.1: front page, every area and mode, flows, tabs, hide, TV remote, entry */
#include "app.h"
extern int gfx_audit; extern char gfx_audit_where[48]; void gfx_audit_dump(void);
#include "calkb.h"
extern Pad host_pads[MAX_PADS]; extern u64 sim_ms; extern int host_native_h;
static unsigned r = 11; static unsigned rnd(void){ r = r * 1103515245u + 12345u; return r >> 16; }
int main(int argc, char **argv){
	int N = argc > 1 ? atoi(argv[1]) : 60000; r = argc > 2 ? atoi(argv[2]) : 11;
	host_pads[0].connected = 1; app_init(); cfg.help_seen = ~0u; gfx_audit = 1;
	remote_new(&cur, "T", P_NEC, 0x18); have_cur = 1;
	for (int i = 0; i < 10; i++) remote_add_btn(&cur, i == 0 ? "SERVICE" : "B", P_NEC, 0x18, i);
	save_cur();
	int hs[3] = { 480, 240, 574 }, seen[8] = { 0 };
	for (int it = 0; it < N; it++) {
		if (it % 700 == 0) { host_native_h = hs[(it / 700) % 3]; if (rnd() % 2) cal_start(); else calui_start(rnd() % (AR_COUNT + 1)); }
		static const u32 B[] = { BTN_A, BTN_A, BTN_A, BTN_B, BTN_UP, BTN_DOWN, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_PLUS, BTN_MINUS, BTN_HOME, BTN_1, BTN_2, 0, 0 };
		u32 b = B[rnd() % 16];
		if (b == BTN_HOME && rnd() % 3) b = BTN_A;
		host_pads[0].wm_down = b; host_pads[0].wm_held = b;
		host_pads[0].ptr_valid = (rnd() % 4) == 0; host_pads[0].px = rnd() % 640; host_pads[0].py = rnd() % 480;
		snprintf(gfx_audit_where, 48, "screen %d", screen); Pad p[MAX_PADS]; memcpy(p, host_pads, sizeof p); app_frame(p); sim_ms += 16;
		if (screen == S_KEYBOARD || screen == S_NAME) { host_pads[0].wm_down = BTN_HOME; memcpy(p, host_pads, sizeof p); app_frame(p); }
		if (screen != S_CAL && screen != S_CALFLOW && rnd() % 50 == 0) cal_start();
		seen[screen == S_CAL ? 0 : screen == S_CALFLOW ? (calui_in_picture() ? 2 : 1) : 3]++;
	}
	gfx_audit_dump();
	printf("cal2 fuzz ok: front %d, menus %d, pictures %d, elsewhere %d\n", seen[0], seen[1], seen[2], seen[3]);
	return 0; }
