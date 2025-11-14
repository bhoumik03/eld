#!/usr/bin/env bash
set -euo pipefail

# Simple runner for ExternDataCallDsoGlobDat

CC=${CC:-clang}
LINK=${LINK:-/local/mnt/workspace/bpatidar/inst/bin/ld.eld}
DYN=${DYN:-/lib64/ld-linux-x86-64.so.2}

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

echo "[build] build libb.so (preemptible dat)"
$CC -fPIC -c Inputs/libb.c -o libb.o
$LINK --as-needed -shared -o libb.so libb.o

echo "[build] build liba.so referencing dat"
$CC -fPIC -c Inputs/liba.c -o liba.o
$LINK --as-needed -shared -o liba.so liba.o libb.so

echo "[build] build main using liba and libb"
$CC -fPIC -c Inputs/lib_main.c -o lib_main.o

echo "[link] link executable"
$LINK -dynamic-linker "$DYN" --as-needed -o globdat.out lib_main.o liba.so libb.so

echo "[run] executable"
./globdat.out || true
echo "exit=$?"

