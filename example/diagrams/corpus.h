/* corpus.h -- reading a corpus of diagram layouts.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * The text format, one file per corpus, written by data/make_corpus.py from the parsers'
 * JSON and read here, so that every number comes from one binary:
 *
 *     G <id> <n> <m>                one graph; id has no spaces
 *     V <x> <y> <w> <h>             n lines, raw coordinates and box size, any units
 *     E <a> <b> [x1 y1 ... xk yk]   m lines in all: a directed edge drawn from a to b, or
 *     U <a> <b> [x1 y1 ... xk yk]   an undirected one; 0-based, a != b; after the endpoints
 *                                   the drawn route from a to b, k >= 2 points in the units
 *                                   of V: the attachment point on a's box, the interior
 *                                   waypoints, the attachment point on b's box. No points
 *                                   means the chord between the centres. An attachment
 *                                   point keeps its offset from its node's centre when the
 *                                   node moves.
 *
 * Lines starting with # are comments. On reading, each layout is shifted so its minimum
 * node coordinate is 0 and divided by the larger extent of the node centres, boxes and
 * waypoints with it, so the drawing fits the unit square and a distance of 0.02 is 2% of the
 * drawing width on every diagram. The three reference-length candidates of energy.h are
 * computed after rescaling, dist holds the hop distances, by breadth-first search, that the
 * stress term reads, and ux, uy is the axis direction under which the flow term is least.
 *
 * Refused, with a message in ERR and -1 returned: n < 2, m < 1, an endpoint out of range or
 * a == b, a zero extent, a box of negative size, a disconnected graph, an odd number of
 * route coordinates, a route of one point, or a malformed line. */
#ifndef DIAGRAMS_CORPUS_H
#define DIAGRAMS_CORPUS_H

#include <stdio.h>
#include <stddef.h>
#include "energy.h"

/* Reads every graph in F. *OUT receives a malloc'd array of K graphs; returns K, or -1. */
long corpus_read(FILE *f, graph **out, char *err, size_t errlen);
void corpus_free(graph *gs, long k);

#endif
