# Incremental update latency — duck1 (-j 8, 5 runs/scenario)

| compiler | scenario | min (ms) | mean (ms) | recompiled |
|:---|:---|---:|---:|:---|
| psacklinger | noop | 105.5 | 109.0 | recompiled 0/352 functions |
| psacklinger | body | 146.0 | 149.8 | recompiled 1/352 functions |
| psacklinger | gvar | 146.4 | 155.9 | recompiled 0/352 functions |
| querylinger | noop | 111.5 | 118.4 | recompiled 0/352 functions |
| querylinger | body | 155.1 | 160.1 | recompiled 1/352 functions |
| querylinger | gvar | 157.9 | 163.7 | recompiled 0/352 functions |
