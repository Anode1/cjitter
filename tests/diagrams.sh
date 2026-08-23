#!/bin/sh
# diagrams.sh: black-box tests of station, the diagram stationarity instrument, through a shell.
#
# The fixture's five layouts are hand-computable (example/diagrams/data/fixture.txt says how),
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
      "id,n,m,L,Ls,ux,uy,crossings,overlap,length,stress,orthogonality,alignment,node-edge,flow"
# square: one crossing in six edges; both diagonals at 45 degrees; sides 1, diagonals sqrt 2,
# median 1, so length = 2 (sqrt2 - 1)^2 / 6 and stress the same (the diagonals are edges).
check "square" "$(row "$out" square)" \
      "square,4,6,1.000000,1.000000,0,1,0.166667,0.000000,0.057191,0.057191,0.166667,0.000000,0.000000,0.166667"
# row: every term is zero at three equally spaced nodes on a line.
check "row" "$(row "$out" row)" \
      "row,3,2,0.500000,0.500000,1,0,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000,0.000000"
# overlap: boxes of side 0.2 at distance 0.05 overlap 0.15 x 0.2 = 0.03 of area 0.12; the far
# node's two edges run at 45 degrees and at atan(1/0.95); node 1 is 0.05/sqrt2 from edge 0-2
# and node 0 is 0.05 from edge 1-2, both within r = 0.1.
check "overlap" "$(row "$out" overlap)" \
      "overlap,3,2,1.414214,1.414214,1,0,0.000000,0.250000,0.000305,0.321855,0.493590,0.316667,0.333947,0.000000"
# The fitted reference length, the default: L = sum(l^2) / sum(l). square: 8 / (4 + 2 sqrt2) =
# 1.171573 for both terms (every pair is an edge), length = (4 (1/L - 1)^2 + 2 (sqrt2/L - 1)^2)
# / 6 = 0.028595. overlap: edges sqrt2 and sqrt(0.95^2 + 1) = 1.379311, L = 1.396981; the pair
# at two hops has r/d = 0.025, so Ls = (2 + 1.902500 + 0.000625) / (1.414214 + 1.379311 +
# 0.025) = 1.384811.
out=$("$station" terms --corpus "$fixture" --align a1)
check "fitted L on the square" "$(row "$out" square | cut -d, -f4-5,10-11)" "1.171573,1.171573,0.028595,0.028595"
check "fitted L and Ls on overlap" "$(row "$out" overlap | cut -d, -f4-5)" "1.396981,1.384811"
check "the row is uniform, so every reference length agrees" "$(row "$out" row | cut -d, -f4-5)" "0.500000,0.500000"
check "cycle: L at two hops, and the stress worked out in the fixture" \
      "$(row "$out" cycle | cut -d, -f4-5,11)" "1.000000,0.923495,0.022876"
check "rsqrt: 1 / sqrt(4)" \
      "$("$station" terms --corpus "$fixture" --L rsqrt | grep '^square,' | cut -d, -f4-5)" "0.500000,0.500000"

out=$("$station" terms --corpus "$fixture" --align grid)
check "gridiness: the square's corners align only in pairs" \
      "$(row "$out" square | cut -d, -f13)" "1.000000"
check "gridiness: the row is one alignment of three" \
      "$(row "$out" row | cut -d, -f13)" "0.000000"
out=$("$station" terms --corpus "$fixture" --align a3)
check "A3: each node of the row sees two others in its row, 1/(1+2)" \
      "$(row "$out" row | cut -d, -f13)" "0.333333"
out=$("$station" terms --corpus "$fixture" --align a2)
# A2: the near pair are 0.05 from a column and on one row, 0.025 each; the far node is 0.95
# from a column and 1 from a row, 0.975; over three nodes.
check "A2 on overlap" "$(row "$out" overlap | cut -d, -f13)" "0.341667"

# ------------------------------------------------------------------- the directional test

out=$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0,0)
check "direct prints the header" "$(firstline "$out")" \
      "id,n,m,E,q,dec,crossings,overlap,length,stress,orthogonality,alignment,node-edge,flow"
check "crossings+overlap: a 2% move cannot uncross the square's diagonals, so it is held" \
      "$(row "$out" square | cut -d, -f5)" "1.000000"
check "crossings+overlap: the row has nothing to lower, ties hold" \
      "$(row "$out" row | cut -d, -f5)" "1.000000"
check "overlap: the two overlapping boxes can separate, the far node is held" \
      "$(row "$out" overlap | cut -d, -f5)" "0.333333"
