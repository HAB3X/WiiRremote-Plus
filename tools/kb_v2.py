# v1.9.1 knowledge base additions: forum-found faults, a fifth area, new settings and questions
import re
p = 'source/calkb.h'; h = open(p).read()
h = h.replace('enum { AR_COLOUR, AR_GREY, AR_GEOM, AR_SHARP, AR_COUNT, AR_ALL = AR_COUNT };',
              'enum { AR_COLOUR, AR_GREY, AR_GEOM, AR_SHARP, AR_SIGNAL, AR_COUNT, AR_ALL = AR_COUNT };')
h = h.replace('#define MAX_QP 24', '#define MAX_QP 40')
h = h.replace('	u8 hw;                  /* 1 = may be a hardware fault: say so */',
              '	u8 hw;                  /* 0 settings, 1 may be a part wearing out, 2 SWITCH OFF NOW, 3 normal: not a fault */')
open(p, 'w').write(h)

p = 'source/calkb.c'; s = open(p).read()
s = s.replace('const char *AREA_NAME[AR_COUNT] = { "Colour", "Grey levels", "Geometry", "Sharpness" };',
              'const char *AREA_NAME[AR_COUNT] = { "Colour", "Grey levels", "Geometry", "Sharpness", "Stability & signal" };')
s = s.replace('	H_SCREEN, H_FOCUS, H_DEGAUSS, H_CONV, H_TECH, W_CABLE, W_VIDEO, W_INPUT, W_CABLETYPE,\n	S_COUNT',
              '	H_SCREEN, H_FOCUS, H_DEGAUSS, H_CONV, H_TECH, W_CABLE, W_VIDEO, W_INPUT, W_CABLETYPE,\n'
              '	/* v1.9.1: stability, signal, Wii setup, safety */\n'
              '	S_VHOLD, S_HAFC, S_DIGSCAN, S_COMB, W_SCART, W_WIIWIDE, W_WIIRGB, W_NOISE, W_WARMUP, H_OFFNOW,\n	S_COUNT')
new_sets = r'''	[S_VHOLD]    = { "V hold", "VHOLD|VERTHOLD|VFREQ|VERTICALHOLD|VHLD", AR_SIGNAL, MN_USER, "Older sets only, often a knob at the back or in the menu. Stops the picture rolling up or down.", "", 0 },
	[S_HAFC]     = { "H AFC / H hold", "HAFC|AFC|HHOLD|HFREQ|HORIZONTALHOLD|AFCMODE|HLOCK", AR_SIGNAL, MN_SERVICE, "How the TV locks onto each line. Some sets have a fast / game / VCR AFC mode that locks better to consoles.", "", 1 },
	[S_DIGSCAN]  = { "100 Hz / Digital Scan", "100HZ|DIGITALSCAN|DRC|DNIE|PROGRESSIVE|NATURALMOTION|DIGITALMODE|DIGITALPLUS", AR_SIGNAL, MN_USER, "Picture processing on 100 Hz and HD CRTs. It combs and judders 240p games and adds lag. Turn it off for games.", "", 0 },
	[S_COMB]     = { "Comb filter", "COMB|COMBFILTER|3DCOMB|YCSEP|YCSEPARATION", AR_SHARP, MN_USER, "Separates colour from brightness on composite. A 3D / adaptive comb cuts dot crawl and rainbows.", "", 0 },
	[W_SCART]    = { "RGB SCART cable", "SCART|RGBSCART|CSYNC|PIN16|SYNC", AR_SIGNAL, MN_SOURCE, "Needs RGB turned on for that socket, a voltage on pin 16, 75 ohm resistors and the right sync (csync or sync on composite).", "", 0 },
	[W_WIIWIDE]  = { "Wii 4:3 / 16:9", "WIDESCREEN|WIIASPECT|SCREENWIDTH|WII16:9", AR_GEOM, MN_SOURCE, "Wii Settings > Screen > Widescreen. On a 4:3 CRT pick 4:3, or everything looks tall and thin.", "", 0 },
	[W_WIIRGB]   = { "Wii RGB / EuRGB60", "EURGB60|PAL60RGB|RGBMODE", AR_COLOUR, MN_SOURCE, "PAL Wii on RGB SCART: 60 Hz must be EuRGB60 (PAL60), not NTSC, or the RGB pins carry S-Video and colour goes wrong.", "", 0 },
	[W_NOISE]    = { "Nearby interference", "INTERFERENCE|DIMMER|CHARGER|NOISE|GROUNDLOOP|AERIALLEAD", AR_SIGNAL, MN_SOURCE, "Dimmers, phone chargers, LED drivers, power boards, the aerial lead (ground loops) and long cables put lines, bars and hum into picture and sound.", "", 0 },
	[W_WARMUP]   = { "Warm-up time", "WARMUP|WARM", AR_ALL, MN_SOURCE, "A CRT changes for its first 20-30 minutes. Set colour, geometry and focus only once it's warm.", "", 0 },
	[H_OFFNOW]   = { "Switch off now", "SWITCHOFF", AR_ALL, MN_HARDWARE, "Turn the TV off at the wall and stop using it. A technician needs to look at it. Never open a CRT: it holds a charge after unplugging.", "", 2 },
};
const int CAL_NSET = S_COUNT;'''
s = s.replace('};\nconst int CAL_NSET = S_COUNT;', new_sets, 1)

