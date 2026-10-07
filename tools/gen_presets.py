#!/usr/bin/env python3
"""Build source/presets_db.c from the public IR databases.

Usage: gen_presets.py <irdb> <Flipper-IRDB> <lirc-remotes> <out.c>

Sources (all free, community made):
  IRDB          https://github.com/probonopd/irdb          (codes/<Brand>/<Type>/<dev>,<sub>.csv)
  Flipper-IRDB  https://github.com/Lucaslhm/Flipper-IRDB   (TVs, Monitors, Projectors, ... and _Converted_)
  LIRC remotes  https://github.com/probonopd/lirc-remotes  (decoded by DecodeIR)
  SmartIR       https://github.com/smartHomeHub/SmartIR (+ forks)  codes/media_player/*.json (Broadlink, ESPHome)
  Flipper Zero  firmware universal remotes: assets/tv.ir, projector.ir (grouped by address)

v2 (1.9.1):
  - Flipper "raw" recordings are decoded too (NEC, NEC ext, Samsung, Sony 12/15, JVC,
    Panasonic, RC5, RC6, RCA), not just the parsed ones.
  - Monitors, Projectors, Universal TV remotes, touch screens and the big _Converted_
    collection are read. Remotes of unknown type count if they clearly drive a TV.
  - IRDB projector / monitor / plasma remotes count.
  - Flipper RCA codes are read.
  - Brands in a label are listed most common first.

Every TV code set whose protocol the Wii can send becomes a preset.
Sets with the same protocol + address that never disagree are merged.
"""
import csv, glob, os, re, sys
from collections import defaultdict, Counter

IRDB, FDB, LIRCDB, OUT = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
EXTRA = sys.argv[5:]   # more folders: SmartIR code folders (json) and Flipper firmware universal remotes (.ir)

P = dict(NEC=0, SAMSUNG=1, RC5=2, SONY=3, JVC=4, PANA=5, SHARP=6, RCA=7, MITSU=8, NECX=9, RC6=10, SONY15=11, RCA38=12)

# function ids (must match presets.h)
FN = ["POWER", "MUTE", "VOL+", "VOL-", "CH+", "CH-", "MENU", "OK", "UP", "DOWN", "LEFT", "RIGHT",
      "BACK", "INPUT", "INFO", "SLEEP", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "PICTURE", "PREV CH",
      "PWR ON", "PWR OFF", "AV1", "AV2", "AV3", "COMP", "HDMI1", "HDMI2", "TV"]
FID = {n: i for i, n in enumerate(FN)}
TVSET = {FID["POWER"], FID["VOL+"], FID["VOL-"], FID["CH+"], FID["CH-"]}

