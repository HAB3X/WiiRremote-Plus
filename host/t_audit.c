/* layout audit: every screen, every guided question, result, fix step and guide step; lists text that doesn't fit */
#include "app.h"
#include "calkb.h"
#include "patterns.h"
extern Pad host_pads[MAX_PADS]; extern u64 sim_ms; extern int host_native_h;
extern int gfx_audit; extern char gfx_audit_where[48]; void gfx_audit_dump(void);
void save_png(const char *name);
void calui_test_show(int kind, int a, int b); void calui_test_tab(int t);
static void frame(int n){ for(int i=0;i<n;i++){ Pad p[MAX_PADS]; memcpy(p,host_pads,sizeof p); app_frame(p); host_pads[0].wm_down=0; sim_ms+=16; } }
static void where(const char *w){ snprintf(gfx_audit_where, 48, "%s", w); }
int main(void){
 host_pads[0].connected=1; host_pads[0].battery=70; app_init(); cfg.help_seen=~0u; frame(2);
 remote_new(&cur, "Living Room Trinitron KV-29", P_NEC, 0x18); have_cur=1;
 const char *N[]={"POWER","TV/AV","MUTE","MENU","UP","DOWN","LEFT","RIGHT","OK","VOL+","VOL-","CH+","CH-","PICTURE","Found 1F","SERVICE MENU LONG"};
 for (int i=0;i<16;i++) remote_add_btn(&cur,N[i],P_NEC,0x18,i);
 cur.nsv=4; const char *SV[]={"G-BIA","G-DRV","NT.V.PHASE","YUV R-Y DC"}; for(int i=0;i<4;i++){ snprintf(cur.sv_name[i],NAME_LEN,"%s",SV[i]); strcpy(cur.sv_before[i],"32"); strcpy(cur.sv_after[i],"30"); }
 save_cur(); remote_screen_reset();
 gfx_audit = 1;
 static const char *SN[] = { "HOME","REMOTE","FINDER","NAME","KEYBOARD","MAP","DIRECT","CAL","SCAN","SETTINGS","CODE","NEWREMOTE","NOTES","SLEEP","PADS","MACRO","PROFILES","VIDEO","PRESETS","IMPORT","KEYMAP","WIZARD","SMART","CONSOLES","SETUP","INPUTMAC","HEALTH","SVALS","VREMOTE","CASINO","CALFLOW" };
 for (int s = 0; s < S_QUIT; s++) {
   if (s == S_CASINO) continue;
   where(SN[s]);
   if (s==S_KEYBOARD) kb_open(K_RENAME_BTN,"UNKNOWN 7","Rename button",NAME_LEN-1);
   else if (s==S_NAME) name_open(K_CODE_BTN,"What did it do?");
   else if (s==S_CAL) cal_start();
   else if (s==S_FINDER) finder_start();
   else if (s==S_MAP) map_start();
   else if (s==S_WIZARD) wizard_start();
   else if (s==S_SMART) smart_start();
   else if (s==S_INPUTMAC) input_macro_start();
   else if (s==S_HEALTH) health_start();
   else if (s==S_SCAN) scan_start();
   else if (s==S_CODE) code_start(2);
   else if (s==S_MACRO) macro_edit_start(-1);
   else if (s==S_CALFLOW) calui_start(AR_GREY);
   else go(s);
   frame(4);
   if (screen==S_KEYBOARD||screen==S_NAME) { host_pads[0].wm_down=BTN_HOME; frame(2); }
   if (screen == S_CAL) { host_pads[0].wm_down=BTN_B; frame(2); }   /* the remote picker */
 }
 /* the Calibration Suite's panels, at 480 and 240 lines */
 int hs[2] = { 480, 240 };
 for (int h = 0; h < 2; h++) {
   host_native_h = hs[h];
   char w[48];
   for (int q = 0; q < CAL_NQ + 2 * CAL_NFAULT; q++) { snprintf(w, sizeof w, "Q%d %dp", q, hs[h]); where(w); calui_test_show(1, q, 0); frame(1); }
   for (int f = 0; f < CAL_NFAULT; f++) {
     snprintf(w, sizeof w, "result %s %dp", CAL_FAULT[f].name, hs[h]); where(w); calui_test_show(2, f, 0); frame(1);
     for (int s = 0; s < CAL_FAULT[f].nfix; s++) { snprintf(w, sizeof w, "fix %d.%d %dp", f, s, hs[h]); where(w); calui_test_show(3, f, s); frame(1); }
     if (h == 0) { snprintf(w, sizeof w, "tabs %d", f); where(w); calui_test_show(2, f, 0); calui_test_tab(1); frame(1); calui_test_tab(2); frame(1); }
   }
   for (int g = 0; g < CAL_NGUIDE; g++) { snprintf(w, sizeof w, "guide %d %dp", g, hs[h]); where(w); calui_test_show(0, g, 0); frame(1); }
   where("what kind"); calui_test_show(7, 0, 0); frame(1);
   where("safety"); calui_test_show(6, 0, 0); frame(1);
 }
 gfx_audit_dump();
 return 0; }
