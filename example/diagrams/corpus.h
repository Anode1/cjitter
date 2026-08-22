/* corpus.h -- reading a corpus of diagram layouts.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * The text format, one file per corpus, written by data/make_corpus.py from the parsers'
 * JSON and read here, so that every number comes from one binary:
 *
 *     G <id> <n> <m>          one graph; id has no spaces
 *     V <x> <y> <w> <h>       n lines, raw coordinates and box size, any units
 *     E <a> <b>               m lines, 0-based endpoints, a != b
 *
 * Lines starting with # are comments. On reading, each layout is shifted so its minimum
 * coordinate is 0 and divided by its larger extent, boxes with it, so the drawing fits the
 * unit square and a distance of 0.02 is 2% of the drawing width on every diagram. The
 * reference length L is the median edge length after rescaling (the upper median at even m),
 * and dist holds the hop distances, by breadth-first search, that the stress term reads.
 *
 * Refused, with a message in ERR and -1 returned: n < 2, m < 1, an endpoint out of range or
 * a == b, a zero extent, a box of negative size, a disconnected graph, or a malformed line. */
#ifndef DIAGRAMS_CORPUS_H
#define DIAGRAMS_CORPUS_H

#include <stdio.h>
#include <stddef.h>
#include "energy.h"

/* Reads every graph in F. *OUT receives a malloc'd array of K graphs; returns K, or -1. */
long corpus_read(FILE *f, graph **out, char *err, size_t errlen);
void corpus_free(graph *gs, long k);

#endif
