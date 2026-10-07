/*
   remote.c - saved remotes and settings on the SD card.
   WiiRremote Plus by Habex. GPL-3.0.
*/
#include "remote.h"
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

const u32 WM_BITS[WM_COUNT] = { BTN_A, BTN_B, BTN_1, BTN_2, BTN_PLUS, BTN_MINUS, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_NC_C, BTN_NC_Z };
const char *WM_NAMES[WM_COUNT] = { "A", "B", "1", "2", "+", "-", "Up", "Down", "Left", "Right", "C", "Z" };
const u32 CC_BITS[CC_COUNT] = { CC_A, CC_B, CC_X, CC_Y, CC_L, CC_R, CC_ZL, CC_ZR, CC_PLUS, CC_MINUS, CC_UP, CC_DOWN, CC_LEFT, CC_RIGHT };
const char *CC_NAMES[CC_COUNT] = { "a", "b", "x", "y", "L", "R", "ZL", "ZR", "+", "-", "Up", "Down", "Left", "Right" };

const char *ROLE_NAME[ROLE_COUNT] = { "Menu", "TV remote", "Off" };
Config cfg = { .pointer = 1, .open_last = 0, .seq_gap = 350, .repeat_ms = 110, .rumble = 1, .dim_secs = 300, .auto_ms = 1500, .safe_hold = 1, .remote_grid = 1, .chips = 1000, .group_size = 6 };
char data_dir[96] = "data";
char base_dir[64] = "data";

void storage_init(void)
{
#ifdef HOST
	strcpy(base_dir, "data");
#else
	strcpy(base_dir, "sd:/wiirremote");
#endif
	mkdir(base_dir, 0777);
	profile_use(cfg.profile);
}

/* Room profiles: Main lives in the base folder, others in base/profiles/NAME */
void profile_use(const char *name)
{
	char p[128];
	if (name != cfg.profile) snprintf(cfg.profile, sizeof cfg.profile, "%s", name ? name : "");
	if (!cfg.profile[0]) snprintf(data_dir, sizeof data_dir, "%s", base_dir);
	else {
		snprintf(p, sizeof p, "%s/profiles", base_dir); mkdir(p, 0777);
		snprintf(data_dir, sizeof data_dir, "%s/profiles/%s", base_dir, cfg.profile);
		mkdir(data_dir, 0777);
	}
	snprintf(p, sizeof p, "%s/remotes", data_dir);
	mkdir(p, 0777);
}

int profiles_list(char names[][NAME_LEN], int max)
{
	int n = 0;
	strcpy(names[n++], "");
	char p[128]; snprintf(p, sizeof p, "%s/profiles", base_dir);
	DIR *d = opendir(p);
	if (!d) return n;
	struct dirent *e;
	while ((e = readdir(d)) && n < max) {
		if (e->d_name[0] == '.') continue;
		snprintf(names[n++], NAME_LEN, "%s", e->d_name);
	}
	closedir(d);
	return n;
}

int profile_create(const char *name)
{
	char p[160];
	snprintf(p, sizeof p, "%s/profiles", base_dir); mkdir(p, 0777);
	snprintf(p, sizeof p, "%s/profiles/%s", base_dir, name);
	return mkdir(p, 0777) == 0;
}

/* "VOL+" -> "VOL+ 2" if VOL+ already exists */
void unique_name(Remote *r, const char *base, char *out)
{
	snprintf(out, NAME_LEN, "%s", base);
	for (int n = 2; n < 100 && remote_find_btn(r, out) >= 0; n++) {
		char b[NAME_LEN]; snprintf(b, sizeof b, "%s", base);
		b[NAME_LEN - 4] = 0;
		snprintf(out, NAME_LEN, "%s %d", b, n);
	}
}

static void trim(char *s)
{
	int n = strlen(s);
	while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = 0;
}