check "the energy at the layout is the weighted sum" "$(row "$out" overlap | cut -d, -f4)" "0.250000"
out=$("$station" direct --corpus "$fixture" --weights 0,0,1,0,0,0,0,0)
check "length: the row is exactly uniform, every move lengthens or shortens an edge" \
      "$(row "$out" row | cut -d, -f5)" "1.000000"
# The cycle is a stress minimum at its own scale, so the fitted L holds every corner at any
# radius, and the median L, which wants the diagonals longer, holds none.
for d in 0.005 0.02 0.2; do
    check "stress, fitted L: the cycle is held at d = $d" \
          "$("$station" direct --corpus "$fixture" --weights 0,0,0,1,0,0,0,0 --d $d | grep '^cycle,' | cut -d, -f5)" "1.000000"
done
check "stress, median L: no corner of the cycle is held" \
      "$("$station" direct --corpus "$fixture" --weights 0,0,0,1,0,0,0,0 --L median | grep '^cycle,' | cut -d, -f5)" "0.000000"
out=$("$station" direct --corpus "$fixture" --weights 0,0,0,0,0,1,0,0 --align a1)
# A1 on overlap: the far node can move toward the near pair's column, and the near node at
# x = 0.05 can move toward the far node's column, so two of three are free; each best move
# lowers the far node's 0.95 by 0.02, and the sum over nodes, over E = 0.316667, is 0.014.
check "alignment A1: two of the overlap nodes can improve an alignment" \
      "$(row "$out" overlap | cut -d, -f5)" "0.333333"
check "and the decrease is reported over E" \
      "$(row "$out" overlap | cut -d, -f6 | cut -c1-5)" "0.014"
out=$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0,0 --nodes)
check "--nodes prints one row per node" "$(printf '%s\n' "$out" | grep -c '^[a-z]*,[0-9]')" "18"
check "a held node reports direction -1" "$(printf '%s\n' "$out" | grep -c ',-1,0.000000000$')" "16"
out=$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0,0 --diffs)
check "--diffs prints the header" "$(firstline "$out")" "id,node,dir,crossings,overlap,length,stress,orthogonality,alignment,node-edge,flow"
check "and a row per node and direction" "$(printf '%s\n' "$out" | grep -c '^[a-z]*,[0-9]')" "288"
# overlap's node 1 moved in direction 0 (+x, away from node 0): the overlap 0.15 x 0.2 shrinks
# by 0.02 x 0.2 = 0.004 of area 0.12, a change of -0.033333.
check "the difference of a term under a move" "$(printf '%s\n' "$out" | grep '^overlap,1,0,' | cut -d, -f5)" "-0.0333333333"

# ------------------------------------------------------------------- polylines and flow

out=$("$station" terms --corpus "$fixture" --align a1 --L median)
check "bend: the chords cross, the routes do not, and R is the length-weighted mean" \
      "$(row "$out" bend | cut -d, -f8,12,14)" "0.000000,0.246053,0.000000"
printf 'G bend 4 3\nV 0 0 0.1 0.1\nV 1 1 0.1 0.1\nV 1 0 0.1 0.1\nV 0 1 0.1 0.1\nU 1 0 1 1 -0.5 1.6 -0.5 0.5 0 0\nU 3 2\nU 2 1\n' > "$tmp/rev.txt"
check "an edge listed from b to a with its waypoints reversed is the same route" \
      "$("$station" terms --corpus "$tmp/rev.txt" --align a1 --L median | tail -1)" "$(row "$out" bend)"
printf 'G bend 4 3\nV 0 0 0.1 0.1\nV 1 1 0.1 0.1\nV 1 0 0.1 0.1\nV 0 1 0.1 0.1\nU 0 1\nU 2 3\nU 1 2\n' > "$tmp/chord.txt"
# The route of 2-3 attached to the top of box 2 at (1, 0.05) and the bottom of box 3 at
# (0, 0.95): orthogonality 0.9 / 1.9 = 0.473684, R = (0.238160 + 0.473684 + 0) / 3 = 0.237281.
# Moving node 2 up by 0.02 carries its attachment point: the same route shifted, still
# crossing nothing, its orthogonality 0.92 / 1.92.
printf 'G bend 4 3\nV 0 0 0.1 0.1\nV 1 1 0.1 0.1\nV 1 0 0.1 0.1\nV 0 1 0.1 0.1\nU 0 1 0 0 -0.5 0.5 -0.5 1.6 1 1\nU 2 3 1 0.05 0 0.95\nU 1 2\n' > "$tmp/attach.txt"
check "an attachment point off the centre is the route's end" \
      "$("$station" terms --corpus "$tmp/attach.txt" --align a1 --L median | tail -1 | cut -d, -f8,12)" "0.000000,0.237281"
