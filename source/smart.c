/*
   smart.c - Smart Match: works out which TV code set your TV uses, and fills in the rest.

   How it thinks (v1.9.1):
   - Every code sent, and every button already saved on your remote, is a piece of evidence.
     Saved buttons are "claims": they count, but only half as much as something you just
     saw with your own eyes, because names can be wrong. The check step turns claims into
     real answers.
   - For every TV code set in the library it asks "if this were your TV, how likely is what
     we saw?", button by button. It knows that a TV rarely has two codes for the same job,
     that Volume up / down get mixed up, and that arrows do nothing outside a menu.
   - "Not in the library" is a real answer too: then it uses what the library teaches about
     how remotes are laid out (Volume down is usually next to Volume up, the digits run in
     order...) to guess where the missing buttons are on YOUR TV's address.
   - The next question is the one that tells it the most (information gain), made safer
     (Mute and Volume before Power and Input) and more useful (buttons you don't have yet).

   Nothing is ever ruled out completely: people miss things and mislabel things.

   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "presets.h"
#include "remote.h"
#include <math.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

const PCode *PRESET_CODES = BUILTIN_CODES;
const PSet *PRESETS_DB = BUILTIN_DB;
int PRESETS_DB_N;

const char *FUNC_NAME[FN_COUNT] = {
	"POWER", "MUTE", "VOL+", "VOL-", "P+", "P-", "MENU", "OK", "UP", "DOWN", "LEFT", "RIGHT",
	"BACK", "INPUT", "INFO", "SLEEP", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "PICTURE", "PREV CH",
	"PWR ON", "PWR OFF", "AV1", "AV2", "AV3", "COMP", "HDMI1", "HDMI2", "TV",
};

#define MAX_EV 512
enum { EV_GUESS, EV_CLAIM, EV_ANSWER, EV_SKIP };
typedef struct { u8 proto, cmd, resp, kind; s8 fid; u16 addr; float w; } Ev;
static Ev ev[MAX_EV];
static int nev, dirty = 1, answers;
static float *lp;          /* log score per preset, plus one extra: "not in the library" */
static float *post;        /* normalised chances */
static int N;
static float *prior;       /* each kind of TV (protocol + address) gets a fair share, split between its variants */
#define CLAIM_W 0.5f

/* ---------- what each button does when it works ---------- */
int resp_for_fid(int f)
{
	switch (f) {
	case FN_POWER: case FN_PWRON: case FN_PWROFF: return R_POWER;
	case FN_AV1: case FN_AV2: case FN_AV3: case FN_COMP: case FN_HDMI1: case FN_HDMI2: case FN_TVTUNER: return R_INPUT;
	case FN_MUTE: return R_MUTE;
	case FN_VOLUP: case FN_VOLDN: return R_VOLUME;
	case FN_CHUP: case FN_CHDN: case FN_PREVCH: return R_CHANNEL;
	case FN_INPUT: return R_INPUT;
	case FN_MENU: case FN_PICTURE: return R_MENU;
	default: if (f >= FN_D0 && f <= FN_D9) return R_CHANNEL; return R_OTHER;
	}
}
/* keys that usually do nothing unless a TV menu is open */
static int needs_menu(int f) { return f == FN_OK || f == FN_UP || f == FN_DOWN || f == FN_LEFT || f == FN_RIGHT || f == FN_BACK; }

/* chance of each response when a button `f` is really sent (f = -1: a code the TV doesn't use) */
static float p_resp(int f, int r)
{
	if (f < 0) {   /* a code with no job on this TV: mostly nothing, now and then something odd */
		static const float U[R_COUNT] = { 0.72f, 0.03f, 0.03f, 0.03f, 0.03f, 0.03f, 0.03f, 0.12f };
		return U[r];
	}
	if (needs_menu(f)) {   /* no menu open = nothing happens; old sets use arrows for volume / channel */
		static const float M[R_COUNT] = { 0.58f, 0.01f, 0.07f, 0.01f, 0.07f, 0.02f, 0.10f, 0.14f };
		return M[r];
	}
	int t = resp_for_fid(f);
	if (t == R_OTHER) {    /* info, sleep: shows something on screen, looks like lots of things */
		static const float O[R_COUNT] = { 0.22f, 0.01f, 0.02f, 0.02f, 0.03f, 0.03f, 0.22f, 0.45f };
		return O[r];
	}
	if (r == t) return 0.80f;
	if (r == R_NONE) return 0.08f;     /* should have shown something: maybe missed it */
	if (r == R_OTHER) return 0.08f;    /* "something happened" fits any button */
	return 0.008f;                      /* you picked a different effect: maybe a slip */
}

/* ---------- per-preset lookup: command -> button ---------- */
static int preset_fid(int i, u8 cmd)
{
	const PSet *s = &PRESETS_DB[i];
	int best = -1;
	for (int k = 0; k < s->n; k++) if (PRESET_CODES[s->off + k].cmd == cmd) {
		int f = PRESET_CODES[s->off + k].fid;
		if (best < 0 || needs_menu(best)) best = f;   /* prefer the non-menu meaning */
	}
	return best;
}
int smart_find_fid(int i, u8 cmd) { return preset_fid(i, cmd); }
static int preset_cmd(int i, int f)
{
	const PSet *s = &PRESETS_DB[i];
	for (int k = 0; k < s->n; k++) if (PRESET_CODES[s->off + k].fid == f) return PRESET_CODES[s->off + k].cmd;
	return -1;
}
static int same_class(int a, int b) { return resp_for_fid(a) == resp_for_fid(b) && resp_for_fid(a) != R_OTHER; }

/* how likely is this piece of evidence if preset i is your TV (i == N: not in the library) */
static float likelihood(int i, const Ev *e)
{
	if (i == N) {
		/* not in the library: your TV has its own layout, somewhere we can't predict from a list */
		if (e->fid >= 0 && e->resp != R_NONE) return 0.05f;   /* a named button: any layout can have it */
		return p_resp(-1, e->resp) * 0.9f + 0.1f * (e->resp == R_NONE ? 0.3f : 0.2f);
	}
	const PSet *s = &PRESETS_DB[i];
	if (s->proto != e->proto || s->addr != e->addr)
		return e->resp == R_NONE ? 0.97f : 0.0005f;         /* another TV's code: it shouldn't react */
	int pf = preset_fid(i, e->cmd);
	if (e->fid >= 0 && e->resp != R_NONE) {
		/* you named the button: check it against what this preset calls that code */
		if (pf == e->fid) return 0.80f;
		if (pf >= 0 && same_class(pf, e->fid)) return 0.12f;   /* Volume up / down mixed up, digits off by one */
		if (pf >= 0) return 0.01f;
		/* this preset doesn't list the code: fine, unless it has that job on another code */
		return preset_cmd(i, e->fid) >= 0 ? 0.015f : 0.05f;   /* 0.05 = no better than "not in the library" */
	}
	if (pf >= 0) return p_resp(pf, e->resp);
	/* right TV, but a code this preset doesn't list: lists are never complete */
	return e->resp == R_NONE ? 0.75f : 0.12f;
}

