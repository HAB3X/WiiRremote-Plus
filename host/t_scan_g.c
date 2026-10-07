/* Find codes with Group test: groups, yes / no, narrowing to one code */
#include "app.h"
extern Pad host_pads[MAX_PADS]; extern u64 sim_ms; extern int host_ir_frames;
void save_png(const char *name);
static void frame(int n){ for(int i=0;i<n;i++){ Pad p[MAX_PADS]; memcpy(p,host_pads,sizeof p); app_frame(p); host_pads[0].wm_down=0; sim_ms+=16; } }
static void press(u32 b){ host_pads[0].wm_down=b; frame(1); frame(2); }
static void click(int x,int y){ host_pads[0].ptr_valid=1; host_pads[0].px=x; host_pads[0].py=y; frame(3); press(BTN_A); host_pads[0].ptr_valid=0; frame(2); }
int main(void){
  host_pads[0].connected=1; app_init(); cfg.help_seen=~0u; cfg.group_test=1; cfg.group_size=8; cfg.addr_first=1; frame(2);
  scan_start(); frame(3); save_png("g01_group_idle");
  click(190, 312); void sg_debug(void); sg_debug(); frame(20); sg_debug(); frame(400); sg_debug(); save_png("g02_group_ask"); printf("sent %d\n", host_ir_frames);
  click(540, 312); frame(400); printf("no -> next group, sent %d\n", host_ir_frames);
  click(410, 312); frame(400);   /* yes */
  save_png("g03_narrow"); click(410, 312); frame(400);
  click(540, 312); frame(400); click(410, 312); frame(400); click(410, 312); frame(10);
  save_png("g04_found"); printf("screen %d\n", screen);
  return 0; }