def norm(name):
    n = name.upper().strip()
    n = re.sub(r"^(KEY|BTN|TV)_", "", n)
    n = n.replace("_", " ").replace("-", " - ").replace("+", " + ")
    n = re.sub(r"\s+", " ", n).strip()
    c = n.replace(" ", "")
    # discrete codes: one job each, great for macros (v1.9.1)
    if c in ("POWERON", "PWRON", "ON", "POWER(ON)", "DISCRETEON", "POWERONDISCRETE", "DISCRETEPOWERON", "TURNON"): return "PWR ON"
    if c in ("POWEROFF", "PWROFF", "OFF", "POWER(OFF)", "DISCRETEOFF", "POWEROFFDISCRETE", "DISCRETEPOWEROFF", "TURNOFF", "STANDBYON"): return "PWR OFF"
    m = re.fullmatch(r"(?:INPUT)?(?:AV|VIDEO|EXT|SCART|CVBS|COMPOSITE)([1-3])", c)
    if m: return "AV" + m.group(1)
    if re.fullmatch(r"(?:INPUT)?(?:COMPONENT|COMP|YPBPR|YUV)1?", c): return "COMP"
    m = re.fullmatch(r"(?:INPUT)?HDMI([12])", c)
    if m: return "HDMI" + m.group(1)
    if c in ("TV", "TUNER", "ANTENNA", "INPUTTV", "TVINPUT", "AIR", "ATV", "RF", "TVMODE"): return "TV"
    if re.search(r"POWER ?(ON|OFF)|DISCRETE|STANDBY ON", n): return None
    if c in ("POWER", "POWERTOGGLE", "POWER(TOGGLE)", "TVPOWER", "STANDBY", "ONOFF", "ON/OFF", "PWR", "POWERON/OFF", "POWERONOFF", "POW", "POWERBUTTON"): return "POWER"
    if c in ("MUTE", "MUTING", "SOUNDOFF", "VOLUMEMUTE", "MUTETOGGLE", "VOLUMEMUTETOGGLE", "AUDIOMUTE", "MUTEAUDIO"): return "MUTE"
    if re.fullmatch(r"(VOL|VOLUME|TVVOL|VOLUMEN)(\+|UP|PLUS)", c): return "VOL+"
    if re.fullmatch(r"(VOL|VOLUME|TVVOL|VOLUMEN)(-|DOWN|DN|MINUS)", c): return "VOL-"
    if re.fullmatch(r"(CH|CHAN|CHANNEL|PROG|PROGRAM|PR|P|TVCH|PAGE)(\+|UP|NEXT|PLUS)", c) and not c.startswith("PAGE"): return "CH+"
    if re.fullmatch(r"(CH|CHAN|CHANNEL|PROG|PROGRAM|PR|P|TVCH)(-|DOWN|DN|PREV|MINUS)", c): return "CH-"
    if c in ("MENU", "SETUP", "MAINMENU", "TVMENU", "SETTINGS"): return "MENU"
    if c in ("OK", "ENTER", "SELECT", "OK/ENTER", "ENTER/OK", "OK/SELECT", "CONFIRM"): return "OK"
    if c in ("UP", "ARROWUP", "CURSORUP", "MENUUP", "UPARROW", "NAVUP", "DIRUP"): return "UP"
    if c in ("DOWN", "ARROWDOWN", "ARROWDN", "CURSORDOWN", "MENUDOWN", "DOWNARROW", "NAVDOWN", "DIRDOWN", "DN", "DNARROW"): return "DOWN"
    if c in ("LEFT", "ARROWLEFT", "ARROWLT", "CURSORLEFT", "MENULEFT", "LEFTARROW", "NAVLEFT", "DIRLEFT"): return "LEFT"
    if c in ("RIGHT", "ARROWRIGHT", "ARROWRT", "CURSORRIGHT", "MENURIGHT", "RIGHTARROW", "NAVRIGHT", "DIRRIGHT"): return "RIGHT"
    if c in ("BACK", "RETURN", "EXIT", "MENUEXIT", "EXITMENU", "CANCEL"): return "BACK"
    if c in ("INPUT", "SOURCE", "AV", "TV/AV", "TV/VIDEO", "INPUTSELECT", "INPUTSOURCE", "INPUTSCROLL", "AV/TV", "SOURCES",
             "EXTERNALINPUT", "VIDEO", "AVSELECT", "TVAV", "AVTV", "INPUTS", "AVINPUT", "TV/INPUT", "EXT", "SRC"): return "INPUT"
    if c in ("INFO", "DISPLAY", "RECALL", "INFO/RECALL", "INFORMATION", "OSD", "STATUS", "DISP"): return "INFO"
    if c in ("SLEEP", "SLEEPTIMER", "TIMER", "OFFTIMER"): return "SLEEP"
    if c in ("PICTURE", "PICTUREMODE", "PMODE", "P.MODE", "PICTURESELECT", "PICTUREMENU", "VIDEOSTATUS", "AVRESET", "PIC", "PICMODE", "IMAGE"): return "PICTURE"
    if c in ("PREVCH", "PREVIOUSCHANNEL", "LASTCHANNEL", "RECALLCHANNEL", "FLASHBACK", "JUMP", "Q.VIEW", "QVIEW", "SWAP", "LAST", "PRE-CH", "PRECH", "PRE - CH"): return "PREV CH"
    m = re.fullmatch(r"(?:DIGIT|NUM|NUMBER|KEY|BTN)?\s*([0-9])(?:/10)?", n)
    if m: return m.group(1)
    return None