static void recompute(void)
{
	if (!dirty) return;
	dirty = 0;
	float prior_none = logf(0.15f);
	for (int i = 0; i <= N; i++) {
		float v = i == N ? prior_none : prior[i];
		for (int k = 0; k < nev; k++) v += ev[k].w * logf(likelihood(i, &ev[k]));
		lp[i] = v;
	}
	float mx = lp[0]; for (int i = 1; i <= N; i++) if (lp[i] > mx) mx = lp[i];
	double sum = 0; for (int i = 0; i <= N; i++) { post[i] = expf(lp[i] - mx); sum += post[i]; }
	for (int i = 0; i <= N; i++) post[i] = (float)(post[i] / sum);
}

/* presets sharing one protocol + address = one kind of TV. gid[i] = its group, worked out once */
static int *gid, NG, *g_first;
static void build_groups(void)
{
	gid = malloc(N * sizeof *gid); g_first = malloc(N * sizeof *g_first); NG = 0;
	for (int i = 0; i < N; i++) {
		int k = -1;
		for (int j = 0; j < i && k < 0; j++) if (PRESETS_DB[j].proto == PRESETS_DB[i].proto && PRESETS_DB[j].addr == PRESETS_DB[i].addr) k = gid[j];
		if (k < 0) { k = NG++; g_first[k] = i; }
		gid[i] = k;
	}
}
/* ---------- what the library teaches about layouts ----------
   For every pair of buttons (a, b): how far apart their codes usually are.
   Volume down is 1 after Volume up on lots of remotes, 2 is 1 after 1, and so on. */
#define DSPAN 48
static u16 (*dist)[FN_COUNT][2 * DSPAN + 1];   /* dist[a][b][d + DSPAN] */
static u32 dist_tot[FN_COUNT][FN_COUNT];
static u16 (*popular)[256];                      /* popular[f][cmd]: codes a button usually has */
static u32 popular_tot[FN_COUNT];

static void learn_layouts(void)
{
	dist = calloc(FN_COUNT, sizeof *dist);
	popular = calloc(FN_COUNT, sizeof *popular);
	for (int i = 0; i < N; i++) {
		const PSet *s = &PRESETS_DB[i];
		int w = s->seen ? s->seen : 1; if (w > 8) w = 8;
		for (int a = 0; a < s->n; a++) {
			const PCode *A = &PRESET_CODES[s->off + a];
			if (popular[A->fid][A->cmd] < 60000) popular[A->fid][A->cmd] += w;
			popular_tot[A->fid] += w;
			for (int b = 0; b < s->n; b++) if (b != a) {
				const PCode *B = &PRESET_CODES[s->off + b];
				int d = (int)B->cmd - (int)A->cmd;
				dist_tot[A->fid][B->fid] += w;
				if (d < -DSPAN || d > DSPAN) continue;
				if (dist[A->fid][B->fid][d + DSPAN] < 60000) dist[A->fid][B->fid][d + DSPAN] += w;
			}
		}
	}
}

static void load_user_library(void);
static float *prior_keep;
static u8 want[FN_COUNT];
void smart_init(void)
{
	if (!lp) {
		load_user_library();
		N = PRESETS_DB_N;
		lp = malloc((N + 1) * sizeof *lp); post = malloc((N + 1) * sizeof *post); prior = malloc(N * sizeof *prior);
		/* group weight = (how many remotes use it)^0.75; variants share it by their own counts */
		float *gw = calloc(N, sizeof *gw), tot = 0;
		for (int i = 0; i < N; i++) {
			if (gw[i] > 0) continue;
			float sum = 0;
			for (int j = i; j < N; j++) if (PRESETS_DB[j].proto == PRESETS_DB[i].proto && PRESETS_DB[j].addr == PRESETS_DB[i].addr) sum += PRESETS_DB[j].seen;
			for (int j = i; j < N; j++) if (PRESETS_DB[j].proto == PRESETS_DB[i].proto && PRESETS_DB[j].addr == PRESETS_DB[i].addr) gw[j] = sum;
		}
		for (int i = 0; i < N; i++) { float share = PRESETS_DB[i].seen / gw[i]; prior[i] = powf(gw[i], 0.75f) * share; tot += prior[i]; }
		for (int i = 0; i < N; i++) prior[i] = logf(0.85f * prior[i] / tot);
		free(gw);
		learn_layouts();
		build_groups();
		prior_keep = malloc(N * sizeof *prior_keep); memcpy(prior_keep, prior, N * sizeof *prior);
	}
	nev = 0; answers = 0; dirty = 1;
}
void smart_reset(void) { if (!lp) smart_init(); nev = 0; answers = 0; dirty = 1; memset(want, 0, sizeof want); }
int smart_answers(void) { return answers; }
int smart_evidence(void) { return nev; }

int smart_has_group(u8 proto, u16 addr)
{
	if (!lp) smart_init();
	for (int k = 0; k < NG; k++) if (PRESETS_DB[g_first[k]].proto == proto && PRESETS_DB[g_first[k]].addr == addr) return 1;
	return 0;
}

static int find_ev(u8 proto, u16 addr, u8 cmd)
{
	for (int k = 0; k < nev; k++) if (ev[k].proto == proto && ev[k].addr == addr && ev[k].cmd == cmd) return k;
	return -1;
}
static void to_end(int k) { Ev t = ev[k]; memmove(&ev[k], &ev[k + 1], (nev - k - 1) * sizeof(Ev)); ev[nev - 1] = t; }
static int add_ev(void)
{
	if (nev == MAX_EV) {
		/* forget the oldest guess (never a real answer or a claim) */
		int k = 0; while (k < nev && ev[k].kind != EV_GUESS) k++;
		if (k == nev) k = 0;
		if (ev[k].kind == EV_ANSWER) answers--;
		memmove(&ev[k], &ev[k + 1], (nev - k - 1) * sizeof(Ev)); nev--;
	}
	return nev++;
}

