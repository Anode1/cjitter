"""Emit figures/tools.tex: one graph, four layouts, one scale.

Panel widths are proportional to each layout's extent, so a centimetre is the
same number of corpus units in every panel and a box has one size on the page.
"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from corpus_to_tikz import read, emit

DATA = os.path.join(os.path.dirname(__file__), "..", "data")
GID = "1069107590.json"
ROW_CM = 15.2  # total width of the four panels

PANELS = [("bpmn.txt", "hand"), ("bpmn_dot.txt", r"\texttt{dot}"),
          ("bpmn_elk.txt", "ELK layered"), ("bpmn_neato.txt", r"\texttt{neato}")]

CAPTION = r"""\caption{The same graph, the same box sizes, four layouts, one scale.
The hand drawing and the two
layered tools share the profile the test reads: held under overlap, free under length and
stress. \texttt{neato} minimises stress and is the control that gives the test its power,
held at 1.00 under stress where every hand layout reads 0.00. The tool layouts carry no
waypoints, so their edges are drawn as chords.}"""

def extent(V):
    lox = min(x - w/2 for x, y, w, h in V); hix = max(x + w/2 for x, y, w, h in V)
    loy = min(y - h/2 for x, y, w, h in V); hiy = max(y + h/2 for x, y, w, h in V)
    return max(hix - lox, hiy - loy)

layouts = [(read(os.path.join(DATA, f), GID), lab) for f, lab in PANELS]
for (V, E), _ in layouts:
    assert V, GID
scale = ROW_CM / sum(extent(V) for (V, E), _ in layouts)
body = " &\n".join(emit(V, E, extent(V) * scale, lab) for (V, E), lab in layouts)

print(r"\begin{figure}[t]\centering")
print(r"\begin{tabular}{@{}c@{\hspace{3pt}}c@{\hspace{3pt}}c@{\hspace{3pt}}c@{}}")
print(body)
print(r"\end{tabular}")
print(CAPTION)
print(r"\label{fig:tools}")
print(r"\end{figure}")