void config_load(void)
{
	char p[96], line[128];
	snprintf(p, sizeof p, "%s/config.txt", base_dir);
	FILE *f = fopen(p, "r");
	if (!f) return;
	while (fgets(line, sizeof line, f)) {
		trim(line);
		char *v = strchr(line, '=');
		if (!v) continue;
		*v++ = 0;
		if (!strcmp(line, "pointer")) cfg.pointer = atoi(v);
		else if (!strcmp(line, "boot_last")) cfg.open_last = atoi(v);   /* v1.5: new key, so it starts Off */
		else if (!strcmp(line, "seq_gap")) cfg.seq_gap = atoi(v);
		else if (!strcmp(line, "repeat_ms")) cfg.repeat_ms = atoi(v);
		else if (!strcmp(line, "rumble")) cfg.rumble = atoi(v);
		else if (!strcmp(line, "dim_secs")) cfg.dim_secs = atoi(v);
		else if (!strcmp(line, "auto_ms")) cfg.auto_ms = atoi(v);
		else if (!strcmp(line, "help_seen")) cfg.help_seen = strtoul(v, NULL, 0);
		else if (!strcmp(line, "light")) cfg.light = atoi(v);
		else if (!strcmp(line, "shift_mode")) cfg.shift_mode = atoi(v) % 3;
		else if (!strcmp(line, "role_bb")) cfg.role_bb = atoi(v) % ROLE_COUNT;
		else if (!strcmp(line, "role_kb")) cfg.role_kb = atoi(v) % ROLE_COUNT;
		else if (!strcmp(line, "role_mouse")) cfg.role_mouse = atoi(v) % ROLE_COUNT;
		else if (!strcmp(line, "fakemote")) cfg.fakemote = atoi(v);
		else if (!strcmp(line, "speaker")) cfg.speaker = atoi(v);
		else if (!strcmp(line, "remote_shape")) cfg.remote_grid = !atoi(v);   /* grid (scrolling pages) is the default */
		else if (!strcmp(line, "safe_hold")) cfg.safe_hold = atoi(v);
		else if (!strcmp(line, "addr_first")) cfg.addr_first = atoi(v);
		else if (!strcmp(line, "group_test")) cfg.group_test = atoi(v);
		else if (!strcmp(line, "group_size")) { cfg.group_size = atoi(v); if (cfg.group_size < 2 || cfg.group_size > 8) cfg.group_size = 6; }
		else if (!strcmp(line, "corner")) cfg.corner = atoi(v);
		else if (!strcmp(line, "auto_off")) cfg.auto_off = atoi(v);
		else if (!strcmp(line, "tilt")) cfg.tilt = atoi(v);
		else if (!strcmp(line, "chips")) cfg.chips = atoi(v);
		else if (!strcmp(line, "profile")) snprintf(cfg.profile, sizeof cfg.profile, "%s", v);
		else if (!strncmp(line, "role_gc", 7) && line[7] >= '0' && line[7] < '0' + MAX_PADS) cfg.role_gc[line[7] - '0'] = atoi(v) % ROLE_COUNT;
		else if (!strncmp(line, "role_wm", 7) && line[7] >= '0' && line[7] < '0' + MAX_PADS) cfg.role_wm[line[7] - '0'] = atoi(v) % ROLE_COUNT;
		else if (!strncmp(line, "role_cc", 7) && line[7] >= '0' && line[7] < '0' + MAX_PADS) cfg.role_cc[line[7] - '0'] = atoi(v) % ROLE_COUNT;
		else if (!strcmp(line, "last")) { strncpy(cfg.last, v, sizeof cfg.last - 1); }
	}
	fclose(f);
	if (cfg.seq_gap < 100) cfg.seq_gap = 100;
	if (cfg.repeat_ms < 60) cfg.repeat_ms = 60;
	if (cfg.auto_ms < 300) cfg.auto_ms = 300;
}

void config_save(void)
{
	char p[96];
	snprintf(p, sizeof p, "%s/config.txt", base_dir);
	FILE *f = fopen(p, "w");
	if (!f) return;
	fprintf(f, "pointer=%d\nboot_last=%d\nseq_gap=%d\nrepeat_ms=%d\nrumble=%d\ndim_secs=%d\nlast=%s\n",
	        cfg.pointer, cfg.open_last, cfg.seq_gap, cfg.repeat_ms, cfg.rumble, cfg.dim_secs, cfg.last);
	fprintf(f, "auto_ms=%d\nhelp_seen=0x%08X\nlight=%d\nshift_mode=%d\nprofile=%s\n", cfg.auto_ms, (unsigned)cfg.help_seen, cfg.light, cfg.shift_mode, cfg.profile);
	for (int i = 0; i < MAX_PADS; i++) fprintf(f, "role_gc%d=%d\n", i, cfg.role_gc[i]);
	fprintf(f, "role_bb=%d\nrole_kb=%d\nrole_mouse=%d\nfakemote=%d\nspeaker=%d\nremote_shape=%d\nsafe_hold=%d\ncorner=%d\nauto_off=%d\ntilt=%d\nchips=%d\n", cfg.role_bb, cfg.role_kb, cfg.role_mouse, cfg.fakemote, cfg.speaker, !cfg.remote_grid, cfg.safe_hold, cfg.corner, cfg.auto_off, cfg.tilt, cfg.chips);
	fprintf(f, "addr_first=%d\ngroup_test=%d\ngroup_size=%d\n", cfg.addr_first, cfg.group_test, cfg.group_size);
	for (int i = 0; i < MAX_PADS; i++) fprintf(f, "role_wm%d=%d\nrole_cc%d=%d\n", i, cfg.role_wm[i], i, cfg.role_cc[i]);
	fclose(f);
}