/* fid: the button you said it was (-1 = not a named job) */
void smart_note_fid(u8 proto, u16 addr, u8 cmd, int resp, int fid, float w)
{
	if (!lp) smart_init();
	int kind = w >= 1 ? EV_ANSWER : EV_GUESS;
	int k = find_ev(proto, addr, cmd);
	if (k >= 0) {
		Ev *e = &ev[k];
		if (kind == EV_ANSWER) {
			if (e->kind != EV_ANSWER) answers++;
			e->resp = resp; e->fid = fid; e->w = 1; e->kind = EV_ANSWER;
		} else if (e->kind == EV_GUESS) {
			if (e->resp == resp) { e->w += w; if (e->w > 0.6f) e->w = 0.6f; }
			else if (e->resp == R_NONE && resp != R_NONE) { e->resp = resp; e->fid = fid; e->w = w; }   /* "it reacted" beats "probably nothing" */
		}
		dirty = 1;
		to_end(k);   /* so "undo" finds it */
		return;
	}
	k = add_ev();
	ev[k] = (Ev){ proto, cmd, (u8)resp, (u8)kind, (s8)fid, addr, w };
	if (kind == EV_ANSWER) answers++;
	dirty = 1;
}
void smart_note(u8 proto, u16 addr, u8 cmd, int resp, float w) { smart_note_fid(proto, addr, cmd, resp, -1, w); }

/* a button already on your remote: believed, but not blindly */
void smart_claim(u8 proto, u16 addr, u8 cmd, int resp, int fid)
{
	if (!lp) smart_init();
	int k = find_ev(proto, addr, cmd);
	if (k >= 0 && ev[k].kind == EV_ANSWER) return;   /* you already showed us */
	if (k < 0) k = add_ev();
	ev[k] = (Ev){ proto, cmd, (u8)resp, EV_CLAIM, (s8)fid, addr, CLAIM_W };
	dirty = 1;
}
/* how a code stands: 0 unknown, 1 guessed, 2 claimed by your remote, 3 you answered */
int smart_status(u8 proto, u16 addr, u8 cmd)
{
	int k = find_ev(proto, addr, cmd);
	return k < 0 ? 0 : ev[k].kind == EV_ANSWER ? 3 : (ev[k].kind == EV_CLAIM || ev[k].kind == EV_SKIP) ? 2 : 1;
}
/* "skip this code": don't ask about it again (it tells us nothing) */
void smart_skip(u8 proto, u16 addr, u8 cmd)
{
	if (!lp) smart_init();
	int k = find_ev(proto, addr, cmd);
	if (k >= 0) { if (ev[k].kind == EV_GUESS) { ev[k].kind = EV_SKIP; ev[k].w = 0; dirty = 1; } return; }
	k = add_ev();
	ev[k] = (Ev){ proto, cmd, R_NONE, EV_SKIP, -1, addr, 0 };
}

void smart_forget_last(void)
{
	for (int k = nev - 1; k >= 0; k--) if (ev[k].kind == EV_ANSWER) {
		memmove(&ev[k], &ev[k + 1], (nev - k - 1) * sizeof(Ev)); nev--; answers--; dirty = 1; return;
	}
}

int smart_top(int *idx, float *pct, int max)
{
	if (!lp) smart_init();
	recompute();
	int n = 0;
	for (int i = 0; i < N; i++) {
		/* insert into the sorted top list */
		int j;
		if (n < max) j = n++;
		else { if (post[i] <= post[idx[max - 1]]) continue; j = max - 1; }
		while (j > 0 && post[idx[j - 1]] < post[i]) { idx[j] = idx[j - 1]; j--; }
		idx[j] = i;
	}
	for (int k = 0; k < n; k++) pct[k] = post[idx[k]] * 100.0f;
	return n;
}
float smart_none_pct(void) { if (!lp) smart_init(); recompute(); return post[N] * 100.0f; }

static SGroup *gcache; static int gcache_n, gcache_ok;
int smart_groups(SGroup *g, int max)
{
	if (!lp) smart_init();
	if (dirty) gcache_ok = 0;
	recompute();
	if (!gcache_ok) {
		if (!gcache) gcache = malloc(NG * sizeof *gcache);
		for (int k = 0; k < NG; k++) { gcache[k].best = g_first[k]; gcache[k].pct = 0; gcache[k].count = 0; }
		for (int i = 0; i < N; i++) {
			SGroup *q = &gcache[gid[i]];
			q->pct += post[i] * 100.0f; q->count++;
			if (post[i] > post[q->best]) q->best = i;
		}
		/* most likely first */
		for (int a = 1; a < NG; a++) { SGroup t = gcache[a]; int b = a; while (b > 0 && gcache[b - 1].pct < t.pct) { gcache[b] = gcache[b - 1]; b--; } gcache[b] = t; }
		gcache_n = NG; gcache_ok = 1;
	}
	int n = gcache_n < max ? gcache_n : max;
	if (n > SMART_MAXG) n = SMART_MAXG;
	memcpy(g, gcache, n * sizeof *g);
	return n;
}

/* your TV's own protocol + address: where it has reacted most, as far as we know */
int smart_home(u8 *proto, u16 *addr)
{
	float best = 0; int bk = -1;
	for (int k = 0; k < nev; k++) {
		if (ev[k].resp == R_NONE || ev[k].kind == EV_GUESS) continue;
		float s = 0;
		for (int j = 0; j < nev; j++) if (ev[j].proto == ev[k].proto && ev[j].addr == ev[k].addr && ev[j].resp != R_NONE && ev[j].kind != EV_GUESS) s += ev[j].w;
		if (s > best) { best = s; bk = k; }
	}
	if (bk >= 0) { *proto = ev[bk].proto; *addr = ev[bk].addr; return 1; }
	SGroup g[SMART_MAXG];
	if (smart_groups(g, 1) && g[0].pct > 30) { *proto = PRESETS_DB[g[0].best].proto; *addr = PRESETS_DB[g[0].best].addr; return 1; }
	return 0;
}

