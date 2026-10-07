#!/bin/sh
# host build (ASan) of the app for tests
cd "$(dirname "$0")"
for f in source/*.c; do
	b=$(basename $f .c)
	[ $b = main ] && continue
	[ $b = plat_wii ] && continue
	gcc -DHOST -g -O1 -fsanitize=address,undefined -Isource -Wall -Wno-format-truncation -Wno-unused-function -c $f -o build_host/$b.o 2>&1 | grep -E "error|warning" | head -20
done
gcc -DHOST -g -O1 -fsanitize=address,undefined -Isource -Wall -Wno-format-truncation -c host/plat_host.c -o build_host/plat_host.o 2>&1 | grep -E "error|warning" | head
echo compiled
