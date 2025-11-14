#!/usr/bin/env bash
set -euo pipefail

# Simple runner for ExternDataCallSoRelative

CC=${CC:-clang}
LINK=${LINK:-/local/mnt/workspace/bpatidar/inst/bin/ld.eld}
DYN=${DYN:-/lib64/ld-linux-x86-64.so.2}

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

echo "[build] shared object defs/use"
$CC -fPIC -c Inputs/so_rel_defs.c -o so_rel_defs.o
$CC -fPIC -c Inputs/so_rel_use.c -o so_rel_use.o

echo "[link] build shared object"
$LINK --as-needed -shared -o librel.so so_rel_defs.o so_rel_use.o

echo "[build] executable using shared object"
$CC -fPIC -c Inputs/so_rel_main.c -o so_rel_main.o

echo "[link] link executable"
$LINK -dynamic-linker "$DYN" --as-needed -o so_rel.out so_rel_main.o librel.so

echo "[run] executable"
./so_rel.out || true
echo "exit=$?"

