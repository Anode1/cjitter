#!/bin/sh
# diagrams.sh: black-box tests of station, the diagram stationarity instrument, through a shell.
#
# The fixture's four layouts are hand-computable (example/diagrams/data/fixture.txt says how),
# so the term values pinned here were worked out independently of the code, and the directional
# test's verdicts on them follow from the geometry.
#
#   make cliut          # or: sh tests/diagrams.sh
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
station=${STATION_BIN:-$root/station}
fixture=$root/example/diagrams/data/fixture.txt
[ -x "$station" ] || { echo "build first: make" >&2; exit 1; }

tmp=$(mktemp -d 2>/dev/null || mktemp -d -t cjitter)
trap 'rm -rf "$tmp"' EXIT

pass=0; fail=0
ok()   { pass=$((pass+1)); }
no()   { fail=$((fail+1)); echo "  FAIL $1"; }
check(){ if [ "$2" = "$3" ]; then ok; else no "$1: expected [$3], got [$2]"; fi; }
firstline(){ printf '%s' "$1" | head -1; }
row(){ printf '%s\n' "$1" | grep "^$2,"; }
digest() { if command -v md5sum >/dev/null 2>&1; then md5sum; \
           elif command -v md5 >/dev/null 2>&1; then md5; \
           else cksum; fi; }

# ------------------------------------------------------------------- the terms

out=$("$station" terms --corpus "$fixture" --align a1 --L median)
check "terms prints the header" "$(firstline "$out")" \
      "id,n,m,L,Ls,crossings,overlap,length,stress,orthogonality,alignment,node-edge"
# square: one crossing in six edges; both diagonals at 45 degrees; sides 1, diagonals sqrt 2,
# median 1, so length = 2 (sqrt2 - 1)^2 / 6 and stress the same (the diagonals are edges).
check "square" "$(row "$out" square)" \
      "square,4,6,1.000000,1.000000,0.166667,0.000000,0.057191,0.057191,0.166667,0.000000,0.000000"
# row: every term is zero at three equally spaced nodes on a line.
check "row" "$(row "$out" row)" \
      "row,3,2,0.500000,0.500000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000"
# overlap: boxes of side 0.2 at distance 0.05 overlap 0.15 x 0.2 = 0.03 of area 0.12; the far
# node's two edges run at 45 degrees and at atan(1/0.95); node 1 is 0.05/sqrt2 from edge 0-2
# and node 0 is 0.05 from edge 1-2, both within r = 0.1.
check "overlap" "$(row "$out" overlap)" \
      "overlap,3,2,1.414214,1.414214,0.000000,0.250000,0.000305,0.321855,0.493590,0.316667,0.333947"
# The fitted reference length, the default: L = sum(l^2) / sum(l). square: 8 / (4 + 2 sqrt2) =
# 1.171573 for both terms (every pair is an edge), length = (4 (1/L - 1)^2 + 2 (sqrt2/L - 1)^2)
# / 6 = 0.028595. overlap: edges sqrt2 and sqrt(0.95^2 + 1) = 1.379311, L = 1.396981; the pair
# at two hops has r/d = 0.025, so Ls = (2 + 1.902500 + 0.000625) / (1.414214 + 1.379311 +
# 0.025) = 1.384811.
out=$("$station" terms --corpus "$fixture" --align a1)
check "fitted L on the square" "$(row "$out" square | cut -d, -f4-5,8-9)" "1.171573,1.171573,0.028595,0.028595"
check "fitted L and Ls on overlap" "$(row "$out" overlap | cut -d, -f4-5)" "1.396981,1.384811"
check "the row is uniform, so every reference length agrees" "$(row "$out" row | cut -d, -f4-5)" "0.500000,0.500000"
check "cycle: L at two hops, and the stress worked out in the fixture" \
      "$(row "$out" cycle | cut -d, -f4-5,9)" "1.000000,0.923495,0.022876"
check "rsqrt: 1 / sqrt(4)" \
      "$("$station" terms --corpus "$fixture" --L rsqrt | grep '^square,' | cut -d, -f4-5)" "0.500000,0.500000"

out=$("$station" terms --corpus "$fixture" --align grid)
check "gridiness: the square's corners align only in pairs" \
      "$(row "$out" square | cut -d, -f11)" "1.000000"
