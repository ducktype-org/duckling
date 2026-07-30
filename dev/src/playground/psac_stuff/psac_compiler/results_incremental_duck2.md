# Incremental update latency — duck2 (-j 8, 5 runs/scenario)

| compiler | scenario | min (ms) | mean (ms) | recompiled |
|:---|:---|---:|---:|:---|
| psacklinger | noop | 100.7 | 107.2 | recompiled 0/153 functions |
| psacklinger | body | 134.0 | 140.0 | recompiled 1/153 functions |
| psacklinger | gvar | 136.5 | 140.2 | recompiled 0/153 functions |
| querylinger | noop | 103.3 | 106.4 | recompiled 0/153 functions |
| querylinger | body | 139.2 | 146.3 | recompiled 1/153 functions |
| querylinger | gvar | 139.1 | 147.7 | recompiled 0/153 functions |
