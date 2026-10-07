/* Calibration Suite v1.9.1: front page, remote picker, modes, the guided flow on Marcus's TV */
#include "app.h"
#include "calkb.h"
extern Pad host_pads[MAX_PADS]; extern u64 sim_ms; extern u32 *app_native_buf; extern int host_native_h;
void save_png(const char *); void save_native(const char *);
void calui_test_state(int *view, int *kind, int *q, int *fault, int *step);
static void frame(int n){ for(int i=0;i<n;i++){ Pad p[MAX_PADS]; memcpy(p,host_pads,sizeof p); app_frame(p); host_pads[0].wm_down=0; sim_ms+=16; } }
static void press(u32 b){ host_pads[0].wm_down=b; frame(1); }
static void click(int x,int y){ host_pads[0].ptr_valid=1; host_pads[0].px=x; host_pads[0].py=y; frame(2); press(BTN_A); host_pads[0].ptr_valid=0; frame(1); }
static void snap(const char*n){ if(!app_native_buf){ printf("no picture for %s\n", n); save_png(n); return; } plat_present_native(app_native_buf); save_native(n); }
int calui_test_sel(void);
void calui_test_setsel(int k);
static int choose(int k){ calui_test_setsel(k); press(BTN_A); frame(2); return 0; }

/* the base case: dark greys green, light greys pink, component only, normal menu does nothing, too tall */
static int answer(int q)
{
	char b[200]; const char *t = cal_q_text(q, b, sizeof b);
	if (q >= CAL_NQ) {
		const char *nm = CAL_FAULT[(q - CAL_NQ) % CAL_NFAULT].name;
		return (!strcmp(nm, "Green darks, pink lights") || !strcmp(nm, "Top / bottom cut off")) ? 0 : 1;
	}
	if (strstr(t, "colours or tints")) return 0;
	if (strstr(t, "colourful things")) return 1;
	if (strstr(t, "size, shape or position")) return 0;
	if (strstr(t, "blurriness")) return 1;
	if (strstr(t, "how dark or bright")) return 1;
	if (strstr(t, "DARK grey")) return 1;
	if (strstr(t, "LIGHT grey")) return 2;
	if (strstr(t, "Grey steps")) return 2;
	if (strstr(t, "every input")) return 1;
	if (strstr(t, "leave the grey tint")) return 0;
	if (strstr(t, "Which cable")) return 0;
	if (strstr(t, "TV Resolution")) return 0;
	if (strstr(t, "60 Hz")) return 0;
	if (strstr(t, "frame lines are missing")) return 1;
	if (strstr(t, "black border")) return 1;
	if (strstr(t, "Zoom")) return 1;
	if (strstr(t, "50 Hz")) return 1;
	if (strstr(t, "grid. What")) return 0;
	return cal_q_nopt(q) > 2 ? 0 : 1;   /* "no", or the first (nothing wrong) choice */
}

int main(void){
	host_pads[0].connected=1; app_init(); cfg.help_seen=~0u; frame(2);
	cal_start(); frame(2); save_png("k01_picker");
	press(BTN_A); frame(3); save_png("k02_front");
	press(BTN_PLUS); frame(2); save_png("k03_remote_overlay");
	press(BTN_B); frame(2);
	click(414 + 91, 176 - 30); frame(2); save_png("k04_modes_grey");
	press(BTN_HOME); frame(2);
	click(46 + 85, 122 + 52); frame(2);           /* Full check-up */
	press(BTN_DOWN); press(BTN_A); frame(2); save_png("k05_warning");           /* Intense -> warning */
	press(BTN_A); frame(3); snap("k06_first_question");
	int view, kind, q, f, st, shots = 0, fixed = 0, seen_result = 0;
	for (int it = 0; it < 120; it++) {
		calui_test_state(&view, &kind, &q, &f, &st);
		if (view != 4) break;
		if (kind == 1) { char b[200]; printf("Q%-3d %s -> %d\n", it, cal_q_text(q, b, 200), answer(q)); choose(answer(q)); if (shots++ == 3) snap("k07_question"); }
		else if (kind == 2) { seen_result = 1; printf("RESULT %s\n", CAL_FAULT[f].name); snap(fixed ? "k10_result2" : "k08_result"); if (!fixed) { press(BTN_RIGHT); frame(2); snap("k09_settings_tab"); press(BTN_B); press(BTN_LEFT); frame(2); snap("k12_problems_tab"); press(BTN_B); } choose(0); }
		else if (kind == 3) {
			printf("  FIX step %d: ", st + 1);
			const CalStep *s = &CAL_FAULT[f].fix[st]; printf("%s\n", s->how);
			if (st == 1 && !fixed) { snap("k13_fix"); calui_test_setsel(0); press(BTN_DOWN); press(BTN_DOWN); frame(3); snap("k14_hidden"); press(BTN_UP); frame(2); press(BTN_LEFT); frame(2); snap("k14b_left_tab"); press(BTN_RIGHT); frame(2); }
			/* the fix works on the step that names the right setting */
			int right = (fixed == 0 && st >= 2) || (fixed == 1 && st >= 2);
			choose(right ? 0 : 2);
		}
		else if (kind == 4 && !seen_result) { printf("FIRST: what kind? colours or greys\n"); snap("k05c_kind"); choose(0); }
		else if (kind == 4) { fixed++; printf("FIXED, anything else? %s\n", fixed < 2 ? "yes" : "no"); choose(fixed < 2 ? 1 : 6); }
		else if (kind == 6) { snap("k05b_safety"); choose(0); }
		else if (kind == 5) { printf("STUCK\n"); snap("k15_stuck"); choose(2); }
		else { press(BTN_A); }
	}
	frame(2); save_png("k16_done");
	printf("fixed %d\n", fixed);
	return 0;
}
