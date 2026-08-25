"""The budget-trend figure: implementations shown worse than the control, by budget.

Reads the audit driver's stderr summary (summary_uniform.txt) and writes a pgfplots
figure. Usage: python3 trend_fig.py summary_uniform.txt > trend.tex
"""
import re
import sys

worse = {}
for line in open(sys.argv[1]):
    m = re.match(r"D=(\d+)\s+bf=(\d+)\s+better\s+(\d+)\s+not shown\s+(\d+)\s+worse\s+(\d+)", line)
    if m:
        d, b, _, _, w = (int(g) for g in m.groups())
        worse.setdefault(d, []).append((b, w))

marks = {2: "*", 5: "square*", 10: "triangle*", 20: "diamond*"}
styles = {2: "", 5: ",dashed", 10: ",dotted", 20: ",dashdotted"}
print(r"""\begin{figure}[t]\centering
\begin{tikzpicture}
\begin{axis}[width=11cm,height=6.2cm,
  xmode=log,log basis x=10,
  xlabel={budget factor (evaluations per dimension)},
  ylabel={shown worse than uniform sampling, of 296},
  ymin=40,ymax=85,xmin=40,xmax=12500,
  xtick={50,100,500,1000,5000,10000},
  xticklabels={50,100,500,1000,5000,10000},
  legend pos=north west,legend cell align=left,
  grid=major,grid style={black!12},
  tick label style={font=\footnotesize},label style={font=\small},
  legend style={font=\footnotesize}]""")
for d in (2, 5, 10, 20):
    pts = "".join(f"({b},{w})" for b, w in sorted(worse[d]))
    print(f"\\addplot[thick,mark={marks[d]},mark size=1.6pt{styles[d]}] coordinates\n  {{{pts}}};")
    print(f"\\addlegendentry{{$d={d}$}}")
print(r"""\end{axis}
\end{tikzpicture}
\caption{Implementations shown strictly worse than uniform sampling at equal budget,
of 296, exact sign test with Holm at 5\% per (dimension, budget). The count rises
toward the top budget in every dimension, from a mid-grid minimum in 5, 10 and 20
dimensions: a search that stalls is overtaken by a control that keeps sampling.}
\label{fig:trend}
\end{figure}""")
