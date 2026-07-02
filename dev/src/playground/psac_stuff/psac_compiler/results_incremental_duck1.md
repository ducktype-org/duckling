# Incremental update latency — duck1 (-j 8, 5 runs/scenario)

| compiler | scenario | min (ms) | mean (ms) | recompiled |
|:---|:---|---:|---:|:---|
| psacklinger | noop | 98.9 | 101.2 | recompiled 0/352 functions |
| psacklinger | body | 144.5 | 152.8 | recompiled 1/352 functions |
| psacklinger | gvar | 208.7 | 222.8 | recompiled 352/352 functions |
| querylinger | noop | 98.0 | 101.6 | recompiled 0/352 functions |
| querylinger | body | 141.8 | 149.6 | recompiled 1/352 functions |
| querylinger | gvar | 145.5 | 153.1 | recompiled 0/352 functions |
