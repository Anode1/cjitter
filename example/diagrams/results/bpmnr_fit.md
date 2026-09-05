# LP fits and weight sweeps on the seeded random BPMN 300 (fit.py over fit/bpmnr.diffs, as run_all.sh does for the committed corpora)

== bpmnr terms C,O,L
terms                COL
residual / node      0.0005
q fitted             0.997
q held-out (5x2)     0.992 [0.982, 0.998]
crossings            1.000
overlap              -0.000
length               0.000
diagrams 300, nodes 6866, rows 109856
== bpmnr terms C,O,L,S,R,A,N,F
terms                COLSRANF
residual / node      0.0003
q fitted             0.504
q held-out (5x2)     0.552 [0.479, 0.983]
crossings            0.024
overlap              0.026
length               0.000
stress               0.021
orthogonality        0.064
alignment            0.813
node-edge            0.008
flow                 0.044
diagrams 300, nodes 6866, rows 109856
== bpmnr terms L,S
terms                LS
residual / node      0.0063
q fitted             0.011
q held-out (5x2)     0.011 [0.008, 0.014]
length               0.000
stress               1.000
diagrams 300, nodes 6866, rows 109856
== bpmnr terms R,A,N,F
terms                RANF
residual / node      0.0003
q fitted             0.654
q held-out (5x2)     0.651 [0.617, 0.689]
orthogonality        0.066
alignment            0.881
node-edge            0.008
flow                 0.045
diagrams 300, nodes 6866, rows 109856
== bpmnr terms C,O,A
terms                COA
residual / node      0.0004
q fitted             0.787
q held-out (5x2)     0.866 [0.754, 0.996]
crossings            0.016
overlap              0.025
alignment            0.959
diagrams 300, nodes 6866, rows 109856
== bpmnr sweep L on C,O
weight on length (base C,O equal)  q
 0.000  0.986
 0.001  0.012
 0.010  0.012
 0.050  0.012
 0.100  0.012
 0.200  0.012
 0.500  0.011
 1.000  0.010
== bpmnr sweep S on C,O
weight on stress (base C,O equal)  q
 0.000  0.986
 0.001  0.013
 0.010  0.013
 0.050  0.013
 0.100  0.013
 0.200  0.012
 0.500  0.012
 1.000  0.011
== bpmnr sweep A on C,O
weight on alignment (base C,O equal)  q
 0.000  0.986
 0.001  0.790
 0.010  0.790
 0.050  0.790
 0.100  0.789
 0.200  0.789
 0.500  0.789
 1.000  0.794
== bpmnr sweep F on C,O
weight on flow (base C,O equal)  q
 0.000  0.986
 0.001  0.811
 0.010  0.811
 0.050  0.811
 0.100  0.811
 0.200  0.811
 0.500  0.810
 1.000  0.819
== bpmnr sweep A on C,O, A3 kernel
weight on alignment (base C,O equal)  q
 0.000  0.986
 0.001  0.387
 0.010  0.380
 0.050  0.374
 0.100  0.372
 0.200  0.369
 0.500  0.362
 1.000  0.334
FIT_BPMNR_DONE