/* your TV's address from codes you said worked (not from names, not from guesses) */
int smart_home_confirmed(u8 *proto, u16 *addr)
{
	float best = 0; int bk = -1;
	for (int k = 0; k < nev; k++) {
		if (ev[k].resp == R_NONE || ev[k].kind != EV_ANSWER) continue;
		float s = 0;
		for (int j = 0; j < nev; j++) if (ev[j].proto == ev[k].proto && ev[j].addr == ev[k].addr && ev[j].resp != R_NONE && ev[j].kind == EV_ANSWER) s += 1;
		if (s > best) { best = s; bk = k; }
	}
	if (bk < 0) return 0;
	*proto = ev[bk].proto; *addr = ev[bk].addr; return 1;
}
void smart_want(int fid, int on) { if (fid >= 0 && fid < FN_COUNT) want[fid] = (u8)(on != 0); }
int smart_wanted(int *fids, int max) { int n = 0; for (int f = 0; f < FN_COUNT && n < max; f++) if (want[f]) fids[n++] = f; return n; }

/* which buttons we already know on the home address (answered or claimed, and it reacted) */
static int known_at(u8 proto, u16 addr, int *cmd_of)
{
	int n = 0;
	for (int f = 0; f < FN_COUNT; f++) cmd_of[f] = -1;
	for (int k = 0; k < nev; k++) {
		const Ev *e = &ev[k];
		if (e->proto != proto || e->addr != addr || e->fid < 0 || e->resp == R_NONE || e->kind == EV_GUESS) continue;
		if (cmd_of[e->fid] < 0 || e->kind == EV_ANSWER) { if (cmd_of[e->fid] < 0) n++; cmd_of[e->fid] = e->cmd; }
	}
	return n;
}

/* not in the library: where is button f likely to be, from the buttons we know?
   fills p[256] (sums to 1). Returns 0 if there is nothing to go on. */
static int layout_guess(int f, const int *cmd_of, float *p)
{
	double lg[256];
	int any = 0;
	for (int x = 0; x < 256; x++) {
		/* how common this code is for this button across all remotes (weak) */
		lg[x] = 0.35 * log((popular[f][x] + 0.5) / (popular_tot[f] + 128.0));
	}
	for (int a = 0; a < FN_COUNT; a++) {
		if (cmd_of[a] < 0 || a == f || dist_tot[a][f] < 20) continue;
		any = 1;
		for (int x = 0; x < 256; x++) {
			int d = x - cmd_of[a];
			double c = (d >= -DSPAN && d <= DSPAN) ? dist[a][f][d + DSPAN] : 0;
			lg[x] += log((c + 0.3) / (dist_tot[a][f] + 0.3 * 256));
		}
	}
	if (!any) return 0;
	double mx = -1e30; for (int x = 0; x < 256; x++) if (lg[x] > mx) mx = lg[x];
	double sum = 0; for (int x = 0; x < 256; x++) { p[x] = (float)exp(lg[x] - mx); sum += p[x]; }
	for (int x = 0; x < 256; x++) p[x] = (float)(p[x] / sum);
	/* a code already known for something else is not this button */
	for (int a = 0; a < FN_COUNT; a++) if (cmd_of[a] >= 0) p[cmd_of[a]] = 0;
	return 1;
}

/* the best preset for this TV, with gaps filled from its look-alikes */
int smart_build(int best, u8 *cmd_for_fid)
{
	recompute();
	const PSet *b = &PRESETS_DB[best];
	for (int f = 0; f < FN_COUNT; f++) cmd_for_fid[f] = 0xFF;
	int order[64], no = 0;
	order[no++] = best;
	for (int i = 0; i < N && no < 64; i++) if (i != best && PRESETS_DB[i].proto == b->proto && PRESETS_DB[i].addr == b->addr) order[no++] = i;
	for (int a = 1; a < no; a++) { int t = order[a], c = a; while (c > 1 && post[order[c - 1]] < post[t]) { order[c] = order[c - 1]; c--; } order[c] = t; }
	/* what you showed us wins over any list */
	int shown[FN_COUNT]; known_at(b->proto, b->addr, shown);
	int got = 0;
	for (int f = 0; f < FN_COUNT; f++) {
		int k = shown[f] >= 0 ? find_ev(b->proto, b->addr, (u8)shown[f]) : -1;
		if (k >= 0 && ev[k].kind == EV_ANSWER) { cmd_for_fid[f] = (u8)shown[f]; got++; }
	}
	for (int a = 0; a < no; a++) {
		if (a > 0 && post[order[a]] < 0.15f * post[best]) break;   /* only look-alikes that still fit */
		const PSet *s = &PRESETS_DB[order[a]];
		for (int k = 0; k < s->n; k++) {
			const PCode *c = &PRESET_CODES[s->off + k];
			if (cmd_for_fid[c->fid] != 0xFF) continue;
			/* never a code you said did nothing (menu keys excepted), nor one you showed does another job */
			int bad = 0;
			int e = find_ev(s->proto, s->addr, c->cmd);
			if (e >= 0 && ev[e].kind == EV_ANSWER) {
				if (ev[e].resp == R_NONE && !needs_menu(c->fid)) bad = 1;
				if (ev[e].fid >= 0 && ev[e].fid != c->fid && ev[e].resp != R_NONE) bad = 1;
			}
			for (int f = 0; f < FN_COUNT && !bad; f++) if (cmd_for_fid[f] == c->cmd) bad = 1;
			if (bad && (a > 0 || (e >= 0 && ev[e].fid >= 0))) continue;
			cmd_for_fid[c->fid] = c->cmd; got++;
		}
	}
	/* pairs: you showed Volume up is on code X but this list has it elsewhere, so its Volume down
	   is suspect too. Use the layout guess (usually right next to X) when it's a clear favourite */
	static const int PAIR[][2] = { { FN_VOLUP, FN_VOLDN }, { FN_CHUP, FN_CHDN } };
	for (int q = 0; q < 2; q++) for (int side = 0; side < 2; side++) {
		int f = PAIR[q][side], g = PAIR[q][!side];
		int k = shown[f] >= 0 ? find_ev(b->proto, b->addr, (u8)shown[f]) : -1;
		if (k < 0 || ev[k].kind != EV_ANSWER) continue;
		int kg = shown[g] >= 0 ? find_ev(b->proto, b->addr, (u8)shown[g]) : -1;
		if (kg >= 0 && ev[kg].kind == EV_ANSWER) continue;       /* you showed both */
		if (preset_cmd(best, f) == shown[f]) continue;            /* the list agrees with you */
		static float p[256];
		if (!layout_guess(g, shown, p)) continue;
		int bx = 0; for (int x = 1; x < 256; x++) if (p[x] > p[bx]) bx = x;
		int taken = 0; for (int h = 0; h < FN_COUNT; h++) if (h != g && cmd_for_fid[h] == bx) taken = 1;
		if (p[bx] > 0.2f && !taken) { if (cmd_for_fid[g] == 0xFF) got++; cmd_for_fid[g] = (u8)bx; }
		else if (cmd_for_fid[g] != 0xFF) { cmd_for_fid[g] = 0xFF; got--; }   /* not clear: better none than a wrong one */
	}
	return got;
}

