/*
   music.c - the casino's music and sound effects, made live by a small synth.
   No sound files: a walking bass, an electric piano, vibes and brushes play jazz changes,
   and the card, chip, slot and roulette sounds come from the same voices.
   The platform pulls finished audio from music_render (stereo, 16 bit).
   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "common.h"
#include <math.h>

/* ------------------------------------------------------------------ */
/* basics                                                             */
/* ------------------------------------------------------------------ */
#define TAB 4096
static float sine_tab[TAB];
static int tab_ok;
static inline float sn(float ph) { return sine_tab[(int)(ph * TAB) & (TAB - 1)]; }   /* ph in turns */
static u32 nz = 22222;
static inline float noise(void) { nz ^= nz << 13; nz ^= nz >> 17; nz ^= nz << 5; return (int)(nz & 0xFFFF) / 32768.0f - 1.0f; }
static float midi_f(float m) { return 440.0f * powf(2.0f, (m - 69.0f) / 12.0f); }
static u32 mrng = 99;
static int mr(int n) { mrng = mrng * 1103515245u + 12345u; return (int)((mrng >> 16) % (u32)n); }

/* ------------------------------------------------------------------ */
/* voices                                                             */
/* ------------------------------------------------------------------ */
enum { I_BASS, I_KEYS, I_VIBES, I_RIDE, I_HAT, I_CLINK, I_SWISH, I_THUMP, I_TICK, I_BLIP };
typedef struct {
	u8 on, ins, mus;          /* mus = part of the music (fades with it), else a sound effect */
	float ph, ph2, f, amp, env, dec, pan, age, lp, lp2, hp, send;
} Voice;
#define NV 40
static Voice V[NV];

static Voice *voice_new(void)
{
	int best = 0; float oldest = -1;
	for (int i = 0; i < NV; i++) {
		if (!V[i].on) { best = i; oldest = 1e9f; break; }
		if (V[i].age > oldest) { oldest = V[i].age; best = i; }
	}
	Voice *v = &V[best];
	memset(v, 0, sizeof *v);
	v->on = 1;
	return v;
}
static float rate_now = 48000;
static void voice_start(int ins, float f, float amp, float tau, float pan, int mus, float send)
{
	Voice *v = voice_new();
	v->ins = ins; v->f = f; v->amp = amp; v->env = 1; v->pan = pan; v->mus = mus; v->send = send;
	v->dec = expf(-1.0f / (tau * rate_now));
}