check "gridiness: the row is one alignment of three" \
      "$(row "$out" row | cut -d, -f11)" "0.000000"
out=$("$station" terms --corpus "$fixture" --align a3)
check "A3: each node of the row sees two others in its row, 1/(1+2)" \
      "$(row "$out" row | cut -d, -f11)" "0.333333"
out=$("$station" terms --corpus "$fixture" --align a2)
# A2: the near pair are 0.05 from a column and on one row, 0.025 each; the far node is 0.95
# from a column and 1 from a row, 0.975; over three nodes.
check "A2 on overlap" "$(row "$out" overlap | cut -d, -f11)" "0.341667"

# ------------------------------------------------------------------- the directional test

out=$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0)
check "direct prints the header" "$(firstline "$out")" \
      "id,n,m,E,q,dec,crossings,overlap,length,stress,orthogonality,alignment,node-edge"
check "crossings+overlap: a 2% move cannot uncross the square's diagonals, so it is held" \
      "$(row "$out" square | cut -d, -f5)" "1.000000"
check "crossings+overlap: the row has nothing to lower, ties hold" \
      "$(row "$out" row | cut -d, -f5)" "1.000000"
check "overlap: the two overlapping boxes can separate, the far node is held" \
      "$(row "$out" overlap | cut -d, -f5)" "0.333333"
check "the energy at the layout is the weighted sum" "$(row "$out" overlap | cut -d, -f4)" "0.250000"
out=$("$station" direct --corpus "$fixture" --weights 0,0,1,0,0,0,0)
check "length: the row is exactly uniform, every move lengthens or shortens an edge" \
      "$(row "$out" row | cut -d, -f5)" "1.000000"
# The cycle is a stress minimum at its own scale, so the fitted L holds every corner at any
# radius, and the median L, which wants the diagonals longer, holds none.
for d in 0.005 0.02 0.2; do
    check "stress, fitted L: the cycle is held at d = $d" \
          "$("$station" direct --corpus "$fixture" --weights 0,0,0,1,0,0,0 --d $d | grep '^cycle,' | cut -d, -f5)" "1.000000"
done
check "stress, median L: no corner of the cycle is held" \
      "$("$station" direct --corpus "$fixture" --weights 0,0,0,1,0,0,0 --L median | grep '^cycle,' | cut -d, -f5)" "0.000000"
out=$("$station" direct --corpus "$fixture" --weights 0,0,0,0,0,1,0 --align a1)
# A1 on overlap: the far node can move toward the near pair's column, and the near node at
# x = 0.05 can move toward the far node's column, so two of three are free; each best move
# lowers the far node's 0.95 by 0.02, and the sum over nodes, over E = 0.316667, is 0.014.
check "alignment A1: two of the overlap nodes can improve an alignment" \
      "$(row "$out" overlap | cut -d, -f5)" "0.333333"
check "and the decrease is reported over E" \
      "$(row "$out" overlap | cut -d, -f6 | cut -c1-5)" "0.014"
out=$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0 --nodes)
check "--nodes prints one row per node" "$(printf '%s\n' "$out" | grep -c '^[a-z]*,[0-9]')" "14"
check "a held node reports direction -1" "$(printf '%s\n' "$out" | grep -c ',-1,0.000000000$')" "12"

# ------------------------------------------------------------------- check: a control is the same graphs

check "a corpus is the same graphs as itself" "$("$station" check --corpus "$fixture" --against "$fixture")" "same 4 graphs"
sed 's/^V 0.05 0 0.2 0.2$/V 0.05 0 0.2 0.3/' "$fixture" > "$tmp/box.txt"
sed 's/^E 1 3$/E 0 3/' "$fixture" > "$tmp/edge.txt"
sed -n '1,/^E 1 3/p' "$fixture" > "$tmp/short.txt"
chk() {   # name, expected message, against
    set +e
    out=$("$station" check --corpus "$fixture" --against "$3" 2>&1 >/dev/null); rc=$?
    set -e
    check "$1" "$(firstline "$out")" "$2"
    check "$1 exits 2" "$rc" "2"
}
chk "a box size that differs is named, in raw units" "station: graph 2: overlap: node 1 is 0.2 x 0.2 against 0.2 x 0.3" "$tmp/box.txt"
chk "an edge that differs is named" "station: graph 0: square: edge 5 is 1-3 against 0-3" "$tmp/edge.txt"
chk "a shorter corpus is refused" "station: $fixture has 4 graphs, $tmp/short.txt has 1" "$tmp/short.txt"