/* how nice a button is to test while you're watching the Wii on that TV */
static float safety(int f)
{
	switch (f) {
	case FN_MUTE: case FN_VOLUP: case FN_VOLDN: return 1.0f;   /* shows over the Wii picture */
	case FN_MENU: return 0.8f;
	case FN_INFO: case FN_PICTURE: case FN_SLEEP: return 0.6f;
	case FN_CHUP: case FN_CHDN: case FN_PREVCH: return 0.25f;  /* leaves the Wii's AV input */
	case FN_INPUT: case FN_AV1: case FN_AV2: case FN_AV3: case FN_COMP: case FN_HDMI1: case FN_HDMI2: case FN_TVTUNER: return 0.2f;
	case FN_POWER: case FN_PWROFF: return 0.06f;                /* turns the TV off */
	case FN_PWRON: return 0.3f;                                 /* does nothing to a TV that's on: safe, but you can't see it */
	case -1: return 0.35f;                                      /* an unknown code: could be anything */
	default: return f >= FN_D0 && f <= FN_D9 ? 0.2f : 0.05f;   /* menu keys: useless without a menu */
	}
}
/* how much a button is worth having on the remote */
static float worth(int f)
{
	switch (f) {
	case FN_POWER: case FN_MUTE: case FN_VOLUP: case FN_VOLDN: case FN_INPUT: return 1.0f;
	case FN_PWRON: case FN_PWROFF: case FN_AV1: case FN_AV2: case FN_AV3: case FN_COMP: case FN_HDMI1: case FN_HDMI2: case FN_TVTUNER: return 0.3f;   /* nice for macros, after the everyday ones */
	case FN_CHUP: case FN_CHDN: case FN_MENU: return 0.8f;
	case FN_OK: case FN_UP: case FN_DOWN: case FN_LEFT: case FN_RIGHT: case FN_BACK: return 0.6f;
	default: return 0.4f;
	}
}

static float ent(const float *p) { float h = 0; for (int r = 0; r < R_COUNT; r++) if (p[r] > 1e-6f) h -= p[r] * logf(p[r]); return h; }

/* the most useful code to try next.
   *fid = the button it probably is (-1 = unknown), *conf = how sure (0..1) */
int smart_suggest2(u8 *proto, u16 *addr, u8 *cmd, int *fid, float *conf, int use_library)
{
	if (!lp) smart_init();
	recompute();
	enum { TOP = 40, MAXC = 1600 };
	int idx[TOP]; float pct[TOP];
	int nt = smart_top(idx, pct, TOP);
	float pi[TOP + 1], mass = 0;
	for (int a = 0; a < nt; a++) { pi[a] = use_library ? pct[a] / 100 : 0; mass += pi[a]; }
	pi[nt] = use_library ? post[N] : 1; mass += pi[nt];
	for (int a = 0; a <= nt; a++) pi[a] /= mass;

	/* layout guesses for your TV's own address (used by "not in the library") */
	u8 hp = P_NEC; u16 ha = 0; int home = smart_home(&hp, &ha);
	int cmd_of[FN_COUNT]; int nk = home ? known_at(hp, ha, cmd_of) : 0;
	static float guess[FN_COUNT][256]; static int has_guess[FN_COUNT];
	for (int f = 0; f < FN_COUNT; f++) has_guess[f] = home && nk && cmd_of[f] < 0 && layout_guess(f, cmd_of, guess[f]);

	/* candidates: codes the top presets use, plus every code on your TV's address */
	static struct { u8 p, c; u16 a; } cand[MAXC];
	int nc = 0;
	for (int a = 0; a < nt && use_library; a++) {
		const PSet *s = &PRESETS_DB[idx[a]];
		for (int k = 0; k < s->n && nc < MAXC; k++) {
			u8 c = PRESET_CODES[s->off + k].cmd; int dup = 0;
			for (int j = 0; j < nc && !dup; j++) dup = cand[j].p == s->proto && cand[j].a == s->addr && cand[j].c == c;
			if (!dup) { cand[nc].p = s->proto; cand[nc].a = s->addr; cand[nc].c = c; nc++; }
		}
	}
	int home_from = nc;
	if (home) {
		int lim = (hp == P_RC5 || hp == P_SONY || hp == P_SONY15) ? 128 : 256;
		for (int c = 0; c < lim && nc < MAXC; c++) {
			int dup = 0;
			for (int j = 0; j < home_from && !dup; j++) dup = cand[j].p == hp && cand[j].a == ha && cand[j].c == c;
			if (!dup) { cand[nc].p = hp; cand[nc].a = ha; cand[nc].c = (u8)c; nc++; }
		}
	}

	/* once a code has worked, stay on that TV's address: other TVs' codes would do nothing */
	u8 cp = 0; u16 ca = 0; int confirmed = smart_home_confirmed(&cp, &ca);
	float best = -1;
	for (int j = 0; j < nc; j++) {
		u8 P = cand[j].p, C = cand[j].c; u16 A = cand[j].a;
		if (smart_status(P, A, C) >= 2) continue;          /* on your remote already, or answered */
		if (confirmed && (P != cp || A != ca)) continue;
		/* what each belief says will happen, and which button it is */
		float mix[R_COUNT] = { 0 }, hsum = 0, fp[FN_COUNT + 1] = { 0 };   /* fp[FN_COUNT] = no job */
		for (int a = 0; a <= nt; a++) {
			if (pi[a] < 1e-5f) continue;
			float pr[R_COUNT];
			if (a < nt) {
				const PSet *s = &PRESETS_DB[idx[a]];
				int f = (s->proto == P && s->addr == A) ? preset_fid(idx[a], C) : -2;
				for (int r = 0; r < R_COUNT; r++) pr[r] = f == -2 ? (r == R_NONE ? 0.97f : 0.03f / (R_COUNT - 1)) : p_resp(f, r);
				fp[f >= 0 ? f : FN_COUNT] += pi[a];
			} else {
				/* not in the library: the layout guesses on your address, else "probably nothing" */
				float pj = 0, pf[FN_COUNT] = { 0 };
				if (home && P == hp && A == ha)
					for (int f = 0; f < FN_COUNT; f++) if (has_guess[f]) { pf[f] = guess[f][C] * 0.7f; pj += pf[f]; }
				if (pj > 0.9f) { for (int f = 0; f < FN_COUNT; f++) pf[f] *= 0.9f / pj; pj = 0.9f; }
				for (int r = 0; r < R_COUNT; r++) {
					float v = (1 - pj) * p_resp(-1, r);
					for (int f = 0; f < FN_COUNT; f++) if (pf[f] > 0) v += pf[f] * p_resp(f, r);
					pr[r] = v;
				}
				for (int f = 0; f < FN_COUNT; f++) fp[f] += pi[a] * pf[f];
				fp[FN_COUNT] += pi[a] * (1 - pj);
			}
			for (int r = 0; r < R_COUNT; r++) mix[r] += pi[a] * pr[r];
			hsum += pi[a] * ent(pr);
		}
		float gain = ent(mix) - hsum; if (gain < 0) gain = 0;
		/* a button you don't have yet is worth finding even when we're already sure of the TV */
		float newv = 0, saf = 0, tot = 0; int fbest = -1; float fb = 0;
		for (int f = 0; f < FN_COUNT; f++) {
			if (fp[f] <= 0) continue;
			int have = home && P == hp && A == ha && cmd_of[f] >= 0;
			if (!have) newv += fp[f] * worth(f);
			saf += fp[f] * safety(f); tot += fp[f];
			if (fp[f] > fb) { fb = fp[f]; fbest = f; }
		}
		saf += fp[FN_COUNT] * safety(-1); tot += fp[FN_COUNT];
		if (tot > 0) saf /= tot;
		/* a button of yours turned out wrong: its right code comes first */
		float wantv = 0; for (int f = 0; f < FN_COUNT; f++) if (want[f]) wantv += fp[f];
		if (wantv > fb * 0.5f) for (int f = 0; f < FN_COUNT; f++) if (want[f] && fp[f] >= fb * 0.5f) fbest = f;
		float score = (gain + 0.35f * newv + 2.0f * wantv + 0.01f) * (wantv > 0.05f ? 1.0f : saf);
		if (score > best) { best = score; *proto = P; *addr = A; *cmd = C; *fid = fbest; *conf = tot > 0 ? fb / tot : 0; }
	}
	return best >= 0;
}
int smart_suggest(u8 *proto, u16 *addr, u8 *cmd, int *fid)
{
	float c; int r = smart_suggest2(proto, addr, cmd, fid, &c, 1);
	if (r && c < 0.35f) *fid = -1;
	return r;
}