/* one sample of one voice */
static float lfo_t;
static inline float voice_tick(Voice *v, float dt, float pitch)
{
	float f = v->f * (v->mus ? pitch : 1.0f), s = 0, e = v->env;
	float att = v->age < 0.004f ? v->age / 0.004f : 1.0f;
	switch (v->ins) {
	case I_BASS:          /* upright bass: round, a bit of growl that dies fast */
		s = sn(v->ph) + (0.5f * sn(v->ph * 2) + 0.25f * sn(v->ph * 3)) * (0.4f + 0.6f * e);
		if (v->age < 0.02f) s += noise() * 0.25f * (1 - v->age / 0.02f);   /* the pluck */
		v->ph += f * dt; if (v->ph > 64) v->ph -= 64;
		break;
	case I_KEYS: {        /* electric piano: FM, a bark on the attack, slow tremolo */
		float idx = e * e * 1.3f;
		float mod = sn(v->ph2) * idx;
		s = sn(v->ph + mod * 0.22f) + 0.12f * sn(v->ph * 7.0f) * e * e * e * e;
		s *= 1.0f - 0.12f * (0.5f + 0.5f * sn(lfo_t * 4.2f));
		v->ph += f * dt; v->ph2 += f * dt;
		if (v->ph > 64) v->ph -= 64; if (v->ph2 > 64) v->ph2 -= 64;
		break; }
	case I_VIBES:         /* vibraphone: pure bars, the motor tremolo */
		s = sn(v->ph) + 0.1f * sn(v->ph * 4.0f) * e + 0.05f * sn(v->ph * 10.0f) * e * e;
		s *= 1.0f - 0.35f * (0.5f + 0.5f * sn(lfo_t * 5.5f));
		v->ph += f * dt; if (v->ph > 64) v->ph -= 64;
		break;
	case I_RIDE: {        /* ride cymbal: metal partials over hiss */
		float n = noise(); v->hp = n - v->lp; v->lp += (n - v->lp) * 0.35f;
		s = v->hp * 0.6f + 0.25f * (sn(v->ph) + sn(v->ph * 1.47f) + sn(v->ph * 1.83f)) * e;
		v->ph += f * dt; if (v->ph > 64) v->ph -= 64;
		break; }
	case I_HAT: {         /* brush or foot hat: filtered hiss */
		float n = noise(); v->lp += (n - v->lp) * 0.5f; s = n - v->lp;
		break; }
	case I_CLINK:         /* clay chips knocking: two bright partials, very short */
		s = sn(v->ph) + 0.6f * sn(v->ph * 1.52f) + 0.3f * sn(v->ph * 2.23f);
		if (v->age < 0.003f) s += noise() * 0.6f;
		v->ph += f * dt; if (v->ph > 64) v->ph -= 64;
		break;
	case I_SWISH: {       /* a card sliding on felt: band of hiss that swells then drops */
		float n = noise();
		v->lp += (n - v->lp) * (v->f / rate_now * 6.283f > 0.9f ? 0.9f : v->f / rate_now * 6.283f);
		v->lp2 += (v->lp - v->lp2) * 0.08f;
		s = (v->lp - v->lp2) * 2.2f;
		att = v->age < 0.012f ? v->age / 0.012f : 1.0f;
		break; }
	case I_THUMP:         /* a heavy thunk: a falling tone and a knock */
		s = sn(v->ph) * 1.2f;
		if (v->age < 0.006f) s += noise() * 0.7f;
		v->ph += (f * (0.45f + 0.55f * e)) * dt; if (v->ph > 64) v->ph -= 64;
		break;
	case I_BLIP: {        /* a voice: a buzzy vowel that slides a little */
		float fm = f * (1.0f + 0.08f * (1 - e));
		s = sn(v->ph) * (0.55f + 0.45f * sn(v->ph * 2.01f)) + 0.3f * sn(v->ph * 3.0f) + 0.12f * sn(v->ph2);
		v->ph += fm * dt; v->ph2 += fm * 0.5f * dt;
		if (v->ph > 64) v->ph -= 64; if (v->ph2 > 64) v->ph2 -= 64;
		break; }
	default:              /* tick: the ball hitting a fret */
		s = sn(v->ph) + noise() * 0.5f;
		v->ph += f * dt; if (v->ph > 64) v->ph -= 64;
		break;
	}
	v->env *= v->dec;
	v->age += dt;
	if (v->env < 0.0006f) v->on = 0;
	return s * e * att * v->amp;
}

