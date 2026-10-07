/*
   ir.c - IR protocol encoders for the Wii sensor bar.
   WiiRremote Plus by Habex, based on WiiRremote by Abdelali221 (GPL-3.0).
   Protocol timings follow the Arduino-IRremote library.
*/
#include "common.h"
#include "irlow.h"

const char *PROTO_NAME[P_COUNT] = { "NEC", "SAMSUNG", "RC5", "SONY", "JVC", "PANASONIC", "SHARP", "RCA", "MITSUBISHI", "NECX", "RC6", "SONY15", "RCA38" };

u16 addr_wrap(u8 proto, int a)
{
	static const int MAXA[P_COUNT] = { 256, 256, 32, 32, 256, 65536, 32, 16, 256, 65536, 256, 256, 16 };
	int m = MAXA[proto < P_COUNT ? proto : 0];
	return (u16)(((a % m) + m) % m);
}

int proto_from_name(const char *s)
{
	for (int i = 0; i < P_COUNT; i++) if (!strcmp(s, PROTO_NAME[i])) return i;
	return -1;
}

static void pd_byte(u8 b, u32 mark, u32 zero, u32 one)
{
	for (int i = 0; i < 8; i++) {  /* LSB first */
		irlow_mark(mark);
		irlow_space((b >> i) & 1 ? one : zero);
	}
}

static void nec(u8 addr, u8 cmd)
{
	irlow_begin(38000);
	irlow_mark(9000); irlow_space(4500);
	pd_byte(addr, 563, 563, 1688);
	pd_byte((u8)~addr, 563, 563, 1688);
	pd_byte(cmd, 563, 563, 1688);
	pd_byte((u8)~cmd, 563, 563, 1688);
	irlow_mark(563);
	irlow_end();
}

static void nec_repeat(void)
{
	irlow_begin(38000);
	irlow_mark(9000); irlow_space(2250);
	irlow_mark(563);
	irlow_end();
}

static void samsung(u8 addr, u8 cmd)
{
	irlow_begin(38000);
	irlow_mark(4500); irlow_space(4500);
	pd_byte(addr, 560, 560, 1690);
	pd_byte(addr, 560, 560, 1690);
	pd_byte(cmd, 560, 560, 1690);
	pd_byte((u8)~cmd, 560, 560, 1690);
	irlow_mark(560);
	irlow_end();
}

static int rc5_toggle = 0;

static void rc5(u8 addr, u8 cmd)
{
	u16 bits = (1 << 13) | (((cmd & 0x40) ? 0 : 1) << 12) | ((rc5_toggle & 1) << 11)
	         | ((addr & 0x1F) << 6) | (cmd & 0x3F);
	u8 halves[28];
	for (int i = 0; i < 14; i++) {
		int b = (bits >> (13 - i)) & 1;   /* 1 = space then mark */
		halves[i * 2] = b ? 0 : 1;
		halves[i * 2 + 1] = b ? 1 : 0;
	}
	irlow_begin(36000);
	int i = (halves[0] == 0) ? 1 : 0;
	while (i < 28) {
		int lvl = halves[i]; u32 len = 0;
		while (i < 28 && halves[i] == lvl) { len += 889; i++; }
		if (lvl) irlow_mark(len); else irlow_space(len);
	}
	irlow_end();
}

static void sony_frame_n(u8 addr, u8 cmd, int abits)
{
	irlow_begin(40000);
	irlow_mark(2400);
	for (int i = 0; i < 7; i++) { irlow_space(600); irlow_mark((cmd >> i) & 1 ? 1200 : 600); }
	for (int i = 0; i < abits; i++) { irlow_space(600); irlow_mark((addr >> i) & 1 ? 1200 : 600); }
	irlow_end();
}
static void sony_frame(u8 addr, u8 cmd) { sony_frame_n(addr, cmd, 5); }

static u32 ir_lvl;   /* interrupt state saved by ir_send */