void remote_new(Remote *r, const char *name, u8 proto, u16 addr)
{
	memset(r, 0, sizeof *r);
	strncpy(r->name, name, NAME_LEN - 1);
	r->proto = proto; r->addr = addr;
	memset(r->map_wm, -1, sizeof r->map_wm);
	memset(r->map_cc, -1, sizeof r->map_cc);
	memset(r->map_wm2, -1, sizeof r->map_wm2);
}

static int remote_load_file(Remote *r, const char *p, const char *file);
int remote_load(Remote *r, const char *file_in)
{
	char p[128], file[40];
	/* copy the name first: remote_load(&cur, cur.file) would otherwise wipe it (remote_new clears r) */
	snprintf(file, sizeof file, "%s", file_in);
	snprintf(p, sizeof p, "%s/remotes/%s", data_dir, file);
	return remote_load_file(r, p, file);
}
/* a remote file anywhere (Smart match's library folder): not one of your remotes, so no file name */
int remote_load_path(Remote *r, const char *path)
{
	char p[160]; snprintf(p, sizeof p, "%s", path);
	return remote_load_file(r, p, "");
}
static int remote_load_file(Remote *r, const char *p, const char *file)
{
	char line[160];
	FILE *f = fopen(p, "r");
	if (!f) return 0;
	remote_new(r, "Remote", P_NEC, 0);
	strncpy(r->file, file, sizeof r->file - 1);
	int has_maps = 0;
	while (fgets(line, sizeof line, f)) {
		trim(line);
		char *v = strchr(line, '=');
		if (!v) continue;
		*v++ = 0;
		if (!strcmp(line, "maps")) has_maps = 1;
		else if (!strcmp(line, "name")) strncpy(r->name, v, NAME_LEN - 1);
		else if (!strcmp(line, "proto")) { int pr = proto_from_name(v); if (pr >= 0) r->proto = pr; }
		else if (!strcmp(line, "addr")) r->addr = strtol(v, NULL, 0);
		else if (!strcmp(line, "btn") && r->nbtn < MAX_BTNS) {
			/* NAME|PROTO|ADDR|CMD[|fav] */
			char *parts[5] = {0}; int n = 0; char *s = v;
			parts[n++] = s;
			while (*s && n < 5) { if (*s == '|') { *s = 0; parts[n++] = s + 1; } s++; }
			if (n >= 4) {
				int pr = proto_from_name(parts[1]);
				if (pr < 0) continue;
				RBtn *b = &r->btn[r->nbtn++];
				memset(b, 0, sizeof *b);
				strncpy(b->name, parts[0], NAME_LEN - 1);
				b->proto = pr; b->addr = strtol(parts[2], NULL, 0); b->cmd = strtol(parts[3], NULL, 0);
				if (n == 5) { b->fav = strchr(parts[4], 'F') != NULL; b->dead = strchr(parts[4], 'D') != NULL; char *sp = strchr(parts[4], 'S'); if (sp) b->slot = atoi(sp + 1); }
			}
		}
		else if (!strcmp(line, "macro") && r->nbtn < MAX_BTNS) {
			/* NAME|fav|PROTO:ADDR:CMD,PROTO:ADDR:CMD,... */
			char *p1 = strchr(v, '|'); if (!p1) continue; *p1++ = 0;
			char *p2 = strchr(p1, '|'); if (!p2) continue; *p2++ = 0;
			RBtn *b = &r->btn[r->nbtn];
			memset(b, 0, sizeof *b);
			strncpy(b->name, v, NAME_LEN - 1);
			b->fav = strchr(p1, 'F') != NULL; b->dead = strchr(p1, 'D') != NULL; { char *sp = strchr(p1, 'S'); if (sp) b->slot = atoi(sp + 1); }
			char *tok = strtok(p2, ",");
			while (tok && b->nsteps < MAX_STEPS) {
				char pn[16]; unsigned a, c, w = 0;
				char *c1 = strchr(tok, ':');
				if (c1 && sscanf(c1 + 1, "%x:%x:%u", &a, &c, &w) >= 2) {
					int len = c1 - tok; if (len > 15) len = 15;
					memcpy(pn, tok, len); pn[len] = 0;
					int pr = proto_from_name(pn);
					if (pr >= 0) { b->steps[b->nsteps].proto = pr; b->steps[b->nsteps].addr = a; b->steps[b->nsteps].cmd = c; b->steps[b->nsteps].wait = w; b->nsteps++; }
				}
				tok = strtok(NULL, ",");
			}
			if (b->nsteps) { b->proto = b->steps[0].proto; b->addr = b->steps[0].addr; b->cmd = b->steps[0].cmd; r->nbtn++; }
		}
		else if (!strcmp(line, "sv") && r->nsv < MAX_SV) {
			char *a = strchr(v, '|'); if (!a) continue; *a++ = 0;
			char *b = strchr(a, '|'); if (b) *b++ = 0;
			int k = r->nsv++;
			snprintf(r->sv_name[k], NAME_LEN, "%s", v);
			snprintf(r->sv_before[k], 12, "%s", a);
			snprintf(r->sv_after[k], 12, "%s", b ? b : "");
		}
		else if (!strcmp(line, "console") && r->ncons < MAX_CONS) {
			/* NAME|button|notes */
			char *a = strchr(v, '|'); if (!a) continue; *a++ = 0;
			char *b = strchr(a, '|'); if (b) *b++ = 0;
			int k = r->ncons++;
			snprintf(r->cons_name[k], NAME_LEN, "%s", v);
			r->cons_btn[k] = atoi(a);
			snprintf(r->cons_note[k], CONS_NOTE, "%s", b ? b : "");
		}
		else if (!strcmp(line, "note") && r->nnotes < MAX_NOTES) {
			strncpy(r->notes[r->nnotes++], v, NOTE_LEN - 1);
		}
		else if (!strcmp(line, "key") && r->nkeys < 32) {
			unsigned kc; int bi;
			if (sscanf(v, "%u:%d", &kc, &bi) == 2) { r->key_code[r->nkeys] = kc; r->key_btn[r->nkeys] = bi; r->nkeys++; }
		}
		else if (!strcmp(line, "wm2")) {
			char *c = strrchr(v, ':');
			if (!c) continue;
			*c++ = 0;
			for (int i = 0; i < WM_COUNT; i++) if (!strcmp(v, WM_NAMES[i])) r->map_wm2[i] = atoi(c);
		}
		else if (!strcmp(line, "wm") || !strcmp(line, "cc")) {
			/* KEY:index */
			char *c = strrchr(v, ':');
			if (!c) continue;
			*c++ = 0;
			int idx = atoi(c);
			if (line[0] == 'w') { for (int i = 0; i < WM_COUNT; i++) if (!strcmp(v, WM_NAMES[i])) r->map_wm[i] = idx; }
			else { for (int i = 0; i < CC_COUNT; i++) if (!strcmp(v, CC_NAMES[i])) r->map_cc[i] = idx; }
		}
	}
	fclose(f);
	if (!has_maps) remote_automap(r);
	for (int i = 0; i < WM_COUNT; i++) if (r->map_wm[i] >= r->nbtn) r->map_wm[i] = -1;
	for (int i = 0; i < CC_COUNT; i++) if (r->map_cc[i] >= r->nbtn) r->map_cc[i] = -1;
	for (int i = 0; i < WM_COUNT; i++) if (r->map_wm2[i] >= r->nbtn) r->map_wm2[i] = -1;
	for (int i = 0; i < r->nkeys; i++) if (r->key_btn[i] >= r->nbtn) r->key_btn[i] = -1;
	for (int i = 0; i < r->ncons; i++) if (r->cons_btn[i] >= r->nbtn) r->cons_btn[i] = -1;
	return 1;
}

