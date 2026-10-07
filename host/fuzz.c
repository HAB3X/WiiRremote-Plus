#include "app.h"
extern int gfx_audit; extern char gfx_audit_where[48]; void gfx_audit_dump(void);
extern Pad host_pads[MAX_PADS]; extern u64 sim_ms;
static unsigned r = 1; static unsigned rnd(void){ r = r*1103515245u+12345u; return r>>8; }
static void frame(void){ Pad p[MAX_PADS]; memcpy(p,host_pads,sizeof p); if(!app_frame(p)) { app_init(); } host_pads[0].wm_down=0; host_pads[0].cc_down=0; sim_ms+=16; }
int main(int argc,char**argv){ int N = argc>1?atoi(argv[1]):200000; r = argc>2?atoi(argv[2]):1;
 host_pads[0].connected=1; host_pads[0].battery=50; host_pads[0].has_classic=1; app_init(); cfg.help_seen=~0u; gfx_audit=1;
 static const u32 B[] = { BTN_A, BTN_A, BTN_A, BTN_B, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_1, BTN_2, BTN_PLUS, BTN_MINUS, BTN_HOME };
 int stuck_max=0;
 for (int i=0;i<N;i++){
   int k = rnd()%40;
   if (k < 17) { u32 b=B[k]; host_pads[0].wm_down=b; host_pads[0].wm_held=b; }
   else host_pads[0].wm_held=0;
   if (rnd()%50==0) { host_pads[0].ptr_valid=1; host_pads[0].px=rnd()%640; host_pads[0].py=rnd()%480; } else if (rnd()%20==0) host_pads[0].ptr_valid=0;
   snprintf(gfx_audit_where, 48, "screen %d", screen); frame();
   /* every so often: can HOME always get us to Home? */
   if (i % 5000 == 4999) {
     int j; host_pads[0].ptr_valid=0; host_pads[0].wm_held=0;
     for (j=0;j<40 && screen!=S_HOME;j++){ host_pads[0].wm_down=BTN_HOME; frame(); frame(); frame(); }
     if (screen==S_CASINO) { go(S_HOME); continue; }   /* the casino keeps you in on purpose */
     if (screen==S_CASINO) { go(S_HOME); continue; }
     if (screen!=S_HOME) { void calui_debug(void); printf("STUCK on screen %d at step %d\n", screen, i); calui_debug(); void sm_debug(void); sm_debug(); return 1; }
     if (j>stuck_max) stuck_max=j;
   }
 }
  gfx_audit_dump(); printf("ok, max HOME presses to get Home: %d\n", stuck_max); return 0; }