/* button names people type -> function */
int fid_from_name(const char *name)
{
	char a[24]; int n = 0;
	for (const char *p = name; *p && n < 23; p++) {
		char c = *p; if (c >= 'a' && c <= 'z') c -= 32;
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '-') a[n++] = c;
	}
	a[n] = 0;
	static const struct { const char *n; int f; } T[] = {
		{ "POWER", FN_POWER }, { "PWR", FN_POWER }, { "STANDBY", FN_POWER }, { "ONOFF", FN_POWER }, { "POWERTOGGLE", FN_POWER }, { "TVPOWER", FN_POWER },
		{ "MUTE", FN_MUTE }, { "MUTING", FN_MUTE }, { "SOUNDOFF", FN_MUTE },
		{ "MENU", FN_MENU }, { "SETUP", FN_MENU }, { "SETTINGS", FN_MENU }, { "OK", FN_OK }, { "ENTER", FN_OK }, { "SELECT", FN_OK },
		{ "UP", FN_UP }, { "DOWN", FN_DOWN }, { "LEFT", FN_LEFT }, { "RIGHT", FN_RIGHT }, { "CURSORUP", FN_UP }, { "CURSORDOWN", FN_DOWN }, { "CURSORLEFT", FN_LEFT }, { "CURSORRIGHT", FN_RIGHT },
		{ "BACK", FN_BACK }, { "EXIT", FN_BACK }, { "RETURN", FN_BACK }, { "INPUT", FN_INPUT }, { "SOURCE", FN_INPUT }, { "AV", FN_INPUT }, { "TVAV", FN_INPUT }, { "AVTV", FN_INPUT }, { "TVVIDEO", FN_INPUT },
		{ "INFO", FN_INFO }, { "DISPLAY", FN_INFO }, { "RECALL", FN_INFO }, { "OSD", FN_INFO }, { "SLEEP", FN_SLEEP }, { "SLEEPTIMER", FN_SLEEP },
		{ "PICTURE", FN_PICTURE }, { "PMODE", FN_PICTURE }, { "PICTUREMODE", FN_PICTURE }, { "PICMODE", FN_PICTURE },
		{ "PREVCH", FN_PREVCH }, { "LASTCH", FN_PREVCH }, { "PRECH", FN_PREVCH }, { "QVIEW", FN_PREVCH }, { "FLASHBACK", FN_PREVCH }, { "LASTCHANNEL", FN_PREVCH },
		{ "PWRON", FN_PWRON }, { "POWERON", FN_PWRON }, { "ON", FN_PWRON }, { "PWROFF", FN_PWROFF }, { "POWEROFF", FN_PWROFF }, { "OFF", FN_PWROFF },
		{ "AV1", FN_AV1 }, { "AV2", FN_AV2 }, { "AV3", FN_AV3 }, { "VIDEO1", FN_AV1 }, { "VIDEO2", FN_AV2 }, { "VIDEO3", FN_AV3 }, { "EXT1", FN_AV1 }, { "EXT2", FN_AV2 }, { "EXT3", FN_AV3 },
		{ "SCART1", FN_AV1 }, { "SCART2", FN_AV2 }, { "COMP", FN_COMP }, { "COMPONENT", FN_COMP }, { "YPBPR", FN_COMP }, { "HDMI1", FN_HDMI1 }, { "HDMI2", FN_HDMI2 },
		{ "TV", FN_TVTUNER }, { "TUNER", FN_TVTUNER }, { "ANTENNA", FN_TVTUNER }, { "AERIAL", FN_TVTUNER },
	};
	for (size_t k = 0; k < sizeof T / sizeof T[0]; k++) if (!strcmp(a, T[k].n)) return T[k].f;
	if (n == 1 && a[0] >= '0' && a[0] <= '9') return FN_D0 + a[0] - '0';
	if (n == 2 && a[0] == '0' && a[1] >= '0' && a[1] <= '9') return FN_D0 + a[1] - '0';   /* "07" */
	/* Volume / Channel with any spelling: VOL+, VOLUP, VOLUME UP, VOL PLUS, CH+, CHANNEL UP, PROG+, P+ ... */
	static const char *VOL[] = { "VOLUME", "VOL", "TVVOL" };
	static const char *CH[] = { "CHANNEL", "CHAN", "CH", "PROGRAM", "PROG", "PR", "P", "TVCH" };
	for (int t = 0; t < 2; t++) {
		const char **L = t ? CH : VOL; int nl = t ? 8 : 3;
		for (int k = 0; k < nl; k++) {
			size_t m = strlen(L[k]);
			if (strncmp(a, L[k], m)) continue;
			const char *r = a + m;
			if (!strcmp(r, "+") || !strcmp(r, "UP") || !strcmp(r, "PLUS") || !strcmp(r, "NEXT") || !strcmp(r, "++")) return t ? FN_CHUP : FN_VOLUP;
			if (!strcmp(r, "-") || !strcmp(r, "DOWN") || !strcmp(r, "DN") || !strcmp(r, "MINUS") || !strcmp(r, "PREV") || !strcmp(r, "--")) return t ? FN_CHDN : FN_VOLDN;
		}
	}
	if (!strncmp(a, "POWER", 5) && (!strcmp(a + 5, "ONOFF") || !strcmp(a + 5, "BUTTON"))) return FN_POWER;
	return -1;
}

