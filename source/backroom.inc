/*
   backroom.inc - the Back Room on the casino floor: pick Liar's Dice or Texas Hold'em,
   who sits in, the buy-in and the house rules.
   Included in casino_games.inc. WiiRremote Plus by Habex. GPL-3.0.
*/
static int br_game;   /* 0 Liar's Dice, 1 Texas Hold'em */
static int setup_open;   /* the setup panel for a table game is showing */
static int back_room(int blocked)
{
	(void)blocked;
	if (in->down & BTN_B && !blocked) { setup_open = 0; ui_set_focus(8200); in->down = 0; return 0; }
	/* which game, and a way back to the room */
	if (ui_button(8220, 256, 130, 96, 34, "< Back", ST_NORMAL) & UI_CLICK) { setup_open = 0; ui_set_focus(8200); in->down = 0; return 0; }
	gfx_round(360, 130, 244, 34, 12, RGB(0, 0, 0), 140);
	text_c(F_BODYB, 482, 133, br_game ? "Texas Hold'em" : "Liar's Dice", RGB(255, 240, 200), 255);
	text_fit(F_SMALL, 258, 176, 346, br_game ? "A sit-and-go: everyone starts even. Last one with chips wins." : "Bid, bluff, and call Liar! Last cup standing takes the pot.", RGB(220, 210, 190), 255);
	/* who sits in */
	int *opp = br_game ? &S.he_opp : &S.ld_opp;
	text(F_SMALL, 258, 234, br_game ? "At the poker table:" : "At the dice table:", RGB(230, 220, 200), 255);
	int nopp = 0; for (int w = 0; w < W_N; w++) nopp += (*opp >> w) & 1;
	for (int w = 0; w < W_N; w++) {
		int x = 256 + (w % 3) * 118, y = 254 + (w / 3) * 32;
		int on = (*opp >> w) & 1;
		if (ui_button(8230 + w, x, y, 112, 28, SHORT_NAME[w], on ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) {
			if (on && nopp <= 1) ui_toast("At least one of them has to play");
			else { *opp ^= 1 << w; stats_save(); plat_rumble(8); }
		}
	}
	/* house rules */
	int y = 322;
	char b1[32], b2[32], b3[32], b4[32], b5[32];
	int buy = br_game ? LD_BUY[S.he_buy % 5] : LD_BUY[S.ld_buy % 5];
	snprintf(b1, sizeof b1, "Buy-in %d", buy);
	if (ui_button(8240, 256, y, 112, 28, b1, ST_NORMAL) & UI_CLICK) { if (br_game) S.he_buy = (S.he_buy + 1) % 5; else S.ld_buy = (S.ld_buy + 1) % 5; stats_save(); }
	if (!br_game) {
		snprintf(b2, sizeof b2, "%d dice each", S.ld_dice);
		snprintf(b3, sizeof b3, "Ones wild: %s", S.ld_wild ? "On" : "Off");
		snprintf(b4, sizeof b4, "Spot on: %s", S.ld_spot ? "On" : "Off");
		snprintf(b5, sizeof b5, "Palifico: %s", S.ld_pali ? "On" : "Off");
		if (ui_button(8241, 374, y, 112, 28, b2, ST_NORMAL) & UI_CLICK) { S.ld_dice = S.ld_dice >= 5 ? 3 : S.ld_dice + 1; stats_save(); }
		if (ui_button(8242, 492, y, 112, 28, b3, ST_NORMAL) & UI_CLICK) { S.ld_wild ^= 1; stats_save(); }
		if (ui_button(8243, 256, y + 32, 112, 28, b4, ST_NORMAL) & UI_CLICK) { S.ld_spot ^= 1; stats_save(); }
		if (ui_button(8244, 374, y + 32, 112, 28, b5, ST_NORMAL) & UI_CLICK) { S.ld_pali ^= 1; stats_save(); }
	} else {
		snprintf(b2, sizeof b2, "Played %d, won %d", S.he_played, S.he_won);
		text_c(F_SMALL, 489, y + 4, b2, RGB(220, 210, 190), 255);
		snprintf(b3, sizeof b3, "Blinds rise every 8 hands");
		text_c(F_SMALL, 312, y + 38, b3, RGB(200, 200, 190), 220);
	}
	if (ui_ibutton(8245, 492, y + 32, 112, 28, IC_PLAY, "Play", ST_NORMAL) & UI_CLICK) {
		if (cfg.chips < buy) ui_toast("Not enough chips for the buy-in");
		else { if (br_game) he_start(); else ld_start(); return 1; }
	}
	/* pass the remote: a friend takes a seat (plays on the house) */
	if (ui_button(8246, 492, 226, 112, 26, S.p2 ? "2 players" : "1 player", S.p2 ? ST_PRIMARY : ST_NORMAL) & UI_CLICK) { S.p2 ^= 1; plat_rumble(8); if (S.p2) ui_toast("Player 2 sits in. Pass the Wii Remote on their turn"); }
	if (!br_game && S.ld_played) { char t[48]; snprintf(t, sizeof t, "Played %d, won %d", S.ld_played, S.ld_won); text_c(F_SMALL, 430, 234, t, RGB(200, 200, 190), 200); }
	return 0;
}
