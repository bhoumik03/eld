#!/usr/bin/env bash
set -euo pipefail

# Simple runner for ExternDataCallPieRelative
# Uses clang for compilation and ld.eld for linking.

CC=${CC:-clang}
LINK=${LINK:-/local/mnt/workspace/bpatidar/inst/bin/ld.eld}
DYN=${DYN:-/lib64/ld-linux-x86-64.so.2}

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

echo "[build] PIE non-preemptible data defs/use"
$CC -fPIC -c Inputs/pie_rel_defs.c -o pie_defs.o
$CC -fPIC -c Inputs/pie_rel_use.c -o pie_use.o

echo "[link] PIE executable via ld.eld"
$LINK -pie -dynamic-linker "$DYN" --as-needed -o pie.out pie_defs.o pie_use.o

echo "[run] PIE executable"
./pie.out || true
echo "exit=$?"

echo "[link] Non-PIE executable via ld.eld"
$LINK -dynamic-linker "$DYN" --as-needed -o nonpie.out pie_defs.o pie_use.o

echo "[run] Non-PIE executable"
./nonpie.out || true
echo "exit=$?"