printf 'G bend 4 3\nV 0 0 0.1 0.1\nV 1 1 0.1 0.1\nV 1 0.02 0.1 0.1\nV 0 1 0.1 0.1\nU 0 1 0 0 -0.5 0.5 -0.5 1.6 1 1\nU 2 3 1 0.07 0 0.95\nU 1 2\n' > "$tmp/attach2.txt"
moved=$("$station" terms --corpus "$tmp/attach2.txt" --align a1 --L median | tail -1 | cut -d, -f12)
check "and moves with its node: the test's move up is the layout drawn moved up" \
      "$("$station" direct --corpus "$tmp/attach.txt" --weights 0,0,0,0,1,0,0,0 --diffs | grep '^bend,2,4,' | cut -d, -f8 | cut -c1-9)" \
      "$(printf '%s - 0.237281\n' "$moved" | bc -l | sed 's/^-\./-0./' | cut -c1-9)"
check "and as chords the same graph has one crossing in three edges" \
      "$("$station" terms --corpus "$tmp/chord.txt" --align a1 --L median | tail -1 | cut -d, -f8)" "0.333333"
# flow: the square's edges run 0-1 right, 1-2 up, 2-3 left, 3-0 down, 0-2 and 1-3 diagonal;
# reading upward, only 3-0 runs backward, 1 of 6. The cycle reads rightward (the first of
# the four directions on a tie) with 2-3 backward: 1 of 4.
check "flow on the square" "$(row "$out" square | cut -d, -f6,7,15)" "0,1,0.166667"
check "flow on the cycle" "$(row "$out" cycle | cut -d, -f6,7,15)" "1,0,0.250000"
out=$("$station" direct --corpus "$fixture" --weights 0,0,0,0,0,0,0,1)
check "flow holds the two nodes of the cycle whose edges run with the reading direction" \
      "$(row "$out" cycle | cut -d, -f5)" "0.500000"
check "an undirected graph has flow 0 and every node held" "$(row "$out" bend | cut -d, -f4,5)" "0.000000,1.000000"

# ------------------------------------------------------------------- check: a control is the same graphs

check "a corpus is the same graphs as itself" "$("$station" check --corpus "$fixture" --against "$fixture")" "same 5 graphs"
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
chk "an edge that differs is named" "station: graph 0: square: edge 5 is 1-3 (1) against 0-3 (1)" "$tmp/edge.txt"
sed 's/^E 1 3$/U 1 3/' "$fixture" > "$tmp/dir.txt"
chk "a direction that differs is named" "station: graph 0: square: edge 5 is 1-3 (1) against 1-3 (0)" "$tmp/dir.txt"
chk "a shorter corpus is refused" "station: $fixture has 5 graphs, $tmp/short.txt has 1" "$tmp/short.txt"

# ------------------------------------------------------------------- descend, through the library

out=$("$station" descend --corpus "$fixture" --weights 0,1,0,0,0,0,0,0 --budget 400 --seeds 3 --converge 2000)
check "descend prints the header" "$(firstline "$out")" \
      "id,n,m,E,q,climb,random,cap_climb,cap_random,rho_climb,rho_random,wins,p,converged,q_converged"
check "a layout the climber cannot improve is left where it is, and the control is not" \
      "$(row "$out" square | cut -d, -f6-9)" "0.000000,0.000000,0.0000,0.7436"
check "the overlapping boxes: the climber removes energy within the cap" \
      "$(row "$out" overlap | cut -d, -f4,6,8,10)" "0.250000,0.172949,0.6667,0.3082"
check "wins are counted over seeds and tested by the exact sign test" \
      "$(row "$out" square | cut -d, -f12,13)" "0,1"
a=$("$station" descend --corpus "$fixture" --weights 0,1,0,0,0,0,0,0 --budget 400 --seeds 3 --converge 2000 | digest)
b=$("$station" descend --corpus "$fixture" --weights 0,1,0,0,0,0,0,0 --budget 400 --seeds 3 --converge 2000 | digest)
check "descend reproduces byte for byte" "$a" "$b"

# ------------------------------------------------------------------- reproducibility