s = s.replace('	F_HALO, F_SOFT, F_FOCUS_EDGE, F_MOIRE, F_SVM, F_DOTCRAWL, F_FLICKER,\n	F_COUNT',
 '	F_HALO, F_SOFT, F_FOCUS_EDGE, F_MOIRE, F_SVM, F_DOTCRAWL, F_FLICKER,\n'
 '	/* v1.9.1 (forum research): stability and signal */\n'
 '	F_VROLL, F_HTEAR, F_FLAG, F_JITTER, F_DROPOUT, F_RGB_SYNC, F_RGB_CHECKER, F_DIGITAL, F_GHOST, F_HERRING, F_JAILBARS, F_CROSSTALK,\n'
 '	F_BUZZ, F_HUMBAR, F_WHINE, F_ARCING, F_480P_GREY, F_480P_BLANK,\n'
 '	/* tube and brightness */\n'
 '	F_FOG, F_WARM_CAST, F_BURNIN, F_WEAK_TUBE, F_SLOW_START, F_PROTECT, F_DAMPER, F_BRIGHT_FLICKER,\n'
 '	/* colour, geometry, sharpness */\n'
 '	F_RGB_FALLBACK, F_RGB_HOT, F_SVIDEO_BW, F_PAL_RGB_NTSC, F_COL_INTERMIT, F_WII_WIDE, F_WII_BORDER, F_SWIM60, F_EW_FAULT, F_GEOM_DRIFT,\n'
 '	F_VCOLLAPSE, F_HCOLLAPSE, F_RAINBOW, F_DEFLICKER, F_FOCUS_DRIFT,\n'
 '	F_COUNT')