/* F = favourite, D = dead, S<n> = spot on the on-screen remote */
static const char *btn_flags(const RBtn *b)
{
	static char f[16];
	snprintf(f, sizeof f, "%s%s", b->fav ? "F" : "-", b->dead ? "D" : "");
	if (b->slot) { char t[8]; snprintf(t, sizeof t, "S%d", b->slot); strcat(f, t); }
	return f;
}

int remote_save(Remote *r)
{
	char p[128];
	if (!r->file[0]) {
		/* pick an unused file name */
		for (int n = 1; n < 1000; n++) {
			snprintf(r->file, sizeof r->file, "remote%03d.txt", n);
			snprintf(p, sizeof p, "%s/remotes/%s", data_dir, r->file);
			FILE *t = fopen(p, "r");
			if (!t) break;
			fclose(t);
		}
	}
	snprintf(p, sizeof p, "%s/remotes/%s", data_dir, r->file);
	FILE *f = fopen(p, "w");
	if (!f) return 0;
	fprintf(f, "name=%s\nproto=%s\naddr=0x%02X\nmaps=1\n", r->name, PROTO_NAME[r->proto], r->addr);
	for (int i = 0; i < r->nbtn; i++) {
		RBtn *b = &r->btn[i];
		if (b->nsteps) {
			fprintf(f, "macro=%s|%s|", b->name, btn_flags(b));
			for (int k = 0; k < b->nsteps; k++)
				fprintf(f, "%s%s:%02X:%02X:%u", k ? "," : "", PROTO_NAME[b->steps[k].proto], b->steps[k].addr, b->steps[k].cmd, (unsigned)b->steps[k].wait);
			fprintf(f, "\n");
		} else {
			fprintf(f, "btn=%s|%s|0x%02X|0x%02X|%s\n", b->name, PROTO_NAME[b->proto], b->addr, b->cmd, btn_flags(b));
		}
	}
	for (int i = 0; i < r->nnotes; i++) fprintf(f, "note=%s\n", r->notes[i]);
	for (int i = 0; i < r->ncons; i++) fprintf(f, "console=%s|%d|%s\n", r->cons_name[i], r->cons_btn[i], r->cons_note[i]);
	for (int i = 0; i < r->nsv; i++) fprintf(f, "sv=%s|%s|%s\n", r->sv_name[i], r->sv_before[i], r->sv_after[i]);
	for (int i = 0; i < r->nkeys; i++) fprintf(f, "key=%u:%d\n", (unsigned)r->key_code[i], r->key_btn[i]);
	for (int i = 0; i < WM_COUNT; i++) if (r->map_wm[i] >= 0) fprintf(f, "wm=%s:%d\n", WM_NAMES[i], r->map_wm[i]);
	for (int i = 0; i < CC_COUNT; i++) if (r->map_cc[i] >= 0) fprintf(f, "cc=%s:%d\n", CC_NAMES[i], r->map_cc[i]);
	for (int i = 0; i < WM_COUNT; i++) if (r->map_wm2[i] >= 0) fprintf(f, "wm2=%s:%d\n", WM_NAMES[i], r->map_wm2[i]);
	fclose(f);
	return 1;
}