sets = []   # (brand, model, proto, addr, {fid: cmd})

def map_proto(pr, dev, sub, fn):
    """DecodeIR / IRDB protocol names -> (our protocol, address), command"""
    if not (0 <= fn <= 255) or dev < 0: return None, fn
    if pr in ("NEC1", "NEC", "NEC2"):
        if sub in (-1, (~dev) & 255): key = (P["NEC"], dev)
        else: key = (P["NECX"], dev | (sub << 8))
    elif pr in ("NECx1", "NECx2"):
        if sub in (-1, dev): key = (P["SAMSUNG"], dev)
        else: return None, fn
    elif pr in ("RC5", "RC5x") and dev < 32 and fn < 128: key = (P["RC5"], dev)
    elif pr in ("RC6", "RC6-0-16") and dev <= 255: key = (P["RC6"], dev)
    elif pr == "Sony12" and dev < 32 and fn < 128: key = (P["SONY"], dev)
    elif pr == "Sony15" and fn < 128: key = (P["SONY15"], dev)
    elif pr == "JVC": key = (P["JVC"], dev)
    elif pr == "Panasonic": key = (P["PANA"], dev | ((sub if sub >= 0 else 0) << 8))
    elif pr == "Sharp" and dev < 32: key = (P["SHARP"], dev)
    elif pr in ("RCA", "RCA(Old)") and dev < 16: key = (P["RCA"], dev)
    elif pr == "RCA-38" and dev < 16: key = (P["RCA38"], dev)
    elif pr == "Mitsubishi": key = (P["MITSU"], dev)
    else: return None, fn
    return key, fn

# ---------------- raw recordings -> (protocol, address, command) ----------------
def near(v, t, tol=0.28): return abs(v - t) <= t * tol

def pulse_bits(d, i, n, mark, zero, one, lsb=True):
    """n bits coded in the space length. returns (value, next index) or None"""
    v = 0
    for b in range(n):
        if i + 1 >= len(d) or not near(d[i], mark, 0.4): return None
        s = d[i + 1]
        if near(s, one, 0.3): bit = 1
        elif near(s, zero, 0.4): bit = 0
        else: return None
        v |= bit << (b if lsb else n - 1 - b)
        i += 2
    return v, i

def dec_nec_like(d):
    if len(d) < 66: return None
    if near(d[0], 9000) and near(d[1], 4500): kind = "NEC"
    elif near(d[0], 4500) and near(d[1], 4500): kind = "SAM"
    else: return None
    r = pulse_bits(d, 2, 32, 560, 560, 1690)
    if not r: return None
    v = r[0]; a0, a1, c0, c1 = v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >> 24) & 255
    if c1 != (~c0) & 255: return None
    if kind == "SAM" and a0 == a1: return (P["SAMSUNG"], a0, c0)
    if a1 == (~a0) & 255: return (P["NEC"], a0, c0)
    return (P["NECX"], a0 | (a1 << 8), c0)

def dec_sony(d):
    if len(d) < 25 or not near(d[0], 2400, 0.2): return None
    bits = []; i = 1
    while i + 1 < len(d) and near(d[i], 600, 0.4) and len(bits) < 20:
        m = d[i + 1]
        if near(m, 1200, 0.3): bits.append(1)
        elif near(m, 600, 0.4): bits.append(0)
        else: break
        i += 2
    n = len(bits)
    if n not in (12, 15): return None
    cmd = sum(b << k for k, b in enumerate(bits[:7]))
    addr = sum(b << k for k, b in enumerate(bits[7:]))
    return (P["SONY"] if n == 12 else P["SONY15"], addr, cmd)

def dec_jvc(d):
    if len(d) < 34 or not (near(d[0], 8400, 0.2) and near(d[1], 4200, 0.2)): return None
    r = pulse_bits(d, 2, 16, 526, 526, 1574)
    if not r: return None
    return (P["JVC"], r[0] & 255, r[0] >> 8)

