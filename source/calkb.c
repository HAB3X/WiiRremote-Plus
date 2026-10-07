/*
   calkb.c - the Calibration Suite's knowledge: CRT settings, faults, questions, fixes.

   Sources: service manual adjustment procedures (LG MC-991A, Toshiba, Sanyo LA76xxx-based
   Chinese chassis), the ConsoleMods CRT colour calibration guide, CRT Database's colour
   adjustment FAQ, and the AVS Forum service menu threads. The rules of thumb:
   - BIAS (cut-off, background, "BIA", "CUT") sets each colour in the DARK greys.
   - DRIVE (gain, "DRV") sets each colour in the BRIGHT greys.
   - Adjust drive on a light grey and bias on a dark grey, then repeat: they affect each other.
   - A tint on only one input comes from that input's own settings or the cable, not the tube.
   - 50 Hz and 60 Hz pictures have their own geometry ("NT." = the 60 Hz / NTSC set).

   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "calkb.h"
#include "patterns.h"
#include <math.h>
#include <stdlib.h>
#include <strings.h>

const char *AREA_NAME[AR_COUNT] = { "Colour", "Grey levels", "Geometry", "Sharpness", "Stability & signal" };
const char *MENU_NAME[4] = { "Normal menu", "Service menu", "Technician", "Wii / cable" };

/* ================================================================== */
/* settings                                                           */
/* ================================================================== */
enum {
	/* normal menu */
	S_BRIGHT, S_CONTRAST, S_COLOUR, S_TINT, S_SHARP, S_COLTEMP, S_PMODE, S_ASPECT, S_COLSYS, S_NR, S_VMU,
	/* service: grey and colour */
	S_SBRI, S_SCON, S_SCOL, S_STINT, S_RBIAS, S_GBIAS, S_BBIAS, S_RDRV, S_GDRV, S_BDRV, S_CBW, S_CROSSBW,
	S_RYBY_GBL, S_RYBY_ANG, S_BYDC, S_SEC_BYDC, S_SEC_RYDC, S_YUV_BYDC, S_YUV_RYDC, S_SSHARP, S_SVM,
	/* service: geometry */
	S_HPHASE, S_HSIZE, S_EWPARA, S_EWCORN, S_EWTRAP, S_VSIZE, S_VPOS, S_VLIN, S_VSC,
	S_NT_HPHASE, S_NT_VPHASE, S_NT_VLIN, S_NT_VPOS, S_NT_VSC, S_ZOOM, S_WIDE, S_TILT,
	/* knobs, technician, Wii, cable */
	H_SCREEN, H_FOCUS, H_DEGAUSS, H_CONV, H_TECH, W_CABLE, W_VIDEO, W_INPUT, W_CABLETYPE,
	/* v1.9.1: stability, signal, Wii setup, safety */
	S_VHOLD, S_HAFC, S_DIGSCAN, S_COMB, W_SCART, W_WIIWIDE, W_WIIRGB, W_NOISE, W_WARMUP, H_OFFNOW,
	S_COUNT
};

const CalSetting CAL_SET[S_COUNT] = {
	[S_BRIGHT]   = { "Brightness", "BRIGHT|BRIGHTNESS|BRI|BLACKLEVEL", AR_GREY, MN_USER, "Black level: how dark the darkest parts are.", "Blacks get lighter (grey)", 0 },
	[S_CONTRAST] = { "Contrast", "CONTRAST|CONT|PICTURE|WHITELEVEL", AR_GREY, MN_USER, "White level: how bright the brightest parts are.", "Whites get brighter; too high and they merge and bloom", 0 },
	[S_COLOUR]   = { "Colour", "COLOUR|COLOR|SATURATION|SAT|COL", AR_COLOUR, MN_USER, "How strong all colours are. Doesn't change greys.", "Colours get stronger", 0 },
	[S_TINT]     = { "Tint / Hue", "TINT|HUE|PHASE", AR_COLOUR, MN_USER, "Turns colours around the colour wheel (NTSC only on most sets): faces greener or more purple.", "Colours shift towards green (or purple, depends on the set)", 0 },
	[S_SHARP]    = { "Sharpness", "SHARP|SHARPNESS|DETAIL|APERTURE", AR_SHARP, MN_USER, "Adds edge boost to fine detail.", "Edges get crisper, then white halos appear", 0 },
	[S_COLTEMP]  = { "Colour temperature", "COLORTEMP|COLOURTEMP|COLTEMP|TEMP|WARM|COOL|WHITEBALANCE|TONE", AR_GREY, MN_USER, "Cool / Normal / Warm: the colour of white. Cool = bluish, Warm = reddish.", "Towards Warm = whites less blue", 0 },
	[S_PMODE]    = { "Picture mode", "PICTUREMODE|PMODE|PSM|PICMODE|DYNAMIC|STANDARD|MOVIE|GAME", AR_GREY, MN_USER, "Presets like Dynamic / Standard / Movie. Dynamic pushes contrast, sharpness and blue.", "", 0 },
	[S_ASPECT]   = { "Aspect / zoom mode", "ASPECT|ZOOM|WIDE|16:9|4:3|SCREENMODE|FORMAT|SIZE", AR_GEOM, MN_USER, "4:3, Zoom, Wide, 16:9. Zoom and Wide stretch or cut the picture.", "", 0 },
	[S_COLSYS]   = { "Colour system", "COLORSYSTEM|COLOURSYSTEM|SYSTEM|COLSYS|AUTO|PAL|NTSC", AR_COLOUR, MN_USER, "PAL / NTSC / Auto. The wrong one gives a black & white or flashing picture.", "", 0 },
	[S_NR]       = { "Noise reduction", "NR|DNR|NOISEREDUCTION|3DNR", AR_SHARP, MN_USER, "Smooths noise. Can smear colour and fine detail.", "Smoother, softer", 0 },
	[S_VMU]      = { "Velocity modulation", "VM|SVM|SCANVELOCITY|VELOCITY|EDGEENHANCE", AR_SHARP, MN_USER, "Speeds the beam up at edges to fake sharpness. Makes thick, uneven edges.", "Thicker, darker edges", 0 },

	[S_SBRI]     = { "Sub brightness", "SBRI|SUBBRI|SUBBRIGHT|SUBBRIGHTNESS|SBRT|BRIGHTSUB|SUBBRT", AR_GREY, MN_SERVICE, "The factory black level under your Brightness setting.", "Blacks lighter; too high = grey blacks and lines at the top", 1 },
	[S_SCON]     = { "Sub contrast", "SCON|SUBCON|SUBCONT|SUBCONTRAST|SCNT|CONTSUB", AR_GREY, MN_SERVICE, "The factory white level under your Contrast setting.", "Brighter whites; too high = blooming", 1 },
	[S_SCOL]     = { "Sub colour", "SCOL|SUBCOL|SUBCOLOR|SUBCOLOUR|SCLR|COLORSUB", AR_COLOUR, MN_SERVICE, "Factory colour strength under your Colour setting.", "Stronger colours", 1 },
	[S_STINT]    = { "Sub tint", "STINT|SUBTINT|SUBHUE|STNT|TINTSUB", AR_COLOUR, MN_SERVICE, "Factory tint under your Tint setting (NTSC).", "Colours shift around the wheel", 1 },
	[S_RBIAS]    = { "Red bias (cut-off)", "RBIA|RBIAS|RCUT|RCUTOFF|REDCUT|REDBIAS|RB|RBKG|RLOW|CUTR|BIASR|RBRT", AR_GREY, MN_SERVICE, "How much red there is in DARK greys.", "More red in dark greys", 2 },
	[S_GBIAS]    = { "Green bias (cut-off)", "GBIA|GBIAS|GCUT|GCUTOFF|GREENCUT|GREENBIAS|GB|GBKG|GLOW|CUTG|BIASG|GBRT", AR_GREY, MN_SERVICE, "How much green there is in DARK greys.", "More green in dark greys", 2 },
	[S_BBIAS]    = { "Blue bias (cut-off)", "BBIA|BBIAS|BCUT|BCUTOFF|BLUECUT|BLUEBIAS|BB|BBKG|BLOW|CUTB|BIASB|BBRT", AR_GREY, MN_SERVICE, "How much blue there is in DARK greys.", "More blue in dark greys", 2 },
	[S_RDRV]     = { "Red drive (gain)", "RDRV|RDRIVE|RGAIN|REDDRIVE|REDGAIN|RG|RHIGH|DRVR|GAINR|RD", AR_GREY, MN_SERVICE, "How much red there is in BRIGHT greys and white.", "More red in light greys", 2 },
	[S_GDRV]     = { "Green drive (gain)", "GDRV|GDRIVE|GGAIN|GREENDRIVE|GREENGAIN|GG|GHIGH|DRVG|GAING|GD", AR_GREY, MN_SERVICE, "How much green there is in BRIGHT greys. Many sets don't have it: green is the reference.", "More green in light greys", 2 },
	[S_BDRV]     = { "Blue drive (gain)", "BDRV|DDRV|BDRIVE|BGAIN|BLUEDRIVE|BLUEGAIN|BG|BHIGH|DRVB|GAINB|BD", AR_GREY, MN_SERVICE, "How much blue there is in BRIGHT greys and white. (\"D-DRV\" on some menus is a typo for this.)", "More blue in light greys", 2 },
	[S_CBW]      = { "C.B/W", "CBW|CB/W|C.B/W", AR_GREY, MN_SERVICE, "Not the same on every set. On many Chinese chassis it's a test switch, not a level. Leave it alone unless your service manual says what it does.", "", 2 },
	[S_CROSSBW]  = { "Cross B/W", "CROSSBW|CROSSB/W|CROSS", AR_GREY, MN_SERVICE, "A test screen switch on many chassis (a white cross-hatch or a plain screen for setting up). Not a picture level: turn it back off after use.", "", 1 },
	[S_RYBY_GBL] = { "R-Y/B-Y gain balance", "RYBYGBL|R-Y/B-YGBL|RYBYGAIN|RYBYBAL|GBL", AR_COLOUR, MN_SERVICE, "Balance between the red and blue colour signals. Wrong = reds too strong next to blues (or the other way). Greys don't change.", "Reds stronger, blues weaker", 2 },
	[S_RYBY_ANG] = { "R-Y/B-Y angle", "RYBYANG|R-Y/B-YANG|RYBYANGLE|ANG|DEMODANGLE", AR_COLOUR, MN_SERVICE, "Angle between the red and blue colour signals. Wrong = greens and magentas off even with Tint right. Greys don't change.", "Greens / magentas shift", 2 },
	[S_BYDC]     = { "B-Y DC level", "BYDC|B-YDC|BYDCLEVEL|B-YDCLEVEL", AR_COLOUR, MN_SERVICE, "Offset of the blue colour signal for aerial / AV / S-Video. Wrong = the whole picture, greys too, leans blue or yellow on those inputs.", "More blue (less yellow) in everything", 2 },
	[S_SEC_BYDC] = { "SECAM B-Y DC", "SECAMBYDC|SECAMB-YDC|SECBYDC", AR_COLOUR, MN_SERVICE, "Same, but only for SECAM signals (France / Eastern Europe). A Wii never sends SECAM: leave it alone.", "", 2 },
	[S_SEC_RYDC] = { "SECAM R-Y DC", "SECAMRYDC|SECAMR-YDC|SECRYDC", AR_COLOUR, MN_SERVICE, "Only for SECAM signals. A Wii never sends SECAM: leave it alone.", "", 2 },
	[S_YUV_BYDC] = { "YUV B-Y DC (component)", "YUVBYDC|YUVB-YDC|YPBPRBYDC|COMPBYDC|YUVDC|YUV-DC|PBDC", AR_COLOUR, MN_SERVICE, "Offset of the blue signal (Pb) on the COMPONENT input only. Wrong = component picture leans blue or yellow, greys too, at every brightness.", "More blue (less yellow) on component", 2 },
	[S_YUV_RYDC] = { "YUV R-Y DC (component)", "YUVRYDC|YUVR-YDC|YPBPRRYDC|COMPRYDC|PRDC", AR_COLOUR, MN_SERVICE, "Offset of the red signal (Pr) on the COMPONENT input only. Wrong = component picture leans pink or green, greys too, at every brightness.", "More pink (less green) on component", 2 },
	[S_SSHARP]   = { "Sub sharpness", "SSHARP|SUBSHARP|SUBSHARPNESS|SSHP|APER|APERTURE|PEAKING", AR_SHARP, MN_SERVICE, "Factory edge boost under your Sharpness setting.", "More edge boost", 1 },
	[S_SVM]      = { "SVM / VM level", "SVMLEVEL|VMLEVEL|VMGAIN|SVMGAIN|VMON", AR_SHARP, MN_SERVICE, "Service setting for velocity modulation (edge thickening).", "Thicker, darker edges", 1 },

	[S_HPHASE]   = { "H phase / H position", "HPHASE|HPOS|HPOSITION|HSHIFT|HS|HCENT|HCENTER|HCENTRE", AR_GEOM, MN_SERVICE, "Moves the picture left / right (50 Hz).", "Picture moves (left or right, depends on the set)", 1 },
	[S_HSIZE]    = { "H size / width", "HSIZE|HWIDTH|EWWIDTH|EWW|EW|WIDTH|HAMP|HS", AR_GEOM, MN_SERVICE, "Picture width. Cheap sets often don't have it: width is then fixed or a knob inside.", "Wider", 1 },
	[S_EWPARA]   = { "Pincushion (EW parabola)", "EWPARA|EP|PARABOLA|PIN|PINCUSHION|EWPAR|PCC", AR_GEOM, MN_SERVICE, "Bends the left and right edges in or out in the middle.", "Middle of the sides pushes out (or in)", 1 },
	[S_EWCORN]   = { "Corners (EW corner)", "EWCORNER|EC|CORNER|CORN|UCORNER|LCORNER", AR_GEOM, MN_SERVICE, "Straightens the sides near the four corners.", "Corners move out (or in)", 1 },
	[S_EWTRAP]   = { "Trapezium / keystone", "EWTRAP|ET|TRAP|TRAPEZIUM|TRAPEZOID|KEY|KEYSTONE|TRAPEZ", AR_GEOM, MN_SERVICE, "Makes the top as wide as the bottom.", "Top wider (or narrower)", 1 },
	[S_VSIZE]    = { "V size / height", "VSIZE|VAMP|VHEIGHT|HEIGHT|VH|VS|VSZ", AR_GEOM, MN_SERVICE, "Picture height (50 Hz).", "Taller", 1 },
	[S_VPOS]     = { "V position", "VPOS|VPOSITION|VPOSI|VSHIFT|VCENT|VCENTER|VCENTRE|VP", AR_GEOM, MN_SERVICE, "Moves the picture up / down (50 Hz).", "Picture moves up (or down)", 1 },
	[S_VLIN]     = { "V linearity", "VLIN|VLINE|VLINEARITY|VL|VLINE", AR_GEOM, MN_SERVICE, "Balance between the top and bottom halves: one stretched, the other squashed.", "Top grows, bottom shrinks (or the other way)", 1 },
	[S_VSC]      = { "V S-correction", "VSC|VSCORRECTION|SCORR|SC|VSCOR", AR_GEOM, MN_SERVICE, "Evens out line spacing between the middle and the top and bottom edges.", "Edges stretch, middle squashes (or the other way)", 1 },
	[S_NT_HPHASE]= { "60 Hz H phase", "NTHPHASE|NTSCHPHASE|60HPHASE|NHPHASE|NTHPOS|NTHP", AR_GEOM, MN_SERVICE, "Left / right position for 60 Hz pictures (Wii at 60 Hz, 480i, 480p).", "Picture moves sideways", 1 },
	[S_NT_VPHASE]= { "60 Hz V phase", "NTVPHASE|NTSCVPHASE|60VPHASE|NVPHASE|NTVP", AR_GEOM, MN_SERVICE, "60 Hz vertical adjustment. Depends on the chassis: often the 60 Hz HEIGHT, sometimes position. Change it 1 step and watch which one moves.", "Taller or moves (check)", 1 },
	[S_NT_VLIN]  = { "60 Hz V linearity", "NTVLINE|NTVLIN|NTSCVLIN|60VLIN|NVLIN", AR_GEOM, MN_SERVICE, "Top / bottom balance for 60 Hz pictures.", "Top grows, bottom shrinks", 1 },
	[S_NT_VPOS]  = { "60 Hz V position", "NTVPOSI|NTVPOS|NTSCVPOS|60VPOS|NVPOS", AR_GEOM, MN_SERVICE, "Up / down position for 60 Hz pictures.", "Picture moves up (or down)", 1 },
	[S_NT_VSC]   = { "60 Hz V S-correction", "NTVSC|NTSCVSC|60VSC|NVSC", AR_GEOM, MN_SERVICE, "Line spacing middle vs edges for 60 Hz pictures.", "Edges stretch", 1 },
	[S_ZOOM]     = { "Zoom size", "ZOOMSIZE|ZOOMVSIZE|ZOOM", AR_GEOM, MN_SERVICE, "Height used only in Zoom mode.", "Taller in Zoom mode", 1 },
	[S_WIDE]     = { "Wide size", "WIDESIZE|WIDEVSIZE|WIDE|16:9SIZE", AR_GEOM, MN_SERVICE, "Height used only in Wide / 16:9 mode.", "Taller in Wide mode", 1 },
	[S_TILT]     = { "Tilt / rotation", "TILT|ROTATION|ROTATE|ROT|PICTURETILT", AR_GEOM, MN_USER, "Turns the picture to level it. Normal menu on bigger sets, missing on small ones.", "Turns clockwise (or anti)", 0 },

	[H_SCREEN]   = { "Screen knob (G2)", "SCREEN|G2|SCREENVR|SCREENPOT", AR_GREY, MN_HARDWARE, "Knob on the flyback transformer. Sets the tube's overall black level. HIGH VOLTAGE: technician only.", "Brighter blacks, retrace lines", 2 },
	[H_FOCUS]    = { "Focus knob", "FOCUS|FOCUSVR|FOCUSPOT", AR_SHARP, MN_HARDWARE, "Knob on the flyback transformer. Sharpens the dots. HIGH VOLTAGE: technician only.", "", 2 },
	[H_DEGAUSS]  = { "Degauss", "DEGAUSS|DEGAUS|DEMAGNETISE", AR_COLOUR, MN_HARDWARE, "Removes magnetism from the tube. Most sets do it at power on: turn off for 20 minutes, then on. Some have it in a menu.", "", 0 },
	[H_CONV]     = { "Convergence magnets", "CONVERGENCE|CONV|PURITY|MAGNETS", AR_COLOUR, MN_HARDWARE, "Rings and magnets on the tube's neck. Set at the factory: technician only.", "", 2 },
	[H_TECH]     = { "A TV technician", "TECHNICIAN|REPAIR", AR_ALL, MN_HARDWARE, "Some faults are parts wearing out (capacitors, the tube). Settings can't fix those.", "", 2 },
	[W_CABLE]    = { "Component cable plugs", "CABLE|PLUGS|COMPONENTCABLE|YPBPR", AR_COLOUR, MN_SOURCE, "Green = Y, Blue = Pb, Red = Pr (video), plus red / white audio. A swapped plug makes wrong colours.", "", 0 },
	[W_VIDEO]    = { "Wii video settings", "WIIVIDEO|VIDEOMODE|480I|480P|50HZ|60HZ|PAL60", AR_ALL, MN_SOURCE, "Wii Settings > Screen: 50/60 Hz and EDTV (480p). WiiRremote Plus > Calibration > Video shows the output. 60 Hz uses the TV's 60 Hz (\"NT.\") geometry.", "", 0 },
	[W_INPUT]    = { "Another input", "INPUT|AV|SOURCE", AR_ALL, MN_SOURCE, "Compare with another input (the TV's own menu, aerial TV, AV). Tells a TV fault from an input or cable fault.", "", 0 },
	[W_CABLETYPE]= { "Better cable", "COMPOSITE|SVIDEO|COMPONENT|RGB|CABLETYPE", AR_SHARP, MN_SOURCE, "Composite (yellow plug) smears colour and crawls. Component (or RGB SCART) is sharper and cleaner.", "", 0 },
	[S_VHOLD]    = { "V hold", "VHOLD|VERTHOLD|VFREQ|VERTICALHOLD|VHLD", AR_SIGNAL, MN_USER, "Older sets only, often a knob at the back or in the menu. Stops the picture rolling up or down.", "", 0 },
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
const int CAL_NSET = S_COUNT;

/* "G-Bia", "R.BIAS", "d drv" -> upper case, letters, digits and / only */
static void squash(const char *s, char *o, int n)
{
	int k = 0;
	for (; *s && k < n - 1; s++) {
		char c = *s; if (c >= 'a' && c <= 'z') c -= 32;
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == ':') o[k++] = c;
	}
	o[k] = 0;
}
int cal_setting_find(const char *menu_name)
{
	char a[40]; squash(menu_name, a, sizeof a);
	if (!a[0]) return -1;
	for (int i = 0; i < S_COUNT; i++) { char nm[40]; squash(CAL_SET[i].name, nm, sizeof nm); if (!strcmp(nm, a)) return i; }   /* our own name */
	for (int i = 0; i < S_COUNT; i++) {
		const char *p = CAL_SET[i].aliases;
		while (p && *p) {
			const char *e = strchr(p, '|'); int l = e ? (int)(e - p) : (int)strlen(p);
			char al[40]; int m = l < 39 ? l : 39; memcpy(al, p, m); al[m] = 0;
			char sq[40]; squash(al, sq, sizeof sq);
			if (!strcmp(sq, a)) return i;
			p = e ? e + 1 : NULL;
		}
	}
	/* "NT.V.Posistion" (typos), "V Posi": try the start */
	for (int i = 0; i < S_COUNT; i++) {
		const char *p = CAL_SET[i].aliases;
		while (p && *p) {
			const char *e = strchr(p, '|'); int l = e ? (int)(e - p) : (int)strlen(p);
			if (l >= 4 && !strncmp(a, p, l) && (int)strlen(a) <= l + 5) return i;
			p = e ? e + 1 : NULL;
		}
	}
	return -1;
}

