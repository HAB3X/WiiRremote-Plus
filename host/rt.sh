#!/bin/sh
# rebuild changed sources and run a host test: sh rt.sh t_cal2
cd "$(dirname "$0")"
for f in source/*.c; do
	b=$(basename $f .c)
	[ $b = main ] && continue
	[ $b = plat_wii ] && continue
	o=build_host/$b.o
	if [ ! -f $o ] || [ $f -nt $o ] || [ source/app.h -nt $o ] || [ source/calkb.h -nt $o ] || [ source/remote.h -nt $o ] || [ source/presets.h -nt $o ]; then
		gcc -DHOST -g -O1 -fsanitize=address,undefined -Isource -Wall -Wno-format-truncation -Wno-unused-function -Wno-misleading-indentation -Wno-restrict -Wno-unused-variable -c $f -o $o 2>&1 | grep -E "error|warning" | head -20
	fi
done
gcc -DHOST -g -O1 -fsanitize=address,undefined -Isource host/$1.c build_host/*.o -lm -o bin_$1 || exit 1
mkdir -p shots4
ASAN_OPTIONS=detect_leaks=0 timeout 170 ./bin_$1 $2 $3
python3 - <<'P'
import glob, os
from PIL import Image
for f in glob.glob('shots4/*.ppm'):
    p = f[:-4] + '.png'
    if not os.path.exists(p) or os.path.getmtime(p) < os.path.getmtime(f):
        Image.open(f).save(p)
P