int remote_delete(const char *file)
{
	char p[128];
	snprintf(p, sizeof p, "%s/remotes/%s", data_dir, file);
	return remove(p) == 0;
}

int remotes_list(char files[][40], char names[][NAME_LEN], int max)
{
	char p[96];
	snprintf(p, sizeof p, "%s/remotes", data_dir);
	DIR *d = opendir(p);
	if (!d) return 0;
	int n = 0;
	struct dirent *e;
	while ((e = readdir(d)) && n < max) {
		int len = strlen(e->d_name);
		if (len < 5 || strcmp(e->d_name + len - 4, ".txt")) continue;
		strncpy(files[n], e->d_name, 39); files[n][39] = 0;
		n++;
	}
	closedir(d);
	/* sort by file name so the order stays stable */
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++)
			if (strcmp(files[i], files[j]) > 0) {
				char t[40]; strcpy(t, files[i]); strcpy(files[i], files[j]); strcpy(files[j], t);
			}
	static Remote tmp;
	for (int i = 0; i < n; i++) {
		if (remote_load(&tmp, files[i])) strcpy(names[i], tmp.name);
		else strcpy(names[i], files[i]);
	}
	return n;
}

int remote_add_btn(Remote *r, const char *name, u8 proto, u16 addr, u8 cmd)
{
	if (r->nbtn >= MAX_BTNS) return -1;
	RBtn *b = &r->btn[r->nbtn];
	memset(b, 0, sizeof *b);
	strncpy(b->name, name, NAME_LEN - 1);
	b->proto = proto; b->addr = addr; b->cmd = cmd;
	return r->nbtn++;
}

