# Incremental update latency — duck2 (-j 8, 5 runs/scenario)

| compiler | scenario | min (ms) | mean (ms) | recompiled |
|:---|:---|---:|---:|:---|
| psacklinger | noop | 97.4 | 100.8 | recompiled 0/153 functions |
| psacklinger | body | 128.3 | 132.7 | recompiled 1/153 functions |
| psacklinger | gvar | 257.1 | 263.5 | recompiled 153/153 functions |
| querylinger | noop | 98.9 | 101.5 | recompiled 0/153 functions |
| querylinger | body | 129.0 | 134.8 | recompiled 1/153 functions |
| querylinger | gvar | 132.1 | 138.3 | recompiled 0/153 functions |