/* ------------------------------------------------------------------ */
/* the tunes                                                          */
/* ------------------------------------------------------------------ */
enum { Q_M7, Q_7, Q_MAJ7, Q_M7B5, Q_7B9, Q_M9, Q_13, Q_M6 };
static const s8 Q_TONES[8][4] = { { 0, 3, 7, 10 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11 }, { 0, 3, 6, 10 }, { 0, 4, 7, 10 }, { 0, 3, 7, 10 }, { 0, 4, 7, 10 }, { 0, 3, 7, 9 } };
static const s8 Q_VOICE[8][4] = { { 3, 7, 10, 14 }, { 4, 9, 10, 14 }, { 4, 7, 11, 14 }, { 3, 6, 10, 12 }, { 4, 10, 13, 19 }, { 3, 7, 10, 14 }, { 4, 10, 14, 21 }, { 3, 7, 9, 14 } };
typedef struct { u8 pc, q; } Chord;
typedef struct {
	float bpm, swing, reverb;
	int key;                /* for the win jingles */
	int noir;               /* 1 = slow and sparse */
	Chord ch[16];           /* two chords a bar, 8 bars */
} Tune;
#define C_(p, q) { p, q }
enum { NC = 0, NCs, ND, NEb, NE, NF, NFs, NG, NAb, NA, NBb, NB };
static const Tune TUNES[3] = {
	/* the Main Floor: medium swing in F */
	{ 118, 0.64f, 0.32f, 65, 0, {
		C_(NG, Q_M9), C_(NG, Q_M9), C_(NC, Q_13), C_(NC, Q_13), C_(NF, Q_MAJ7), C_(NF, Q_MAJ7), C_(ND, Q_7B9), C_(ND, Q_7B9),
		C_(NG, Q_M9), C_(NG, Q_M9), C_(NC, Q_13), C_(NC, Q_13), C_(NA, Q_M7), C_(ND, Q_7B9), C_(NG, Q_M7), C_(NC, Q_7) } },
	/* the Suite: slow noir in C minor */
	{ 76, 0.66f, 0.5f, 60, 1, {
		C_(NC, Q_M9), C_(NC, Q_M9), C_(NC, Q_M9), C_(NC, Q_M9), C_(NAb, Q_MAJ7), C_(NAb, Q_MAJ7), C_(NG, Q_7B9), C_(NG, Q_7B9),
		C_(NF, Q_M9), C_(NF, Q_M9), C_(NF, Q_M9), C_(NF, Q_M9), C_(ND, Q_M7B5), C_(NG, Q_7B9), C_(NC, Q_M9), C_(NG, Q_7B9) } },
	/* the Back Room: fast minor swing in D, like a card game in a smoky cellar */
	{ 152, 0.63f, 0.28f, 62, 0, {
		C_(ND, Q_M6), C_(ND, Q_M6), C_(ND, Q_M6), C_(ND, Q_M6), C_(NG, Q_M6), C_(NG, Q_M6), C_(NA, Q_7B9), C_(NA, Q_7B9),
		C_(ND, Q_M6), C_(ND, Q_M6), C_(NG, Q_M6), C_(NG, Q_M6), C_(NE, Q_M7B5), C_(NA, Q_7B9), C_(ND, Q_M6), C_(NA, Q_7B9) } },
};

/* ------------------------------------------------------------------ */
/* the band                                                           */
/* ------------------------------------------------------------------ */
static int tune = -1, want_tune = -1, vol = 2;
static float mus_gain, mus_target, creep, creep_want;
static float bp; static int beat, bar, chorus, half_done, started;
static int bass_prev = 41, mel_prev = 72, comp_mask;

static const Tune *T(void) { return &TUNES[tune < 0 ? 0 : tune]; }
static Chord chord_at(int b, int bt) { return T()->ch[((b % 8) * 2 + (bt >= 2)) & 15]; }
static int nearest(int pc, int prev, int lo, int hi)   /* the note of pitch class pc nearest prev, in lo..hi */
{
	int best = lo, bd = 999;
	for (int m = lo; m <= hi; m++) if ((m % 12) == pc) { int d = abs(m - prev); if (d < bd) { bd = d; best = m; } }
	return best;
}

static void keys_hit(Chord c, float vel)
{
	/* rootless voicing around middle C */
	int base = nearest(c.pc, 50, 46, 57);
	float tau = T()->noir ? 2.2f : 1.1f;
	for (int k = 0; k < 4; k++) {
		int m = base + Q_VOICE[c.q][k];
		while (m > 72) m -= 12;
		voice_start(I_KEYS, midi_f(m), 0.075f * vel, tau, -0.25f + k * 0.05f, 1, 0.4f);
	}
}