static void fix_maps_after_remove(Remote *r, int i)
{
	for (int k = 0; k < WM_COUNT; k++) { if (r->map_wm[k] == i) r->map_wm[k] = -1; else if (r->map_wm[k] > i) r->map_wm[k]--; }
	for (int k = 0; k < CC_COUNT; k++) { if (r->map_cc[k] == i) r->map_cc[k] = -1; else if (r->map_cc[k] > i) r->map_cc[k]--; }
	for (int k = 0; k < WM_COUNT; k++) { if (r->map_wm2[k] == i) r->map_wm2[k] = -1; else if (r->map_wm2[k] > i) r->map_wm2[k]--; }
	for (int k = 0; k < r->nkeys; k++) { if (r->key_btn[k] == i) r->key_btn[k] = -1; else if (r->key_btn[k] > i) r->key_btn[k]--; }
	for (int k = 0; k < r->ncons; k++) { if (r->cons_btn[k] == i) r->cons_btn[k] = -1; else if (r->cons_btn[k] > i) r->cons_btn[k]--; }
}

void remote_del_btn(Remote *r, int i)
{
	if (i < 0 || i >= r->nbtn) return;
	memmove(&r->btn[i], &r->btn[i + 1], (r->nbtn - i - 1) * sizeof(RBtn));
	r->nbtn--;
	fix_maps_after_remove(r, i);
}

static void swap_idx(s8 *m, int n, int a, int b)
{
	for (int k = 0; k < n; k++) { if (m[k] == a) m[k] = b; else if (m[k] == b) m[k] = a; }
}

void remote_move_btn(Remote *r, int i, int dir)
{
	int j = i + dir;
	if (i < 0 || j < 0 || i >= r->nbtn || j >= r->nbtn) return;
	RBtn t = r->btn[i]; r->btn[i] = r->btn[j]; r->btn[j] = t;
	swap_idx(r->map_wm, WM_COUNT, i, j);
	swap_idx(r->map_cc, CC_COUNT, i, j);
	swap_idx(r->map_wm2, WM_COUNT, i, j);
	swap_idx(r->key_btn, r->nkeys, i, j);
	swap_idx(r->cons_btn, r->ncons, i, j);
}

static int ieq(const char *a, const char *b)
{
	while (*a && *b) { if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0; a++; b++; }
	return *a == *b;
}

int remote_find_btn(Remote *r, const char *name)
{
	for (int i = 0; i < r->nbtn; i++) if (ieq(r->btn[i].name, name)) return i;
	return -1;
}

/* Fill empty mappings with sensible defaults based on button names. */
/* "Vol_up", "vol up", "VOLUP" -> "VOL+" so Flipper and hand-typed names match */
static void norm_name(const char *in, char *out)
{
	int n = 0;
	for (; *in && n < 30; in++) {
		char c = *in;
		if (c >= 'a' && c <= 'z') c -= 32;
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '-') out[n++] = c;
	}
	out[n] = 0;
	static const char *al[][2] = {
		{ "VOLUP", "VOL+" }, { "VOLUMEUP", "VOL+" }, { "VOLDN", "VOL-" }, { "VOLDOWN", "VOL-" }, { "VOLUMEDOWN", "VOL-" },
		{ "CHUP", "CH+" }, { "CHNEXT", "CH+" }, { "CHANNELUP", "CH+" }, { "CHDN", "CH-" }, { "CHDOWN", "CH-" },
		{ "CHPREV", "CH-" }, { "CHANNELDOWN", "CH-" }, { "P+", "CH+" }, { "P-", "CH-" }, { "PR+", "CH+" }, { "PR-", "CH-" }, { "POWERON", "POWER" }, { "SELECT", "OK" },
	};
	for (size_t k = 0; k < sizeof al / sizeof al[0]; k++) if (!strcmp(out, al[k][0])) { strcpy(out, al[k][1]); return; }
}
static int find_norm(Remote *r, const char *want)
{
	char a[32];
	for (int i = 0; i < r->nbtn; i++) { norm_name(r->btn[i].name, a); if (!strcmp(a, want)) return i; }
	return -1;
}

void remote_automap(Remote *r)
{
	struct { int wm; int cc; const char *names[3]; } rules[] = {
		{ 6, 10, { "UP", "CH+", NULL } },
		{ 7, 11, { "DOWN", "CH-", NULL } },
		{ 8, 12, { "LEFT", "VOL-", NULL } },
		{ 9, 13, { "RIGHT", "VOL+", NULL } },
		{ 0, 0,  { "OK", "ENTER", "SELECT" } },
		{ 1, 1,  { "EXIT", "BACK", "RETURN" } },
		{ 4, 8,  { "MENU", NULL, NULL } },
		{ 5, 9,  { "INPUT", "AV", "SOURCE" } },
		{ 2, 4,  { "VOL-", NULL, NULL } },
		{ 3, 5,  { "VOL+", NULL, NULL } },
		{ -1, 2, { "MUTE", NULL, NULL } },
		{ -1, 3, { "POWER", NULL, NULL } },
		{ -1, 6, { "CH-", NULL, NULL } },
		{ -1, 7, { "CH+", NULL, NULL } },
	};
	for (size_t k = 0; k < sizeof rules / sizeof rules[0]; k++) {
		int idx = -1;
		for (int n = 0; n < 3 && rules[k].names[n] && idx < 0; n++) idx = find_norm(r, rules[k].names[n]);
		if (idx < 0) continue;
		if (rules[k].wm >= 0 && r->map_wm[rules[k].wm] < 0) r->map_wm[rules[k].wm] = idx;
		if (rules[k].cc >= 0 && r->map_cc[rules[k].cc] < 0) r->map_cc[rules[k].cc] = idx;
	}
}