/* what a saved button should do on the TV, from its name (even when it isn't one of our jobs) */
int resp_from_name(const char *name)
{
	int f = fid_from_name(name);
	if (f >= 0) return resp_for_fid(f);
	char a[24]; int n = 0;
	for (const char *p = name; *p && n < 23; p++) { char c = *p; if (c >= 'a' && c <= 'z') c -= 32; if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) a[n++] = c; }
	a[n] = 0;
	if (strstr(a, "POWER") || strstr(a, "STANDBY")) return R_POWER;
	if (strstr(a, "HDMI") || strstr(a, "COMPONENT") || strstr(a, "SCART") || strstr(a, "RGB") || strstr(a, "YPBPR") || strstr(a, "INPUT") || !strncmp(a, "AV", 2) || strstr(a, "VIDEO")) return R_INPUT;
	if (strstr(a, "MUTE")) return R_MUTE;
	if (strstr(a, "VOL")) return R_VOLUME;
	if (strstr(a, "MENU")) return R_MENU;
	return R_OTHER;
}

/* what you showed a code does: the button (>= 0), -1 = it reacted but we don't know as what,
   -2 = it did nothing, -3 = not answered yet */
int smart_answer_fid(u8 proto, u16 addr, u8 cmd)
{
	int k = find_ev(proto, addr, cmd);
	if (k < 0 || ev[k].kind != EV_ANSWER) return -3;
	return ev[k].resp == R_NONE ? -2 : ev[k].fid;
}

#ifdef HOST
/* tests only: pretend a whole kind of TV is missing from the library */
void smart_test_exclude(u8 proto, u16 addr, int on)
{
	if (!lp) smart_init();
	for (int i = 0; i < N; i++) prior[i] = (on && PRESETS_DB[i].proto == proto && PRESETS_DB[i].addr == addr) ? -1e30f : prior_keep[i];
	dirty = 1;
}
#endif

/* ================================================================== */
/* Your own library: every remote you've made, and shared files        */
/* ================================================================== */
/* Put Flipper .ir, LIRC .conf or exported WiiRremote .txt files in wiirremote/library:
   Smart match learns them without making them into remotes. Your saved remotes count too,
   so a TV you've mapped helps with the next one (same make, same chassis). */
#define MAX_USER_SETS 240
static PSet *u_sets; static PCode *u_codes; static char **u_src; static int nuser;
int smart_library_user(void) { return nuser; }

static int add_user_remote(const Remote *r, const char *src, int yours, PSet *S, PCode *C, int *nc, int maxc)
{
	int added = 0;
	/* one set per protocol + address on the remote */
	for (int b = 0; b < r->nbtn && nuser + added < MAX_USER_SETS; b++) {
		const RBtn *x = &r->btn[b];
		if (x->nsteps || x->dead) continue;
		int first = 1;
		for (int q = 0; q < b; q++) if (!r->btn[q].nsteps && !r->btn[q].dead && r->btn[q].proto == x->proto && r->btn[q].addr == x->addr) first = 0;
		if (!first) continue;
		int cmd_of[FN_COUNT]; for (int f = 0; f < FN_COUNT; f++) cmd_of[f] = -1;
		int nf = 0, vis = 0;
		for (int q = b; q < r->nbtn; q++) {
			const RBtn *y = &r->btn[q];
			if (y->nsteps || y->dead || y->proto != x->proto || y->addr != x->addr) continue;
			int f = fid_from_name(y->name);
			if (f < 0 || cmd_of[f] >= 0) continue;
			cmd_of[f] = y->cmd; nf++;
			if (resp_for_fid(f) != R_OTHER || f == FN_MENU) vis = 1;
		}
		if (nf < 2 || !vis || *nc + nf > maxc) continue;
		PSet *s = &S[nuser + added];
		char lab[64]; snprintf(lab, sizeof lab, yours ? "%s (yours)" : "%s (library)", r->name);
		char *l = malloc(strlen(lab) + 1); strcpy(l, lab);
		s->brands = l; s->proto = x->proto; s->addr = x->addr; s->n = (u8)nf; s->off = (u16)(BUILTIN_CODES_N + *nc); s->seen = yours ? 3 : 4;
		for (int f = 0; f < FN_COUNT; f++) if (cmd_of[f] >= 0) { C[*nc].fid = (u8)f; C[*nc].cmd = (u8)cmd_of[f]; (*nc)++; }
		u_src[nuser + added] = NULL;
		if (yours && src) { u_src[nuser + added] = malloc(strlen(src) + 1); strcpy(u_src[nuser + added], src); }
		added++;
	}
	return added;
}