new_faults = r'''
	/* ================= v1.9.1: from forum research (repairfaq, RetroRGB, ConsoleMods, Sunthar, GBAtemp...) ================= */
	/* ---- stability and signal ---- */
	[F_VROLL] = { "Picture rolls up or down", AR_SIGNAL, "The picture keeps rolling up or down, or a black bar drifts through it.",
		"No steady sync: wrong cable or sync type, 60 Hz on a 50 Hz-only set, or the vertical hold drifting.", PIC(PAT_GRID, 0, 0),
		NF(ST(W_VIDEO, 0, "Try the Wii at 50 Hz. Steady now? The TV can't do 60 Hz: play at 50 Hz"), ST(W_CABLE, 0, "Try another cable or input"), ST(S_VHOLD, 0, "Older set: turn V hold slowly until the picture locks, then stop in the middle of the locked range"), ST(H_TECH, 0, "Rolls on every cable and at 50 Hz: vertical section capacitors: technician")), 5, 1 },
	[F_HTEAR] = { "Picture tears into diagonal stripes", AR_SIGNAL, "The picture breaks into slanted stripes or is torn sideways.",
		"The TV can't lock to each line: wrong sync (sync on green, TTL sync), 480p on a TV that only takes 480i, or H hold drift.", PIC(PAT_GRID, 0, 0),
		NF(ST(W_VIDEO, 0, "Wii Settings > Screen: pick Standard (480i). Only EDTV / HD CRTs take 480p"), ST(W_SCART, 0, "RGB SCART: use a cable with proper csync or sync on composite"), ST(S_HAFC, 0, "Service menu AFC / H hold: try the other mode"), ST(H_TECH, 0, "Still tearing on a good cable at 480i: technician")), 3, 1 },
	[F_FLAG] = { "Top of the picture bends", AR_SIGNAL, "The top few lines bend or hook left or right (flagging).",
		"Slightly off-standard console timing meets the TV's line lock (AFC), or weak sync. Common on some sets.", PIC(PAT_GRID, 0, 0),
		NF(ST(S_HAFC, 0, "Service menu: AFC / H AFC mode, try fast / game / VCR"), ST(W_SCART, 0, "Try another cable or sync type"), ST(W_VIDEO, 0, "Try 480i instead of 240p modes"), ST(H_TECH, 0, "Worse over time: capacitors in the sync path: technician")), 4, 1 },
	[F_JITTER] = { "Vertical lines wiggle", AR_SIGNAL, "Straight up-and-down lines shimmer or wiggle, worst on still text.",
		"Jitter from the cable or console, noisy power, or something magnetic nearby. Some Sony sets have a little by design.", PIC(PAT_MONO, 0, 0),
		NF(ST(W_NOISE, 0, "Move power boards, chargers and speakers away; plug the TV into another wall socket"), ST(W_CABLE, 0, "Try another cable"), ST(H_TECH, 0, "Getting worse: power supply capacitors: technician")), 3, 1 },
	[F_DROPOUT] = { "Picture blanks for a moment", AR_SIGNAL, "Every so often the screen goes black (or loses colour) for a second.",
		"A loose plug, or interference on the mains (a Wii case was traced to a faulty dimmer switch).", PIC(PAT_GREY50, 0, 0),
		NF(ST(W_CABLE, 0, "Wiggle the cable at both ends: if it blanks, reseat or replace it"), ST(W_NOISE, 0, "Turn off dimmers and nearby chargers; does it stop?"), ST(H_TECH, 0, "Still blanking with a good cable: technician")), 3, 1 },
	[F_RGB_SYNC] = { "RGB SCART rolls or shows nothing", AR_SIGNAL, "On RGB SCART the picture rolls, tears or doesn't show, but composite is fine.",
		"Sync problem: TTL (5 V) sync that's too strong, sync the TV doesn't take (csync vs composite video), or a missing blanking voltage on pin 16.", PIC(PAT_SMPTE, 0, 0),
		NF(ST(W_SCART, 0, "Use a cable with 75 ohm csync (or one that syncs on composite video, which every TV takes)"), ST(W_INPUT, 0, "Try the other SCART socket: often only one does RGB"), ST(W_VIDEO, 0, "PAL Wii: set 60 Hz to EuRGB60, or use 50 Hz")), 4, 0 },
	[F_RGB_CHECKER] = { "Fine checkerboard over RGB", AR_SIGNAL, "A fine crawling checkerboard on flat colours, on RGB only.",
		"The cable uses composite video as sync and the colour signal leaks into the RGB wires (poor shielding).", PIC(PAT_GREY50, 0, 0),
		NF(ST(W_SCART, 0, "Use a shielded cable with csync or sync on luma")), 3, 0 },
	[F_DIGITAL] = { "Games comb, judder or lag", AR_SIGNAL, "Old 240p games look combed or juddery, moving things break up, or there's lag. Usually a 100 Hz or HD CRT.",
		"100 Hz / Digital Scan processing treats 240p as interlaced and adds a frame or more of lag.", PIC(PAT_SPRITE, 0, 0),
		NF(ST(S_DIGSCAN, 0, "Turn 100 Hz / Digital Scan / DRC / Natural motion OFF (or pick a Game mode)"), ST(W_VIDEO, 0, "Try 480i or 480p: these sets handle them better than 240p"), ST(H_TECH, 0, "Can't be turned off: the TV itself. A plain 50 / 60 Hz CRT is better for retro games")), 4, 0 },
	[F_GHOST] = { "Faint copy of every edge", AR_SIGNAL, "A faint second outline a little to the right of edges (ghosting or ringing).",
		"Long or cheap cable, an unswitched SCART splitter, or worn plugs.", PIC(PAT_SHARP, 0, 0),
		NF(ST(W_CABLE, 0, "Use a short shielded cable, straight into the TV"), ST(W_INPUT, 0, "Remove passive splitters; use a switching SCART switch")), 4, 0 },
	[F_HERRING] = { "Moving diagonal lines", AR_SIGNAL, "Fine diagonal lines or a herringbone pattern drift over the picture.",
		"Radio interference from nearby devices (dimmers, chargers, cheap power supplies) or an RF lead.", PIC(PAT_GREY50, 0, 0),
		NF(ST(W_NOISE, 0, "Unplug chargers, dimmers and LED lamps one at a time to find it"), ST(W_CABLETYPE, 0, "Not on the aerial (RF) input: use AV, S-Video or component"), ST(W_CABLE, 0, "Use a shielded cable")), 4, 0 },
	[F_JAILBARS] = { "Faint vertical bands", AR_SIGNAL, "Faint vertical stripes on flat colours (jail bars).",
		"Power noise in the console or the TV. Try another source to see which.", PIC(PAT_GREY50, 0, 0),
		NF(ST(W_INPUT, 0, "Show the TV's own menu or another console: bars still there? It's the TV"), ST(W_CABLE, 0, "Try another cable"), ST(H_TECH, 0, "TV side: capacitors near the flyback or neck board: technician")), 3, 1 },
	[F_CROSSTALK] = { "Another picture shows through", AR_SIGNAL, "A faint image of another console or channel behind the picture.",
		"A cheap SCART switch or splitter, or other devices left on.", PIC(PAT_FIELD, 4, 0),
		NF(ST(W_INPUT, 0, "Turn the other devices off; does it go?"), ST(W_CABLE, 0, "Use a better switch or plug straight into the TV")), 2, 0 },
	[F_BUZZ] = { "Buzz that follows the picture", AR_SIGNAL, "A buzz in the sound that gets louder on bright screens or white text.",
		"Video or sync leaking into the sound wires in the cable (common with cheap RGB SCART).", PIC(PAT_BLOOM, 0, 0),
		NF(ST(W_SCART, 0, "Use a better shielded cable with separate grounds"), ST(W_NOISE, 0, "Unplug the aerial lead: less buzz? Ground loop")), 4, 0 },
	[F_HUMBAR] = { "Hum with dark bars drifting up", AR_SIGNAL, "Steady hum in the sound, and one or two soft dark bars slowly drifting up the picture.",
		"A ground loop (often through the aerial lead) or a failing power supply capacitor.", PIC(PAT_GREY50, 0, 0),
		NF(ST(W_NOISE, 0, "Unplug the aerial lead and other connected devices: gone? Use a ground isolator"), ST(H_TECH, 0, "Still there with only the Wii connected: power supply capacitors: technician")), 3, 1 },
	[F_WHINE] = { "High-pitched whine", AR_SIGNAL, "A thin high squeal from the TV.",
		"The line transformer and yoke vibrate at about 15.7 kHz. Normal on most CRTs; younger people hear it more.", PIC(PAT_GREY50, 0, 0),
		NF(ST(H_TECH, 0, "Normal. Only if it's loud or changes with the picture: loose windings: technician")), 3, 3 },
	[F_ARCING] = { "Crackling, sizzling or a burning smell", AR_SIGNAL, "Snapping, sizzling or crackling, an ozone or burning smell, or sparkles in the picture.",
		"High voltage arcing at the flyback, the anode cap or the neck. Dust and damp make it worse. A fire risk.", PIC(PAT_FIELD, 4, 0),
		NF(ST(H_OFFNOW, 0, "SWITCH OFF AT THE WALL NOW. Don't use the TV until a technician has checked it")), 2, 2 },
	[F_480P_GREY] = { "Can't pick 480p on the Wii", AR_SIGNAL, "Wii Settings > Screen: EDTV / HDTV (480p) is greyed out.",
		"The Wii only offers 480p when it detects a component cable. A cheap or faulty cable, or a worn AV port, stops that.", PIC(PAT_SMPTE, 0, 0),
		NF(ST(W_CABLE, 0, "Reseat the component cable at the Wii; try an official or better cable"), ST(W_VIDEO, 0, "Most CRTs can't show 480p anyway: Standard (480i) is right for them")), 3, 0 },
	[F_480P_BLANK] = { "Blank or rolling after picking 480p", AR_SIGNAL, "After choosing 480p the screen is black, rolling or static.",
		"Most CRTs (even with component) only take 480i. Only EDTV / HD CRTs show 480p.", PIC(PAT_SMPTE, 0, 0),
		NF(ST(W_VIDEO, 0, "Wait 15 s: the Wii goes back if you don't confirm. Or plug in the yellow AV cable and set Standard (480i)")), 4, 0 },

	/* ---- tube and brightness ---- */
	[F_FOG] = { "One colour floods the screen", AR_GREY, "The whole screen is flooded red, green or blue with slanted lines, and Brightness does nothing.",
		"A short inside the tube (heater to cathode) or a failed part on the board on the tube's neck.", PIC(PAT_FIELD, 4, 0),
		NF(ST(H_TECH, 0, "Technician: they swap the colour drives to tell the tube from the board. High voltage on the neck board")), 2, 1 },
	[F_WARM_CAST] = { "Tint that changes as it warms up", AR_GREY, "A tint (often blue or green) for the first 10-20 minutes that fades, or the other way round.",
		"One colour gun is weaker and slower to warm up (the tube ageing), or bias parts drifting with heat.", PIC(PAT_STEPS, 0, 0),
		NF(ST(W_WARMUP, 0, "Leave the TV on 30 minutes, then calibrate the greys warm"), ST(S_GBIAS, 0, "Rebalance bias on the dark grey once warm (the weak colour: raise its bias)"), ST(S_GDRV, 0, "Then drive on the light grey"), ST(H_TECH, 0, "Gets worse over months: tired tube: technician")), 5, 1 },
	[F_BURNIN] = { "Ghost of a logo burned in", AR_GREY, "A faint image of a logo, score or HUD that's always there, even on a plain screen.",
		"Burn-in: the phosphor has worn where a still image sat. It's permanent.", PIC(PAT_FIELD, 0, 128),
		NF(ST(S_CONTRAST, -1, "Lower Contrast to slow any more wear (it can't undo it)"), ST(H_TECH, 0, "Only a new tube removes it")), 3, 3 },
	[F_WEAK_TUBE] = { "Bright parts silvery or smeared", AR_GREY, "Whites look silvery or blotchy, and bright things smear or trail to the right.",
		"Worn cathodes (weak emission), or drive and contrast set too high.", PIC(PAT_BLOOM, 0, 0),
		NF(ST(S_CONTRAST, -1, "Lower Contrast until the smearing stops"), ST(S_SCON, -1, "Still smearing: lower sub contrast a step"), ST(S_RDRV, -1, "Only one colour smears: lower that colour's drive a step"), ST(H_TECH, 0, "Still bad: tired tube (rejuvenation or a new tube): technician")), 3, 1 },
	[F_SLOW_START] = { "Takes minutes to get bright", AR_GREY, "The picture is dim and slowly gets brighter over several minutes.",
		"A worn tube or low heater voltage.", PIC(PAT_FIELD, 0, 255),
		NF(ST(W_WARMUP, 0, "Some slow start is normal on old tubes: time it"), ST(H_TECH, 0, "More than a few minutes: heater circuit or tube: technician")), 3, 1 },
	[F_PROTECT] = { "Flash, then it turns itself off", AR_GREY, "The screen flashes white with lines and the TV shuts off, or the standby light blinks.",
		"The TV's protection is tripping: high voltage too high, too much beam current, or a short.", PIC(PAT_FIELD, 4, 0),
		NF(ST(H_OFFNOW, 0, "Stop turning it back on. Count the standby light blinks: that code tells a technician the fault")), 2, 2 },
	[F_DAMPER] = { "One or two thin lines across (Trinitron)", AR_GREY, "One or two very thin dark lines across the screen, always in the same place.",
		"Trinitron and Diamondtron tubes have fine damper wires. Normal, not a fault.", PIC(PAT_FIELD, 0, 255),
		NF(ST(H_TECH, 0, "Nothing to fix: every Trinitron has them")), 3, 3 },
	[F_BRIGHT_FLICKER] = { "Brightness flickers on its own", AR_GREY, "Brightness pumps or flickers at random, dark areas unsteady.",
		"A leaking focus / screen divider in the flyback, or a dirty tube socket (worse when damp).", PIC(PAT_FIELD, 0, 50),
		NF(ST(H_TECH, 0, "Flyback or tube socket: technician. High voltage")), 2, 1 },

	/* ---- colour ---- */
	[F_RGB_FALLBACK] = { "RGB cable looks like composite", AR_COLOUR, "On RGB SCART the picture is blurry with crawling dots, or black & white.",
		"The TV isn't switching to RGB: no voltage on pin 16, RGB not turned on for that socket, or that socket has no RGB.", PIC(PAT_BLEED, 0, 0),
		NF(ST(W_INPUT, 0, "Pick the RGB input (AV1 RGB / Source RGB) in the menu, or try the other SCART socket"), ST(W_SCART, 0, "Cable: needs a voltage on pin 16 (a 50-280 ohm resistor from pin 8 to 16)"), ST(W_WIIRGB, 0, "PAL Wii at 60 Hz: EuRGB60, not NTSC")), 4, 0 },
	[F_RGB_HOT] = { "RGB washed out or too bright", AR_COLOUR, "On RGB everything is brighter and more washed out than on composite (or darker).",
		"The cable is missing its 75 ohm resistors and overdrives the TV, or the RGB sub contrast is off.", PIC(PAT_PLUGE, 0, 0),
		NF(ST(W_SCART, 0, "Use a cable with 75 ohm resistors on R, G and B"), ST(S_SCON, 0, "Service menu RGB sub contrast (e.g. RGBP on Sony): set so PLUGE and white match composite")), 2, 0 },
	[F_SVIDEO_BW] = { "S-Video: no colour", AR_COLOUR, "S-Video shows a picture but in black & white.",
		"The input isn't set to S-Video (Y/C), the SCART socket has no S-Video, or a cable fault.", PIC(PAT_SMPTE, 0, 0),
		NF(ST(W_INPUT, 0, "Set that input to S-Video / Y-C in the menu"), ST(W_CABLE, 0, "Try the other socket or another cable")), 2, 0 },
	[F_PAL_RGB_NTSC] = { "PAL Wii on RGB: odd colours at 60 Hz", AR_COLOUR, "PAL Wii on RGB SCART: fine at 50 Hz, but red-tinted or black & white at 60 Hz.",
		"In NTSC mode the Wii sends S-Video on the RGB pins. It needs EuRGB60 (PAL60).", PIC(PAT_SMPTE, 0, 0),
		NF(ST(W_WIIRGB, 0, "Wii Settings > Screen > 50/60 Hz: EuRGB60. Loaders like Nintendont: force PAL60 / EuRGB60"), ST(W_VIDEO, 0, "Or play at 50 Hz")), 3, 0 },
	[F_COL_INTERMIT] = { "Colour cuts in and out", AR_COLOUR, "Colour flickers or disappears, and changes when you touch the cable or tap the TV.",
		"A loose plug or bent pin (Wii AV port or SCART), or cracked solder joints in the TV.", PIC(PAT_SMPTE, 0, 0),
		NF(ST(W_CABLE, 0, "Reseat the cable at the Wii and the TV; try another cable"), ST(H_TECH, 0, "Tapping the TV changes it: cracked joints: technician")), 3, 1 },
	[F_RAINBOW] = { "Rainbow stripes on composite", AR_SHARP, "Rainbow colours flicker in striped, dithered or see-through areas on composite.",
		"Normal for composite video: fine patterns look like colour to the TV. Not a fault.", PIC(PAT_STRIPES, 0, 0),
		NF(ST(S_COMB, 0, "A 3D / adaptive comb filter reduces it"), ST(W_CABLETYPE, 0, "S-Video, component or RGB removes it")), 3, 3 },

	/* ---- geometry ---- */
	[F_WII_WIDE] = { "Everything tall and thin", AR_GEOM, "People and circles look squashed tall and thin; the whole picture fits but looks narrow.",
		"The Wii is set to 16:9 (widescreen) on a 4:3 TV.", PIC(PAT_GRID, 0, 0),
		NF(ST(W_WIIWIDE, 0, "Wii Settings > Screen > Widescreen: 4:3. Or use the TV's 16:9 squeeze mode if it has one")), 5, 0 },
	[F_WII_BORDER] = { "Thin black bars at the sides", AR_GEOM, "Narrow black bars at the left and right edges; everything else fine.",
		"Normal: the Wii's picture (640 wide) is a little narrower than a full TV line. Games differ.", PIC(PAT_OVERSCAN, 0, 0),
		NF(ST(S_HSIZE, +1, "If it bothers you: H size / width up until the bars just go"), ST(W_VIDEO, 0, "Homebrew and emulators have their own width settings")), 4, 3 },
	[F_SWIM60] = { "Slow wave at 60 Hz only", AR_GEOM, "The picture slowly swims or waves at 60 Hz, but is steady at 50 Hz.",
		"Your mains is 50 Hz (Australia, UK, Europe): stray 50 Hz magnetic fields beat against a 60 Hz picture.", PIC(PAT_GRID, 0, 0),
		NF(ST(W_NOISE, 0, "Move power boards, speakers, transformers and other TVs away (and the TV from mains wiring in the wall)"), ST(W_VIDEO, 0, "Or play at 50 Hz")), 3, 1 },
	[F_EW_FAULT] = { "Sides pinched, setting does nothing", AR_GEOM, "The sides bow or pinch in and the pincushion / width settings change nothing.",
		"The east-west correction circuit has failed (a common fault).", PIC(PAT_GRID, 0, 0),
		NF(ST(S_EWPARA, 0, "Check: move pincushion 5 steps. Nothing at all? It's the circuit"), ST(H_TECH, 0, "East-west (diode modulator) circuit: technician")), 3, 1 },
	[F_GEOM_DRIFT] = { "Picture moves as it warms up", AR_GEOM, "Over half an hour the picture creeps sideways, shrinks or grows.",
		"Parts drifting with heat. A little is normal.", PIC(PAT_OVERSCAN, 0, 0),
		NF(ST(W_WARMUP, 0, "Set the geometry only after 30 minutes on"), ST(H_TECH, 0, "Large drift: capacitors: technician")), 4, 1 },
	[F_VCOLLAPSE] = { "Just a bright line across the middle", AR_GEOM, "No picture, only a bright horizontal line across the centre.",
		"The vertical scan has stopped (vertical output chip or a capacitor).", PIC(PAT_FIELD, 4, 0),
		NF(ST(H_OFFNOW, 0, "SWITCH OFF NOW: the line burns the screen. Technician: vertical output")), 2, 2 },
	[F_HCOLLAPSE] = { "Just a line down the middle", AR_GEOM, "No picture, only a vertical line or narrow stripe down the middle.",
		"The horizontal scan coil isn't connected (yoke or its plug).", PIC(PAT_FIELD, 4, 0),
		NF(ST(H_OFFNOW, 0, "SWITCH OFF NOW. Technician: high voltage is still there")), 1, 2 },

	/* ---- sharpness ---- */
	[F_DEFLICKER] = { "480i soft even on component", AR_SHARP, "The whole 480i picture is a little soft or blurred, on a good cable, even with Sharpness right.",
		"The Wii's deflicker filter blurs 480i on purpose. Not the TV.", PIC(PAT_STRIPES, 0, 0),
		NF(ST(W_VIDEO, 0, "Calibration > Video: deflicker off (this app's pictures are sent without it). Games: 480p, or loaders that turn it off")), 4, 0 },
	[F_FOCUS_DRIFT] = { "Focus changes as it warms up", AR_SHARP, "Sharp when cold and soft after half an hour (or the other way).",
		"The focus divider in the flyback drifts with heat.", PIC(PAT_MONO, 0, 0),
		NF(ST(W_WARMUP, 0, "Judge focus only when warm"), ST(H_FOCUS, 0, "Focus knob when warm: technician (high voltage)")), 3, 1 },
};
const int CAL_NFAULT = F_COUNT;'''
s = s.replace('};\nconst int CAL_NFAULT = F_COUNT;', new_faults, 1)