a=$("$station" direct --corpus "$fixture" --weights 1,1,1,1,1,1,1,0 --align a3 | digest)
b=$("$station" direct --corpus "$fixture" --weights 1,1,1,1,1,1,1,0 --align a3 | digest)
check "two runs are byte-identical" "$a" "$b"
# The same graphs in the other order give the same rows: nothing leaks between graphs.
{ sed -n '/^G row/,/^E 1 2/p' "$fixture"; sed -n '/^G square/,/^E 1 3/p' "$fixture"; } > "$tmp/rev.txt"
a=$("$station" direct --corpus "$fixture" --weights 1,1,1,1,1,1,1,0 | grep -E '^(square|row),' | sort)
b=$("$station" direct --corpus "$tmp/rev.txt" --weights 1,1,1,1,1,1,1,0 | grep -E '^(square|row),' | sort)
check "order independence" "$a" "$b"
check "four directions and sixteen agree where every move is a tie" \
      "$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0,0 --dirs 4 | grep '^row,' | cut -d, -f5,6)" \
      "$("$station" direct --corpus "$fixture" --weights 1,1,0,0,0,0,0,0 --dirs 16 | grep '^row,' | cut -d, -f5,6)"

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
refuse "descend needs weights" "station: descend needs --weights" descend --corpus "$fixture"
refuse "seven weights are refused" \
       "station: --weights needs eight non-negative numbers C,O,L,S,R,A,N,F, not '1,1,1,1,1,1,1'" \
       direct --corpus "$fixture" --weights 1,1,1,1,1,1,1
refuse "a negative weight is refused" \
       "station: --weights needs eight non-negative numbers C,O,L,S,R,A,N,F, not '1,-1,1,1,1,1,1,1'" \
       direct --corpus "$fixture" --weights 1,-1,1,1,1,1,1,1
refuse "an unknown alignment is refused" "station: --align must be a1, a2, a3 or grid, not 'a4'" \
       terms --corpus "$fixture" --align a4
refuse "an unknown reference length is refused" "station: --L must be fit, median or rsqrt, not 'mean'" \
       terms --corpus "$fixture" --L mean
refuse "check needs --against" "station: check needs --against" check --corpus "$fixture"
refuse "a zero radius is refused" "station: --d must be a number in [1e-09, 1], not '0'" \
       direct --corpus "$fixture" --weights 1,1,1,1,1,1,1,0 --d 0
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
bad "an odd route list"          "line 4, graph odd: odd number of route coordinates" "G odd 2 1
V 0 0 1 1
V 1 1 1 1
E 0 1 0.5"
bad "a route of one point"       "line 4, graph one: a route needs two end points" "G one 2 1
V 0 0 1 1
V 1 1 1 1
E 0 1 0.5 0.5"
bad "a malformed edge line"      "line 4, graph edge: expected 'E a b' or 'U a b'" "G edge 2 1
V 0 0 1 1
V 1 1 1 1
X 0 1"
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
check "WikiPathways, first twenty, C+O+L" "$(qcol --corpus "$tmp/hs.txt" --weights 1,1,1,0,0,0,0,0)" \
      "0.00 0.00 0.00 0.00 0.00 0.00 0.05 0.05 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 "
check "WikiPathways, first twenty, alignment A1" "$(qcol --corpus "$tmp/hs.txt" --weights 0,0,0,0,0,1,0,0 --align a1)" \
      "0.50 0.88 0.50 0.55 0.70 0.86 1.00 0.33 1.00 0.51 0.23 0.70 0.77 0.68 0.73 0.59 0.62 0.29 0.62 0.30 "
check "BPMN, first twenty, alignment A1" "$(qcol --corpus "$tmp/bpmn.txt" --weights 0,0,0,0,0,1,0,0 --align a1)" \
      "0.91 1.00 0.66 0.25 0.81 1.00 1.00 1.00 1.00 1.00 0.29 0.56 0.88 0.72 0.75 1.00 1.00 1.00 0.58 0.93 "
check "Reactome, first twenty, stress: the hand layout" "$(qcol --corpus "$tmp/sbgn.txt" --weights 0,0,0,1,0,0,0,0)" \
      "0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.03 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 "
check "Reactome, first twenty, stress: neato's layout, the power check" "$(qcol --corpus "$tmp/sbgn_neato.txt" --weights 0,0,0,1,0,0,0,0)" \
      "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 0.90 0.96 1.00 1.00 0.96 "
check "BPMN, first twenty, stress: neato's layout" "$(qcol --corpus "$tmp/bpmn_neato.txt" --weights 0,0,0,1,0,0,0,0)" \
      "1.00 1.00 1.00 0.93 0.93 1.00 1.00 1.00 1.00 1.00 1.00 0.93 1.00 1.00 1.00 1.00 0.95 1.00 0.96 1.00 "

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
