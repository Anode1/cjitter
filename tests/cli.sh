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
      "$(printf '%s' "$out" | grep -cE '^(climb|anneal|ga) .*(better|not shown)$')" "3"
check "labels explains what better means" \
      "$(printf '%s' "$out" | grep -c 'exact one-sided sign')" "1"

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

# The block option: a real one runs and says so, and a junk one is refused rather than read as
# zero and passed off as the default, which is the trap atol sets for every argument here.
set +e
out=$("$labels" $NL $NE $NS 2); rc=$?
set -e
check "labels takes a block"        "$rc" "0"
check "and says what it moved"      "$(printf '%s' "$out" | grep -c 'One proposal moves 2 of the 24 variables, so a label at a time')" "1"
check "the default block is the whole vector" \
      "$(printf '%s' "$("$labels" $NL $NE $NS)" | grep -c 'One proposal moves 24 of the 24 variables (the whole vector)')" "1"
set +e
out=$("$labels" $NL $NE $NS junk 2>&1 >/dev/null); rc=$?
set -e
check "a junk block is refused, not read as the default" \
      "$(firstline "$out")" "labels: need at least one variable per block"
check "junk block exit"             "$rc" "2"

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
check "erd scores the centroid heuristic in both styles" \
      "$(printf '%s' "$out" | grep -c '^centroid ')" "2"
check "erd scores the human's layout in both styles" \
      "$(printf '%s' "$out" | grep -c '^human ')" "2"
# The router's calibration against the one certain fact: the hand layout achieved 0 crossings
# and 0 penetration. The printed shortfall is the router's floor and every score's caveat.
check "erd calibrates both edge models against the hand layout" \
      "$(printf '%s' "$out" | grep -c 'under this edge model (the hand')" "2"
check "the straight model's calibration is pinned" \
      "$(printf '%s' "$out" | grep '1943.96 penetration')" \
      "           20 crossings, 1943.96 penetration under this edge model (the hand"
check "the routed model's calibration is pinned" \
      "$(printf '%s' "$out" | grep '323 penetration')" \
      "           27 crossings, 323 penetration under this edge model (the hand"
check "both returned layouts report their feasibility" \
      "$(printf '%s' "$out" | grep -c '^under this edge model:')" "2"
check "the routed layout's feasibility is pinned" \
      "$(printf '%s' "$out" | grep '65 crossings')" \
      "under this edge model: 65 crossings, 0.40782 penetration"
check "erd prints both table headers" "$(printf '%s' "$out" | grep -c 'vs random')" "2"
check "erd reports all four methods twice" \
      "$(printf '%s' "$out" | grep -cE '^(random|climb|anneal|ga) ')" "8"
check "erd prints a seed-1 layout per style" \
      "$(printf '%s' "$out" | grep -c 'the layout climb found at seed 1')" "2"
check "erd places every migration table, both styles" \
      "$(printf '%s' "$out" | grep -c ' at (')" "20"
# The repair's canvas bound, checked on what the run actually returned. The overlap push-out
# once shoved tables past the clamp, and an infeasible layout was returned as best. The sign
# is part of the pattern deliberately: an earlier version matched only [0-9]*, so the exact
# symptom this check exists for, a negative coordinate, fell out of the pipeline unseen.
check "every placed table is on the canvas" \
      "$(printf '%s' "$out" | sed -n 's/.*at (\(-\{0,1\}[0-9][0-9]*\), \(-\{0,1\}[0-9][0-9]*\)).*/\1 \2/p' | \
         awk '$1<0||$1>2949||$2<0||$2>1966{bad++} END{print bad+0}')" "0"
check "and the pattern loses none of the twenty lines" \
      "$(printf '%s\n' "$out" | sed -n 's/.*at (\(-\{0,1\}[0-9][0-9]*\), \(-\{0,1\}[0-9][0-9]*\)).*/\1/p' | wc -l | tr -d ' ')" "20"

# The oracle: the exact layout the example ships. The rerun check above only catches
# nondeterminism; this catches a code change that silently moves the answer. If a change moves
# these on purpose -- objective, search, repair -- re-measure, update the pins AND the README,
# which quotes the same scores.
check "the reference scores are the shipped ones, both styles" \
      "$(printf '%s' "$out" | grep -cE '^(centroid *(226609|194470)|human *(218207|157552)) ')" "4"