# the purity yes / no question becomes a choice on the white screen (covers more faults)
old_pur = '''	YN("On the white screen, are there patches of colour (often in the corners)?", AR_COLOUR, PIC(PAT_FIELD, 0, 255), 5, 0,
		QP(F_PURITY, 94), QP(F_CONVERGE, 15)),'''
new_pur = '''	CH("Plain white screen. What do you see?", AR_ALL, PIC(PAT_FIELD, 0, 255),
		OPTS("Even white", "Colour patches (often corners)", "A faint ghost of a logo or HUD", "1 or 2 thin lines across", "Silvery or blotchy bright parts", "Brightness flickers"), BASE(84, 4, 3, 4, 3, 2),
		QO(F_PURITY, 1, 90), QO(F_CONVERGE, 1, 12), QO(F_BURNIN, 2, 88), QO(F_DAMPER, 3, 90), QO(F_WEAK_TUBE, 4, 75), QO(F_BRIGHT_FLICKER, 5, 85), QO(F_SLOW_START, 4, 20)),'''
assert old_pur in s; s = s.replace(old_pur, new_pur)

# which cable: a choice instead of "are you using component"
old_cab = '''	YN("Are you using component (green, blue and red video plugs)?", AR_ALL, PIC(PAT_SMPTE, 0, 0), 60, 0,
		QP(F_COMP_DC_GP, 97), QP(F_COMP_DC_BY, 97), QP(F_SWAP_PBPR, 99), QP(F_YPB_SWAP, 99), QP(F_DOTCRAWL, 4), QP(F_BLEED, 15), QP(F_AV_DC, 5), QP(F_NO_COLOUR, 25)),'''
