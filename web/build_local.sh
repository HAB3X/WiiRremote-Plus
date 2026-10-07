#!/bin/sh
# rebuild the PC preview page from ../source
set -e
# WASI_SYSROOT: a wasi-libc sysroot (e.g. from wasi-sdk)
SYSROOT="${WASI_SYSROOT:-/opt/wasi-sdk/share/wasi-sysroot}"
cd "$(dirname "$0")"
for f in ../source/*.c; do b=$(basename $f .c); [ $b = main ] || [ $b = plat_wii ] && continue; clang --target=wasm32-wasi --sysroot="$SYSROOT" -DHOST -DWEB -O2 -I../source -c -Wall -Wno-format-truncation -Wno-unused-function $f -o $b.o; done
clang --target=wasm32-wasi --sysroot="$SYSROOT" -DHOST -DWEB -O2 -I../source -c -Wall -Wno-format-truncation -Wno-unused-function plat_web.c -o plat_web.o
clang --target=wasm32-wasi --sysroot="$SYSROOT" -O2 -mexec-model=reactor -Wl,--allow-undefined -Wl,-z,stack-size=1048576 -Wl,--initial-memory=67108864 -Wl,--strip-all -o app.wasm *.o
gzip -9 -c app.wasm | base64 -w0 > app.wasm.gz.b64
python3 - <<'P'
import json
s=open('template.html').read()
a=s.index('        <div class="overlay" id="start">'); b=s.index('        <div class="overlay" id="quit"')
s=s[:a]+s[b:]
s=s.replace('document.getElementById("go").addEventListener("click", start);','start();')
s=s.replace('  document.getElementById("start").hidden = true; audio();\n  try { await boot(); } catch (e) { st.textContent = "Could not start: " + e.message; document.getElementById("start").hidden = false; return; }','  try { await boot(); } catch (e) { st.textContent = "Could not start: " + e.message; return; }')
s=s.replace('@@WASM@@',open('app.wasm.gz.b64').read().strip())
s=s.replace('@@SEED@@',json.dumps({"data/remotes/remote001.txt":open('seed_remote001.txt').read()}))
open('wiirremote_pc.html','w').write(s)
P
echo built