# ------------------------------------------------------------------- reproducibility

a=$("$station" direct --corpus "$fixture" --weights 1,1,1,1,1,1,1 --align a3 | digest)
b=$("$station" direct --corpus "$fixture" --weights 1,1,1,1,1,1,1 --align a3 | digest)
check "two runs are byte-identical" "$a" "$b"
# The same graphs in the other order give the same rows: nothing leaks between graphs.
{ sed -n '/^G row/,/^E 1 2/p' "$fixture"; sed -n '/^G square/,/^E 1 3/p' "$fixture"; } > "$tmp/rev.txt"
a=$("$station" direct --corpus "$fixture" --weights 1,1,1,1,1,1,1 | grep -E '^(square|row),' | sort)
b=$("$station" direct --corpus "$tmp/rev.txt" --weights 1,1,1,1,1,1,1 | grep -E '^(square|row),' | sort)
check "order independence" "$a" "$b"
check "four directions and sixteen agree where every move is a tie" \
      "$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0 --dirs 4 | grep '^row,' | cut -d, -f5,6)" \
      "$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0 --dirs 16 | grep '^row,' | cut -d, -f5,6)"

# ------------------------------------------------------------------- refusals

set +e
out=$("$station" 2>&1); rc=$?
set -e
check "no arguments is a usage message" "$(firstline "$out" | cut -c1-14)" "usage: station"
check "and exit 2" "$rc" "2"
refuse() {   # name, expected first line, args...
    name=$1; want=$2; shift 2
    set +e
    out=$("$station" "$@" 2>&1 >/dev/null); rc=$?
    set -e
    check "$name" "$(firstline "$out")" "$want"
    check "$name exits 2" "$rc" "2"
}
refuse "direct needs weights" "station: direct needs --weights" direct --corpus "$fixture"
refuse "six weights are refused" \
       "station: --weights needs seven non-negative numbers C,O,L,S,R,A,N, not '1,1,1,1,1,1'" \
       direct --corpus "$fixture" --weights 1,1,1,1,1,1
refuse "a negative weight is refused" \
       "station: --weights needs seven non-negative numbers C,O,L,S,R,A,N, not '1,-1,1,1,1,1,1'" \
       direct --corpus "$fixture" --weights 1,-1,1,1,1,1,1
refuse "an unknown alignment is refused" "station: --align must be a1, a2, a3 or grid, not 'a4'" \
       terms --corpus "$fixture" --align a4
refuse "an unknown reference length is refused" "station: --L must be fit, median or rsqrt, not 'mean'" \
       terms --corpus "$fixture" --L mean
refuse "check needs --against" "station: check needs --against" check --corpus "$fixture"
refuse "a zero radius is refused" "station: --d must be a number in [1e-09, 1], not '0'" \
       direct --corpus "$fixture" --weights 1,1,1,1,1,1,1 --d 0
refuse "a missing corpus is refused" "station: cannot open $tmp/none.txt" terms --corpus "$tmp/none.txt"
bad() {   # name, expected message tail, corpus text
    printf '%s\n' "$3" > "$tmp/bad.txt"
    refuse "$1" "station: $tmp/bad.txt: $2" terms --corpus "$tmp/bad.txt"
}
bad "one node is refused"        "line 1, graph one: need at least two nodes"  "G one 1 1
V 0 0 1 1
E 0 0"
bad "no edges is refused"        "line 1, graph none: need at least one edge"  "G none 2 0
V 0 0 1 1
V 1 1 1 1"
bad "a self loop is refused"     "line 4, graph loop: bad edge endpoint"       "G loop 2 1
V 0 0 1 1
V 1 1 1 1
E 1 1"
bad "an endpoint out of range"   "line 4, graph range: bad edge endpoint"      "G range 2 1
V 0 0 1 1
V 1 1 1 1
E 0 2"
bad "a zero extent is refused"   "line 4, graph flat: zero extent"             "G flat 2 1
V 3 3 1 1
V 3 3 1 1
E 0 1"
bad "a disconnected graph"       "line 5, graph parts: not connected"          "G parts 3 1
V 0 0 1 1
V 1 1 1 1
V 2 0 1 1
E 0 1"
bad "a malformed node line"      "line 2, graph junk: expected 'V x y w h'"    "G junk 2 1
V 0 0 1
V 1 1 1 1
E 0 1"
bad "a negative box"             "line 2, graph neg: negative box size"        "G neg 2 1
V 0 0 -1 1
V 1 1 1 1
E 0 1"