/* gap between repeated frames: let interrupts run (audio, Bluetooth, video) while we wait */
static void busy_ms(u64 t, int ms)
{
#ifndef HOST
	irlow_unlock(ir_lvl);
	while (plat_ms() - t < (u64)ms) { }
	ir_lvl = irlow_lock();
#else
	(void)t; (void)ms;
#endif
}

static void sony_n(u8 addr, u8 cmd, int abits)
{
	/* Sony TVs want the frame 3 times, 45ms apart */
	for (int r = 0; r < 3; r++) {
		u64 t = plat_ms();
		sony_frame_n(addr, cmd, abits);
		busy_ms(t, 45);
	}
}

static void sony(u8 addr, u8 cmd)
{
	for (int r = 0; r < 3; r++) {
		u64 t = plat_ms();
		sony_frame(addr, cmd);
#ifndef HOST
		while (plat_ms() - t < 45) { }
#else
		(void)t;
#endif
	}
}

/* JVC: 8-bit address + 8-bit command, 38 kHz; repeats have no header */
static void jvc(u8 addr, u8 cmd, int header)
{
	irlow_begin(38000);
	if (header) { irlow_mark(8400); irlow_space(4200); }
	pd_byte(addr, 526, 526, 1574);
	pd_byte(cmd, 526, 526, 1574);
	irlow_mark(526);
	irlow_end();
}

/* Panasonic (Kaseikyo 48 bit): 02 20 device subdevice(0) command check, 37 kHz */
static void pana(u16 addr, u8 cmd)
{
	u8 dev = addr & 0xFF, sub = addr >> 8;   /* high byte = sub-device (0 for most TVs) */
	irlow_begin(37000);
	irlow_mark(3456); irlow_space(1728);
	u8 b[6] = { 0x02, 0x20, dev, sub, cmd, (u8)(dev ^ sub ^ cmd) };
	for (int i = 0; i < 6; i++) pd_byte(b[i], 432, 432, 1296);
	irlow_mark(432);
	irlow_end();
}

/* NEC with a 16-bit address (no inverted address byte) */
static void necx(u16 addr, u8 cmd)
{
	irlow_begin(38000);
	irlow_mark(9000); irlow_space(4500);
	pd_byte(addr & 0xFF, 563, 563, 1688);
	pd_byte(addr >> 8, 563, 563, 1688);
	pd_byte(cmd, 563, 563, 1688);
	pd_byte((u8)~cmd, 563, 563, 1688);
	irlow_mark(563);
	irlow_end();
}

/* RC6 mode 0 (newer Philips): 36 kHz, T = 444 us, Manchester, 1 = mark then space */
static int rc6_toggle;
static void rc6(u8 addr, u8 cmd)
{
	u8 lv[80]; u8 ln[80]; int n = 0;
#define PUSH(l, t) do { if (n && lv[n - 1] == (l)) ln[n - 1] += (t); else { lv[n] = (l); ln[n] = (t); n++; } } while (0)
	PUSH(1, 6); PUSH(0, 2);                 /* leader */
	PUSH(1, 1); PUSH(0, 1);                 /* start bit = 1 */
	for (int i = 0; i < 3; i++) { PUSH(0, 1); PUSH(1, 1); }   /* mode 000 */
	if (rc6_toggle & 1) { PUSH(1, 2); PUSH(0, 2); } else { PUSH(0, 2); PUSH(1, 2); }   /* trailer (double) */
	u16 data = (addr << 8) | cmd;
	for (int i = 15; i >= 0; i--) { if ((data >> i) & 1) { PUSH(1, 1); PUSH(0, 1); } else { PUSH(0, 1); PUSH(1, 1); } }
#undef PUSH
	irlow_begin(36000);
	for (int i = 0; i < n; i++) { if (lv[i]) irlow_mark(ln[i] * 444); else irlow_space(ln[i] * 444); }
	irlow_end();
}

