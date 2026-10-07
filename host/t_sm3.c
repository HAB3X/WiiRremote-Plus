/* Smart match order: named buttons, then unnamed, then the right code for wrong ones, then extras (on your TV's address) */
#include "app.h"
#include "presets.h"
extern Pad host_pads[MAX_PADS]; extern u64 sim_ms;
void save_png(const char *name);
void sm_test_state(int *phase, const char **ckname, u8 *p, u16 *a, u8 *c, int *fid);
static void frame(int n){ for(int i=0;i<n;i++){ Pad p[MAX_PADS]; memcpy(p,host_pads,sizeof p); app_frame(p); host_pads[0].wm_down=0; sim_ms+=16; } }
static void press(u32 b){ host_pads[0].wm_down=b; frame(1); frame(2); }
static void click(int x,int y){ host_pads[0].ptr_valid=1; host_pads[0].px=x; host_pads[0].py=y; frame(3); press(BTN_A); host_pads[0].ptr_valid=0; frame(2); }
static void st(const char *tag){ int ph, fid; const char *n; u8 p, c; u16 a; sm_test_state(&ph, &n, &p, &a, &c, &fid);
	if (ph == 0) printf("%-16s checking \"%s\"\n", tag, n); else printf("%-16s trying %s %02X/%02X (maybe %s)\n", tag, PROTO_NAME[p], a, c, fid >= 0 ? FUNC_NAME[fid] : "?"); }
int main(void){
  host_pads[0].connected=1; app_init(); cfg.help_seen=~0u; frame(2);
  remote_new(&cur, "Odd TV", P_NEC, 0x18); have_cur=1;
  remote_add_btn(&cur, "Found 1F", P_NEC, 0x18, 0x1F);
  remote_add_btn(&cur, "VOL-", P_NEC, 0x18, 0x0C);     /* wrong: does nothing */
  remote_add_btn(&cur, "MUTE", P_NEC, 0x18, 0x09);
  remote_add_btn(&cur, "POWER", P_NEC, 0x18, 0x08);
  save_cur();
  smart_start(); frame(3); st("start"); save_png("m01");
  click(261, 247); st("mute: yes");
  click(111, 297); st("vol-: nothing");
  click(111, 347); st("power: skip");
  click(261, 247); frame(2); save_png("m02_what"); click(192, 102); st("found: vol+");
  for (int i = 0; i < 8; i++) { click(111, 297); st("nothing"); }
  return 0; }
