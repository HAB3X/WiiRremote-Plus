/*
   WiiRremote Plus by Habex.
   Based on WiiRremote by Abdelali221 (https://github.com/abdelali221/WiiRremote).
   GPL-3.0-or-later.
*/
#ifndef HOST
#include "app.h"
#include <wiiuse/wpad.h>

extern u32 *app_native_buf;
int plat_quit_requested(void);
int plat_poweroff_requested(void);
void plat_shutdown(void);

int main(int argc, char **argv)
{
	plat_init();
	app_init();
	Pad pads[MAX_PADS];
	while (1) {
		plat_poll(pads);
		if (!app_frame(pads)) break;
		if (plat_quit_requested()) break;
		if (app_native_buf) plat_present_native(app_native_buf);
		else plat_present(canvas);
		plat_wait_frame();
	}
	int off = plat_poweroff_requested();
	plat_shutdown();
	if (off) SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	/* launched from the Homebrew Channel: its stub is at 0x80001800, exit() jumps back to it.
	   No stub (e.g. a forwarder): go to the Wii Menu instead of hanging */
	if (*(volatile u32 *)0x80001804 == 0x53545542 && *(volatile u32 *)0x80001808 == 0x48415858) exit(0);   /* "STUBHAXX" */
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	return 0;
}
#endif