static void on_step(int st)   /* st = 8th note 0..7 in the bar */
{
	const Tune *t = T();
	int bt = st / 2, off = st & 1;
	Chord c = chord_at(bar, bt);
	/* a new comping rhythm each bar */
	if (st == 0) {
		static const u8 PAT[6] = { 0x09, 0x48, 0x12, 0x21, 0x84, 0x28 };   /* bit n = 8th note n */
		comp_mask = t->noir ? 0x11 : PAT[mr(6)];
		if (!t->noir && mr(4) == 0) comp_mask |= 0x01;
	}
	/* bass: walking quarters (swing), or half notes (noir) */
	if (!off) {
		Chord nx = bt == 3 ? chord_at(bar + 1, 0) : chord_at(bar, bt + 1);
		int m;
		int change = nx.pc != c.pc || nx.q != c.q;
		if (bt == 0 || (bt == 2 && chord_at(bar, 1).pc != c.pc)) m = nearest(c.pc, bass_prev, 36, 50);
		else if ((bt == 3 || change) && !t->noir) m = nearest((nx.pc + (mr(2) ? 1 : 11)) % 12, bass_prev, 36, 50);   /* lead in by a half step */
		else m = nearest((c.pc + Q_TONES[c.q][1 + mr(3)]) % 12, bass_prev, 36, 50);
		int play = !t->noir || bt == 0 || bt == 2 || (bt == 3 && mr(3) == 0);
		if (play) {
			if (t->noir && bt == 2 && chord_at(bar, 1).pc == c.pc) m = nearest((c.pc + 7) % 12, bass_prev, 36, 50);
			voice_start(I_BASS, midi_f(m), 0.30f, t->noir ? 0.9f : 0.45f, 0, 1, 0.06f);
			bass_prev = m;
		}
	}
	/* piano */
	if (comp_mask & (1 << st)) keys_hit(c, 0.7f + mr(30) / 100.0f);
	/* drums: ride "ding, ding-ga", foot hat on 2 and 4 */
	if (!t->noir) {
		if (!off || st == 3 || st == 7) voice_start(I_RIDE, 3150, (off ? 0.035f : 0.05f), 0.32f, 0.35f, 1, 0.2f);
		if (st == 2 || st == 6) voice_start(I_HAT, 0, 0.05f, 0.035f, 0.3f, 1, 0.05f);
	} else {
		if (!off) voice_start(I_HAT, 0, st == 2 || st == 6 ? 0.05f : 0.03f, 0.12f, 0.3f, 1, 0.3f);   /* brushes */
		if (st == 0 && (bar & 1) == 0) voice_start(I_RIDE, 2900, 0.03f, 1.2f, 0.4f, 1, 0.5f);
	}
	/* vibes: a made-up melody in the first half of every other chorus */
	int solo = (chorus & 1) && bar < 6 && !(bar == 5 && st > 4);
	if (solo && (t->noir ? (!off && mr(10) < 4) : mr(10) < 5)) {
		int lo = t->noir ? 62 : 67, hi = t->noir ? 79 : 84, cand[12], n = 0;
		for (int m = lo; m <= hi; m++) {
			int iv = ((m - c.pc) % 12 + 12) % 12;
			if (iv == Q_TONES[c.q][1] || iv == Q_TONES[c.q][2] || iv == Q_TONES[c.q][3] || iv == 2 || iv == 0)
				if (abs(m - mel_prev) <= 5 && n < 12) cand[n++] = m;
		}
		int m = n ? cand[mr(n)] : nearest(c.pc, mel_prev, lo, hi);
		voice_start(I_VIBES, midi_f(m), (t->noir ? 0.13f : 0.12f) + mr(6) / 100.0f, t->noir ? 1.8f : 1.0f, 0.3f, 1, 0.45f);
		mel_prev = m;
	}
}

static void band_reset(void)
{
	bp = 0; beat = 0; bar = 0; chorus = 0; half_done = 0; started = 0;
	for (int i = 0; i < NV; i++) if (V[i].on && V[i].mus) V[i].dec = expf(-1.0f / (0.05f * rate_now));
}