new_cab = '''	CH("Which cable goes from the Wii to the TV?", AR_ALL, PIC(PAT_SMPTE, 0, 0),
		OPTS("Component (green, blue, red)", "RGB SCART", "S-Video", "Composite (one yellow plug)"), BASE(40, 22, 8, 30),
		QO(F_COMP_DC_GP, 0, 97), QO(F_COMP_DC_BY, 0, 97), QO(F_SWAP_PBPR, 0, 99), QO(F_YPB_SWAP, 0, 99), QO(F_480P_GREY, 0, 70), QO(F_480P_BLANK, 0, 95),
		QO(F_RGB_SYNC, 1, 97), QO(F_RGB_CHECKER, 1, 97), QO(F_RGB_FALLBACK, 1, 97), QO(F_RGB_HOT, 1, 97), QO(F_PAL_RGB_NTSC, 1, 98), QO(F_BUZZ, 1, 60),
		QO(F_SVIDEO_BW, 2, 97), QO(F_DOTCRAWL, 3, 90), QO(F_RAINBOW, 3, 92), QO(F_BLEED, 3, 70), QO(F_AV_DC, 3, 70), QO(F_NO_COLOUR, 3, 60), QO(F_DEFLICKER, 0, 60)),'''
assert old_cab in s; s = s.replace(old_cab, new_cab)