# ------------------------------------------------------------------- the corpora
# Every tool control is the same graphs as its hand corpus, and the first twenty graphs of
# each corpus give the q they gave when the table in the README was made: the corpora, the
# energy and the directions are pinned together, so a change in any shows here before it
# shows in the table. The alignment rows are the sensitive ones: a direction vector off by
# 1e-6, which a Taylor series at an angle near 2 pi gives, lowers them by up to 0.1.
data=$root/example/diagrams/data
for c in hs sbgn bpmn; do
    for tool in neato prism dot; do
        check "$c $tool is the same graphs" "$("$station" check --corpus "$data/$c.txt" --against "$data/${c}_$tool.txt")" \
              "same $(grep -c '^G ' "$data/$c.txt") graphs"
    done
    awk '/^G /{n++} n<=20' "$data/$c.txt" > "$tmp/$c.txt"
    awk '/^G /{n++} n<=20' "$data/${c}_neato.txt" > "$tmp/${c}_neato.txt"
done
qcol() { "$station" direct "$@" | tail -n +2 | cut -d, -f5 | cut -c1-4 | tr '\n' ' '; }
check "WikiPathways, first twenty, C+O+L" "$(qcol --corpus "$tmp/hs.txt" --weights 1,1,1,0,0,0,0)" \
      "0.00 0.00 0.00 0.00 0.00 0.00 0.21 0.05 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 "
check "WikiPathways, first twenty, alignment A1" "$(qcol --corpus "$tmp/hs.txt" --weights 0,0,0,0,0,1,0 --align a1)" \
      "0.50 0.88 0.50 0.55 0.70 0.86 1.00 0.33 1.00 0.51 0.23 0.70 0.77 0.68 0.73 0.59 0.62 0.29 0.62 0.30 "
check "BPMN, first twenty, alignment A1" "$(qcol --corpus "$tmp/bpmn.txt" --weights 0,0,0,0,0,1,0 --align a1)" \
      "1.00 0.51 0.84 0.73 0.96 0.70 1.00 0.50 0.55 0.65 1.00 1.00 0.65 0.42 0.73 1.00 0.87 0.47 0.76 0.38 "
check "Reactome, first twenty, stress: the hand layout" "$(qcol --corpus "$tmp/sbgn.txt" --weights 0,0,0,1,0,0,0)" \
      "0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.03 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 "
check "Reactome, first twenty, stress: neato's layout, the power check" "$(qcol --corpus "$tmp/sbgn_neato.txt" --weights 0,0,0,1,0,0,0)" \
      "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 0.90 0.96 1.00 1.00 0.96 "
check "BPMN, first twenty, stress: neato's layout" "$(qcol --corpus "$tmp/bpmn_neato.txt" --weights 0,0,0,1,0,0,0)" \
      "0.95 1.00 0.97 0.93 1.00 1.00 1.00 1.00 1.00 1.00 1.00 0.95 1.00 1.00 1.00 0.94 0.93 1.00 1.00 1.00 "

# ------------------------------------------------------------------- the energy's arithmetic
# The terms promise + - * / fabs sqrt and a polynomial exp, so a value reproduces across
# platforms. nm on the object says what libm the energy actually calls.
if command -v nm >/dev/null 2>&1; then
    ${CC:-cc} -std=c99 -ffp-contract=off -O2 -c "$root/example/diagrams/energy.c" -o "$tmp/energy.o"
    check "energy.c calls no transcendental libm" \
          "$(nm "$tmp/energy.o" | grep -cE ' U _?(exp|exp2|log|pow|cos|sin|tan|atan2?)$')" "0"
fi

echo "diagrams: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