check "the straight layout's score is pinned" \
      "$(printf '%s' "$out" | grep 'score 116506')" \
      "the layout climb found at seed 1, score 116506:"
check "the routed layout's score is pinned" \
      "$(printf '%s' "$out" | grep 'score 53061.4')" \
      "the layout climb found at seed 1, score 53061.4:"
check "and all twenty placements are pinned" \
      "$(printf '%s' "$out" | grep ' at (' | digest)" \
      "$(printf '%s\n' "  AI at (162, 275)" "  AM at (2835, 1077)" "  AP at (1234, 1130)" \
         "  D at (835, 1799)" "  E at (322, 60)" "  J at (1476, 494)" "  P at (2386, 303)" \
         "  Q at (1589, 1056)" "  R at (2355, 1473)" "  Y at (79, 420)" \
         "  AI at (1628, 1776)" "  AM at (1346, 915)" "  AP at (2606, 1818)" \
         "  D at (1949, 1385)" "  E at (85, 1901)" "  J at (1842, 1745)" "  P at (2493, 755)" \
         "  Q at (2792, 1850)" "  R at (1504, 1531)" "  Y at (1026, 1484)" | digest)"

out2=$("$erd")
"$erd" > "$tmp/e1"
"$erd" > "$tmp/e2"
check "erd twice is byte-identical" "$(digest < "$tmp/e1")" "$(digest < "$tmp/e2")"

# --svg: the same computation drawn instead of reported, so it must carry the same numbers as
# the pinned report: all three panel scores, and one rect per table in each of the three
# panels (3x44) plus the page and three canvases.
set +e
"$erd" --svg > "$tmp/e.svg" 2>/dev/null; rc=$?
set -e
check "erd --svg exits 0"            "$rc" "0"
check "and emits an svg"             "$(head -c 4 "$tmp/e.svg")" "<svg"
check "with every table drawn four times" "$(grep -c '<rect' "$tmp/e.svg")" "181"
check "and every connector routed, four panels" "$(grep -c '<polyline' "$tmp/e.svg")" "236"
check "carrying the pinned centroid score" "$(grep -c '194470' "$tmp/e.svg")" "1"
check "the pinned search score"      "$(grep -c '53061.4' "$tmp/e.svg")" "1"
check "and the pinned human score"   "$(grep -c '157552' "$tmp/e.svg")" "1"
set +e
"$erd" --svg-straight > "$tmp/es.svg" 2>/dev/null; rc=$?
set -e
check "erd --svg-straight exits 0"   "$rc" "0"
check "and draws the same four panels" "$(grep -c '<rect' "$tmp/es.svg")" "181"
check "with straight connectors this time" "$(grep -c '<polyline' "$tmp/es.svg")" "236"
set +e
out=$("$erd" junk 2>&1 >/dev/null); rc=$?
set -e
check "an unknown erd argument is refused" "$(firstline "$out")" "erd: options are --svg, --svg-straight and --block N"
check "unknown erd argument exit"    "$rc" "2"

# erd's block option. The svg path is used for the positive case: it runs one search instead of
# the whole five-seed comparison, so the suite gains a second rather than half a minute.
check "erd's default block is the whole vector" \
      "$(printf '%s' "$out2" | grep -c 'One proposal moves 20 of the 20 variables (the whole vector)')" "1"
set +e
"$erd" --block 2 --svg > "$tmp/eb.svg" 2>/dev/null; rc=$?
set -e
check "erd --block 2 --svg exits 0" "$rc" "0"
check "and still draws four panels" "$(grep -c '<rect' "$tmp/eb.svg")" "181"
set +e
out=$("$erd" --block 0 2>&1 >/dev/null); rc=$?
set -e
check "a zero block is refused"     "$(firstline "$out")" "erd: --block needs at least one variable"
check "zero block exit"             "$rc" "2"
set +e
out=$("$erd" --block junk 2>&1 >/dev/null); rc=$?
set -e
check "a junk block is refused too" "$(firstline "$out")" "erd: --block needs at least one variable"

# ------------------------------------------------------------------ verdict

echo "cli: $pass passed, $fail failed"
[ $fail -eq 0 ]