# colour bars: black & white also means S-Video or RGB not switching
s = s.replace('QO(F_BLEED, 6, 80), QO(F_DOTCRAWL, 6, 75), QO(F_UNI_GREEN, 5, 25), QO(F_UNI_PINK, 5, 25)),',
              'QO(F_BLEED, 6, 80), QO(F_DOTCRAWL, 6, 75), QO(F_UNI_GREEN, 5, 25), QO(F_UNI_PINK, 5, 25),\n'
              '		QO(F_SVIDEO_BW, 3, 90), QO(F_RGB_FALLBACK, 6, 60), QO(F_RGB_FALLBACK, 3, 30), QO(F_PAL_RGB_NTSC, 3, 40), QO(F_PAL_RGB_NTSC, 5, 30), QO(F_COL_INTERMIT, 3, 70), QO(F_RGB_HOT, 2, 40)),')

# the opening questions: stability / signal / sound is its own area now
new_q = '''
	/* ================= v1.9.1 questions ================= */
	YN("Is the picture unsteady (rolling, tearing, jumping), noisy (lines, bars, ghosts), or is it the sound?", AR_ALL, PIC(PAT_GREY50, 0, 0), 8, 1,
		QP(F_VROLL, 95), QP(F_HTEAR, 95), QP(F_FLAG, 85), QP(F_JITTER, 85), QP(F_DROPOUT, 90), QP(F_RGB_SYNC, 90), QP(F_RGB_CHECKER, 85), QP(F_DIGITAL, 80),
		QP(F_GHOST, 80), QP(F_HERRING, 92), QP(F_JAILBARS, 88), QP(F_CROSSTALK, 88), QP(F_BUZZ, 92), QP(F_HUMBAR, 92), QP(F_WHINE, 90), QP(F_ARCING, 92),
		QP(F_480P_GREY, 60), QP(F_480P_BLANK, 90), QP(F_WAVY, 50), QP(F_SWIM60, 60), QP(F_BRIGHT_FLICKER, 50), QP(F_COL_INTERMIT, 50)),
	CH("Look at the grid for a while. What is the picture doing?", AR_SIGNAL, PIC(PAT_GRID, 0, 0),
		OPTS("Staying still", "Rolling up or down", "Tearing into slanted stripes", "Top edge bends sideways", "Up-and-down lines wiggle", "Blanks for a moment now and then", "Slowly waves or swims"), BASE(80, 3, 2, 4, 4, 3, 4),
		QO(F_VROLL, 1, 90), QO(F_RGB_SYNC, 1, 40), QO(F_RGB_SYNC, 2, 30), QO(F_480P_BLANK, 1, 30), QO(F_480P_BLANK, 2, 30), QO(F_HTEAR, 2, 88), QO(F_FLAG, 3, 88), QO(F_JITTER, 4, 85),
		QO(F_DROPOUT, 5, 88), QO(F_SWIM60, 6, 85), QO(F_WAVY, 6, 70), QO(F_HUMBAR, 6, 25), QO(F_COL_INTERMIT, 5, 20)),
	CH("Plain grey screen: what can you see on it?", AR_SIGNAL, PIC(PAT_GREY50, 0, 0),
		OPTS("Nothing, plain grey", "Faint vertical bands", "Moving diagonal lines", "Faint copy of another picture", "Soft dark bars drifting up", "Fine checkerboard"), BASE(82, 4, 4, 2, 3, 5),
		QO(F_JAILBARS, 1, 90), QO(F_HERRING, 2, 90), QO(F_CROSSTALK, 3, 88), QO(F_HUMBAR, 4, 88), QO(F_RGB_CHECKER, 5, 88), QO(F_DOTCRAWL, 5, 30), QO(F_BURNIN, 3, 30)),
	YN("Sharpness picture: is there a faint second outline a little to the right of edges?", AR_SIGNAL, PIC(PAT_SHARP, 0, 0), 8, 0,
		QP(F_GHOST, 90), QP(F_HALO, 30), QP(F_SVM, 20)),
	CH("Listen while this flashes black and white. What do you hear?", AR_SIGNAL, PIC(PAT_BLOOM, 0, 0),
		OPTS("Nothing odd", "Buzz that changes with the picture", "Steady hum", "High-pitched whine", "Crackling, snapping or sizzling"), BASE(80, 5, 5, 8, 2),
		QO(F_BUZZ, 1, 90), QO(F_HUMBAR, 2, 85), QO(F_WHINE, 3, 90), QO(F_ARCING, 4, 90)),
	YN("Is the TV a 100 Hz, 'Digital Scan', or HD CRT (usually a late model with lots of picture features)?", AR_SIGNAL, PIC(PAT_SPRITE, 0, 0), 10, 0,
		QP(F_DIGITAL, 92)),
	YN("Moving things in old games: do they comb, judder or break up (not smooth like on a plain CRT)?", AR_SIGNAL, PIC(PAT_SPRITE, 0, 0), 8, 0,
		QP(F_DIGITAL, 88), QP(F_FLICKER, 25)),
	CH("Wii Settings > Screen > TV Resolution. What's it set to?", AR_SIGNAL, PIC(PAT_SMPTE, 0, 0),
		OPTS("Standard (480i)", "EDTV/HDTV (480p) and it works", "480p, and now it's blank or rolling", "480p is greyed out"), BASE(70, 25, 2, 3),
		QO(F_480P_BLANK, 2, 95), QO(F_480P_GREY, 3, 95), QO(F_HTEAR, 2, 30), QO(F_DEFLICKER, 0, 90)),
	YN("Is the Wii set to Widescreen (16:9)?", AR_GEOM, PIC(PAT_GRID, 0, 0), 25, 0,
		QP(F_WII_WIDE, 97), QP(F_ZOOM, 20)),
	YN("Is this a PAL Wii on RGB SCART, set to 60 Hz?", AR_COLOUR, PIC(PAT_SMPTE, 0, 0), 10, 0,
		QP(F_PAL_RGB_NTSC, 95), QP(F_RGB_FALLBACK, 30), QP(F_RGB_SYNC, 30)),
	YN("Does the problem change as the TV warms up (the first 20-30 minutes)?", AR_ALL, PIC(PAT_STEPS, 0, 0), 12, 0,
		QP(F_WARM_CAST, 95), QP(F_GEOM_DRIFT, 95), QP(F_FOCUS_DRIFT, 95), QP(F_SLOW_START, 95), QP(F_PROTECT, 40), QP(F_BRIGHT_FLICKER, 40)),
	YN("Plain black screen: is one colour flooding it, with slanted lines, and Brightness does nothing?", AR_GREY, PIC(PAT_FIELD, 4, 0), 2, 0,
		QP(F_FOG, 95), QP(F_RETRACE, 30), QP(F_BLACK_GREY, 15)),
	YN("Wiggle the video cable gently at the Wii. Does the picture or colour change?", AR_ALL, PIC(PAT_SMPTE, 0, 0), 6, 0,
		QP(F_COL_INTERMIT, 92), QP(F_DROPOUT, 80), QP(F_YPB_SWAP, 20)),
	YN("On RGB, does the picture look brighter or more washed out than on composite?", AR_COLOUR, PIC(PAT_PLUGE, 0, 0), 5, 0,
		QP(F_RGB_HOT, 90), QP(F_BLACK_GREY, 30)),
	YN("Move pincushion (EW parabola) 5 steps. Do the sides change at all?", AR_GEOM, PIC(PAT_GRID, 0, 0), 90, 0,
		QP(F_EW_FAULT, 5), QP(F_PIN, 97)),
	YN("Is it only at 60 Hz that the picture slowly waves (steady at 50 Hz)?", AR_GEOM, PIC(PAT_GRID, 0, 0), 5, 0,
		QP(F_SWIM60, 92), QP(F_WAVY, 20)),
	YN("Are there narrow black bars only at the left and right, with nothing cut off?", AR_GEOM, PIC(PAT_OVERSCAN, 0, 0), 25, 0,
		QP(F_WII_BORDER, 94), QP(F_UNDER, 60), QP(F_OVER_V, 10)),
	YN("Stripes on composite: do rainbow colours shimmer in the striped areas?", AR_SHARP, PIC(PAT_STRIPES, 0, 0), 15, 0,
		QP(F_RAINBOW, 90), QP(F_DOTCRAWL, 60)),
	YN("Is it only 480i that looks soft (480p or the TV's own menus look sharper)?", AR_SHARP, PIC(PAT_STRIPES, 0, 0), 10, 0,
		QP(F_DEFLICKER, 88), QP(F_SOFT, 20)),
};
const int CAL_NQ = (int)(sizeof CAL_Q / sizeof CAL_Q[0]);'''
s = s.replace('};\nconst int CAL_NQ = (int)(sizeof CAL_Q / sizeof CAL_Q[0]);', new_q, 1)

