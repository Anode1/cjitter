#!/bin/sh
# analyse.sh: black-box tests of analyse.py, the paired statistics, through a shell.
#
# The two fixtures are five diagrams whose differences are 0.5, 0, 0.5, 0.25, 0.25, so every
# pinned number below is hand arithmetic: four non-zero differences, all positive, ranked
# 1.5, 1.5, 3.5, 3.5, so V is its maximum 10 and P(V >= 10) = 1/16; the ten Walsh averages
# are three 0.25, four 0.375 and three 0.5, so the shift is 0.375 and, the critical rank at
# n = 4 being the first, the interval runs from the first to the tenth of them.
#
#   sh tests/analyse.sh
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
analyse="$root/example/diagrams/analyse.py"
[ -f "$analyse" ] || { echo "no $analyse" >&2; exit 1; }

tmp=$(mktemp -d 2>/dev/null || mktemp -d -t cjitter)
trap 'rm -rf "$tmp"' EXIT

pass=0; fail=0
ok()   { pass=$((pass+1)); }
no()   { fail=$((fail+1)); echo "  FAIL $1"; }
check(){ if [ "$2" = "$3" ]; then ok; else no "$1: expected [$3], got [$2]"; fi; }
firstline(){ printf '%s' "$1" | head -1; }
row(){ printf '%s\n' "$1" | grep "^$2" | tr -s ' '; }

# a and b pair on id, in opposite orders; dec is zero in both, so its differences all vanish.
printf 'id,n,m,E,q,dec\ng1,3,2,0.5,1.00,0\ng2,3,2,0.5,0.50,0\ng3,3,2,0.5,0.75,0\ng4,3,2,0.5,0.25,0\ng5,3,2,0.5,0.50,0\n' > "$tmp/a.csv"
printf 'id,n,m,E,q,dec\ng5,3,2,0.5,0.25,0\ng4,3,2,0.5,0.00,0\ng3,3,2,0.5,0.25,0\ng2,3,2,0.5,0.50,0\ng1,3,2,0.5,0.50,0\n' > "$tmp/b.csv"
# c minus d is 0.5, 0, -0.25, 0.25, 0.25: ranks 4, 2, 2, 2, V = 8, and the four subsets of
# them summing to 8 or more give P(V >= 8) = 4/16. Three of four signs are positive, so the
# sign test is (4 + 1)/16.
printf 'id,n,m,E,q,dec\ng1,3,2,0.5,0.75,0\ng2,3,2,0.5,0.50,0\ng3,3,2,0.5,0.25,0\ng4,3,2,0.5,0.50,0\ng5,3,2,0.5,0.50,0\n' > "$tmp/c.csv"
printf 'id,n,m,E,q,dec\ng1,3,2,0.5,0.25,0\ng2,3,2,0.5,0.50,0\ng3,3,2,0.5,0.50,0\ng4,3,2,0.5,0.25,0\ng5,3,2,0.5,0.25,0\n' > "$tmp/d.csv"

# ------------------------------------------------------------------- one pair

out=$(python3 "$analyse" "$tmp/a.csv" "$tmp/b.csv" --alternative greater)
check "the header" "$(firstline "$out")" \
      "pair    n  nz  median A  median B  median d     HL  CI lo  CI hi  p sign  p signrank"
check "a vs b, greater" "$(row "$out" 'a vs b')" \
      "a vs b 5 4 0.500 0.250 0.250 0.375 0.250 0.500 0.0625 0.0625"
out=$(python3 "$analyse" "$tmp/a.csv" "$tmp/b.csv")
check "two-sided is the default, and doubles both p" "$(row "$out" 'a vs b')" \
      "a vs b 5 4 0.500 0.250 0.250 0.375 0.250 0.500 0.125 0.125"
check "the footer names the column and the null" "$(printf '%s\n' "$out" | tail -1)" \
      "column q, alternative two-sided, d = A - B; 95% interval, zeros dropped, exact null."
out=$(python3 "$analyse" "$tmp/b.csv" "$tmp/a.csv" --alternative less)
check "the pair the other way round is the mirror" "$(row "$out" 'b vs a')" \
      "b vs a 5 4 0.250 0.500 -0.250 -0.375 -0.500 -0.250 0.0625 0.0625"

# --column dec: every difference is zero, so nothing is left to test.
out=$(python3 "$analyse" "$tmp/a.csv" "$tmp/b.csv" --column dec)
check "a column with no differences" "$(row "$out" 'a vs b')" \
      "a vs b 5 0 0.000 0.000 0.000 0.000 0.000 0.000 1 1"

out=$(python3 "$analyse" "$tmp/a.csv" "$tmp/b.csv" --md)
check "--md prints a markdown row" "$(row "$out" '| a vs b')" \
      "| a vs b | 5 | 4 | 0.500 | 0.250 | 0.250 | 0.375 | 0.250 | 0.500 | 0.125 | 0.125 |"

# ------------------------------------------------------------------- the family

out=$(python3 "$analyse" --family q "$tmp/a.csv" "$tmp/b.csv" other "$tmp/c.csv" "$tmp/d.csv" \
      --alternative greater)
# Holm over 1/16 and 4/16: the smaller doubles to 0.125, the larger keeps 0.25.
check "the first of the family" "$(row "$out" 'q ')" \
      "q 5 4 0.500 0.250 0.250 0.375 0.250 0.500 0.0625 0.0625 0.125"
check "the second of the family" "$(row "$out" 'other')" \
      "other 5 4 0.500 0.250 0.250 0.250 -0.250 0.500 0.3125 0.25 0.25"

# ------------------------------------------------------------------- refusals

refuse() {   # name, expected first line, args...
    name=$1; want=$2; shift 2
    set +e
    out=$(python3 "$analyse" "$@" 2>&1 >/dev/null); rc=$?
    set -e
    check "$name" "$(firstline "$out")" "$want"
    check "$name exits 1" "$rc" "1"
}
sed '$d' "$tmp/b.csv" > "$tmp/short.csv"
refuse "an id in one file only is named" "analyse.py: $tmp/short.csv has no row for id g1" \
       "$tmp/a.csv" "$tmp/short.csv"
refuse "an unknown column is refused" "analyse.py: $tmp/a.csv has no column p" \
       "$tmp/a.csv" "$tmp/b.csv" --column p
refuse "an unknown alternative is refused" \
       "analyse.py: --alternative must be greater, less or two-sided, not up" \
       "$tmp/a.csv" "$tmp/b.csv" --alternative up
refuse "one file is refused" "analyse.py: wants two CSV files, or --family LABEL A.csv B.csv ..." \
       "$tmp/a.csv"
refuse "an incomplete family is refused" "analyse.py: --family wants LABEL A.csv B.csv per pair" \
       --family one "$tmp/a.csv"
refuse "a missing file is refused" "analyse.py: cannot open $tmp/none.csv" \
       "$tmp/none.csv" "$tmp/b.csv"

# ------------------------------------------------------------------- the script's own tests

out=$(python3 "$analyse" --selftest)
check "the self-test passes" "$(printf '%s\n' "$out" | tail -1 | sed 's/[0-9]* passed/N passed/')" \
      "analyse: N passed, 0 failed"

echo "analyse: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