/* ================================================================== */
/* faults                                                             */
/* ================================================================== */
enum {
	/* grey */
	F_DARK_GREEN, F_DARK_PINK, F_DARK_BLUE, F_DARK_YELLOW, F_DARK_RED, F_DARK_CYAN,
	F_LIGHT_GREEN, F_LIGHT_PINK, F_LIGHT_BLUE, F_LIGHT_YELLOW, F_LIGHT_RED, F_LIGHT_CYAN,
	F_TRACK_GP, F_TRACK_PG, F_TRACK_BY, F_TRACK_YB,
	F_UNI_BLUE, F_UNI_WARM, F_UNI_GREEN, F_UNI_PINK,
	F_BLACK_CRUSH, F_BLACK_GREY, F_RETRACE, F_WHITE_CLIP, F_DIM, F_PUMP,
	/* colour */
	F_TINT_HUE, F_SAT_HIGH, F_SAT_LOW, F_NO_COLOUR, F_RB_BAL, F_HUE_AXIS, F_SWAP_PBPR, F_YPB_SWAP,
	F_COMP_DC_GP, F_COMP_DC_BY, F_AV_DC, F_PURITY, F_CONVERGE, F_GUN_DEAD, F_BLEED,
	/* geometry */
	F_OVER_V, F_OVER_H, F_UNDER, F_OFF_H, F_OFF_V, F_60HZ, F_ZOOM, F_VLIN, F_VSC, F_PIN, F_KEY, F_CORNER, F_TILT, F_WAVY, F_FOLD,
	/* sharpness */
	F_HALO, F_SOFT, F_FOCUS_EDGE, F_MOIRE, F_SVM, F_DOTCRAWL, F_FLICKER,
	/* v1.9.1 (forum research): stability and signal */
	F_VROLL, F_HTEAR, F_FLAG, F_JITTER, F_DROPOUT, F_RGB_SYNC, F_RGB_CHECKER, F_DIGITAL, F_GHOST, F_HERRING, F_JAILBARS, F_CROSSTALK,
	F_BUZZ, F_HUMBAR, F_WHINE, F_ARCING, F_480P_GREY, F_480P_BLANK,
	/* tube and brightness */
	F_FOG, F_WARM_CAST, F_BURNIN, F_WEAK_TUBE, F_SLOW_START, F_PROTECT, F_DAMPER, F_BRIGHT_FLICKER,
	/* colour, geometry, sharpness */
	F_RGB_FALLBACK, F_RGB_HOT, F_SVIDEO_BW, F_PAL_RGB_NTSC, F_COL_INTERMIT, F_WII_WIDE, F_WII_BORDER, F_SWIM60, F_EW_FAULT, F_GEOM_DRIFT,
	F_VCOLLAPSE, F_HCOLLAPSE, F_RAINBOW, F_DEFLICKER, F_FOCUS_DRIFT,
	F_COUNT
};

#define PIC(p, a, b) { p, a, b }
#define DARKG PIC(PAT_FIELD, 0, 40)
#define LIGHTG PIC(PAT_FIELD, 0, 200)
#define STEPS PIC(PAT_STEPS, 0, 0)
#define ST(s, d, h) { s, d, h }
#define NF(...) { __VA_ARGS__ }, (u8)(sizeof((CalStep[]){ __VA_ARGS__ }) / sizeof(CalStep))