static void load_user_library(void)
{
	PRESETS_DB = BUILTIN_DB; PRESET_CODES = BUILTIN_CODES; PRESETS_DB_N = BUILTIN_N; nuser = 0;
	int maxc = 65535 - BUILTIN_CODES_N; if (maxc > 6000) maxc = 6000;
	PSet *S = malloc(MAX_USER_SETS * sizeof *S); PCode *C = malloc(maxc * sizeof *C);
	u_src = calloc(MAX_USER_SETS, sizeof *u_src);
	if (!S || !C || !u_src) { free(S); free(C); return; }
	int nc = 0;
	static Remote tmp;
	/* 1) the library folder (shared by every room profile) */
	char dir[128]; snprintf(dir, sizeof dir, "%s/library", base_dir);
	mkdir(dir, 0777);
	{
		char rp[160]; snprintf(rp, sizeof rp, "%s/READ_ME.txt", dir);
		FILE *t = fopen(rp, "r");
		if (t) fclose(t);
		else if ((t = fopen(rp, "w"))) {
			fputs("Teach Smart match more TVs: put remote files here.\n"
			      "Flipper Zero .ir, LIRC .conf, or a WiiRremote remote .txt (Remote > More > Export).\n"
			      "They don't become remotes: Smart match just learns them.\n", t);
			fclose(t);
		}
	}
	DIR *d = opendir(dir);
	if (d) {
		struct dirent *e;
		while ((e = readdir(d)) && nuser < MAX_USER_SETS) {
			int l = strlen(e->d_name);
			if (e->d_name[0] == '.' || !strncasecmp(e->d_name, "READ", 4)) continue;
			if (!(l > 3 && !strcasecmp(e->d_name + l - 3, ".ir")) && !(l > 5 && !strcasecmp(e->d_name + l - 5, ".conf"))
			    && !(l > 5 && !strcasecmp(e->d_name + l - 5, ".lirc")) && !(l > 4 && !strcasecmp(e->d_name + l - 4, ".txt"))) continue;
			char p[256]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
			int skipped;
			if (ir_file_to_remote(p, &tmp, &skipped) > 0) nuser += add_user_remote(&tmp, NULL, 0, S, C, &nc, maxc);
		}
		closedir(d);
	}
	/* 2) your own remotes, in this room profile */
	static char files[MAX_REMOTES][40], names[MAX_REMOTES][NAME_LEN];
	int nr = remotes_list(files, names, MAX_REMOTES);
	for (int i = 0; i < nr && nuser < MAX_USER_SETS; i++)
		if (remote_load(&tmp, files[i])) nuser += add_user_remote(&tmp, files[i], 1, S, C, &nc, maxc);
	if (!nuser) { free(S); free(C); return; }
	/* one list: built-in first, then yours */
	PSet *all = malloc((BUILTIN_N + nuser) * sizeof *all);
	PCode *allc = malloc((BUILTIN_CODES_N + nc) * sizeof *allc);
	if (!all || !allc) { free(all); free(allc); free(S); free(C); nuser = 0; return; }
	memcpy(all, BUILTIN_DB, BUILTIN_N * sizeof *all); memcpy(all + BUILTIN_N, S, nuser * sizeof *all);
	memcpy(allc, BUILTIN_CODES, BUILTIN_CODES_N * sizeof *allc); memcpy(allc + BUILTIN_CODES_N, C, nc * sizeof *allc);
	free(S); free(C);
	u_sets = all; u_codes = allc;
	PRESETS_DB = all; PRESET_CODES = allc; PRESETS_DB_N = BUILTIN_N + nuser;
}

/* read it all again (a remote was saved, or files were added): the next smart_init rebuilds everything */
void smart_library_reload(void)
{
	if (!lp) return;
	for (int i = 0; i < nuser; i++) { free((void *)u_sets[BUILTIN_N + i].brands); free(u_src[i]); }
	free(u_src); u_src = NULL;
	if (u_sets) { free(u_sets); free(u_codes); u_sets = NULL; u_codes = NULL; }
	free(lp); free(post); free(prior); free(prior_keep); free(dist); free(popular); free(gid); free(g_first); free(gcache);
	lp = post = prior = prior_keep = NULL; dist = NULL; popular = NULL; gid = g_first = NULL; gcache = NULL; gcache_ok = 0;
	memset(dist_tot, 0, sizeof dist_tot); memset(popular_tot, 0, sizeof popular_tot);
	nuser = 0;
	smart_init();
}

/* matching a remote against its own copy in the library would be cheating */
void smart_library_exclude(const char *file)
{
	if (!lp) smart_init();
	for (int i = 0; i < N; i++) {
		int u = i - BUILTIN_N;
		int mine = u >= 0 && u < nuser && file && file[0] && u_src[u] && !strcmp(u_src[u], file);
		prior[i] = mine ? -1e30f : prior_keep[i];
	}
	dirty = 1; gcache_ok = 0;
}

/* ---- pause and come back: everything Smart match knows, as text lines ---- */
void smart_write(FILE *f)
{
	for (int k = 0; k < nev; k++)
		fprintf(f, "ev=%d,%d,%d,%d,%d,%d,%.3f\n", ev[k].kind, ev[k].proto, ev[k].addr, ev[k].cmd, ev[k].resp, ev[k].fid, ev[k].w);
}
int smart_read_line(const char *line)
{
	int kind, p, a, c, r, fid; float w;
	if (strncmp(line, "ev=", 3) || sscanf(line + 3, "%d,%d,%d,%d,%d,%d,%f", &kind, &p, &a, &c, &r, &fid, &w) != 7) return 0;
	if (kind < EV_GUESS || kind > EV_SKIP || p < 0 || p >= P_COUNT || r < 0 || r >= R_COUNT || fid < -1 || fid >= FN_COUNT || c < 0 || c > 255) return 0;
	if (!lp) smart_init();
	int k = find_ev((u8)p, (u16)a, (u8)c);
	if (k >= 0) { if (ev[k].kind == EV_ANSWER) answers--; }
	else k = add_ev();
	ev[k] = (Ev){ (u8)p, (u8)c, (u8)r, (u8)kind, (s8)fid, (u16)a, w };
	if (kind == EV_ANSWER) answers++;
	dirty = 1;
	return 1;
}