/* Sharp (lots of old Sharp CRTs): 5-bit address, 8-bit command, sent twice (2nd inverted) */
static void sharp_frame(u8 addr, u8 cmd, int second)
{
	u16 v = (addr & 0x1F) | ((u16)(second ? (u8)~cmd : cmd) << 5) | ((second ? 0 : 1) << 13) | ((second ? 1 : 0) << 14);
	irlow_begin(38000);
	for (int i = 0; i < 15; i++) { irlow_mark(320); irlow_space((v >> i) & 1 ? 1680 : 680); }
	irlow_mark(320);
	irlow_end();
}
static void sharp(u8 addr, u8 cmd)
{
	u64 t = plat_ms();
	sharp_frame(addr, cmd, 0);
	busy_ms(t, 40);
	sharp_frame(addr, cmd, 1);
}

/* RCA / Thomson / GE CRTs: 56 kHz, 4-bit address + 8-bit command, then both inverted, MSB first */
static void rca(u8 addr, u8 cmd, int khz)
{
	u32 v = ((u32)(addr & 0xF) << 20) | ((u32)cmd << 12) | ((u32)(~addr & 0xF) << 8) | (u8)~cmd;
	irlow_begin(khz * 1000);
	irlow_mark(4000); irlow_space(4000);
	for (int i = 23; i >= 0; i--) { irlow_mark(500); irlow_space((v >> i) & 1 ? 2000 : 1000); }
	irlow_mark(500);
	irlow_end();
}

/* Mitsubishi CRTs: 32.6 kHz, unit 300 us, 8-bit device + 8-bit command, sent twice */
static void mitsu_frame(u8 dev, u8 cmd)
{
	irlow_begin(32600);
	for (int i = 0; i < 8; i++) { irlow_mark(300); irlow_space((dev >> i) & 1 ? 2100 : 900); }
	for (int i = 0; i < 8; i++) { irlow_mark(300); irlow_space((cmd >> i) & 1 ? 2100 : 900); }
	irlow_mark(300);
	irlow_end();
}
static void mitsu(u8 dev, u8 cmd)
{
	u64 t = plat_ms();
	mitsu_frame(dev, cmd);
	busy_ms(t, 30);
	mitsu_frame(dev, cmd);
}

void ir_send(u8 proto, u16 addr, u8 cmd)
{
	ir_lvl = irlow_lock();
	switch (proto) {
		case P_NEC:     nec(addr, cmd); break;
		case P_SAMSUNG: samsung(addr, cmd); break;
		case P_RC5:     rc5_toggle ^= 1; rc5(addr, cmd); break;
		case P_SONY:    sony(addr, cmd); break;
		case P_JVC:     jvc(addr, cmd, 1); break;
		case P_PANA:    pana(addr, cmd); break;
		case P_SHARP:   sharp(addr, cmd); break;
		case P_RCA:     rca(addr, cmd, 56); break;
		case P_RCA38:   rca(addr, cmd, 38); break;
		case P_MITSU:   mitsu(addr, cmd); break;
		case P_NECX:    necx(addr, cmd); break;
		case P_RC6:     rc6_toggle ^= 1; rc6(addr, cmd); break;
		case P_SONY15:  sony_n(addr, cmd, 8); break;
	}
	irlow_unlock(ir_lvl);
	irlow_idle();
}

void ir_send_repeat(u8 proto, u16 addr, u8 cmd)
{
	ir_lvl = irlow_lock();
	switch (proto) {
		case P_NEC:     nec_repeat(); break;
		case P_SAMSUNG: samsung(addr, cmd); break;
		case P_RC5:     rc5(addr, cmd); break;        /* same toggle = held */
		case P_SONY:    sony_frame(addr, cmd); break;
		case P_JVC:     jvc(addr, cmd, 0); break;
		case P_PANA:    pana(addr, cmd); break;
		case P_SHARP:   sharp(addr, cmd); break;
		case P_RCA:     rca(addr, cmd, 56); break;
		case P_RCA38:   rca(addr, cmd, 38); break;
		case P_MITSU:   mitsu_frame(addr, cmd); break;
		case P_NECX:    nec_repeat(); break;
		case P_RC6:     rc6(addr, cmd); break;
		case P_SONY15:  sony_frame_n(addr, cmd, 8); break;
	}
	irlow_unlock(ir_lvl);
	irlow_idle();
}
