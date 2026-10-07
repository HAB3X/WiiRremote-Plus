/* diagnosis test: every fault, a person who slips now and then, sometimes 2 faults at once */
#include "calkb.h"
#include <stdio.h>
#include <string.h>
static unsigned rs = 5; static float fr(void){ rs = rs*1103515245u+12345u; return ((rs>>8)&0xFFFFFF)/16777216.0f; }
/* what a person with these faults answers (the first fault that lists this question decides) */
static int pick(int q, const int *faults, int nf)
{
	if (q >= CAL_NQ) { int is = 0; for (int k = 0; k < nf; k++) is |= faults[k] == (q - CAL_NQ) % CAL_NFAULT; return fr() < (is ? 0.88f : 0.07f) ? 0 : 1; }
	int n = cal_q_nopt(q); float d[8] = { 0 }; int used = 0;
	for (int k = 0; k < nf && !used; k++) for (int j = 0; j < CAL_Q[q].nqp; j++) if (CAL_Q[q].qp[j].f == faults[k]) used = 1;
	/* same rule the engine uses, done here by hand so the test doesn't trust the engine */
	float listed = 0, rest = 0; int has[8] = { 0 };
	for (int k = 0; k < nf; k++) { int any = 0;
		for (int j = 0; j < CAL_Q[q].nqp; j++) if (CAL_Q[q].qp[j].f == faults[k]) any = 1;
		if (!any) continue;
		for (int j = 0; j < CAL_Q[q].nqp; j++) if (CAL_Q[q].qp[j].f == faults[k]) { d[CAL_Q[q].qp[j].o] += CAL_Q[q].qp[j].p / 100.0f; has[CAL_Q[q].qp[j].o] = 1; }
		break; }
	for (int o = 0; o < n; o++) { if (has[o]) listed += d[o]; else rest += CAL_Q[q].base[o]; }
	for (int o = 0; o < n; o++) if (!has[o]) d[o] = rest > 0 ? (1 - listed) * CAL_Q[q].base[o] / rest : 0;
	if (CAL_Q[q].gate && !used) { d[0] = 0.05f; d[1] = 0.95f; }
	float r = fr(), c = 0; for (int o = 0; o < n; o++) { c += d[o]; if (r < c) return o; } return n - 1;
}
static int run(int area, const int *faults, int nf, float slip, int unsure, int *nq, int verbose)
{
	cal_dx_start(area);
	int q, n = 0;
	while ((q = cal_dx_next()) >= 0) {
		int a = pick(q, faults, nf);
		if (fr() < slip) a = (int)(fr() * cal_q_nopt(q));
		if (fr() < unsure) a = ANS_UNSURE;
		if (verbose) { char b[200]; printf("   Q: %s -> %s\n", cal_q_text(q, b, 200), a == ANS_UNSURE ? "not sure" : cal_q_opt(q, a)); }
		cal_dx_answer(q, a); n++;
	}
	int f[3]; float p[3]; int k = cal_dx_top(f, p, 3);
	*nq = n;
	if (verbose) for (int i = 0; i < k; i++) printf("   -> %.0f%% %s\n", p[i] * 100, CAL_FAULT[f[i]].name);
	for (int i = 0; i < nf; i++) if (k && f[0] == faults[i]) return 1;
	return 0;
}
static int override_q = -1, override_a = -1;
static int base_case(int trials, float slip)
{
	/* Marcus's TV: grey tracking (green darks, pink lights) on component, and the picture too big */
	int ok = 0; long qs = 0;
	for (int t = 0; t < trials; t++) {
		int faults[2] = { 12 /* F_TRACK_GP */, -1 }, nf = 2, found = 0;
		for (int i = 0; i < CAL_NFAULT; i++) if (!strcmp(CAL_FAULT[i].name, "Top / bottom cut off")) faults[1] = i;
		for (int i = 0; i < CAL_NFAULT; i++) if (!strcmp(CAL_FAULT[i].name, "Green darks, pink lights")) faults[0] = i;
		int left[2] = { faults[0], faults[1] }, nl = 2, rounds = 0;
		int excluded[8], ne = 0;
		while (nl && rounds < 4) {
			cal_dx_start(AR_ALL);
			for (int e = 0; e < ne; e++) cal_dx_exclude(excluded[e]);
			int q, n = 0;
			while ((q = cal_dx_next()) >= 0) {
				int a = pick(q, left, nl);
				char b[200]; const char *tx = cal_q_text(q, b, 200);
				if (strstr(tx, "on every input")) a = 1;              /* "Only component" */
				if (strstr(tx, "leave the grey tint as it is")) a = 0; /* the normal menu doesn't change it */
				if (fr() < slip) a = (int)(fr() * cal_q_nopt(q));
				cal_dx_answer(q, a); n++;
			}
			qs += n;
			int f[1]; float p[1]; if (!cal_dx_top(f, p, 1)) break;
			int hit = -1; for (int k = 0; k < nl; k++) if (left[k] == f[0]) hit = k;
			if (trials == 1) printf("  round %d: %d questions -> %s (%s)\n", rounds + 1, n, CAL_FAULT[f[0]].name, hit >= 0 ? "right" : "wrong");
			excluded[ne++] = f[0];
			if (hit >= 0) { left[hit] = left[--nl]; found++; }
			rounds++;
		}
		if (found == 2) ok++;
	}
	printf("base case (your TV): both problems found in %d%% of runs, %.1f questions in total\n", 100 * ok / trials, (float)qs / trials);
	return 0;
}
int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "base")) { base_case(1, 0.06f); return base_case(200, 0.06f); }
	if (argc > 4) { int f = atoi(argv[4]); for (int t = 0; t < 3; t++) { int nq, fl[1] = { f }; printf("== %s (%s)\n", CAL_FAULT[f].name, run(CAL_FAULT[f].area, fl, 1, 0.06f, 0.05f, &nq, 1) ? "right" : "WRONG"); } return 0; }
	int trials = argc > 1 ? atoi(argv[1]) : 40; float slip = argc > 2 ? atof(argv[2]) : 0.06f;
	int ok_area = 0, ok_all = 0, tot = 0, qa = 0, qall = 0, worst = 100, wf = -1;
	for (int f = 0; f < CAL_NFAULT; f++) {
		int oka = 0, okl = 0;
		for (int t = 0; t < trials; t++) {
			int nq, fl[1] = { f };
			oka += run(CAL_FAULT[f].area, fl, 1, slip, 0.05f, &nq, 0); qa += nq;
			okl += run(AR_ALL, fl, 1, slip, 0.05f, &nq, 0); qall += nq;
		}
		ok_area += oka; ok_all += okl; tot += trials;
		int pc = 100 * oka / trials; if (pc < worst) { worst = pc; wf = f; }
		if (100 * oka / trials < 80 || argc > 3) printf("  %-48s area %3d%%  all %3d%%\n", CAL_FAULT[f].name, 100 * oka / trials, 100 * okl / trials);
	}
	printf("single fault: right %d%% (area), %d%% (everything); %.1f / %.1f questions; worst: %s %d%%\n",
		100 * ok_area / tot, 100 * ok_all / tot, (float)qa / tot, (float)qall / tot, CAL_FAULT[wf].name, worst);
	return 0;
}