# new troubleshooting notes
new_notes = '''	{ AR_SIGNAL, "Picture rolls", "Try the Wii at 50 Hz and another cable. Old sets: V hold. Rolls on everything: technician." },
	{ AR_SIGNAL, "Slanted tearing after picking 480p", "Most CRTs only take 480i. Wait 15 s for the Wii to go back, or use the AV cable and pick Standard." },
	{ AR_SIGNAL, "RGB SCART rolls or blank", "Sync: use a 75 ohm csync or sync-on-composite cable. Try the other SCART socket." },
	{ AR_SIGNAL, "Lines, bars or buzz", "Unplug the aerial lead, dimmers and chargers one at a time. A ground loop or noise source is the usual cause." },
	{ AR_SIGNAL, "Games comb or lag on a 100 Hz set", "Turn 100 Hz / Digital Scan / DRC off, or use a Game mode." },
	{ AR_ALL, "Crackling, burning smell, a single line, or it shuts itself off", "Switch off at the wall. Don't open the TV: a CRT holds a dangerous charge after unplugging." },
	{ AR_ALL, "Warm it up first", "Leave the TV on for 20-30 minutes before setting greys, geometry or focus." },
	{ AR_GREY, "Thin lines across a Trinitron", "Damper wires. Normal." },
	{ AR_COLOUR, "RGB looks like composite", "Pick the RGB input; the cable needs pin 16 voltage. PAL Wii at 60 Hz: EuRGB60." },
	{ AR_GEOM, "Tall, thin picture", "The Wii is on 16:9. Set it to 4:3." },
	{ AR_ALL, "Before any service change", "Write down (or enter in Ultra mode) the original number.'''
s = s.replace('	{ AR_ALL, "Before any service change", "Write down (or enter in Ultra mode) the original number.', new_notes, 1)
open(p, 'w').write(s)
print('ok')
