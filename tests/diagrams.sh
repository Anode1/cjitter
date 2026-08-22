#!/bin/sh
# diagrams.sh: black-box tests of station, the diagram stationarity instrument, through a shell.
#
# The fixture's three layouts are hand-computable (example/diagrams/data/fixture.txt says how),
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

out=$("$station" terms --corpus "$fixture" --align a1)
check "terms prints the header" "$(firstline "$out")" \
      "id,n,m,L,crossings,overlap,length,stress,orthogonality,alignment,node-edge"
# square: one crossing in six edges; both diagonals at 45 degrees; sides 1, diagonals sqrt 2,
# median 1, so length = 2 (sqrt2 - 1)^2 / 6 and stress the same (the diagonals are edges).
check "square" "$(row "$out" square)" \
      "square,4,6,1.000000,0.166667,0.000000,0.057191,0.057191,0.166667,0.000000,0.000000"
# row: every term is zero at three equally spaced nodes on a line.
check "row" "$(row "$out" row)" \
      "row,3,2,0.500000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000"
# overlap: boxes of side 0.2 at distance 0.05 overlap 0.15 x 0.2 = 0.03 of area 0.12; the far
# node's two edges run at 45 degrees and at atan(1/0.95); node 1 is 0.05/sqrt2 from edge 0-2
# and node 0 is 0.05 from edge 1-2, both within r = 0.1.
check "overlap" "$(row "$out" overlap)" \
      "overlap,3,2,1.414214,0.000000,0.250000,0.000305,0.321855,0.493590,0.316667,0.333947"

out=$("$station" terms --corpus "$fixture" --align grid)
check "gridiness: the square's corners align only in pairs" \
      "$(row "$out" square | cut -d, -f10)" "1.000000"
check "gridiness: the row is one alignment of three" \
      "$(row "$out" row | cut -d, -f10)" "0.000000"
out=$("$station" terms --corpus "$fixture" --align a3)
check "A3: each node of the row sees two others in its row, 1/(1+2)" \
      "$(row "$out" row | cut -d, -f10)" "0.333333"
out=$("$station" terms --corpus "$fixture" --align a2)
# A2: the near pair are 0.05 from a column and on one row, 0.025 each; the far node is 0.95
# from a column and 1 from a row, 0.975; over three nodes.
check "A2 on overlap" "$(row "$out" overlap | cut -d, -f10)" "0.341667"

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
out=$("$station" direct --corpus "$fixture" --weights 0,0,0,0,0,1,0 --align a1)
# A1 on overlap: the far node can move toward the near pair's column, and the near node at
# x = 0.05 can move toward the far node's column, so two of three are free; each best move
# lowers the far node's 0.95 by 0.02, and the sum over nodes, over E = 0.316667, is 0.014.
check "alignment A1: two of the overlap nodes can improve an alignment" \
      "$(row "$out" overlap | cut -d, -f5)" "0.333333"
check "and the decrease is reported over E" \
      "$(row "$out" overlap | cut -d, -f6 | cut -c1-5)" "0.014"
out=$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0 --nodes)
check "--nodes prints one row per node" "$(printf '%s\n' "$out" | grep -c '^[a-z]*,[0-9]')" "10"
check "a held node reports direction -1" "$(printf '%s\n' "$out" | grep -c ',-1,0.000000000$')" "8"

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
