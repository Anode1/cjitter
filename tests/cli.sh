#!/bin/sh
# cli.sh: black-box tests: the built examples, driven through a shell, the way a user meets them.
#
# make ut calls functions; exit codes, refusal messages and the byte-for-byte determinism of a
# whole run are only testable from here.
#
#   make cliut          # or: sh tests/cli.sh
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
# LABELS_BIN= and ERD_BIN= point the same checks at other builds, which is how they run under
# the sanitizers.
labels=${LABELS_BIN:-$root/labels}
erd=${ERD_BIN:-$root/erd}
[ -x "$labels" ] && [ -x "$erd" ] || { echo "build first: make" >&2; exit 1; }

tmp=$(mktemp -d 2>/dev/null || mktemp -d -t cjitter)
trap 'rm -rf "$tmp"' EXIT

pass=0; fail=0
ok()   { pass=$((pass+1)); }
no()   { fail=$((fail+1)); echo "  FAIL $1"; }
check(){ if [ "$2" = "$3" ]; then ok; else no "$1: expected [$3], got [$2]"; fi; }
firstline(){ printf '%s' "$1" | head -1; }
# BSD and GNU disagree about the name of every checksum tool. Both are used only to compare two
# outputs for equality, so any stable digest will do.
digest() { if command -v md5sum >/dev/null 2>&1; then md5sum; \
           elif command -v md5 >/dev/null 2>&1; then md5; \
           else cksum; fi; }

# Small runs: this suite tests messages, exit codes and reproducibility, not search quality.
NL=12; NE=800; NS=3

# ------------------------------------------------------------------- labels

set +e
out=$("$labels" $NL $NE $NS); rc=$?
set -e
check "labels runs"                    "$rc" "0"
check "labels states the problem"      "$(printf '%s' "$out" | grep -c 'of the area is label')" "1"
check "labels prints the table header" "$(printf '%s' "$out" | grep -c 'vs random')" "1"
check "labels names the control"       "$(printf '%s' "$out" | grep -c 'the control$')" "1"
check "labels reports all four methods" \
      "$(printf '%s' "$out" | grep -cE '^(random|climb|anneal|ga) ')" "4"
check "every verdict is one the interface defines" \
      "$(printf '%s' "$out" | grep -cE '^(climb|anneal|ga) .*(better|inside noise|no better)$')" "3"
check "labels explains what better means" \
      "$(printf '%s' "$out" | grep -c 'luckiest seed')" "1"

# The refusals. Each of these is a message and an exit 2, not a run over a junk value: atol
# reads "ninety" as 0, and 0 labels must refuse, not place nothing and report success.
set +e
out=$("$labels" 1 2>&1 >/dev/null); rc=$?
set -e
check "one label is refused"        "$(firstline "$out")" "labels: need at least two labels"
check "and exits 2, not 0"          "$rc" "2"
set +e
out=$("$labels" ninety 2>&1 >/dev/null); rc=$?
set -e
check "a junk count is refused, not read as zero" \
      "$(firstline "$out")" "labels: need at least two labels"
check "junk count exit"             "$rc" "2"
set +e
out=$("$labels" 12 junk 3 2>&1 >/dev/null); rc=$?
set -e
check "junk evaluations are refused, not a header and exit 0" \
      "$(firstline "$out")" "labels: need at least one evaluation"
check "junk evaluations exit"       "$rc" "2"
set +e
out=$("$labels" 12 800 junk 2>&1 >/dev/null); rc=$?
set -e
check "junk seeds are refused, not silence and exit 0" \
      "$(firstline "$out")" "labels: need at least one seed"
check "junk seeds exit"             "$rc" "2"

# Determinism: the whole run, byte for byte. This is the claim the README makes and the reason
# -ffp-contract=off is in the flags.
"$labels" $NL $NE $NS > "$tmp/l1"
"$labels" $NL $NE $NS > "$tmp/l2"
check "the same run twice is byte-identical" \
      "$(digest < "$tmp/l1")" "$(digest < "$tmp/l2")"

# ---------------------------------------------------------------------- erd

set +e
out=$("$erd"); rc=$?
set -e
check "erd runs"                     "$rc" "0"
check "erd states the problem"       "$(printf '%s' "$out" | grep -c 'added by a migration')" "1"
check "erd scores the centroid heuristic" \
      "$(printf '%s' "$out" | grep -c '^centroid ')" "1"
check "erd prints the table header"  "$(printf '%s' "$out" | grep -c 'vs random')" "1"
check "erd reports all four methods" \
      "$(printf '%s' "$out" | grep -cE '^(random|climb|anneal|ga) ')" "4"
check "erd prints the best layout"   \
      "$(printf '%s' "$out" | grep -c 'best layout found by')" "1"
check "erd places every new table"   \
      "$(printf '%s' "$out" | grep -c '^  new table ')" "3"
# The repair's canvas bound, checked on what the run actually returned: a 130x90 table's centre
# on the 900x600 canvas lies in [65,835] x [45,555]. The overlap push-out once shoved tables
# past the clamp, and an infeasible layout was returned as best.
check "every placed table is wholly on the canvas" \
      "$(printf '%s' "$out" | sed -n 's/.*at (\([0-9]*\), \([0-9]*\)).*/\1 \2/p' | \
         awk '$1<65||$1>835||$2<45||$2>555{bad++} END{print bad+0}')" "0"

"$erd" > "$tmp/e1"
"$erd" > "$tmp/e2"
check "erd twice is byte-identical" "$(digest < "$tmp/e1")" "$(digest < "$tmp/e2")"

# ------------------------------------------------------------------ verdict

echo "cli: $pass passed, $fail failed"
[ $fail -eq 0 ]