const CalFault CAL_FAULT[F_COUNT] = {
	/* ---- grey: one colour in the dark greys = that colour's BIAS (cut-off) is off ---- */
	[F_DARK_GREEN] = { "Green in the dark greys", AR_GREY, "Dark greys and shadows look green. Bright greys may be fine.",
		"Green bias (cut-off) is too high compared with red and blue.", DARKG,
		NF(ST(S_GBIAS, -1, "Lower G bias 1-2 steps at a time, on the dark grey"), ST(S_RBIAS, +1, "Or raise red and blue bias together (if green bias is missing)"), ST(S_BBIAS, +1, "...and blue bias the same amount"), ST(S_SBRI, 0, "Then check the black level again: bias changes it")), 8, 0 },
	[F_DARK_PINK] = { "Pink / magenta in the dark greys", AR_GREY, "Dark greys look pink or purple.",
		"Green bias is too LOW (or red and blue bias too high).", DARKG,
		NF(ST(S_GBIAS, +1, "Raise G bias 1-2 steps at a time, on the dark grey"), ST(S_RBIAS, -1, "Or lower red bias..."), ST(S_BBIAS, -1, "...and blue bias together"), ST(S_SBRI, 0, "Then check the black level again")), 6, 0 },
	[F_DARK_BLUE] = { "Blue in the dark greys", AR_GREY, "Shadows and dark greys look blue.",
		"Blue bias too high.", DARKG, NF(ST(S_BBIAS, -1, "Lower B bias 1-2 steps"), ST(S_RBIAS, +1, "Or raise red and green bias"), ST(S_GBIAS, +1, "...together")), 6, 0 },
	[F_DARK_YELLOW] = { "Yellow / brown in the dark greys", AR_GREY, "Dark greys look yellow, brown or olive.",
		"Blue bias too LOW.", DARKG, NF(ST(S_BBIAS, +1, "Raise B bias 1-2 steps"), ST(S_RBIAS, -1, "Or lower red and green bias"), ST(S_GBIAS, -1, "...together")), 5, 0 },
	[F_DARK_RED] = { "Red in the dark greys", AR_GREY, "Dark greys look red or brown-red.",
		"Red bias too high.", DARKG, NF(ST(S_RBIAS, -1, "Lower R bias 1-2 steps"), ST(S_GBIAS, +1, "Or raise green and blue bias"), ST(S_BBIAS, +1, "...together")), 5, 0 },
	[F_DARK_CYAN] = { "Cyan / teal in the dark greys", AR_GREY, "Dark greys look cyan, teal or blue-green.",
		"Red bias too LOW.", DARKG, NF(ST(S_RBIAS, +1, "Raise R bias 1-2 steps"), ST(S_GBIAS, -1, "Or lower green and blue bias"), ST(S_BBIAS, -1, "...together")), 5, 0 },
	/* ---- one colour in the light greys = that colour's DRIVE (gain) is off ---- */
	[F_LIGHT_GREEN] = { "Green in the light greys", AR_GREY, "Light greys and white look green.",
		"Green drive too high (or red and blue drive too low).", LIGHTG, NF(ST(S_GDRV, -1, "Lower G drive 1-2 steps, on the light grey"), ST(S_RDRV, +1, "No G drive? Raise red drive..."), ST(S_BDRV, +1, "...and blue drive together")), 5, 0 },
	[F_LIGHT_PINK] = { "Pink / magenta in the light greys", AR_GREY, "Light greys and white look pink or purple.",
		"Green drive too LOW (or red and blue drive too high).", LIGHTG, NF(ST(S_GDRV, +1, "Raise G drive 1-2 steps, on the light grey"), ST(S_RDRV, -1, "No G drive? Lower red drive..."), ST(S_BDRV, -1, "...and blue drive together")), 6, 0 },
	[F_LIGHT_BLUE] = { "Blue in the light greys", AR_GREY, "White and light greys look blue.",
		"Blue drive too high (or colour temperature on Cool).", LIGHTG, NF(ST(S_COLTEMP, 0, "Try Colour temperature: Normal or Warm first"), ST(S_BDRV, -1, "Lower B drive 1-2 steps"), ST(S_RDRV, +1, "Or raise red drive a little")), 7, 0 },
	[F_LIGHT_YELLOW] = { "Yellow in the light greys", AR_GREY, "White and light greys look yellow or cream.",
		"Blue drive too LOW (or colour temperature on Warm).", LIGHTG, NF(ST(S_COLTEMP, 0, "Try Colour temperature: Normal first"), ST(S_BDRV, +1, "Raise B drive 1-2 steps"), ST(S_RDRV, -1, "Or lower red drive a little")), 5, 0 },
	[F_LIGHT_RED] = { "Red in the light greys", AR_GREY, "White and light greys look reddish or orange.",
		"Red drive too high (or colour temperature on Warm).", LIGHTG, NF(ST(S_COLTEMP, 0, "Try Colour temperature: Normal"), ST(S_RDRV, -1, "Lower R drive 1-2 steps")), 5, 0 },
	[F_LIGHT_CYAN] = { "Cyan in the light greys", AR_GREY, "White and light greys look cyan or blue-green.",
		"Red drive too LOW.", LIGHTG, NF(ST(S_RDRV, +1, "Raise R drive 1-2 steps"), ST(S_BDRV, -1, "Or lower blue drive a little")), 4, 0 },
	/* ---- tracking: darks one colour, lights the opposite ---- */
	[F_TRACK_GP] = { "Green darks, pink lights", AR_GREY, "Dark greys green AND light greys pink. The normal menu can't fix it.",
		"Grey scale tracking: green bias too high and green drive too low. Bias rules the darks, drive the lights, so each end goes a different way.", STEPS,
		NF(ST(W_INPUT, 0, "Only on one input? Check the cable and that input's own settings first"), ST(S_GBIAS, -1, "Darks: lower G bias on the DARK grey until it's neutral"), ST(S_GDRV, +1, "Lights: raise G drive on the LIGHT grey (no G drive: lower R and B drive)"), ST(S_RDRV, -1, "...lower R drive"), ST(S_BDRV, -1, "...and B drive together"), ST(S_GBIAS, 0, "Go back to the dark grey: drive nudges it. Repeat 2-3 times"), ST(S_SBRI, 0, "Last: check the black level")), 7, 0 },
	[F_TRACK_PG] = { "Pink darks, green lights", AR_GREY, "Dark greys pink AND light greys green.",
		"Tracking: green bias too low and green drive too high.", STEPS,
		NF(ST(S_GBIAS, +1, "Darks: raise G bias on the dark grey"), ST(S_GDRV, -1, "Lights: lower G drive on the light grey (no G drive: raise R and B drive)"), ST(S_RDRV, +1, "...raise R drive"), ST(S_BDRV, +1, "...and B drive"), ST(S_GBIAS, 0, "Repeat darks, then lights, 2-3 times")), 5, 0 },
	[F_TRACK_BY] = { "Blue darks, yellow lights", AR_GREY, "Dark greys blue, light greys yellow.",
		"Tracking: blue bias too high and blue drive too low.", STEPS,
		NF(ST(S_BBIAS, -1, "Darks: lower B bias on the dark grey"), ST(S_BDRV, +1, "Lights: raise B drive on the light grey"), ST(S_BBIAS, 0, "Repeat 2-3 times")), 4, 0 },
	[F_TRACK_YB] = { "Yellow darks, blue lights", AR_GREY, "Dark greys yellow or brown, light greys blue.",
		"Tracking: blue bias too low and blue drive too high.", STEPS,
		NF(ST(S_BBIAS, +1, "Darks: raise B bias on the dark grey"), ST(S_BDRV, -1, "Lights: lower B drive on the light grey"), ST(S_BBIAS, 0, "Repeat 2-3 times")), 5, 0 },
	/* ---- the same tint at every brightness ---- */
	[F_UNI_BLUE] = { "Everything slightly blue", AR_GREY, "Greys and white all lean blue, dark and light alike.",
		"Colour temperature on Cool (very common factory setting), or blue drive and bias both high.", PIC(PAT_GREY50, 0, 0),
		NF(ST(S_COLTEMP, 0, "Set Colour temperature to Normal or Warm"), ST(S_PMODE, 0, "Picture mode: Standard or Movie, not Dynamic"), ST(S_BDRV, -1, "Still blue: lower B drive a step..."), ST(S_BBIAS, -1, "...and B bias a step")), 9, 0 },
	[F_UNI_WARM] = { "Everything red / yellow", AR_GREY, "Greys and white all lean red, orange or yellow.",
		"Colour temperature on Warm, or red drive and bias both high.", PIC(PAT_GREY50, 0, 0),
		NF(ST(S_COLTEMP, 0, "Set Colour temperature to Normal"), ST(S_RDRV, -1, "Lower R drive a step..."), ST(S_RBIAS, -1, "...and R bias a step")), 5, 0 },
	[F_UNI_GREEN] = { "Everything green", AR_GREY, "Greys all lean green at every brightness.",
		"Green bias and drive both high, OR a colour offset on that input (component DC levels, cable).", PIC(PAT_GREY50, 0, 0),
		NF(ST(W_INPUT, 0, "Other inputs fine? It's that input: see the component DC levels"), ST(S_YUV_RYDC, +1, "Component only: raise YUV R-Y DC a step (less green)"), ST(S_GBIAS, -1, "All inputs: lower G bias..."), ST(S_GDRV, -1, "...and G drive a step each")), 5, 0 },
	[F_UNI_PINK] = { "Everything pink / purple", AR_GREY, "Greys all lean pink or purple at every brightness.",
		"Green too low overall, or a colour offset on that input.", PIC(PAT_GREY50, 0, 0),
		NF(ST(W_INPUT, 0, "Other inputs fine? It's that input"), ST(S_YUV_RYDC, -1, "Component only: lower YUV R-Y DC a step (less pink)"), ST(S_GBIAS, +1, "All inputs: raise G bias..."), ST(S_GDRV, +1, "...and G drive a step each")), 4, 0 },
	/* ---- black and white level ---- */
	[F_BLACK_CRUSH] = { "Dark detail lost", AR_GREY, "Shadows are solid black: the darkest bars of the PLUGE don't show.",
		"Black level too low.", PIC(PAT_PLUGE, 0, 0), NF(ST(S_BRIGHT, +1, "Raise Brightness until the darkest bar just shows"), ST(S_SBRI, +1, "Brightness maxed out? Raise sub brightness a step, then Brightness back to middle")), 7, 0 },
	[F_BLACK_GREY] = { "Blacks look grey", AR_GREY, "Black is dark grey, the picture looks washed out, even in a dark room.",
		"Black level too high: Brightness, sub brightness, or the screen knob (G2).", PIC(PAT_PLUGE, 0, 0),
		NF(ST(S_BRIGHT, -1, "Lower Brightness until the blackest bar disappears"), ST(S_SBRI, -1, "Still grey at low Brightness: lower sub brightness a step"), ST(H_SCREEN, -1, "Still grey: the screen knob (G2) is too high: technician")), 7, 0 },
	[F_RETRACE] = { "Thin diagonal lines", AR_GREY, "Faint slanted lines across the picture, easiest to see on dark screens, often at the top.",
		"Retrace lines: black level set too high (sub brightness / screen knob).", PIC(PAT_FIELD, 0, 10),
		NF(ST(S_BRIGHT, -1, "Lower Brightness"), ST(S_SBRI, -1, "Lower sub brightness until they just go"), ST(H_SCREEN, -1, "Still there: the screen knob (G2) is set too high: technician")), 5, 1 },
	[F_WHITE_CLIP] = { "Whites merge / bloom", AR_GREY, "Bright blocks all look the same white; bright things swell or blur.",
		"Contrast (white level) too high.", PIC(PAT_WHITECLIP, 0, 0), NF(ST(S_CONTRAST, -1, "Lower Contrast until every block shows"), ST(S_SCON, -1, "Contrast low and still clipping: lower sub contrast a step")), 7, 0 },
	[F_DIM] = { "Picture too dim", AR_GREY, "Everything is dim even with Contrast up.",
		"Sub contrast low, an eco / picture mode, or a worn tube.", PIC(PAT_WHITECLIP, 0, 0),
		NF(ST(S_PMODE, 0, "Check the picture mode and any eco / power saving setting"), ST(S_SCON, +1, "Raise sub contrast a step"), ST(H_TECH, 0, "Still dim and soft: the tube may be worn: technician")), 3, 1 },
	[F_PUMP] = { "Picture breathes", AR_GREY, "The picture grows or dims when a bright scene comes on.",
		"High voltage regulation: bright scenes load the supply. Some is normal on old sets.", PIC(PAT_BLOOM, 0, 0),
		NF(ST(S_CONTRAST, -1, "Lower Contrast: less load, less breathing"), ST(S_SCON, -1, "Or sub contrast a step"), ST(H_TECH, 0, "A lot of it: failing parts: technician")), 3, 1 },

	/* ---- colour ---- */
	[F_TINT_HUE] = { "Faces too green or too purple", AR_COLOUR, "Skin looks greenish or purple; yellow looks off.",
		"Tint / hue (NTSC signals). PAL sets usually have no Tint.", PIC(PAT_SKIN, 0, 0),
		NF(ST(S_TINT, 0, "Menu: Tint until faces look natural"), ST(S_STINT, 0, "Tint maxed out: sub tint a step"), ST(W_VIDEO, 0, "Wii at 60 Hz sends NTSC-style colour: try PAL60 / 50 Hz to compare")), 6, 0 },
	[F_SAT_HIGH] = { "Colours too strong", AR_COLOUR, "Colours glow, reds bleed and smear.",
		"Colour too high (or Dynamic picture mode).", PIC(PAT_SMPTE, 0, 0), NF(ST(S_COLOUR, -1, "Menu: Colour down until the red bar stops glowing"), ST(S_PMODE, 0, "Not Dynamic mode"), ST(S_SCOL, -1, "Colour low and still too strong: sub colour a step")), 6, 0 },
	[F_SAT_LOW] = { "Colours washed out", AR_COLOUR, "Colours look pale and faded.",
		"Colour too low, or a cable / input problem.", PIC(PAT_SMPTE, 0, 0), NF(ST(S_COLOUR, +1, "Menu: Colour up until colours look rich but the red bar doesn't glow"), ST(W_CABLE, 0, "Check the cable and plugs"), ST(S_SCOL, +1, "Colour maxed out: sub colour a step")), 5, 0 },
	[F_NO_COLOUR] = { "No colour, or colour flashes", AR_COLOUR, "Black & white picture, or colour that comes and goes / rolls.",
		"The TV is decoding the wrong colour system (PAL / NTSC / PAL60), or a bad cable.", PIC(PAT_SMPTE, 0, 0),
		NF(ST(S_COLSYS, 0, "Colour system: Auto, or match the Wii (60 Hz = NTSC/PAL60, 50 Hz = PAL)"), ST(W_VIDEO, 0, "Try the Wii at 50 Hz, or 60 Hz, and compare"), ST(W_CABLETYPE, 0, "Use component: no colour decoding to go wrong")), 5, 0 },
	[F_RB_BAL] = { "Reds and blues out of balance", AR_COLOUR, "Reds far stronger than blues (or the other way) with Colour set right. Greys are fine.",
		"R-Y / B-Y gain balance.", PIC(PAT_SMPTE, 0, 0),
		NF(ST(S_COLOUR, 0, "Set Colour first (red bar strong but clean)"), ST(S_RYBY_GBL, 0, "Then R-Y/B-Y G.BL until red and blue bars look equally strong")), 3, 0 },
	[F_HUE_AXIS] = { "Greens and magentas wrong", AR_COLOUR, "Tint is right for skin, but greens look yellow-green or magenta is off. Greys fine.",
		"R-Y / B-Y demodulation angle.", PIC(PAT_SMPTE, 0, 0), NF(ST(S_TINT, 0, "Set Tint first"), ST(S_RYBY_ANG, 0, "Then R-Y/B-Y angle a step at a time, green bar vs magenta bar")), 2, 0 },
	[F_SWAP_PBPR] = { "Reds and blues swapped", AR_COLOUR, "Red things look blue and blue things look red; greys are fine. Component only.",
		"The red (Pr) and blue (Pb) component plugs are swapped.", PIC(PAT_SMPTE, 0, 0), NF(ST(W_CABLE, 0, "Swap the red and blue VIDEO plugs (the red one next to green)")), 4, 0 },
	[F_YPB_SWAP] = { "Whole picture green or purple, very wrong", AR_COLOUR, "Very green (or purple) picture, wrong brightness, component only.",
		"The green (Y) plug is in a colour socket: Y and Pb / Pr are mixed up, or the input is set to RGB.", PIC(PAT_SMPTE, 0, 0),
		NF(ST(W_CABLE, 0, "Green plug into the green (Y) socket, blue to Pb, red to Pr"), ST(W_INPUT, 0, "Set that input to YPbPr / component, not RGB")), 3, 0 },
	[F_COMP_DC_GP] = { "Component: everything leans green or pink", AR_COLOUR, "Only on component: all greys lean green (or pink), dark and light the same. Other inputs fine.",
		"The component input's red colour offset (YUV R-Y DC) is off.", PIC(PAT_GREY50, 0, 0),
		NF(ST(W_CABLE, 0, "Check the plugs are pushed right in"), ST(S_YUV_RYDC, 0, "YUV R-Y DC a step at a time on 50% grey until neutral"), ST(S_YUV_BYDC, 0, "Then YUV B-Y DC if it leans blue / yellow")), 3, 0 },
	[F_COMP_DC_BY] = { "Component: everything leans blue or yellow", AR_COLOUR, "Only on component: all greys lean blue (or yellow). Other inputs fine.",
		"The component input's blue colour offset (YUV B-Y DC) is off.", PIC(PAT_GREY50, 0, 0),
		NF(ST(W_CABLE, 0, "Check the plugs"), ST(S_YUV_BYDC, 0, "YUV B-Y DC a step at a time on 50% grey until neutral")), 3, 0 },
	[F_AV_DC] = { "AV / aerial: everything leans a colour", AR_COLOUR, "Only on AV or aerial: all greys lean blue / yellow (or pink / green). Component fine.",
		"The decoder's colour offsets (B-Y DC level) for those inputs.", PIC(PAT_GREY50, 0, 0),
		NF(ST(S_BYDC, 0, "B-Y DC level a step at a time on grey"), ST(S_SEC_BYDC, 0, "Leave the SECAM ones: the Wii never sends SECAM")), 2, 0 },
	[F_PURITY] = { "Colour patches", AR_COLOUR, "Blotches of colour (often purple, green or yellow) in corners or patches on a white or single-colour screen.",
		"The tube is magnetised (speakers, magnets, moving the TV) or purity is off.", PIC(PAT_FIELD, 0, 255),
		NF(ST(H_DEGAUSS, 0, "Keep magnets and speakers away. Turn off 20 min, then on (it degausses). Repeat 2-3 times"), ST(H_CONV, 0, "Still there after a week: purity rings: technician")), 7, 1 },
	[F_CONVERGE] = { "Colour fringes on lines", AR_COLOUR, "White lines have red, green or blue edges, worst near the sides.",
		"Convergence: the three beams don't land together. Magnets on the neck.", PIC(PAT_CONVERGE, 0, 0),
		NF(ST(H_DEGAUSS, 0, "Degauss first (turn off 20 min, on)"), ST(H_CONV, 0, "Convergence magnets: technician")), 5, 1 },
	[F_GUN_DEAD] = { "One colour missing", AR_COLOUR, "Red, green or blue is missing everywhere: the red-only (or green / blue) screen is black.",
		"A colour isn't reaching the tube: cable, neck board or the tube itself.", PIC(PAT_FIELD, 1, 255),
		NF(ST(W_CABLE, 0, "Try another input and cable first"), ST(W_INPUT, 0, "Missing on every input: it's the TV"), ST(H_TECH, 0, "Neck board or tube fault: technician")), 2, 1 },
	[F_BLEED] = { "Colour smears", AR_COLOUR, "Colour runs past edges (especially red), or is shifted a little right.",
		"Composite / RF signal, colour too high, or noise reduction.", PIC(PAT_BLEED, 0, 0),
		NF(ST(W_CABLETYPE, 0, "Use component (or S-Video)"), ST(S_COLOUR, -1, "Lower Colour a little"), ST(S_NR, 0, "Turn noise reduction off")), 6, 0 },

	/* ---- geometry ---- */
	[F_OVER_V] = { "Top / bottom cut off", AR_GEOM, "Part of the top or bottom is off the screen.",
		"Height too big (V size). At 60 Hz it's the 60 Hz height.", PIC(PAT_OVERSCAN, 0, 0),
		NF(ST(S_ASPECT, 0, "Aspect: 4:3 / Normal, not Zoom or Wide"), ST(W_VIDEO, 0, "Wii at 60 Hz? Change the 60 Hz (NT.) settings, not the PAL ones"), ST(S_VSIZE, -1, "Lower V size (NT.V.Phase / 60 Hz height at 60 Hz)"), ST(S_VPOS, 0, "Then centre it with V position")), 8, 0 },
	[F_OVER_H] = { "Sides cut off", AR_GEOM, "Part of the left or right is off the screen.",
		"Width too big (H size / EW width), a zoom mode, or off centre.", PIC(PAT_OVERSCAN, 0, 0),
		NF(ST(S_ASPECT, 0, "Aspect: 4:3 / Normal"), ST(S_HPHASE, 0, "Only one side cut? Centre it with H phase first"), ST(S_HSIZE, -1, "Lower H size / EW width (60 Hz set at 60 Hz)"), ST(H_TECH, 0, "No width setting: it's a knob inside or fixed: technician")), 7, 0 },
	[F_UNDER] = { "Black borders", AR_GEOM, "Black bands around the picture: it's too small.",
		"Size set too small (or the Wii's own border).", PIC(PAT_OVERSCAN, 0, 0),
		NF(ST(W_VIDEO, 0, "Wii games often have a thin border of their own: judge on the test picture"), ST(S_VSIZE, +1, "Raise V size"), ST(S_HSIZE, +1, "Raise H size / EW width")), 4, 0 },
	[F_OFF_H] = { "Off centre sideways", AR_GEOM, "More cut off (or more border) on one side than the other.",
		"H phase / H position.", PIC(PAT_EDGE, 0, 0), NF(ST(S_HPHASE, 0, "H phase a step at a time (60 Hz: NT.H.Phase) until even")), 6, 0 },
	[F_OFF_V] = { "Off centre up / down", AR_GEOM, "More cut off at the top than the bottom, or the other way.",
		"V position.", PIC(PAT_EDGE, 0, 0), NF(ST(S_VPOS, 0, "V position a step at a time (60 Hz: NT.V.Posi) until even")), 5, 0 },
	[F_60HZ] = { "Wrong only at 60 Hz", AR_GEOM, "Size or position is fine at 50 Hz but wrong at 60 Hz (or the other way).",
		"The TV keeps separate geometry for 50 Hz and 60 Hz (NTSC). The Wii at 60 Hz uses the 60 Hz set.", PIC(PAT_OVERSCAN, 0, 0),
		NF(ST(W_VIDEO, 0, "Set the Wii to the mode you play in"), ST(S_NT_VPHASE, 0, "Change the NT. settings: NT.V.Phase / size first"), ST(S_NT_VPOS, 0, "...then NT.V.Posi"), ST(S_NT_HPHASE, 0, "...then NT.H.Phase"), ST(S_NT_VLIN, 0, "...and NT.V.Line if top and bottom differ")), 7, 0 },
	[F_ZOOM] = { "Stretched by a zoom mode", AR_GEOM, "Picture stretched, cut or squashed like a zoom: circles aren't round.",
		"Aspect / zoom mode, or Zoom / Wide size in the service menu.", PIC(PAT_GRID, 0, 0),
		NF(ST(S_ASPECT, 0, "Aspect: 4:3 / Normal (and the Wii set to 4:3 for a 4:3 TV)"), ST(S_ZOOM, -1, "Only in Zoom mode: Zoom size"), ST(S_WIDE, -1, "Only in Wide mode: Wide size")), 5, 0 },
	[F_VLIN] = { "Top and bottom uneven", AR_GEOM, "Squares taller at the top than the bottom (or the other way).",
		"Vertical linearity (V.Line).", PIC(PAT_GRID, 0, 0), NF(ST(S_VLIN, 0, "V linearity a step at a time until the top and bottom squares match"), ST(S_VSIZE, 0, "Then height again"), ST(S_VPOS, 0, "...and centre again")), 5, 0 },
	[F_VSC] = { "Middle vs edges uneven", AR_GEOM, "Squares in the middle a different height from those at the top and bottom.",
		"Vertical S-correction (V.SC).", PIC(PAT_GRID, 0, 0), NF(ST(S_VSC, 0, "V S-correction a step at a time"), ST(S_VSIZE, 0, "Then height again")), 3, 0 },
	[F_PIN] = { "Sides bowed", AR_GEOM, "Left and right edges bow in (pincushion) or out (barrel).",
		"East-west parabola.", PIC(PAT_GRID, 0, 0), NF(ST(S_EWPARA, 0, "EW parabola / pincushion until the side lines are straight"), ST(S_EWCORN, 0, "Then corners")), 4, 0 },
	[F_KEY] = { "Top and bottom different widths", AR_GEOM, "Picture is a trapezium: top wider or narrower than the bottom.",
		"EW trapezium / keystone.", PIC(PAT_GRID, 0, 0), NF(ST(S_EWTRAP, 0, "Trapezium until top and bottom are the same width")), 3, 0 },
	[F_CORNER] = { "Corners bent", AR_GEOM, "Sides straight in the middle but bent near the corners.",
		"EW corner.", PIC(PAT_GRID, 0, 0), NF(ST(S_EWCORN, 0, "EW corner (upper / lower) a step at a time")), 3, 0 },
	[F_TILT] = { "Picture tilted", AR_GEOM, "The whole picture is rotated a little.",
		"Earth's magnetic field (the TV was moved or turned), magnetism, or the yoke.", PIC(PAT_GRID, 0, 0),
		NF(ST(S_TILT, 0, "Tilt / rotation setting if your TV has one"), ST(H_DEGAUSS, 0, "Degauss (off 20 min, on)"), ST(H_TECH, 0, "Still tilted: the yoke can be turned: technician")), 4, 1 },
	[F_WAVY] = { "Wavy or wobbly edges", AR_GEOM, "Straight lines ripple or wobble, or a slow wave rolls up the screen.",
		"Power supply hum (old capacitors) or something magnetic nearby.", PIC(PAT_GRID, 0, 0),
		NF(ST(H_DEGAUSS, 0, "Move speakers / other TVs / power boards away"), ST(H_TECH, 0, "Still wavy: capacitors: technician")), 2, 1 },
	[F_FOLD] = { "Folded or bright band at the top", AR_GEOM, "The top is squashed into bright folded lines.",
		"Vertical deflection fault.", PIC(PAT_GRID, 0, 0), NF(ST(S_VLIN, 0, "V linearity can hide a little of it"), ST(H_TECH, 0, "Vertical output fault: technician")), 1, 1 },

	/* ---- sharpness ---- */
	[F_HALO] = { "White outlines on edges", AR_SHARP, "Edges have white (or black) outlines; fine lines look ringed.",
		"Too much sharpness / edge boost.", PIC(PAT_SHARP, 0, 0), NF(ST(S_SHARP, -1, "Lower Sharpness until the halos go"), ST(S_VMU, 0, "Turn velocity modulation (VM / SVM) off"), ST(S_SSHARP, -1, "Sharpness at 0 and still halos: sub sharpness a step")), 7, 0 },
	[F_SOFT] = { "Picture soft everywhere", AR_SHARP, "Fine detail blurry all over the screen.",
		"Sharpness too low, composite cable, or focus.", PIC(PAT_SHARP, 0, 0),
		NF(ST(W_CABLETYPE, 0, "Use component (or S-Video)"), ST(S_SHARP, +1, "Raise Sharpness until just before halos"), ST(H_FOCUS, 0, "Still soft: the focus knob: technician")), 5, 1 },
	[F_FOCUS_EDGE] = { "Soft only at the edges", AR_SHARP, "Sharp in the middle, blurry in the corners.",
		"Focus (and dynamic focus). Some softness in the corners is normal for a CRT.", PIC(PAT_SHARP, 0, 0),
		NF(ST(H_FOCUS, 0, "Focus knob on the flyback (split focus on big sets): technician")), 3, 1 },
	[F_MOIRE] = { "Shimmer on fine patterns", AR_SHARP, "Fine stripes shimmer or show wavy patterns.",
		"Moire: fine detail against the tube's mask. Worse with sharpness up.", PIC(PAT_STRIPES, 0, 0),
		NF(ST(S_SHARP, -1, "Lower Sharpness"), ST(W_VIDEO, 0, "Wii deflicker (Calibration > Video) softens it")), 4, 0 },
	[F_SVM] = { "Thick, uneven edges", AR_SHARP, "Edges look thick, and bright areas look narrower than they should.",
		"Velocity modulation (VM / SVM).", PIC(PAT_SHARP, 0, 0), NF(ST(S_VMU, 0, "Turn VM / SVM off in the normal menu"), ST(S_SVM, -1, "Or lower the SVM level in the service menu")), 3, 0 },
	[F_DOTCRAWL] = { "Crawling dots on edges", AR_SHARP, "Dots crawl along colour edges; rainbow shimmer on fine lines.",
		"Composite (yellow plug) video: colour and brightness share one wire.", PIC(PAT_SMPTE, 0, 0), NF(ST(W_CABLETYPE, 0, "Use component (or S-Video)")), 5, 0 },
	[F_FLICKER] = { "Thin lines flicker", AR_SHARP, "Thin horizontal lines flicker (interlace).",
		"Interlaced video (480i / 576i). Normal for CRTs; the Wii can soften it.", PIC(PAT_STRIPES, 0, 0),
		NF(ST(W_VIDEO, 0, "Wii deflicker on (Calibration > Video), or 480p if the TV takes it")), 4, 0 },

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
const int CAL_NFAULT = F_COUNT;

/* ================================================================== */
/* questions                                                          */
/* ================================================================== */
#define QP(f, p) { f, 0, p }          /* yes / no question: chance of "yes" */
#define QO(f, o, p) { f, o, p }       /* choice question: chance of answer o */
#define ENT(...) { __VA_ARGS__ }, (u8)(sizeof((CalQP[]){ __VA_ARGS__ }) / sizeof(CalQP))
#define YN(text, area, pic, leak, gate, ...) { text, area, pic, { "Yes", "No" }, { leak, 100 - leak }, ENT(__VA_ARGS__), gate }
#define OPTS(...) { __VA_ARGS__ }
#define BASE(...) { __VA_ARGS__ }
#define CH(text, area, pic, opts, base, ...) { text, area, pic, opts, base, ENT(__VA_ARGS__), 0 }
#define TINTS OPTS("Plain grey", "Green", "Pink / purple", "Blue", "Yellow / brown", "Red / orange", "Cyan / teal")

const CalQuestion CAL_Q[] = {
	/* ---- openers: which area (yes / no, as asked for) ---- */
	YN("Is it a problem with colours or tints (including grey that isn't grey)?", AR_ALL, PIC(PAT_SMPTE, 0, 0), 12, 1,
		QP(F_DARK_GREEN, 90), QP(F_DARK_PINK, 90), QP(F_DARK_BLUE, 90), QP(F_DARK_YELLOW, 90), QP(F_DARK_RED, 90), QP(F_DARK_CYAN, 90),
		QP(F_LIGHT_GREEN, 90), QP(F_LIGHT_PINK, 90), QP(F_LIGHT_BLUE, 85), QP(F_LIGHT_YELLOW, 85), QP(F_LIGHT_RED, 85), QP(F_LIGHT_CYAN, 90),
		QP(F_TRACK_GP, 95), QP(F_TRACK_PG, 95), QP(F_TRACK_BY, 95), QP(F_TRACK_YB, 95), QP(F_UNI_BLUE, 85), QP(F_UNI_WARM, 85), QP(F_UNI_GREEN, 90), QP(F_UNI_PINK, 90),
		QP(F_TINT_HUE, 90), QP(F_SAT_HIGH, 85), QP(F_SAT_LOW, 85), QP(F_NO_COLOUR, 95)),
	YN("Is it a problem with colours on colourful things (bars, faces), rather than greys?", AR_ALL, PIC(PAT_SMPTE, 0, 0), 10, 1,
		QP(F_TINT_HUE, 92), QP(F_SAT_HIGH, 90), QP(F_SAT_LOW, 90), QP(F_NO_COLOUR, 92), QP(F_RB_BAL, 90), QP(F_HUE_AXIS, 90), QP(F_SWAP_PBPR, 95),
		QP(F_YPB_SWAP, 90), QP(F_PURITY, 85), QP(F_CONVERGE, 85), QP(F_GUN_DEAD, 90), QP(F_BLEED, 90), QP(F_COMP_DC_GP, 30), QP(F_COMP_DC_BY, 30), QP(F_AV_DC, 30)),
	YN("Is it the size, shape or position of the picture?", AR_ALL, PIC(PAT_GRID, 0, 0), 8, 1,
		QP(F_OVER_V, 95), QP(F_OVER_H, 95), QP(F_UNDER, 95), QP(F_OFF_H, 95), QP(F_OFF_V, 95), QP(F_60HZ, 95), QP(F_ZOOM, 95), QP(F_VLIN, 90),
		QP(F_VSC, 90), QP(F_PIN, 90), QP(F_KEY, 90), QP(F_CORNER, 85), QP(F_TILT, 95), QP(F_WAVY, 90), QP(F_FOLD, 90)),
	YN("Is it blurriness, outlines or shimmer on detail?", AR_ALL, PIC(PAT_SHARP, 0, 0), 8, 1,
		QP(F_HALO, 95), QP(F_SOFT, 95), QP(F_FOCUS_EDGE, 90), QP(F_MOIRE, 90), QP(F_SVM, 85), QP(F_DOTCRAWL, 85), QP(F_FLICKER, 85), QP(F_CONVERGE, 40), QP(F_BLEED, 40)),
	YN("Is it how dark or bright things are (blacks, whites)?", AR_ALL, PIC(PAT_PLUGE, 0, 0), 10, 1,
		QP(F_BLACK_CRUSH, 95), QP(F_BLACK_GREY, 95), QP(F_RETRACE, 70), QP(F_WHITE_CLIP, 95), QP(F_DIM, 95), QP(F_PUMP, 80)),

	/* ---- greys: which colour, where ---- */
	CH("Look at the DARK grey. Which colour is it closest to?", AR_GREY, DARKG, TINTS, BASE(84, 4, 3, 3, 2, 2, 2),
		QO(F_DARK_GREEN, 1, 85), QO(F_TRACK_GP, 1, 85), QO(F_UNI_GREEN, 1, 80), QO(F_COMP_DC_GP, 1, 45), QO(F_COMP_DC_GP, 2, 40),
		QO(F_DARK_PINK, 2, 85), QO(F_TRACK_PG, 2, 85), QO(F_UNI_PINK, 2, 80),
		QO(F_DARK_BLUE, 3, 85), QO(F_TRACK_BY, 3, 85), QO(F_UNI_BLUE, 3, 55), QO(F_COMP_DC_BY, 3, 45), QO(F_COMP_DC_BY, 4, 40),
		QO(F_DARK_YELLOW, 4, 85), QO(F_TRACK_YB, 4, 85), QO(F_UNI_WARM, 4, 40), QO(F_UNI_WARM, 5, 40),
		QO(F_DARK_RED, 5, 85), QO(F_DARK_CYAN, 6, 80), QO(F_AV_DC, 3, 30), QO(F_AV_DC, 4, 30)),
	CH("Now the LIGHT grey. Which colour is it closest to?", AR_GREY, LIGHTG, TINTS, BASE(80, 3, 3, 6, 3, 3, 2),
		QO(F_LIGHT_GREEN, 1, 85), QO(F_TRACK_PG, 1, 85), QO(F_UNI_GREEN, 1, 80), QO(F_COMP_DC_GP, 1, 45), QO(F_COMP_DC_GP, 2, 40),
		QO(F_LIGHT_PINK, 2, 85), QO(F_TRACK_GP, 2, 85), QO(F_UNI_PINK, 2, 80),
		QO(F_LIGHT_BLUE, 3, 85), QO(F_TRACK_YB, 3, 85), QO(F_UNI_BLUE, 3, 85), QO(F_COMP_DC_BY, 3, 45), QO(F_COMP_DC_BY, 4, 40),
		QO(F_LIGHT_YELLOW, 4, 85), QO(F_TRACK_BY, 4, 85), QO(F_UNI_WARM, 4, 45), QO(F_UNI_WARM, 5, 45),
		QO(F_LIGHT_RED, 5, 85), QO(F_LIGHT_CYAN, 6, 80), QO(F_AV_DC, 3, 30), QO(F_AV_DC, 4, 30)),
	CH("Grey steps, dark to light. Where is there a tint?", AR_GREY, STEPS,
		OPTS("No tint anywhere", "The same tint all the way", "Changes colour from dark to light", "Only in the dark steps", "Only in the light steps"), BASE(82, 6, 3, 5, 4),
		QO(F_TRACK_GP, 2, 85), QO(F_TRACK_PG, 2, 85), QO(F_TRACK_BY, 2, 85), QO(F_TRACK_YB, 2, 85),
		QO(F_UNI_BLUE, 1, 80), QO(F_UNI_WARM, 1, 80), QO(F_UNI_GREEN, 1, 80), QO(F_UNI_PINK, 1, 80), QO(F_COMP_DC_GP, 1, 80), QO(F_COMP_DC_BY, 1, 80), QO(F_AV_DC, 1, 75),
		QO(F_DARK_GREEN, 3, 75), QO(F_DARK_PINK, 3, 75), QO(F_DARK_BLUE, 3, 75), QO(F_DARK_YELLOW, 3, 75), QO(F_DARK_RED, 3, 75), QO(F_DARK_CYAN, 3, 75),
		QO(F_LIGHT_GREEN, 4, 75), QO(F_LIGHT_PINK, 4, 75), QO(F_LIGHT_BLUE, 4, 70), QO(F_LIGHT_YELLOW, 4, 70), QO(F_LIGHT_RED, 4, 70), QO(F_LIGHT_CYAN, 4, 75)),
	CH("Is it on every input? (Compare aerial TV, AV, or the TV's own menu screens)", AR_ALL, PIC(PAT_GREY50, 0, 0),
		OPTS("Every input", "Only component", "Only AV / aerial / S-Video"), BASE(84, 8, 8),
		QO(F_COMP_DC_GP, 1, 90), QO(F_COMP_DC_BY, 1, 90), QO(F_SWAP_PBPR, 1, 95), QO(F_YPB_SWAP, 1, 95), QO(F_AV_DC, 2, 90),
		QO(F_NO_COLOUR, 2, 45), QO(F_BLEED, 2, 55), QO(F_DOTCRAWL, 2, 85), QO(F_60HZ, 1, 25), QO(F_60HZ, 2, 25)),
	YN("Does changing Colour temperature (Cool / Normal / Warm) in the normal menu fix the tint?", AR_GREY, PIC(PAT_GREY50, 0, 0), 5, 0,
		QP(F_UNI_BLUE, 75), QP(F_UNI_WARM, 70), QP(F_LIGHT_BLUE, 55), QP(F_LIGHT_YELLOW, 45), QP(F_LIGHT_RED, 40)),
	YN("Do Colour, Tint and the picture modes in the normal menu leave the grey tint as it is?", AR_GREY, PIC(PAT_GREY50, 0, 0), 30, 0,
		QP(F_TRACK_GP, 92), QP(F_TRACK_PG, 92), QP(F_TRACK_BY, 90), QP(F_TRACK_YB, 90), QP(F_DARK_GREEN, 90), QP(F_DARK_PINK, 90), QP(F_LIGHT_PINK, 88),
		QP(F_LIGHT_GREEN, 88), QP(F_COMP_DC_GP, 88), QP(F_COMP_DC_BY, 88), QP(F_UNI_BLUE, 25), QP(F_UNI_WARM, 30)),
	YN("PLUGE: can you see the darkest bar (just above black)?", AR_GREY, PIC(PAT_PLUGE, 0, 0), 75, 0,
		QP(F_BLACK_CRUSH, 4), QP(F_BLACK_GREY, 97), QP(F_RETRACE, 90)),
	YN("With the room dark, does the black background look black (not dark grey)?", AR_GREY, PIC(PAT_PLUGE, 0, 0), 85, 0,
		QP(F_BLACK_GREY, 5), QP(F_RETRACE, 25), QP(F_BLACK_CRUSH, 97)),
	YN("On this dark screen, can you see faint slanted lines (often near the top)?", AR_GREY, PIC(PAT_FIELD, 0, 10), 4, 0,
		QP(F_RETRACE, 92), QP(F_BLACK_GREY, 30)),
	YN("White clipping: do the brightest blocks all merge into one white?", AR_GREY, PIC(PAT_WHITECLIP, 0, 0), 6, 0,
		QP(F_WHITE_CLIP, 92), QP(F_PUMP, 30), QP(F_DIM, 3)),
	YN("Is the whole picture dim, even with Contrast turned up?", AR_GREY, PIC(PAT_WHITECLIP, 0, 0), 5, 0,
		QP(F_DIM, 92), QP(F_BLACK_CRUSH, 20)),
	YN("When the screen flashes white, does the picture grow, shrink or dim?", AR_GREY, PIC(PAT_BLOOM, 0, 0), 10, 0,
		QP(F_PUMP, 92), QP(F_WHITE_CLIP, 30)),

	/* ---- colour ---- */
	CH("Look at the colour bars. What looks wrong?", AR_COLOUR, PIC(PAT_SMPTE, 0, 0),
		OPTS("Nothing, they look right", "Too strong / glowing", "Pale / faded", "Black & white, or flashing", "Reds and blues swapped", "Everything very green or purple", "Colours smear or crawl"),
		BASE(78, 4, 4, 2, 2, 3, 7),
		QO(F_SAT_HIGH, 1, 85), QO(F_SAT_LOW, 2, 85), QO(F_NO_COLOUR, 3, 90), QO(F_SWAP_PBPR, 4, 90), QO(F_YPB_SWAP, 5, 85), QO(F_GUN_DEAD, 5, 45), QO(F_GUN_DEAD, 4, 20),
		QO(F_BLEED, 6, 80), QO(F_DOTCRAWL, 6, 75), QO(F_UNI_GREEN, 5, 25), QO(F_UNI_PINK, 5, 25),
		QO(F_SVIDEO_BW, 3, 90), QO(F_RGB_FALLBACK, 6, 60), QO(F_RGB_FALLBACK, 3, 30), QO(F_PAL_RGB_NTSC, 3, 40), QO(F_PAL_RGB_NTSC, 5, 30), QO(F_COL_INTERMIT, 3, 70), QO(F_RGB_HOT, 2, 40)),
	YN("Look at the faces. Do they look greenish or purple?", AR_COLOUR, PIC(PAT_SKIN, 0, 0), 6, 0,
		QP(F_TINT_HUE, 90), QP(F_HUE_AXIS, 45), QP(F_UNI_GREEN, 40), QP(F_UNI_PINK, 40), QP(F_TRACK_GP, 25), QP(F_COMP_DC_GP, 40)),
	YN("With Colour set right, are the reds much stronger than the blues (or the other way)?", AR_COLOUR, PIC(PAT_SMPTE, 0, 0), 5, 0,
		QP(F_RB_BAL, 88), QP(F_SAT_HIGH, 12), QP(F_SAT_LOW, 12)),
	YN("With Tint right for faces, do greens look yellowish or magenta look off?", AR_COLOUR, PIC(PAT_SMPTE, 0, 0), 5, 0,
		QP(F_HUE_AXIS, 85), QP(F_TINT_HUE, 25), QP(F_RB_BAL, 25)),
	CH("Plain white screen. What do you see?", AR_ALL, PIC(PAT_FIELD, 0, 255),
		OPTS("Even white", "Colour patches (often corners)", "A faint ghost of a logo or HUD", "1 or 2 thin lines across", "Silvery or blotchy bright parts", "Brightness flickers"), BASE(84, 4, 3, 4, 3, 2),
		QO(F_PURITY, 1, 90), QO(F_CONVERGE, 1, 12), QO(F_BURNIN, 2, 88), QO(F_DAMPER, 3, 90), QO(F_WEAK_TUBE, 4, 75), QO(F_BRIGHT_FLICKER, 5, 85), QO(F_SLOW_START, 4, 20)),
	YN("On the convergence grid, do white lines have red, green or blue edges?", AR_COLOUR, PIC(PAT_CONVERGE, 0, 0), 10, 0,
		QP(F_CONVERGE, 94), QP(F_PURITY, 25), QP(F_BLEED, 25)),
	YN("Red-only screen: is it black? (Then try green-only and blue-only.)", AR_COLOUR, PIC(PAT_FIELD, 1, 255), 2, 0,
		QP(F_GUN_DEAD, 75), QP(F_YPB_SWAP, 20)),
	YN("Colour bleed picture: does colour smear past the edges?", AR_COLOUR, PIC(PAT_BLEED, 0, 0), 12, 0,
		QP(F_BLEED, 92), QP(F_SAT_HIGH, 45), QP(F_DOTCRAWL, 45)),
	CH("Which cable goes from the Wii to the TV?", AR_ALL, PIC(PAT_SMPTE, 0, 0),
		OPTS("Component (green, blue, red)", "RGB SCART", "S-Video", "Composite (one yellow plug)"), BASE(40, 22, 8, 30),
		QO(F_COMP_DC_GP, 0, 97), QO(F_COMP_DC_BY, 0, 97), QO(F_SWAP_PBPR, 0, 99), QO(F_YPB_SWAP, 0, 99), QO(F_480P_GREY, 0, 70), QO(F_480P_BLANK, 0, 95),
		QO(F_RGB_SYNC, 1, 97), QO(F_RGB_CHECKER, 1, 97), QO(F_RGB_FALLBACK, 1, 97), QO(F_RGB_HOT, 1, 97), QO(F_PAL_RGB_NTSC, 1, 98), QO(F_BUZZ, 1, 60),
		QO(F_SVIDEO_BW, 2, 97), QO(F_DOTCRAWL, 3, 90), QO(F_RAINBOW, 3, 92), QO(F_BLEED, 3, 70), QO(F_AV_DC, 3, 70), QO(F_NO_COLOUR, 3, 60), QO(F_DEFLICKER, 0, 60)),
	YN("Is the Wii set to 60 Hz (or 480i / 480p)?", AR_ALL, PIC(PAT_OVERSCAN, 0, 0), 70, 0,
		QP(F_60HZ, 95), QP(F_TINT_HUE, 85), QP(F_NO_COLOUR, 70)),

	/* ---- geometry ---- */
	CH("Overscan picture: which frame lines are missing off the screen?", AR_GEOM, PIC(PAT_OVERSCAN, 0, 0),
		OPTS("None: all four show", "Top and / or bottom", "Left and / or right", "All round", "One side much more than the other"), BASE(70, 8, 8, 8, 6),
		QO(F_OVER_V, 1, 80), QO(F_OVER_V, 3, 12), QO(F_OVER_H, 2, 80), QO(F_OVER_H, 3, 12), QO(F_ZOOM, 3, 55), QO(F_ZOOM, 1, 25),
		QO(F_60HZ, 1, 40), QO(F_60HZ, 3, 30), QO(F_60HZ, 2, 15), QO(F_OFF_H, 4, 80), QO(F_OFF_V, 4, 80), QO(F_UNDER, 0, 88)),
	YN("Is there a black border between the picture and the edge of the glass?", AR_GEOM, PIC(PAT_OVERSCAN, 0, 0), 8, 0,
		QP(F_UNDER, 95), QP(F_OVER_V, 5), QP(F_OVER_H, 5), QP(F_ZOOM, 8), QP(F_OFF_H, 35), QP(F_OFF_V, 35)),
	YN("Is the uneven side left / right (rather than top / bottom)?", AR_GEOM, PIC(PAT_EDGE, 0, 0), 40, 0,
		QP(F_OFF_H, 94), QP(F_OFF_V, 5), QP(F_OVER_H, 80), QP(F_OVER_V, 10)),
	YN("Is the TV in a Zoom, Wide or 16:9 mode? (Check the aspect / picture size button)", AR_GEOM, PIC(PAT_GRID, 0, 0), 5, 0,
		QP(F_ZOOM, 90)),
	YN("Switch the Wii to 50 Hz (Wii Settings > Screen) and back. Does the size or position change?", AR_GEOM, PIC(PAT_OVERSCAN, 0, 0), 20, 0,
		QP(F_60HZ, 92), QP(F_OVER_V, 30), QP(F_OVER_H, 25)),
	CH("Look at the grid. What looks wrong?", AR_GEOM, PIC(PAT_GRID, 0, 0),
		OPTS("Nothing", "Circles squashed or stretched", "Top squares differ from bottom", "Middle rows differ", "Sides bow in or out", "Top and bottom different widths", "Tilted", "Lines wobble"),
		BASE(72, 6, 5, 4, 5, 3, 3, 2),
		QO(F_ZOOM, 1, 85), QO(F_OVER_V, 1, 30), QO(F_OVER_H, 1, 30), QO(F_UNDER, 1, 25), QO(F_VLIN, 2, 85), QO(F_FOLD, 2, 60), QO(F_VSC, 3, 80),
		QO(F_PIN, 4, 85), QO(F_CORNER, 4, 45), QO(F_KEY, 5, 85), QO(F_TILT, 6, 90), QO(F_WAVY, 7, 90)),
	YN("Are only the corners bent (the middle of the sides straight)?", AR_GEOM, PIC(PAT_GRID, 0, 0), 6, 0,
		QP(F_CORNER, 90), QP(F_PIN, 25)),
	YN("Is there a folded or bright band at the top?", AR_GEOM, PIC(PAT_GRID, 0, 0), 2, 0,
		QP(F_FOLD, 94), QP(F_RETRACE, 25)),

	/* ---- sharpness ---- */
	CH("Look at the sharpness picture. What looks wrong?", AR_SHARP, PIC(PAT_SHARP, 0, 0),
		OPTS("Nothing", "White or black outlines on edges", "Blurry everywhere", "Blurry only in the corners", "Thick edges, narrow white areas", "Dots crawl on colour edges", "Shimmer or flicker on fine lines"),
		BASE(70, 6, 5, 7, 3, 4, 5),
		QO(F_HALO, 1, 85), QO(F_SVM, 4, 70), QO(F_SVM, 1, 20), QO(F_SOFT, 2, 85), QO(F_FOCUS_EDGE, 3, 85), QO(F_DOTCRAWL, 5, 85), QO(F_MOIRE, 6, 80), QO(F_FLICKER, 6, 80)),
	YN("Stripes: is it thin HORIZONTAL lines flickering (rather than wavy patterns)?", AR_SHARP, PIC(PAT_STRIPES, 0, 0), 30, 0,
		QP(F_FLICKER, 92), QP(F_MOIRE, 15)),
	YN("Does turning Sharpness down make the outlines go away?", AR_SHARP, PIC(PAT_SHARP, 0, 0), 20, 0,
		QP(F_HALO, 88), QP(F_SVM, 20), QP(F_MOIRE, 60)),

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
const int CAL_NQ = (int)(sizeof CAL_Q / sizeof CAL_Q[0]);
int cal_q_nopt(int q) { if (q >= CAL_NQ) return 2; int n = 0; while (n < MAX_OPT && CAL_Q[q].opt[n]) n++; return n; }
/* questions, including the "is it this?" ones (id CAL_NQ + fault) */
const char *cal_q_text(int q, char *buf, int n)
{
	if (q < CAL_NQ) return CAL_Q[q].text;
	if (q >= CAL_NQ + F_COUNT) snprintf(buf, n, "Let's double-check, look again: %s", CAL_FAULT[q - CAL_NQ - F_COUNT].looks);
	else snprintf(buf, n, "Is this it? %s", CAL_FAULT[q - CAL_NQ].looks);
	return buf;
}
const char *cal_q_opt(int q, int o) { static const char *YNO[2] = { "Yes", "No" }; return q >= CAL_NQ ? YNO[o & 1] : CAL_Q[q].opt[o]; }
CalPic cal_q_pic(int q) { return q >= CAL_NQ ? CAL_FAULT[(q - CAL_NQ) % F_COUNT].pic : CAL_Q[q].pic; }
int cal_q_area(int q) { return q >= CAL_NQ ? CAL_FAULT[(q - CAL_NQ) % F_COUNT].area : CAL_Q[q].area; }

/* ================================================================== */
/* solo guided steps                                                  */
/* ================================================================== */
#define SETS(a, b, c, d) { a, b, c, d }
const CalGuide CAL_GUIDE[] = {
	/* everything starts warm and set up */
	{ AR_GREY, "Warm up", "Leave the TV on for 20-30 minutes before you start. Colour, size and focus change while it warms up.", "The timer says warm (25 minutes or more).", PIC(PAT_WARMUP, 0, 0), SETS(W_WARMUP, -1, -1, -1) },
	/* grey */
	{ AR_GREY, "Picture mode", "Menu: Picture mode: pick Standard or Movie (not Dynamic or Vivid). Colour temperature: Normal.", "No Dynamic / Vivid mode, temperature on Normal.", PIC(PAT_GREY50, 0, 0), SETS(S_PMODE, S_COLTEMP, -1, -1) },
	{ AR_GREY, "Black level", "Menu: Brightness: turn up until the darkest bar shows, then down until it just disappears.", "Left bars gone into black; the next bar just visible.", PIC(PAT_PLUGE, 0, 0), SETS(S_BRIGHT, S_SBRI, H_SCREEN, -1) },
	{ AR_GREY, "Dark detail", "10% window: Menu: Brightness: turn up until the dimmest box is just visible, no further.", "The 10% box just shows; black stays black.", PIC(PAT_IRE, 0, 0), SETS(S_BRIGHT, S_SBRI, -1, -1) },
	{ AR_GREY, "Slanted lines", "Dark screen. Faint slanted lines? Service menu: S-BRI: turn down until they just go.", "No slanted lines on the dark screen.", PIC(PAT_FIELD, 0, 10), SETS(S_SBRI, S_BRIGHT, H_SCREEN, -1) },
	{ AR_GREY, "White level", "Menu: Contrast: turn up until the brightest blocks merge, then down until every block shows.", "Every bright block is different; nothing swells.", PIC(PAT_WHITECLIP, 0, 0), SETS(S_CONTRAST, S_SCON, -1, -1) },
	{ AR_GREY, "Bloom", "FLASHING. Menu: Contrast: turn down until the frame stops growing when the screen flashes white.", "The frame stays the same size.", PIC(PAT_BLOOM, 0, 0), SETS(S_CONTRAST, S_SCON, -1, -1) },
	{ AR_GREY, "Gamma", "Step back. The box that blends into its stripes should be near the middle. Far off: try another Picture mode.", "The middle box blends in best.", PIC(PAT_GAMMA, 0, 0), SETS(S_PMODE, S_CONTRAST, -1, -1) },
	{ AR_GREY, "Light greys", "Service menu: R-DRV / G-DRV / B-DRV: turn down the one that matches the tint you see until the light grey is plain grey.", "Light grey with no pink, green, blue or yellow.", LIGHTG, SETS(S_RDRV, S_GDRV, S_BDRV, -1) },
	{ AR_GREY, "Dark greys", "Service menu: R-BIA / G-BIA / B-BIA: turn down the one that matches the tint you see until the dark grey is plain grey.", "Dark grey with no green, pink, blue or brown.", DARKG, SETS(S_RBIAS, S_GBIAS, S_BBIAS, -1) },
	{ AR_GREY, "Whole grey scale", "Bias and drive nudge each other: redo light greys, then dark greys, until every step is the same grey.", "Black to white, every step the same neutral grey.", STEPS, SETS(S_RBIAS, S_GBIAS, S_RDRV, S_BDRV) },
	{ AR_GREY, "Smooth ramp", "Look along the ramp: no coloured bands. Pink or green band in the dark end: bias. In the light end: drive.", "One smooth grey ramp, no colour bands.", PIC(PAT_RAMP, 0, 0), SETS(S_GBIAS, S_GDRV, S_RBIAS, S_RDRV) },
	{ AR_GREY, "Black level again", "Menu: Brightness (or Service menu: S-BRI): turn down until the darkest bar just disappears.", "Darkest bars just gone into black.", PIC(PAT_PLUGE, 0, 0), SETS(S_BRIGHT, S_SBRI, -1, -1) },
	/* colour */
	{ AR_COLOUR, "Colour order", "Check the bars read: white, yellow, cyan, green, magenta, red, blue. Wrong order means the cable plugs are swapped.", "The seven bars in that order.", PIC(PAT_SMPTE, 0, 0), SETS(W_CABLE, S_COLSYS, -1, -1) },
	{ AR_COLOUR, "EBU bars (50 Hz)", "Yellow must not look green and cyan must not look green. Wrong: Menu: Colour system: Auto or PAL.", "Yellow is yellow, cyan is cyan.", PIC(PAT_EBU, 0, 0), SETS(S_COLSYS, S_COLOUR, -1, -1) },
	{ AR_COLOUR, "Colour strength", "Menu: Colour: turn up until the red bar glows or smears, then down until it stops, then 2 more down.", "Red bar strong but clean.", PIC(PAT_SMPTE, 0, 0), SETS(S_COLOUR, S_SCOL, -1, -1) },
	{ AR_COLOUR, "Tint", "Menu: Tint: turn until the faces look natural, not green or purple. No Tint setting? Press Done.", "Natural skin.", PIC(PAT_SKIN, 0, 0), SETS(S_TINT, S_STINT, -1, -1) },
	{ AR_COLOUR, "Colour chart", "Every patch should be different, and the grey row along the bottom plain grey.", "All patches clear; bottom greys neutral.", PIC(PAT_CHECKER, 0, 0), SETS(S_COLOUR, S_TINT, -1, -1) },
	{ AR_COLOUR, "Bars and greys", "Colour bars over grey steps: bars clean, greys with no tint. Tinted greys: do the Grey levels area.", "Clean bars, plain greys.", PIC(PAT_BARSGREY, 0, 0), SETS(S_COLOUR, S_GBIAS, -1, -1) },
	{ AR_COLOUR, "Red vs blue", "Reds far stronger than blues (or the other way) with Colour right? Service menu: R-Y/B-Y G.BL: turn until they look even.", "Red and blue bars equally strong.", PIC(PAT_SMPTE, 0, 0), SETS(S_RYBY_GBL, S_COLOUR, -1, -1) },
	{ AR_COLOUR, "Blue filter (optional)", "Got a blue filter or a Blue-only mode? Look at these bars through it. Colour: turn until the outer bars match the strip under them.", "Outer bars blend into the strip.", PIC(PAT_SMPTE, 0, 0), SETS(S_COLOUR, S_TINT, -1, -1) },
	{ AR_COLOUR, "Component only", "Component: if the grey leans pink or green, Service menu: YUV R-Y DC: turn until the grey is plain grey. Blue or yellow: YUV B-Y DC.", "Plain grey on component.", PIC(PAT_GREY50, 0, 0), SETS(S_YUV_RYDC, S_YUV_BYDC, -1, -1) },
	{ AR_COLOUR, "Purity", "Plain white: look for colour patches. Any? Turn the TV off for 20 minutes, then on (it degausses).", "Even white all over.", PIC(PAT_FIELD, 0, 255), SETS(H_DEGAUSS, H_CONV, -1, -1) },
	{ AR_COLOUR, "Red screen", "Plain red: even all over, no dark or odd-coloured patches. Patches: turn the TV off 20 minutes, then on.", "Even red everywhere.", PIC(PAT_RED, 0, 0), SETS(H_DEGAUSS, -1, -1, -1) },
	{ AR_COLOUR, "Green screen", "Plain green: even all over. Patches mean magnetism: keep speakers away and degauss.", "Even green everywhere.", PIC(PAT_GREEN, 0, 0), SETS(H_DEGAUSS, -1, -1, -1) },
	{ AR_COLOUR, "Blue screen", "Plain blue: even all over. This one shows purity problems best.", "Even blue everywhere.", PIC(PAT_BLUE, 0, 0), SETS(H_DEGAUSS, H_CONV, -1, -1) },
	{ AR_COLOUR, "Convergence", "White lines should have no red, green or blue edges, especially near the sides.", "Thin white lines, no coloured fringes.", PIC(PAT_CONVERGE, 0, 0), SETS(H_CONV, H_DEGAUSS, -1, -1) },
	{ AR_COLOUR, "Colour bleed", "Colour should stop at the edges. Smearing: use component and turn Colour down a little.", "Sharp colour edges.", PIC(PAT_BLEED, 0, 0), SETS(W_CABLETYPE, S_COLOUR, S_NR, -1) },
	/* geometry */
	{ AR_GEOM, "Setup", "Menu: Aspect: 4:3 / Normal. Wii Settings > Screen: Widescreen 4:3, and the Hz you play at.", "Circles are round.", PIC(PAT_GRID, 0, 0), SETS(S_ASPECT, W_WIIWIDE, W_VIDEO, -1) },
	{ AR_GEOM, "Round circles", "The circles must be round. Oval: check Menu: Aspect and the Wii's 4:3 setting, then V.SIZE and H.SIZE.", "Round circles.", PIC(PAT_MONO, 0, 0), SETS(S_ASPECT, W_WIIWIDE, S_VSIZE, S_HSIZE) },
	{ AR_GEOM, "Left / right", "Service menu: H.PHASE (60 Hz: NT.H.PHASE): turn until the left and right edges are even.", "Same amount cut off on both sides.", PIC(PAT_EDGE, 0, 0), SETS(S_HPHASE, S_NT_HPHASE, -1, -1) },
	{ AR_GEOM, "Up / down", "Service menu: V.POSI (60 Hz: NT.V.POSI): turn until the top and bottom edges are even.", "Same amount cut off top and bottom.", PIC(PAT_EDGE, 0, 0), SETS(S_VPOS, S_NT_VPOS, -1, -1) },
	{ AR_GEOM, "Height", "Service menu: V.SIZE (60 Hz: NT.V.PHASE / NT.V.SIZE): turn down until the top and bottom frame lines just touch the glass.", "Top and bottom frame lines just at the edge.", PIC(PAT_OVERSCAN, 0, 0), SETS(S_VSIZE, S_NT_VPHASE, S_ZOOM, -1) },
	{ AR_GEOM, "Width", "Service menu: H.SIZE / EW WIDTH: turn until the side frame lines just touch the glass. No width setting? It's fixed.", "Side frame lines just at the edge.", PIC(PAT_OVERSCAN, 0, 0), SETS(S_HSIZE, S_WIDE, -1, -1) },
	{ AR_GEOM, "Top vs bottom", "Service menu: V.LINE (60 Hz: NT.V.LINE): turn until the top and bottom squares are the same height.", "Even squares top to bottom.", PIC(PAT_GRID, 0, 0), SETS(S_VLIN, S_NT_VLIN, -1, -1) },
	{ AR_GEOM, "Middle vs edges", "Service menu: V.SC (60 Hz: NT.V.SC): turn until the middle squares match the top and bottom ones.", "Every row of squares the same height.", PIC(PAT_GRID, 0, 0), SETS(S_VSC, S_NT_VSC, -1, -1) },
	{ AR_GEOM, "Straight sides", "Service menu: EW PARA, then EW TRAP, then EW CORNER: turn each until the side lines are straight and parallel.", "Straight, parallel sides; top as wide as bottom.", PIC(PAT_GRID, 0, 0), SETS(S_EWPARA, S_EWTRAP, S_EWCORN, S_TILT) },
	{ AR_GEOM, "Level", "The horizontal lines must be level. Menu: Tilt (if you have it): turn until they are.", "Level lines.", PIC(PAT_GRID, 0, 0), SETS(S_TILT, H_DEGAUSS, -1, -1) },
	{ AR_GEOM, "Even spacing", "Dots evenly spaced across and down. Squashed near an edge: redo Top vs bottom and Straight sides.", "Even spacing everywhere.", PIC(PAT_LINEARITY, 0, 0), SETS(S_VLIN, S_VSC, S_EWPARA, -1) },
	{ AR_GEOM, "Check again", "Size, line and position nudge each other. Redo height and up / down until the frame is even all round.", "Even all round.", PIC(PAT_OVERSCAN, 0, 0), SETS(S_VSIZE, S_VPOS, S_HPHASE, -1) },
	{ AR_GEOM, "Moving grid", "The grid should scroll smoothly with even squares everywhere.", "Smooth, even squares while moving.", PIC(PAT_GRIDSCROLL, 0, 0), SETS(S_VLIN, -1, -1, -1) },
	/* sharpness */
	{ AR_SHARP, "Sharpness", "Menu: Sharpness: turn down until the white outlines around the lines are gone.", "Crisp lines, no halos.", PIC(PAT_SHARP, 0, 0), SETS(S_SHARP, S_SSHARP, S_VMU, -1) },
	{ AR_SHARP, "Edge boost", "Menu: VM / SVM / Velocity modulation: off, if your TV has it. It thickens edges.", "Even line thickness.", PIC(PAT_SHARP, 0, 0), SETS(S_VMU, S_SVM, -1, -1) },
	{ AR_SHARP, "Detail", "Fine lines and dots should each be clear and separate. Merged: Menu: Sharpness up one step at a time until they separate.", "Every fine line visible.", PIC(PAT_CHECKER, 0, 0), SETS(S_SHARP, W_CABLETYPE, -1, -1) },
	{ AR_SHARP, "Fine detail", "Fine stripes: a little shimmer is normal on a CRT. Lots: turn Sharpness down a step.", "Stripes steady.", PIC(PAT_STRIPES, 0, 0), SETS(S_SHARP, W_VIDEO, -1, -1) },
	{ AR_SHARP, "Focus", "Compare the corners with the middle. Much softer corners means focus (a technician job).", "Sharp all over (corners a touch softer is normal).", PIC(PAT_MONO, 0, 0), SETS(H_FOCUS, W_CABLETYPE, -1, -1) },
	{ AR_SHARP, "Colour edges", "Colour should stop cleanly at the lines. Smearing: Menu: Colour down a step, noise reduction off.", "Clean colour edges.", PIC(PAT_BLEED, 0, 0), SETS(S_COLOUR, S_NR, W_CABLETYPE, -1) },
	{ AR_SHARP, "Diagonals", "The spinning lines should stay smooth. Stair steps: turn off any 100 Hz or Digital mode.", "Smooth lines.", PIC(PAT_DIAG, 0, 0), SETS(S_DIGSCAN, -1, -1, -1) },
	{ AR_SHARP, "Moving picture", "The scroll should glide. Smear or judder: turn off 100 Hz / Digital Scan and noise reduction.", "Smooth, sharp scrolling.", PIC(PAT_SCROLL, 0, 0), SETS(S_DIGSCAN, S_NR, -1, -1) },
	{ AR_SHARP, "Real picture", "Last check: grass, sky and faces should look natural and sharp, with no halos.", "It looks like a real scene.", PIC(PAT_SCENE, 0, 0), SETS(S_SHARP, S_COLOUR, -1, -1) },
	/* stability and signal */
	{ AR_SIGNAL, "Steady picture", "Watch the grid for a minute: it shouldn't roll, tear, bend at the top or wiggle.", "Rock steady.", PIC(PAT_GRID, 0, 0), SETS(S_VHOLD, S_HAFC, W_SCART, -1) },
	{ AR_SIGNAL, "Clean picture", "Plain grey: no bands, moving lines, ghost images or drifting bars.", "Plain, even grey.", PIC(PAT_GREY50, 0, 0), SETS(W_NOISE, W_CABLE, -1, -1) },
	{ AR_SIGNAL, "Drop shadow", "240p check: the shadow should look see-through, not flickering hard. Flicker or combing: 100 Hz / Digital mode off.", "See-through shadow.", PIC(PAT_SPRITE, 0, 0), SETS(S_DIGSCAN, W_VIDEO, -1, -1) },
	{ AR_SIGNAL, "Sound", "Listen while it flashes: no buzz that follows the picture, no hum, no crackling.", "Quiet, apart from a faint high whine (normal).", PIC(PAT_BLOOM, 0, 0), SETS(W_NOISE, W_SCART, -1, -1) },
	{ AR_SIGNAL, "Left and right", "A beep on the left, then the right. Each should come from its own side.", "Beeps from the right sides.", PIC(PAT_SPEAKERS, 0, 0), SETS(W_CABLE, -1, -1, -1) },
	{ AR_SIGNAL, "Sound in time", "The beep should play as the ball hits the line. Late: turn off any sound effects or processing.", "Beep on the hit.", PIC(PAT_AVSYNC, 0, 0), SETS(S_DIGSCAN, -1, -1, -1) },
};
const int CAL_NGUIDE = (int)(sizeof CAL_GUIDE / sizeof CAL_GUIDE[0]);

/* ================================================================== */
/* troubleshooting notes                                              */
/* ================================================================== */
const CalNote CAL_NOTE[] = {
	{ AR_GREY, "Dark greys green, light greys pink", "Green bias too high AND green drive too low. Lower G bias on a dark grey, raise G drive (or lower R and B drive) on a light grey. Repeat." },
	{ AR_GREY, "Dark greys pink, light greys green", "The opposite: raise G bias on dark grey, lower G drive on light grey." },
	{ AR_GREY, "Tint only in the darks", "It's the BIAS (cut-off) of that colour. Too much colour X: lower X bias." },
	{ AR_GREY, "Tint only in the lights", "It's the DRIVE (gain). Too much colour X: lower X drive." },
	{ AR_GREY, "Same tint everywhere", "Colour temperature first. Then drive and bias of that colour together. Only one input? That input's DC levels." },
	{ AR_GREY, "Normal menu can't fix a tint", "Colour and Tint don't touch greys. Grey tints live in the service menu (bias / drive)." },
	{ AR_GREY, "No green drive on the menu", "Green is the reference on many sets. Move red and blue instead: less pink = lower R and B drive." },
	{ AR_GREY, "Blacks grey after changing bias", "Bias moves the black level. Redo Brightness / sub brightness last." },
	{ AR_GREY, "Faint slanted lines", "Retrace lines: black level too high. Lower sub brightness (or the screen knob: technician)." },
	{ AR_GREY, "Whites bloom and swell", "Contrast too high. Lower Contrast, then sub contrast." },
	{ AR_COLOUR, "Reds and blues swapped", "Component red (Pr) and blue (Pb) plugs are swapped." },
	{ AR_COLOUR, "Very green, odd brightness", "Green (Y) plug in the wrong socket, or the input is set to RGB instead of YPbPr." },
	{ AR_COLOUR, "Black & white or flashing colour", "Colour system: Auto, or match the Wii (60 Hz = NTSC / PAL60)." },
	{ AR_COLOUR, "Faces green or purple", "Tint (NTSC / 60 Hz only). PAL at 50 Hz doesn't use Tint." },
	{ AR_COLOUR, "Colour patches in corners", "Magnetised tube: keep speakers away, turn off 20 min, on. Repeat. Still there: purity rings (technician)." },
	{ AR_COLOUR, "Coloured edges on white lines", "Convergence: degauss first; the magnets are for a technician." },
	{ AR_COLOUR, "Only component looks wrong", "Check the plugs. Then the component-only settings: YUV R-Y DC (pink / green) and YUV B-Y DC (blue / yellow)." },
	{ AR_COLOUR, "Only AV / aerial looks wrong", "The decoder's settings: B-Y DC level, colour system. SECAM settings don't matter for a Wii." },
	{ AR_COLOUR, "R-Y / B-Y settings", "They change colours, not greys. Set Colour and Tint first; only touch these if one colour is off on its own." },
	{ AR_GEOM, "Fine at 50 Hz, wrong at 60 Hz", "The TV keeps two geometry sets. NT. settings are the 60 Hz ones: the Wii at 60 Hz / 480i / 480p uses them." },
	{ AR_GEOM, "Stretched and cut off", "Check the aspect button first (4:3 / Normal). Then V size (NT.V.Phase at 60 Hz) and width." },
	{ AR_GEOM, "No width setting", "Many small sets have no H size in the menu. Width is then a coil or pot inside: technician." },
	{ AR_GEOM, "Top stretched, bottom squashed", "V linearity. Then height and position again." },
	{ AR_GEOM, "Tilted picture", "Moving or turning the TV changes the Earth's field. Degauss; then the tilt setting if you have one." },
	{ AR_GEOM, "Wobbly edges", "Something magnetic nearby, or old capacitors: move speakers away; else technician." },
	{ AR_SHARP, "White halos", "Sharpness too high, or velocity modulation on. Lower Sharpness; VM / SVM off." },
	{ AR_SHARP, "Soft everywhere", "Use component. Still soft: focus (technician)." },
	{ AR_SHARP, "Crawling dots", "Composite video. Component or S-Video fixes it." },
	{ AR_SHARP, "Flicker on thin lines", "Interlace: normal. Wii deflicker or 480p helps." },
	{ AR_SIGNAL, "Picture rolls", "Try the Wii at 50 Hz and another cable. Old sets: V hold. Rolls on everything: technician." },
	{ AR_SIGNAL, "Slanted tearing after picking 480p", "Most CRTs only take 480i. Wait 15 s for the Wii to go back, or use the AV cable and pick Standard." },
	{ AR_SIGNAL, "RGB SCART rolls or blank", "Sync: use a 75 ohm csync or sync-on-composite cable. Try the other SCART socket." },
	{ AR_SIGNAL, "Lines, bars or buzz", "Unplug the aerial lead, dimmers and chargers one at a time. A ground loop or noise source is the usual cause." },
	{ AR_SIGNAL, "Games comb or lag on a 100 Hz set", "Turn 100 Hz / Digital Scan / DRC off, or use a Game mode." },
	{ AR_ALL, "Crackling, smell, a line, or shuts off", "Switch off at the wall. Don't open the TV: a CRT holds a dangerous charge after unplugging." },
	{ AR_ALL, "Warm it up first", "Leave the TV on for 20-30 minutes before setting greys, geometry or focus." },
	{ AR_GREY, "Thin lines across a Trinitron", "Damper wires. Normal." },
	{ AR_COLOUR, "RGB looks like composite", "Pick the RGB input; the cable needs pin 16 voltage. PAL Wii at 60 Hz: EuRGB60." },
	{ AR_GEOM, "Tall, thin picture", "The Wii is on 16:9. Set it to 4:3." },
	{ AR_ALL, "Before any service change", "Write down (or enter in Ultra mode) the original number. Change 1-2 steps at a time. Never touch items you don't understand." },
};
const int CAL_NNOTE = (int)(sizeof CAL_NOTE / sizeof CAL_NOTE[0]);


/* the label most service menus show (your own menu's name wins when Ultra knows it) */
const char *cal_setting_code(int i)
{
	switch (i) {
	case S_BRIGHT: return "BRIGHT"; case S_CONTRAST: return "CONTRAST"; case S_COLOUR: return "COLOUR"; case S_TINT: return "TINT"; case S_SHARP: return "SHARP";
	case S_SBRI: return "S-BRI"; case S_SCON: return "S-CON"; case S_SCOL: return "S-COL"; case S_STINT: return "S-TINT";
	case S_RBIAS: return "R-BIA (R CUT)"; case S_GBIAS: return "G-BIA (G CUT)"; case S_BBIAS: return "B-BIA (B CUT)";
	case S_RDRV: return "R-DRV (R GAIN)"; case S_GDRV: return "G-DRV (G GAIN)"; case S_BDRV: return "B-DRV (B GAIN)";
	case S_CBW: return "C.B/W"; case S_CROSSBW: return "CROSS B/W";
	case S_RYBY_GBL: return "R-Y/B-Y G.BL"; case S_RYBY_ANG: return "R-Y/B-Y ANG"; case S_BYDC: return "B-Y DC LEVEL";
	case S_SEC_BYDC: return "SECAM B-Y DC"; case S_SEC_RYDC: return "SECAM R-Y DC"; case S_YUV_BYDC: return "YUV B-Y DC"; case S_YUV_RYDC: return "YUV R-Y DC";
	case S_SSHARP: return "S-SHARP"; case S_SVM: return "SVM";
	case S_HPHASE: return "H.PHASE (H POS)"; case S_HSIZE: return "H.SIZE (EW WIDTH)"; case S_EWPARA: return "EW PARA (PIN)"; case S_EWCORN: return "EW CORNER";
	case S_EWTRAP: return "EW TRAP"; case S_VSIZE: return "V.SIZE"; case S_VPOS: return "V.POSI"; case S_VLIN: return "V.LINE"; case S_VSC: return "V.SC";
	case S_NT_HPHASE: return "NT.H.PHASE"; case S_NT_VPHASE: return "NT.V.PHASE (NT.V.SIZE)"; case S_NT_VLIN: return "NT.V.LINE"; case S_NT_VPOS: return "NT.V.POSI"; case S_NT_VSC: return "NT.V.SC";
	case S_ZOOM: return "ZOOM SIZE"; case S_WIDE: return "WIDE SIZE"; case S_HAFC: return "H AFC";
	}
	return CAL_SET[i].name;
}
/* what to look at while changing it */
const char *cal_setting_look(int i)
{
	switch (i) {
	case S_RBIAS: case S_GBIAS: case S_BBIAS: return "the dark grey";
	case S_RDRV: case S_GDRV: case S_BDRV: case S_COLTEMP: return "the light grey";
	case S_BRIGHT: case S_SBRI: case H_SCREEN: return "the darkest bars";
	case S_CONTRAST: case S_SCON: return "the brightest blocks";
	case S_COLOUR: case S_SCOL: return "the red bar";
	case S_TINT: case S_STINT: return "the faces";
	case S_YUV_RYDC: case S_YUV_BYDC: case S_BYDC: return "the grey";
	case S_HPHASE: case S_NT_HPHASE: return "the left and right edges";
	case S_VPOS: case S_NT_VPOS: return "the top and bottom edges";
	case S_VSIZE: case S_NT_VPHASE: case S_ZOOM: case S_WIDE: return "the top and bottom frame lines";
	case S_HSIZE: return "the side frame lines";
	case S_VLIN: case S_VSC: case S_NT_VLIN: case S_NT_VSC: return "the squares, top to bottom";
	case S_EWPARA: case S_EWTRAP: case S_EWCORN: case S_TILT: return "the side lines";
	case S_SHARP: case S_SSHARP: case S_VMU: case S_SVM: return "the edges of the lines";
	}
	return "the picture";
}
/* when to stop turning it */
const char *cal_setting_until(int i)
{
	switch (i) {
	case S_RBIAS: case S_GBIAS: case S_BBIAS: return "the dark grey is plain grey";
	case S_RDRV: case S_GDRV: case S_BDRV: return "the light grey is plain grey";
	case S_COLTEMP: return "white looks like white paper";
	case S_BRIGHT: case S_SBRI: case H_SCREEN: return "the darkest bar just disappears";
	case S_CONTRAST: case S_SCON: return "every bright block shows";
	case S_COLOUR: case S_SCOL: return "the red bar is strong but doesn't glow or smear";
	case S_TINT: case S_STINT: return "faces look natural, not green or purple";
	case S_RYBY_GBL: return "red and blue bars look equally strong";
	case S_RYBY_ANG: return "green and magenta bars look right";
	case S_YUV_RYDC: case S_YUV_BYDC: case S_BYDC: return "the grey is plain grey";
	case S_HPHASE: case S_NT_HPHASE: return "the left and right edges are even";
	case S_VPOS: case S_NT_VPOS: return "the top and bottom edges are even";
	case S_VSIZE: case S_NT_VPHASE: case S_ZOOM: case S_WIDE: return "the top and bottom frame lines just touch the glass";
	case S_HSIZE: return "the side frame lines just touch the glass";
	case S_VLIN: case S_NT_VLIN: return "the top and bottom squares are the same height";
	case S_VSC: case S_NT_VSC: return "the middle squares match the top and bottom ones";
	case S_EWPARA: return "the side lines are straight";
	case S_EWTRAP: return "the top is as wide as the bottom";
	case S_EWCORN: return "the corners are straight";
	case S_TILT: return "the picture is level";
	case S_SHARP: case S_SSHARP: case S_SVM: return "the white outlines are gone";
	}
	return "it looks right";
}
/* the best picture to look at while changing it (pat = -1: keep the fault's own) */
CalPic cal_setting_pic(int i)
{
	CalPic p = { 255, 0, 0 };
	switch (i) {
	case S_RBIAS: case S_GBIAS: case S_BBIAS: p.pat = PAT_FIELD; p.a = 0; p.b = 40; break;
	case S_RDRV: case S_GDRV: case S_BDRV: p.pat = PAT_FIELD; p.a = 0; p.b = 200; break;
	case S_BRIGHT: case S_SBRI: case H_SCREEN: p.pat = PAT_PLUGE; break;
	case S_CONTRAST: case S_SCON: p.pat = PAT_WHITECLIP; break;
	case S_COLOUR: case S_SCOL: case S_RYBY_GBL: p.pat = PAT_SMPTE; break;
	case S_TINT: case S_STINT: case S_RYBY_ANG: p.pat = PAT_SKIN; break;
	case S_YUV_RYDC: case S_YUV_BYDC: case S_BYDC: p.pat = PAT_GREY50; break;
	case S_HPHASE: case S_NT_HPHASE: case S_VPOS: case S_NT_VPOS: case S_VSIZE: case S_NT_VPHASE: case S_HSIZE: case S_ZOOM: case S_WIDE: p.pat = PAT_OVERSCAN; break;
	case S_VLIN: case S_VSC: case S_NT_VLIN: case S_NT_VSC: case S_EWPARA: case S_EWTRAP: case S_EWCORN: case S_TILT: p.pat = PAT_GRID; break;
	case S_SHARP: case S_SSHARP: case S_VMU: case S_SVM: p.pat = PAT_SHARP; break;
	}
	return p;
}

int cal_settings_for(int area, const s16 *rel, int nrel, s16 *out, int max)
{
	int n = 0;
	for (int k = 0; k < nrel && n < max; k++) if (rel[k] >= 0 && rel[k] < S_COUNT) {
		int dup = 0; for (int j = 0; j < n; j++) dup |= out[j] == rel[k];
		if (!dup) out[n++] = rel[k];
	}
	/* then the area's own: normal menu, service, knobs, Wii */
	for (int m = 0; m < 4; m++) for (int i = 0; i < S_COUNT && n < max; i++) {
		const CalSetting *s = &CAL_SET[i];
		if (s->menu != m || !(area == AR_ALL || s->area == area || s->area == AR_ALL)) continue;
		int dup = 0; for (int j = 0; j < n; j++) dup |= out[j] == i;
		if (!dup) out[n++] = (s16)i;
	}
	return n;
}

/* ================================================================== */
/* the diagnosis engine                                               */
/* ================================================================== */
/* One fault at a time (fix it, then look for the next). Every answer is evidence, never proof:
   each question says how likely "yes" is for each fault, and "no" and "not sure" are weighed too.
   The next question is the one that tells the most apart; before suggesting a fix it checks the
   leader with a question it hasn't asked yet, so one wrong answer can't send you the wrong way. */
static float dx_lp[F_COUNT + 1];     /* log chance per fault; last = nothing listed is wrong */
static u8 dx_out[F_COUNT];            /* excluded (fixed or ruled out) */
static s16 dx_q[128]; static u8 dx_a[128]; static int dx_n;
static int dx_area;

/* chance of each answer if f is the fault (f == F_COUNT: nothing listed) */
static void q_dist(int q, int f, float *d)
{
	const CalQuestion *Q = &CAL_Q[q];
	int n = cal_q_nopt(q);
	float listed = 0, rest = 0; int has[MAX_OPT] = { 0 };
	for (int o = 0; o < n; o++) d[o] = 0;
	if (f < F_COUNT) for (int k = 0; k < Q->nqp; k++) if (Q->qp[k].f == f && Q->qp[k].o < n) { d[Q->qp[k].o] += Q->qp[k].p / 100.0f; has[Q->qp[k].o] = 1; }
	for (int o = 0; o < n; o++) { if (has[o]) listed += d[o]; else rest += Q->base[o]; }
	if (listed > 1) listed = 1;
	for (int o = 0; o < n; o++) if (!has[o]) d[o] = rest > 0 ? (1 - listed) * Q->base[o] / rest : 0;
	if (f == F_COUNT && Q->gate) { d[0] = 0.05f; if (n > 1) d[1] = 0.95f; }   /* nothing wrong: "no" to every opener */
}
/* "Is it this? <what it looks like>" - one per fault, asked to break ties and to double-check
   the leader before a fix is suggested. id = CAL_NQ + fault */
static float confirm_lik(int g, int f, int a)
{
	float y = f == g ? 0.88f : f == F_COUNT ? 0.04f : 0.07f;
	if (f != g && f < F_COUNT && CAL_FAULT[f].area == CAL_FAULT[g].area && !strcmp(CAL_FAULT[f].why, CAL_FAULT[g].why)) y = 0.5f;
	float p = a == ANS_YES ? y : 1 - y;
	return 0.94f * p + 0.03f;
}
static float likq(int q, int f, int a);
/* every answer's chance for every fault, worked out once (the Wii's CPU thanks us) */
static float (*lik_tab)[F_COUNT + 1][MAX_OPT];
static void lik_build(void)
{
	lik_tab = malloc(sizeof(*lik_tab) * CAL_NQ);
	if (!lik_tab) return;
	for (int q = 0; q < CAL_NQ; q++) {
		int n = cal_q_nopt(q);
		for (int f = 0; f <= F_COUNT; f++) {
			float d[MAX_OPT]; q_dist(q, f, d);
			/* people get it wrong now and then: every answer has a 6% chance of being a slip */
			for (int a = 0; a < MAX_OPT; a++) lik_tab[q][f][a] = a < n ? 0.94f * d[a] + 0.06f / n : 1;
		}
	}
}
static float lik(const CalQuestion *q, int f, int a)
{
	if (a == ANS_UNSURE) return 1;   /* not sure: tells nothing */
	int qi = (int)(q - CAL_Q), n = cal_q_nopt(qi);
	if (a < 0 || a >= n) return 1;
	if (!lik_tab) lik_build();
	if (lik_tab) return lik_tab[qi][f][a];
	float d[MAX_OPT]; q_dist(qi, f, d);
	return 0.94f * d[a] + 0.06f / n;
}
static float likq(int q, int f, int a)
{
	if (q >= CAL_NQ) return a == ANS_UNSURE ? 1 : confirm_lik((q - CAL_NQ) % F_COUNT, f, a);
	return lik(&CAL_Q[q], f, a);
}
static int in_area(int f) { return dx_area == AR_ALL || CAL_FAULT[f].area == dx_area; }

static void dx_recalc(void)
{
	for (int f = 0; f <= F_COUNT; f++) {
		float v;
		if (f == F_COUNT) v = logf(0.04f);
		else if (dx_out[f] || !in_area(f)) { dx_lp[f] = -1e30f; continue; }
		else v = logf((float)CAL_FAULT[f].prior);
		for (int k = 0; k < dx_n; k++) v += logf(likq(dx_q[k], f, dx_a[k]));
		dx_lp[f] = v;
	}
}
static void dx_post(float *p)
{
	float mx = -1e30f; for (int f = 0; f <= F_COUNT; f++) if (dx_lp[f] > mx) mx = dx_lp[f];
	double s = 0; for (int f = 0; f <= F_COUNT; f++) { p[f] = dx_lp[f] < -1e29f ? 0 : expf(dx_lp[f] - mx); s += p[f]; }
	for (int f = 0; f <= F_COUNT; f++) p[f] = (float)(p[f] / s);
}

void cal_dx_start(int area) { dx_area = area; dx_n = 0; memset(dx_out, 0, sizeof dx_out); dx_recalc(); }
int cal_dx_area(void) { return dx_area; }
int cal_dx_asked(void) { return dx_n; }
void cal_dx_answer(int q, int ans) { if (dx_n < 128) { dx_q[dx_n] = (s16)q; dx_a[dx_n] = (u8)ans; dx_n++; } dx_recalc(); }
void cal_dx_undo(void) { if (dx_n) dx_n--; dx_recalc(); }
void cal_dx_exclude(int f) { if (f >= 0 && f < F_COUNT) dx_out[f] = 1; dx_recalc(); }

int cal_dx_top(int *f, float *p, int max)
{
	float post[F_COUNT + 1]; dx_post(post);
	int n = 0;
	for (int i = 0; i < F_COUNT; i++) {
		if (post[i] <= 0) continue;
		int j;
		if (n < max) j = n++;
		else if (post[i] > p[max - 1]) j = max - 1;
		else continue;
		while (j > 0 && p[j - 1] < post[i]) { f[j] = f[j - 1]; p[j] = p[j - 1]; j--; }
		f[j] = i; p[j] = post[i];
	}
	return n;
}
float cal_dx_none(void) { float post[F_COUNT + 1]; dx_post(post); return post[F_COUNT]; }

static int asked(int q) { for (int k = 0; k < dx_n; k++) if (dx_q[k] == q) return 1; return 0; }
/* a question counts in an area if it can tell apart any of that area's faults */
static int q_in_area(int q)
{
	if (dx_area == AR_ALL || CAL_Q[q].area == dx_area || CAL_Q[q].area == AR_ALL) return 1;
	for (int k = 0; k < CAL_Q[q].nqp; k++) if (CAL_FAULT[CAL_Q[q].qp[k].f].area == dx_area) return 1;
	return 0;
}

/* sure enough: one fault well ahead, AND at least two questions spoke up for it */
int cal_dx_confident(void)
{
	int f[2]; float p[2];
	int n = cal_dx_top(f, p, 2);
	if (!n || p[0] < 0.93f || (n > 1 && p[0] < 14 * p[1])) return 0;
	/* at least two answers that point at it more than 3 to 1 against a TV without it */
	int backed = 0;
	for (int k = 0; k < dx_n; k++) {
		if (dx_a[k] == ANS_UNSURE) continue;
		float a = likq(dx_q[k], f[0], dx_a[k]), b = likq(dx_q[k], F_COUNT, dx_a[k]);
		if (a > 3 * b) backed++;
	}
	/* and you said yes when asked "is this it?" */
	int said_yes = 0;
	for (int k = 0; k < dx_n; k++) if ((dx_q[k] == CAL_NQ + f[0] || dx_q[k] == CAL_NQ + F_COUNT + f[0]) && dx_a[k] == ANS_YES) said_yes = 1;
	return backed >= 3 && said_yes;
}

static float H(const float *p, int n) { float h = 0; for (int i = 0; i < n; i++) if (p[i] > 1e-7f) h -= p[i] * logf(p[i]); return h; }

int cal_dx_next(void)
{
	float post[F_COUNT + 1]; dx_post(post);
	if (cal_dx_confident()) return -1;
	if (dx_n >= 30) return -1;
	float h0 = H(post, F_COUNT + 1);
	float best = 0.02f; int bq = -1;
	for (int q = 0; q < CAL_NQ; q++) {
		if (asked(q) || !q_in_area(q)) continue;
		if (CAL_Q[q].gate && dx_area != AR_ALL) continue;
		int n = cal_q_nopt(q);
		float ex = 0;
		for (int a = 0; a < n; a++) {
			float pa = 0, after[F_COUNT + 1];
			for (int f = 0; f <= F_COUNT; f++) { after[f] = post[f] > 0 ? post[f] * likq(q, f, a) : 0; pa += after[f]; }
			if (pa <= 1e-9f) continue;
			for (int f = 0; f <= F_COUNT; f++) after[f] /= pa;
			ex += pa * H(after, F_COUNT + 1);
		}
		float gain = h0 - ex;
		if (CAL_Q[q].gate && dx_n < 5) gain *= 1.3f;   /* broad questions early */
		if (gain > best) { best = gain; bq = q; }
	}
	/* "is it this?" for the leaders: worth it once the field is narrow, and always before a fix */
	int tf[6]; float tp[6]; int nt = cal_dx_top(tf, tp, 6);
	for (int k = 0; k < nt * 2; k++) {
		/* first "is this it?", then, if the answers still disagree, one "let's double-check" */
		int f1 = tf[k % nt], second = k >= nt;
		int q = CAL_NQ + f1 + (second ? F_COUNT : 0);
		if (asked(q) || tp[k % nt] < 0.03f) continue;
		if (second && !asked(CAL_NQ + f1)) continue;
		float ex = 0;
		for (int a = 0; a < 2; a++) {
			float pa = 0, after[F_COUNT + 1];
			for (int f = 0; f <= F_COUNT; f++) { after[f] = post[f] > 0 ? post[f] * likq(q, f, a) : 0; pa += after[f]; }
			if (pa <= 1e-9f) continue;
			for (int f = 0; f <= F_COUNT; f++) after[f] /= pa;
			ex += pa * H(after, F_COUNT + 1);
		}
		float gain = (h0 - ex) * (dx_n < 3 ? 0.5f : 1.0f);   /* open questions first */
		if (gain > best || (bq < 0 && gain > 0.001f)) { best = gain; bq = q; }
	}
	return bq;
}