/* ------------------------------------------------------------------ */
/* sound effects (can be set to go off a little later)                */
/* ------------------------------------------------------------------ */
typedef struct { int kind, p; float wait; } Pending;
#define NQ 48
static Pending Q[NQ];
static int fx_on = 1;
static void fx_now(int kind, int p);
void music_sfx(int kind, int delay_ms)
{
	if (!fx_on) return;
	if (delay_ms <= 0) { fx_now(kind, 0); return; }
	for (int i = 0; i < NQ; i++) if (!Q[i].kind) { Q[i].kind = kind + 1; Q[i].p = 0; Q[i].wait = delay_ms / 1000.0f; return; }
}
static void fx_later(int kind, int p, float sec)
{
	for (int i = 0; i < NQ; i++) if (!Q[i].kind) { Q[i].kind = kind + 1; Q[i].p = p; Q[i].wait = sec; return; }
}
enum { FX_NOTE = 100 };   /* internal: one vibes note (p = midi) */
static void fx_now(int kind, int p)
{
	int key = tune >= 0 ? T()->key : 65;
	switch (kind) {
	case SFX_CARD: voice_start(I_SWISH, 2600 + mr(900), 0.5f, 0.05f, -0.1f + mr(20) / 100.0f, 0, 0.1f); break;
	case SFX_FLIP: voice_start(I_SWISH, 4200, 0.35f, 0.025f, 0, 0, 0.1f); break;
	case SFX_CHIP:
		voice_start(I_CLINK, 2700 + mr(500), 0.14f, 0.05f, 0.1f, 0, 0.15f);
		fx_later(SFX_CHIP + 50, 0, 0.028f + mr(20) / 1000.0f);
		break;
	case SFX_CHIP + 50: voice_start(I_CLINK, 3300 + mr(600), 0.10f, 0.04f, -0.05f, 0, 0.15f); break;
	case SFX_WIN: { static const int A[4] = { 0, 4, 7, 12 }; for (int k = 0; k < 4; k++) fx_later(FX_NOTE, key + 12 + A[k], k * 0.075f); break; }
	case SFX_BIGWIN: {
		static const int A[9] = { 0, 4, 7, 11, 12, 16, 19, 23, 24 };
		for (int k = 0; k < 9; k++) fx_later(FX_NOTE, key + 12 + A[k], k * 0.06f);
		voice_start(I_RIDE, 2600, 0.12f, 1.4f, 0, 0, 0.5f);
		break; }
	case SFX_ACH: fx_later(FX_NOTE, key + 24, 0); fx_later(FX_NOTE, key + 28, 0.09f); fx_later(FX_NOTE, key + 31, 0.18f); break;
	case SFX_LEVER:
		voice_start(I_THUMP, 90, 0.35f, 0.12f, 0.3f, 0, 0.1f);
		for (int k = 1; k <= 5; k++) fx_later(SFX_TICK + 50, 0, k * 0.035f);
		break;
	case SFX_TICK + 50: voice_start(I_TICK, 1800, 0.06f, 0.01f, 0.3f, 0, 0.05f); break;
	case SFX_REEL: voice_start(I_THUMP, 120, 0.28f, 0.07f, 0, 0, 0.1f); voice_start(I_CLINK, 1900, 0.05f, 0.03f, 0, 0, 0.1f); break;
	case SFX_TICK: voice_start(I_TICK, 4200 + mr(900), 0.07f, 0.012f, -0.2f, 0, 0.15f); break;
	case SFX_LOSE: fx_later(FX_NOTE, key - 1, 0); fx_later(FX_NOTE, key - 6, 0.16f); break;
	case SFX_ROLL:   /* dice rattling in a leather cup */
		for (int k = 0; k < 14; k++) fx_later(SFX_ROLL + 50, 0, k * 0.028f + mr(20) / 1000.0f);
		break;
	case SFX_ROLL + 50: voice_start(I_CLINK, 1300 + mr(900), 0.05f, 0.018f, -0.3f + mr(60) / 100.0f, 0, 0.1f); voice_start(I_TICK, 700 + mr(400), 0.04f, 0.01f, 0, 0, 0.05f); break;
	case SFX_SLAM: voice_start(I_THUMP, 70, 0.45f, 0.14f, 0, 0, 0.2f); voice_start(I_SWISH, 900, 0.3f, 0.05f, 0, 0, 0.1f); break;
	case SFX_LIAR:   /* a hit: a low chord and a thump */
		voice_start(I_THUMP, 55, 0.5f, 0.3f, 0, 0, 0.4f);
		fx_later(FX_NOTE, key - 12, 0); fx_later(FX_NOTE, key - 11, 0.01f); fx_later(FX_NOTE, key - 6, 0.02f);
		break;
	case FX_NOTE: voice_start(I_VIBES, midi_f(p), kind == FX_NOTE && p < key ? 0.10f : 0.16f, 0.9f, 0.1f, 0, 0.45f); break;
	}
}