def dec_pana(d):
    if len(d) < 98 or not (near(d[0], 3456, 0.2) and near(d[1], 1728, 0.25)): return None
    b = []
    i = 2
    for k in range(6):
        r = pulse_bits(d, i, 8, 432, 432, 1296)
        if not r: return None
        b.append(r[0]); i = r[1]
    if b[0] != 0x02 or b[1] != 0x20 or b[5] != (b[2] ^ b[3] ^ b[4]): return None
    return (P["PANA"], b[2] | (b[3] << 8), b[4])

def dec_rca(d, freq):
    if len(d) < 50 or not (near(d[0], 4000, 0.2) and near(d[1], 4000, 0.2)): return None
    r = pulse_bits(d, 2, 24, 500, 1000, 2000, lsb=False)
    if not r: return None
    v = r[0]; a, c, na, nc = v >> 20, (v >> 12) & 255, (v >> 8) & 15, v & 255
    if na != (~a) & 15 or nc != (~c) & 255: return None
    return (P["RCA"] if freq >= 48000 else P["RCA38"], a, c)

def halves(d, unit, start_level=1):
    """durations -> list of half-bit levels (unit long each)"""
    out = []; lvl = start_level
    for x in d:
        k = round(x / unit)
        if k < 1 or k > 6 or abs(x - k * unit) > unit * 0.45: break
        out += [lvl] * k; lvl ^= 1
    return out

def dec_rc5(d):
    h = [0] + halves(d, 889)            # the first half (a space) is not in the recording
    if len(h) < 27: return None
    h = (h + [0])[:28]
    bits = []
    for k in range(14):
        a, b = h[2 * k], h[2 * k + 1]
        if (a, b) == (0, 1): bits.append(1)
        elif (a, b) == (1, 0): bits.append(0)
        else: return None
    if bits[0] != 1: return None
    addr = sum(bits[3 + k] << (4 - k) for k in range(5))
    cmd = sum(bits[8 + k] << (5 - k) for k in range(6)) | ((1 - bits[1]) << 6)
    return (P["RC5"], addr, cmd)

def dec_rc6(d):
    if len(d) < 20 or not (near(d[0], 2666, 0.2) and near(d[1], 889, 0.3)): return None
    h = halves(d[2:], 444)
    if len(h) < 2 + 6 + 4 + 31: return None
    h = h + [0]
    i = 0
    def bit():
        nonlocal i
        a, b = h[i], h[i + 1]; i += 2
        return 1 if (a, b) == (1, 0) else 0 if (a, b) == (0, 1) else None
    if bit() != 1: return None
    mode = [bit() for _ in range(3)]
    if mode != [0, 0, 0]: return None
    if not ((h[i:i + 4] in ([1, 1, 0, 0], [0, 0, 1, 1]))): return None
    i += 4
    v = 0
    for _ in range(16):
        b = bit()
        if b is None: return None
        v = (v << 1) | b
    return (P["RC6"], v >> 8, v & 255)

def decode_raw(data, freq):
    try: d = [abs(int(x)) for x in data.split()]
    except Exception: return None
    # try the start, then after each long gap (a repeat frame may be cleaner)
    starts = [0] + [k + 1 for k in range(1, len(d) - 1, 2) if d[k] > 8000][:3]
    for s in starts:
        seg = d[s:]
        for f in (dec_nec_like, dec_sony, dec_jvc, dec_pana, dec_rc6, dec_rc5):
            r = f(seg)
            if r: return r
        r = dec_rca(seg, freq)
        if r: return r
    return None

# ---------------- IRDB csv ----------------
NON_TV = r"SAT|DVR|VCR|DVD|TUNER|RECEIVER|BOX|CABLE|STB|CAMERA|\bPC\b|AUDIO|AMP|\bCD\b|TAPE|RADIO|STEREO|BLU|LASER|DISC|HOME THEATER|SPEAKER|DVB|PVR|MEDIA|IPTV|SOUND|HIFI|HI-FI|MINIDISC|JUKEBOX|ROKU|APPLE|CHROMECAST|FIRE|XBOX|PLAYSTATION|AIR|FAN|LIGHT|LED STRIP|HEATER|CLIMATE|VACUUM"
TV_RE = r"(\b|_)TV(\b|_)|TELEVIS|TELEVISOR|LCD|PLASMA|MONITOR|PROJECT|CRT|HDTV|OLED|QLED|SMART ?TV|DISPLAY|BEAMER"

