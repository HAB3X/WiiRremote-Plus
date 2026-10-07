#ifndef ICONS_H
#define ICONS_H
#include "gfx.h"
enum {
	IC_POWER, IC_PLUS, IC_MINUS, IC_SEARCH, IC_GAMEPAD, IC_PALETTE, IC_GEAR, IC_RADAR,
	IC_TRASH, IC_PENCIL, IC_BACK, IC_HELP, IC_PLAY, IC_CHECK, IC_CLOSE, IC_REMOTE, IC_MORE,
	IC_UP, IC_DOWN, IC_LEFT, IC_RIGHT, IC_VOLUP, IC_VOLDN, IC_MUTE, IC_TV, IC_MENU, IC_OK,
	IC_SEQ, IC_STAR, IC_CH_UP, IC_CH_DN, IC_INPUT, IC_WRENCH, IC_MOON, IC_NOTE, IC_SAVE, IC_SWAP, IC_PAUSE, IC_COUNT
};
void icon(int id, int cx, int cy, int size, u32 color);
int  icon_for_name(const char *name);   /* -1 if no icon fits */
#endif