int remote_add_macro(Remote *r, const char *name, const Code *steps, int n)
{
	if (r->nbtn >= MAX_BTNS || n <= 0) return -1;
	if (n > MAX_STEPS) n = MAX_STEPS;
	RBtn *b = &r->btn[r->nbtn];
	memset(b, 0, sizeof *b);
	strncpy(b->name, name, NAME_LEN - 1);
	b->nsteps = n;
	memcpy(b->steps, steps, n * sizeof(Code));
	b->proto = steps[0].proto; b->addr = steps[0].addr; b->cmd = steps[0].cmd;
	return r->nbtn++;
}

/* ---------------- backups: data/backup/N/<remote files> ---------------- */

static int copy_file(const char *from, const char *to)
{
	FILE *a = fopen(from, "rb");
	if (!a) return 0;
	FILE *b = fopen(to, "wb");
	if (!b) { fclose(a); return 0; }
	char buf[1024]; size_t n;
	while ((n = fread(buf, 1, sizeof buf, a)) > 0) fwrite(buf, 1, n, b);
	fclose(a); fclose(b);
	return 1;
}

int backup_latest(void)
{
	char p[128];
	int last = 0;
	for (int n = 1; n < 1000; n++) {
		snprintf(p, sizeof p, "%s/backup/%d/done.txt", data_dir, n);
		FILE *f = fopen(p, "r");
		if (!f) break;
		fclose(f);
		last = n;
	}
	return last;
}

int backup_make(void)
{
	char p[128], q[160];
	int n = backup_latest() + 1;
	snprintf(p, sizeof p, "%s/backup", data_dir); mkdir(p, 0777);
	snprintf(p, sizeof p, "%s/backup/%d", data_dir, n); mkdir(p, 0777);
	static char files[MAX_REMOTES][40]; static char names[MAX_REMOTES][NAME_LEN];
	int cnt = remotes_list(files, names, MAX_REMOTES);
	for (int i = 0; i < cnt; i++) {
		snprintf(p, sizeof p, "%s/remotes/%s", data_dir, files[i]);
		snprintf(q, sizeof q, "%s/backup/%d/%s", data_dir, n, files[i]);
		if (!copy_file(p, q)) return 0;
	}
	snprintf(p, sizeof p, "%s/backup/%d/done.txt", data_dir, n);
	FILE *f = fopen(p, "w");
	if (!f) return 0;
	fprintf(f, "%d remotes\n", cnt);
	fclose(f);
	return n;
}

int backup_restore(void)
{
	int n = backup_latest();
	if (!n) return 0;
	char p[128], q[160];
	snprintf(p, sizeof p, "%s/backup/%d", data_dir, n);
	DIR *d = opendir(p);
	if (!d) return 0;
	struct dirent *e;
	while ((e = readdir(d))) {
		int len = strlen(e->d_name);
		if (len < 5 || strcmp(e->d_name + len - 4, ".txt") || !strcmp(e->d_name, "done.txt")) continue;
		snprintf(p, sizeof p, "%s/backup/%d/%s", data_dir, n, e->d_name);
		snprintf(q, sizeof q, "%s/remotes/%s", data_dir, e->d_name);
		copy_file(p, q);
	}
	closedir(d);
	return n;
}