for f in glob.glob(os.path.join(IRDB, "codes", "*", "*", "*.csv")):
    brand = f.split(os.sep)[-3]
    kind = f.split(os.sep)[-2]
    K = kind.upper()
    tvkind = bool(re.search(TV_RE, K)) or K.startswith("TV")
    if re.search(NON_TV, K) and not (K.startswith("TV") or re.search(r"PROJECT|MONITOR|PLASMA", K)): continue
    if not tvkind and not K.startswith("UNKNOWN"): continue
    unknown_kind = not tvkind
    per = defaultdict(dict)
    try:
        rows = list(csv.DictReader(open(f, encoding="utf-8", errors="ignore")))
    except Exception:
        continue
    for r in rows:
        try:
            pr, dev, sub, fn = r["protocol"].strip(), int(r["device"]), int(r["subdevice"]), int(r["function"])
        except Exception:
            continue
        k = norm(r["functionname"])
        if k is None or not (0 <= fn <= 255): continue
        key, fn = map_proto(pr, dev, sub, fn)
        if key is None: continue
        per[key].setdefault(FID[k], fn)
    for key, m in per.items():
        if unknown_kind and not TVSET <= set(m): continue
        sets.append((brand, kind, key[0], key[1], m))

# ---------------- Flipper .ir (parsed + raw) ----------------
def hb(s, i):
    try: return int(s.split()[i], 16)
    except Exception: return 0

def parsed_key(cur):
    pr, a, c = cur.get("protocol", ""), cur.get("address", ""), cur.get("command", "")
    a0, a1, c0, c1 = hb(a, 0), hb(a, 1), hb(c, 0), hb(c, 1)
    if pr == "NEC": return (P["NEC"], a0, c0)
    if pr == "NECext" and c1 == (~c0) & 255:
        return (P["NEC"], a0, c0) if a1 == (~a0) & 255 else (P["NECX"], a0 | (a1 << 8), c0)
    if pr == "Samsung32": return (P["SAMSUNG"], a0, c0)
    if pr in ("RC5", "RC5X") and a0 < 32 and c0 < 128: return (P["RC5"], a0, c0)
    if pr == "RC6": return (P["RC6"], a0, c0)
    if pr == "SIRC" and a0 < 32 and c0 < 128: return (P["SONY"], a0, c0)
    if pr == "SIRC15" and c0 < 128: return (P["SONY15"], a0, c0)
    if pr == "RCA" and a0 < 16: return (P["RCA"], a0, c0)
    if pr == "Kaseikyo" and a1 == 0x02 and hb(a, 2) == 0x20:
        cw = c0 | (c1 << 8)
        if not (cw & 0x0F): return (P["PANA"], a0, (cw >> 4) & 255)
    return None

def read_ir(path):
    per = defaultdict(dict); cur = {}
    try: lines = open(path, encoding="utf-8", errors="ignore").read().splitlines() + ["name:"]
    except Exception: return per
    for ln in lines:
        if ln.startswith("name:"):
            if "name" in cur:
                k = norm(cur["name"])
                if k is not None:
                    r = None
                    if cur.get("type") == "parsed": r = parsed_key(cur)
                    elif cur.get("type") == "raw":
                        try: fq = int(float(cur.get("frequency", "38000")))
                        except Exception: fq = 38000
                        r = decode_raw(cur.get("data", ""), fq)
                    if r and 0 <= r[2] <= 255:
                        per[(r[0], r[1])].setdefault(FID[k], r[2])
            cur = {}
        if ":" in ln and not ln.startswith("#"):
            kk, vv = ln.split(":", 1)
            cur[kk.strip()] = vv.strip()
    return per