/* ------------------------------------------------------------------ */
/* what the casino asks for                                           */
/* ------------------------------------------------------------------ */
void music_play(int t, int volume, int effects)
{
	if (t > 2) t = 2;
	want_tune = t; vol = volume; fx_on = effects;
	mus_target = t < 0 ? 0 : volume <= 1 ? 0.5f : 1.0f;
}
void music_mood(float c) { creep_want = c < 0 ? 0 : c > 1 ? 1 : c; }
int music_busy(void)
{
	if (tune >= 0 || want_tune >= 0) return 1;
	for (int i = 0; i < NV; i++) if (V[i].on) return 1;
	for (int i = 0; i < NQ; i++) if (Q[i].kind) return 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the mixer                                                          */
/* ------------------------------------------------------------------ */
static float duck_hold, duck = 1;
static float lv_m = 1, lv_v = 1, lv_f = 1;
void music_levels(int m, int v, int f) { static const float L[4] = { 0, 0.35f, 0.65f, 1 }; lv_m = L[m & 3]; lv_v = L[v & 3]; lv_f = L[f & 3]; }
#define DL_MAX 4096
static float dlL[DL_MAX], dlR[DL_MAX], rlpL, rlpR, mlpL, mlpR;
static int dpos;

void music_render(s16 *out, int frames, int rate)
{
	if (!tab_ok) { for (int i = 0; i < TAB; i++) sine_tab[i] = sinf(i * 6.2831853f / TAB); tab_ok = 1; }
	if (rate < 8000) rate = 48000;
	rate_now = (float)rate;
	float dt = 1.0f / rate;
	int nL = rate * 37 / 1000, nR = rate * 49 / 1000;
	if (nL >= DL_MAX) nL = DL_MAX - 1; if (nR >= DL_MAX) nR = DL_MAX - 1;
	for (int i = 0; i < frames; i++) {
		/* tune changes: fade out, swap, fade back in */
		if (want_tune != tune) {
			mus_gain -= dt / 0.6f;
			if (mus_gain <= 0 || tune < 0) { mus_gain = 0; tune = want_tune; band_reset(); }
		} else if (mus_gain < mus_target) { mus_gain += dt / 1.2f; if (mus_gain > mus_target) mus_gain = mus_target; }
		else if (mus_gain > mus_target) { mus_gain -= dt / 0.5f; if (mus_gain < mus_target) mus_gain = mus_target; }
		creep += (creep_want - creep) * dt * 1.5f;
		if (duck_hold > 0) duck_hold -= dt;
		duck += ((duck_hold > 0 ? 0.45f : 1.0f) - duck) * dt * 12.0f;
		lfo_t += dt; if (lfo_t > 1000) lfo_t -= 1000;

		/* the band's clock: the tempo drags when he reads you */
		if (tune >= 0) {
			const Tune *t = T();
			if (!started) { started = 1; on_step(0); }
			bp += t->bpm * (1.0f - 0.35f * creep) / (60.0f * rate);
			if (!half_done && bp >= t->swing) { half_done = 1; on_step(beat * 2 + 1); }
			if (bp >= 1) {
				bp -= 1; half_done = 0;
				if (++beat == 4) { beat = 0; if (++bar == 8) { bar = 0; chorus++; } }
				on_step(beat * 2);
			}
		}
		/* sound effects waiting their turn */
		for (int q = 0; q < NQ; q++) if (Q[q].kind) { Q[q].wait -= dt; if (Q[q].wait <= 0) { int k = Q[q].kind - 1, p = Q[q].p; Q[q].kind = 0; fx_now(k, p); } }

		/* out of tune and wobbling while he reads you */
		float pitch = 1.0f - 0.03f * creep + 0.015f * creep * sn(lfo_t * 0.35f);
		float L = 0, R = 0, rin = 0;
		for (int k = 0; k < NV; k++) {
			Voice *v = &V[k];
			if (!v->on) continue;
			float s = voice_tick(v, dt, pitch);
			if (v->mus) s *= mus_gain * duck * lv_m; else if (v->ins == I_BLIP) s *= lv_v; else s *= lv_f;
			L += s * (1.0f - v->pan) * 0.5f * 1.4f; R += s * (1.0f + v->pan) * 0.5f * 1.4f;
			rin += s * v->send;
		}
		/* a small room: two delays feeding each other through a soft filter */
		float fb = (tune >= 0 ? T()->reverb : 0.3f) + creep * 0.25f;
		float yl = dlL[(dpos - nL + DL_MAX) % DL_MAX], yr = dlR[(dpos - nR + DL_MAX) % DL_MAX];
		rlpL += (yl - rlpL) * 0.35f; rlpR += (yr - rlpR) * 0.35f;
		dlL[dpos] = rin * 0.5f + rlpR * fb; dlR[dpos] = rin * 0.5f + rlpL * fb;
		dpos = (dpos + 1) % DL_MAX;
		L += yl * 0.7f; R += yr * 0.7f;
		/* warmth, darker when he's creeping */
		float a = 0.55f - 0.4f * creep;
		mlpL += (L - mlpL) * a; mlpR += (R - mlpR) * a;
		L = mlpL; R = mlpR;
		/* soft limit */
		L *= 1.4f; R *= 1.4f;
		if (L > 3) L = 3; if (L < -3) L = -3; if (R > 3) R = 3; if (R < -3) R = -3;
		L = L * (27 + L * L) / (27 + 9 * L * L); R = R * (27 + R * R) / (27 + 9 * R * R);
		out[i * 2] = (s16)(L * 30000); out[i * 2 + 1] = (s16)(R * 30000);
	}
}

/* each dealer's voice, a blip a syllable: the pitch moves with the letters, so it sounds like talk */
void music_voice(int who, int ch, int creepy)
{
	if (!fx_on) return;
	static const float BASE[6] = { 150, 215, 92, 300, 235, 170 };
	static const float AMP[6] = { 0.30f, 0.26f, 0.34f, 0.24f, 0.22f, 0.27f };
	if (who < 0 || who > 5) who = 0;
	if (ch >= 'A' && ch <= 'Z') ch += 32;
	int vowel = strchr("aeiou", ch) != NULL;
	float f = BASE[who] * (1.0f + (((ch * 37) % 9) - 4) * 0.035f + (vowel ? 0.06f : 0));
	if (who == 3) f *= 0.85f + mr(40) / 100.0f;     /* Squid never has the same voice twice */
	float tau = vowel ? 0.085f : 0.05f;
	duck_hold = 0.25f;   /* the band plays softer while someone is talking */
	voice_start(I_BLIP, f, AMP[who], tau, (who - 2.5f) * 0.08f, 0, creepy ? 0.6f : 0.15f);
	if (creepy) voice_start(I_BLIP, f * 0.5f * 1.013f, AMP[who] * 0.8f, tau * 1.6f, 0, 0, 0.6f);   /* a second voice under his */
}
