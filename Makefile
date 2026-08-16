# Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
#
#   make | check | ut | cliut | ut-asan | ut-ubsan | pedantic | examples | clean
SHELL = /bin/sh

CC       ?= cc
CFLAGS   ?= -O2
CPPFLAGS ?=
LDLIBS   ?=

# No FMA contraction: a run must reproduce from its seed on any compiler and architecture.
STD  = -std=c99 -ffp-contract=off
WARN = -W -Wall -Wshadow -Wconversion
PROJ = $(STD) $(WARN) -Ic
LIBM = -lm

HEADERS = $(wildcard c/*.h)
SRC     = c/cjitter.c c/rng.c
OBJ     = $(SRC:.c=.o)

.PHONY: all check ut cliut ut-asan ut-ubsan pedantic examples clean

all: examples

check: ut cliut

c/%.o: c/%.c $(HEADERS)
	$(CC) $(PROJ) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

examples: labels erd

# The search watched: climb settling the migration's tables into the frozen diagram, a
# smoothed replay of its improvements. The README's moving figure. Not part of check: it
# needs rsvg-convert and ImageMagick, and its output is a committed fixture.
movie: example/erd/erd_movie.c example/erd/erd.c $(OBJ) $(HEADERS)
	$(CC) $(PROJ) $(CPPFLAGS) $(CFLAGS) -o erd_movie example/erd/erd_movie.c $(OBJ) $(LIBM) $(LDLIBS)
	rm -rf movie_frames && mkdir movie_frames
	./erd_movie movie_frames
	for f in movie_frames/frame*.svg; do rsvg-convert -w 640 "$$f" -o "$${f%.svg}.png"; done
	convert -delay 4 -loop 0 movie_frames/frame*.png \
	        \( +clone -set delay 400 \) +swap +delete -layers Optimize example/erd/erd_settle.gif
	rm -rf movie_frames erd_movie
	@echo "example/erd/erd_settle.gif rebuilt"

labels: example/labels.c $(OBJ) $(HEADERS)
	$(CC) $(PROJ) $(CPPFLAGS) $(CFLAGS) -o $@ example/labels.c $(OBJ) $(LIBM) $(LDLIBS)

erd: example/erd/erd.c $(OBJ) $(HEADERS)
	$(CC) $(PROJ) $(CPPFLAGS) $(CFLAGS) -o $@ example/erd/erd.c $(OBJ) $(LIBM) $(LDLIBS)

# ut: in-process unit tests of the library's contract: refusals, the exact budget, determinism,
# the box and the repair invariant. Built from sources, not objects, so a header change rebuilds.
ut: cjitter_ut
	./cjitter_ut
cjitter_ut: tests/tests.c $(SRC) $(HEADERS)
	$(CC) $(PROJ) $(CPPFLAGS) $(CFLAGS) -o $@ tests/tests.c $(SRC) $(LIBM) $(LDLIBS)

# cliut: black-box tests of the built examples. `make ut` calls functions, so it can never see
# an exit code, a refusal message or whether a whole run reproduces byte for byte; these can.
cliut: examples
	sh tests/cli.sh

# The sanitizers run both suites, because the examples' argument handling is only reachable
# through the shell. -fno-sanitize-recover is what makes UBSan a gate rather than a report.
SAN = -g -O1 -fno-omit-frame-pointer
ut-asan: $(HEADERS)
	$(CC) $(PROJ) $(SAN) -fsanitize=address -o cjitter_ut_asan tests/tests.c $(SRC) $(LIBM) && ./cjitter_ut_asan
	$(CC) $(PROJ) $(SAN) -fsanitize=address -o labels_asan example/labels.c $(SRC) $(LIBM)
	$(CC) $(PROJ) $(SAN) -fsanitize=address -o erd_asan example/erd/erd.c $(SRC) $(LIBM)
	LABELS_BIN=$(CURDIR)/labels_asan ERD_BIN=$(CURDIR)/erd_asan sh tests/cli.sh
ut-ubsan: $(HEADERS)
	$(CC) $(PROJ) $(SAN) -fsanitize=undefined -fno-sanitize-recover=all -o cjitter_ut_ubsan tests/tests.c $(SRC) $(LIBM) && ./cjitter_ut_ubsan
	$(CC) $(PROJ) $(SAN) -fsanitize=undefined -fno-sanitize-recover=all -o labels_ubsan example/labels.c $(SRC) $(LIBM)
	$(CC) $(PROJ) $(SAN) -fsanitize=undefined -fno-sanitize-recover=all -o erd_ubsan example/erd/erd.c $(SRC) $(LIBM)
	LABELS_BIN=$(CURDIR)/labels_ubsan ERD_BIN=$(CURDIR)/erd_ubsan sh tests/cli.sh

# -Werror is what makes "must be clean" a gate: gcc exits 0 on warnings, so without it this
# target only enforced its claim on whoever read the scrollback.
pedantic: $(HEADERS)
	@rc=0; tmp=`mktemp -d`; \
	for f in $(SRC) tests/tests.c example/labels.c example/erd/erd.c; do \
	    $(CC) $(PROJ) -pedantic -Wextra -Werror -O2 -c "$$f" -o "$$tmp/p.o" || rc=1; \
	done; \
	rm -rf "$$tmp"; \
	test $$rc -eq 0 && echo "pedantic: clean"; exit $$rc

clean:
	rm -f c/*.o labels erd cjitter_ut cjitter_ut_asan cjitter_ut_ubsan \
	      labels_asan erd_asan labels_ubsan erd_ubsan