def read_ir_signals(path):
    """every signal on its own: [((proto, addr), {fid: cmd})]"""
    out = []; cur = {}
    try: lines = open(path, encoding="utf-8", errors="ignore").read().splitlines() + ["name:"]
    except Exception: return out
    for ln in lines:
        if ln.startswith("name:"):
            if "name" in cur:
                k = norm(cur["name"])
                if k is not None:
                    r = parsed_key(cur) if cur.get("type") == "parsed" else decode_raw(cur.get("data", ""), 38000) if cur.get("type") == "raw" else None
                    if r and 0 <= r[2] <= 255: out.append(((r[0], r[1]), {FID[k]: r[2]}))
            cur = {}
        if ":" in ln and not ln.startswith("#"):
            kk, vv = ln.split(":", 1); cur[kk.strip()] = vv.strip()
    return out

def nice_brand(b):
    b = b.replace("_", " ").strip()
    if b.isupper() and len(b) > 3: b = b.title()
    return b

# ---------------- JP1 / One For All research (tools/jp1_codes.csv, v1.9.1) ----------------
# European and Australian CRT brands gathered from the JP1 forums and hifi-remote.com
# (only protocols the Wii can send: NEC, RC5, RCA-38)
import csv as _csv
_jp1 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "jp1_codes.csv")
_jp1_brands = set()
if os.path.exists(_jp1):
    _per = defaultdict(dict)
    for r in _csv.DictReader(open(_jp1)):
        key, fn = map_proto(r["protocol"], int(r["device"]), int(r["subdevice"]), int(r["obc"]))
        k = norm(r["button"])
        if key is None or k is None: continue
        _per[(r["brand"], r["model"], key)].setdefault(FID[k], fn)
    for (b, m, key), mm in _per.items(): sets.append((b, m, key[0], key[1], mm)); _jp1_brands.add(b.lower())
    print("JP1 research:", len(_per), "code sets")

TV_DIRS = ["TVs", "Monitors", "Projectors", "Universal_TV_Remotes", "Touchscreen_Displays", "Digital_Signs"]
n_raw_files = 0
for top in TV_DIRS:
    for f in glob.glob(os.path.join(FDB, top, "**", "*.ir"), recursive=True):
        parts = f.split(os.sep)
        i = parts.index(top)
        brand = parts[i + 1] if len(parts) > i + 2 else top
        model = os.path.splitext(parts[-1])[0]
        for key, m in read_ir(f).items():
            sets.append((nice_brand(brand), model, key[0], key[1], m))

# the big converted collection: TVs by name, or any remote that clearly drives a TV
for f in glob.glob(os.path.join(FDB, "_Converted_", "*", "**", "*.ir"), recursive=True):
    parts = f.split(os.sep)
    i = parts.index("_Converted_")
    rel = parts[i + 1:]
    if len(rel) < 3: continue
    brand = rel[2] if len(rel) > 3 else rel[-2]
    if brand in ("0-9",) or len(brand) == 1: brand = rel[-2]
    tail = " ".join(rel[2:]).upper().replace("_", " ")
    is_tv = bool(re.search(TV_RE, tail))
    if re.search(NON_TV, tail) and not is_tv: continue
    for key, m in read_ir(f).items():
        if not is_tv and not TVSET <= set(m): continue
        sets.append((nice_brand(brand), os.path.splitext(rel[-1])[0], key[0], key[1], m))

# ---------------- LIRC remotes database ----------------
import xml.etree.ElementTree as ET
TRANSPORT = {"PLAY", "RECORD", "REC", "EJECT", "REWIND", "FASTFORWARD", "FF", "REW"}
for f in glob.glob(os.path.join(LIRCDB, "*", "*.xml")):
    brand = f.split(os.sep)[-2]
    model = os.path.basename(f)[:-4]
    try: root = ET.parse(f).getroot()
    except Exception: continue
    for rem in root.iter("remote"):
        rname = rem.get("name", "")
        per = defaultdict(dict); names = set()
        for code in rem.iter("code"):
            nm = code.get("name", "")
            names.add(re.sub(r"^KEY_", "", nm.upper()))
            d = code.find("decoding")
            if d is None: continue
            try:
                key, fn = map_proto(d.get("protocol", ""), int(d.get("device")), int(d.get("subdevice")), int(d.get("obc")))
            except Exception:
                continue
            if key is None: continue
            k = norm(nm)
            if k is None: continue
            per[key].setdefault(FID[k], fn)
        is_tv = re.search(r"(\b|_)tv(\b|_)|television|crt|teletext|kv[-_]|trinitron|lcd|plasma|monitor|projector|beamer", (model + " " + rname).lower())
        if (names & TRANSPORT) and not is_tv: continue
        for key, mm in per.items():
            if not is_tv and not TVSET <= set(mm): continue
            b = brand.capitalize() if brand.islower() else brand
            sets.append((b, model, key[0], key[1], mm))