/* USB HID key code -> name */
const char *key_name(u8 c)
{
	static char b[12];
	if (c >= 4 && c <= 29) { b[0] = 'A' + c - 4; b[1] = 0; return b; }
	if (c >= 30 && c <= 38) { b[0] = '1' + c - 30; b[1] = 0; return b; }
	if (c == 39) return "0";
	if (c >= 58 && c <= 69) { snprintf(b, sizeof b, "F%d", c - 57); return b; }
	if (c >= 89 && c <= 97) { snprintf(b, sizeof b, "Num %d", c - 88); return b; }
	switch (c) {
		case 40: return "Enter"; case 41: return "Esc"; case 42: return "Backspace"; case 43: return "Tab";
		case 44: return "Space"; case 45: return "-"; case 46: return "="; case 47: return "["; case 48: return "]";
		case 51: return ";"; case 52: return "'"; case 54: return ","; case 55: return "."; case 56: return "/";
		case 73: return "Insert"; case 74: return "Home"; case 75: return "Page Up"; case 76: return "Delete";
		case 77: return "End"; case 78: return "Page Down"; case 79: return "Right"; case 80: return "Left";
		case 81: return "Down"; case 82: return "Up"; case 84: return "Num /"; case 85: return "Num *";
		case 86: return "Num -"; case 87: return "Num +"; case 88: return "Num Enter"; case 98: return "Num 0";
		case 127: return "Mute"; case 128: return "Vol Up"; case 129: return "Vol Down";
	}
	snprintf(b, sizeof b, "Key %u", c);
	return b;
}

/* ---- Export to SD: the remote as a WiiRremote file and as a Flipper Zero .ir file ---- */
int remote_export(Remote *r, char *msg, int msglen)
{
	char dir[96], safe[NAME_LEN], p[160];
	snprintf(dir, sizeof dir, "%s/export", base_dir);
	mkdir(dir, 0777);
	int n = 0;
	for (const char *c = r->name; *c && n < NAME_LEN - 1; c++) safe[n++] = ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9')) ? *c : '_';
	safe[n] = 0;
	if (!n) strcpy(safe, "remote");
	/* 1: copy of the remote file (open it on another Wii: put it in wiirremote/remotes) */
	if (!remote_save(r)) return 0;
	char src[160]; snprintf(src, sizeof src, "%s/remotes/%s", data_dir, r->file);
	FILE *in = fopen(src, "rb");
	snprintf(p, sizeof p, "%s/%s.txt", dir, safe);
	FILE *out = fopen(p, "wb");
	if (!in || !out) { if (in) fclose(in); if (out) fclose(out); return 0; }
	char buf[512]; size_t k;
	while ((k = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, k, out);
	fclose(in); fclose(out);
	/* 2: Flipper Zero / Flipper-IRDB style .ir */
	snprintf(p, sizeof p, "%s/%s.ir", dir, safe);
	FILE *f = fopen(p, "w");
	int ok = 0, skip = 0;
	if (f) {
		fprintf(f, "Filetype: IR signals file\nVersion: 1\n# Made with WiiRremote Plus\n");
		for (int i = 0; i < r->nbtn; i++) {
			RBtn *b = &r->btn[i];
			if (b->nsteps || b->dead) continue;
			const char *pr = NULL; unsigned a[4] = { 0 }, c[4] = { 0 };
			switch (b->proto) {
			case P_NEC: pr = "NEC"; a[0] = b->addr & 0xFF; c[0] = b->cmd; break;
			case P_NECX: pr = "NECext"; a[0] = b->addr & 0xFF; a[1] = b->addr >> 8; c[0] = b->cmd; c[1] = (u8)~b->cmd; break;
			case P_SAMSUNG: pr = "Samsung32"; a[0] = b->addr; c[0] = b->cmd; break;
			case P_RC5: pr = b->cmd > 63 ? "RC5X" : "RC5"; a[0] = b->addr; c[0] = b->cmd; break;
			case P_RC6: pr = "RC6"; a[0] = b->addr; c[0] = b->cmd; break;
			case P_SONY: pr = "SIRC"; a[0] = b->addr; c[0] = b->cmd; break;
			case P_SONY15: pr = "SIRC15"; a[0] = b->addr; c[0] = b->cmd; break;
			case P_PANA: if (!(b->addr >> 8)) { pr = "Kaseikyo"; a[0] = b->addr & 0xFF; a[1] = 0x02; a[2] = 0x20; c[0] = (b->cmd << 4) & 0xFF; c[1] = b->cmd >> 4; } break;
			}
			if (!pr) { skip++; continue; }
			fprintf(f, "#\nname: %s\ntype: parsed\nprotocol: %s\naddress: %02X %02X %02X %02X\ncommand: %02X %02X %02X %02X\n",
			        b->name, pr, a[0], a[1], a[2], a[3], c[0], c[1], c[2], c[3]);
			ok++;
		}
		fclose(f);
	}
	snprintf(msg, msglen, skip ? "Saved to wiirremote/export (%d not in .ir)" : "Saved to wiirremote/export", skip);
	return 1;
}
