# Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
#
#   make | check | pedantic | examples | clean
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

.PHONY: all check pedantic examples clean

all: examples

c/%.o: c/%.c $(HEADERS)
	$(CC) $(PROJ) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

examples: labels erd

labels: example/labels.c $(OBJ) $(HEADERS)
	$(CC) $(PROJ) $(CPPFLAGS) $(CFLAGS) -o $@ example/labels.c $(OBJ) $(LIBM) $(LDLIBS)

erd: example/erd/erd.c $(OBJ) $(HEADERS)
	$(CC) $(PROJ) $(CPPFLAGS) $(CFLAGS) -o $@ example/erd/erd.c $(OBJ) $(LIBM) $(LDLIBS)

check: examples
	./labels 40 4000 3 >/dev/null
	./erd >/dev/null
	@echo "check: both examples ran"

pedantic: $(HEADERS)
	@rc=0; tmp=`mktemp -d`; \
	for f in $(SRC) example/labels.c example/erd/erd.c; do \
	    $(CC) $(PROJ) -pedantic -Wextra -O2 -c "$$f" -o "$$tmp/p.o" || rc=1; \
	done; \
	rm -rf "$$tmp"; \
	test $$rc -eq 0 && echo "pedantic: clean"; exit $$rc

clean:
	rm -f c/*.o labels erd