# ---------------- SmartIR (Home Assistant) media players ----------------
import json, base64
def broadlink_to_raw(b64):
    try: b = base64.b64decode(b64)
    except Exception: return None
    if len(b) < 4 or b[0] != 0x26: return None
    n = b[2] | (b[3] << 8); d = b[4:4 + n]; out = []; i = 0
    while i < len(d):
        v = d[i]; i += 1
        if v == 0:
            if i + 1 >= len(d): break
            v = (d[i] << 8) | d[i + 1]; i += 2
        out.append(int(round(v * 32.84)))   # Broadlink ticks are 269/8192 ms
    return " ".join(str(x) for x in out)
SMART_NAMES = {"volumeup": "VOL+", "volumedown": "VOL-", "mute": "MUTE", "nextchannel": "CH+", "previouschannel": "CH-",
               "power": "POWER", "menu": "MENU", "ok": "OK", "up": "UP", "down": "DOWN", "left": "LEFT", "right": "RIGHT",
               "back": "BACK", "exit": "BACK", "source": "INPUT", "input": "INPUT", "info": "INFO"}
def smartir_name(k):
    c = re.sub(r"[^a-z0-9]", "", k.lower())
    if c in SMART_NAMES: return SMART_NAMES[c]
    m = re.fullmatch(r"(?:channel|ch|num|digit|key)?([0-9])", c)
    if m: return m.group(1)
    return norm(k)
n_smartir = 0
for d in EXTRA:
    for f in glob.glob(os.path.join(d, "**", "*.json"), recursive=True):
        try: j = json.load(open(f, encoding="utf-8", errors="ignore"))
        except Exception: continue
        ctl = j.get("supportedController", ""); brand = j.get("manufacturer", "SmartIR")
        models = j.get("supportedModels") or [""]
        per = defaultdict(dict)
        def walk(dct):
            for k, v in dct.items():
                if isinstance(v, dict): walk(v); continue
                if not isinstance(v, str): continue
                nm = smartir_name(k)
                if nm is None: continue
                if ctl == "Broadlink": raw = broadlink_to_raw(v)
                elif ctl == "ESPHome": raw = " ".join(str(abs(int(x))) for x in re.findall(r"-?\d+", v))
                else: raw = None
                if not raw: continue
                r = decode_raw(raw, 38000)
                if r and 0 <= r[2] <= 255: per[(r[0], r[1])].setdefault(FID[nm], r[2])
        walk(j.get("commands", {}))
        for key, m in per.items():
            sets.append((brand, str(models[0]), key[0], key[1], m)); n_smartir += 1
print("SmartIR sets", n_smartir, file=sys.stderr)

# ---------------- Flipper firmware universal remotes: many TVs in one file, grouped by address ----------------
n_uni = 0
for d in EXTRA:
    for f in glob.glob(os.path.join(d, "**", "*.ir"), recursive=True):
        base = os.path.basename(f).lower()
        if base not in ("tv.ir", "projector.ir", "monitor.ir"): continue
        per = defaultdict(dict); bad = set()
        for key, m in read_ir_signals(f):
            for fid, cmd in m.items():
                if per[key].get(fid, cmd) != cmd: bad.add(key)
                per[key].setdefault(fid, cmd)
        for key, m in per.items():
            if key in bad: continue
            sets.append(("Universal TV", "flipper " + base, key[0], key[1], m)); n_uni += 1
print("universal remote sets", n_uni, file=sys.stderr)

# keep useful sets: at least 3 known functions, one of them a "you can see it" key
VISIBLE = {FID[x] for x in ("POWER", "MUTE", "VOL+", "VOL-", "CH+", "CH-", "MENU", "INPUT")}
# the same remote found in two collections counts once
_seen = set(); _u = []
for s_ in sets:
    k_ = (s_[0].lower().replace("_", " "), s_[2], s_[3], tuple(sorted(s_[4].items())))
    if k_ in _seen: continue
    _seen.add(k_); _u.append(s_)
print("duplicates dropped", len(sets) - len(_u), file=sys.stderr)
sets = _u
full = [s for s in sets if len(s[4]) >= 3 and VISIBLE & set(s[4])]
covered = {(s[2], s[3]) for s in full}
# a TV address nothing else knows: keep even 1-2 buttons (Power, Volume...) so Smart match can still spot it
small = [s for s in sets if len(s[4]) < 3 and VISIBLE & set(s[4]) and (s[2], s[3]) not in covered]
sets = full + small
print("small sets kept for new addresses", len(small), file=sys.stderr)

# merge sets with the same protocol+address that never disagree
groups = defaultdict(list)
for s in sets: groups[(s[2], s[3])].append(s)
merged = []
for key, lst in groups.items():
    lst.sort(key=lambda s: -len(s[4]))
    clusters = []   # [brand counter, codes dict, count]
    for brand, model, pr, ad, m in lst:
        for cl in clusters:
            if all(cl[1].get(fid, cmd) == cmd for fid, cmd in m.items()):
                cl[0][brand] += 1; cl[1].update(m); cl[2] += 1; break
        else:
            clusters.append([Counter({brand: 1}), dict(m), 1])
    for b, m, n in clusters:
        merged.append((b, key[0], key[1], m, n))

merged.sort(key=lambda s: (-s[4], -len(s[3])))

def cstr(s): return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

codes = []
out = []
for brands, pr, ad, m, n in merged:
    # most common brand first; merge case variants ("SONY" / "Sony")
    bc = Counter()
    for b, c in brands.items(): bc[b.strip()] += c
    seenl = {}; order = []
    for b, c in bc.most_common():
        if b.lower().startswith("unknown") or b.lower() in seenl: continue
        seenl[b.lower()] = 1; order.append(b)
    # CRT brands from the JP1 research go straight after the most common one, so they aren't cut off
    if len(order) > 1:
        jp = [b for b in order[1:] if b.lower() in _jp1_brands]
        order = [order[0]] + jp + [b for b in order[1:] if b not in jp]
    label = ", ".join(order) or "Unknown brand"
    if len(label) > 60: label = label[:57].rsplit(",", 1)[0] + "..."
    off = len(codes)
    for fid in sorted(m): codes.append((fid, m[fid]))
    out.append((label, pr, ad, len(m), off, n))

assert len(codes) < 65536
with open(OUT, "w") as f:
    f.write("/* Generated by tools/gen_presets.py from IRDB, Flipper-IRDB, LIRC, SmartIR and Flipper universal remotes (TV code sets). Do not edit. */\n")
    f.write('#include "presets.h"\n\n')
    f.write("const PCode BUILTIN_CODES[%d] = {\n" % len(codes))
    for i in range(0, len(codes), 12):
        f.write("\t" + " ".join("{%d,%d}," % c for c in codes[i:i + 12]) + "\n")
    f.write("};\n\n")
    f.write("const PSet BUILTIN_DB[%d] = {\n" % len(out))
    for label, pr, ad, cnt, off, n in out:
        f.write("\t{ %s, %d, %d, %d, %d, %d },\n" % (cstr(label), pr, ad, cnt, off, min(n, 255)))
    f.write("};\n")
    f.write("const int BUILTIN_N = %d;\nconst int BUILTIN_CODES_N = %d;\n" % (len(out), len(codes)))
print("remotes read", len(sets), "presets", len(out), "codes", len(codes), file=sys.stderr)
print(Counter(o[1] for o in out), file=sys.stderr)
