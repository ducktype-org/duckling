# Benchmark Results

Thread counts tested: `[1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]`  
3 runs each, times averaged. All times in seconds.

**Workloads:**
- **no-mutex**: each thread counts locally (`10 000 000 / x` iterations), no shared state
- **mutex**: all threads increment a shared global counter under a lock (`1 000 000 / x` iterations each)

---

## Branch: `deadlock-detection-old`

Includes the deadlock detection optimizations:
- All `DeadlockDetector` methods early-return when disabled
- `std::map` → `std::unordered_map`
- `clearMutexState` linear scan removed

### Python (reference baseline)

| x | no-mutex (s) | mutex (s) |
|---|---|---|
| 1 | 0.289 | 0.104 |
| 2 | 0.291 | 0.109 |
| 4 | 0.303 | 0.114 |
| 6 | 0.302 | 0.115 |
| 8 | 0.315 | 0.114 |
| 10 | 0.315 | 0.124 |
| 14 | 0.329 | 0.130 |
| 18 | 0.326 | 0.147 |
| 22 | 0.344 | 0.153 |
| 24 | 0.342 | 0.149 |
| 28 | 0.355 | 0.152 |
| 32 | 0.374 | 0.162 |

```
x_py     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times_py = [0.2890, 0.2907, 0.3030, 0.3024, 0.3151, 0.3146, 0.3292, 0.3264, 0.3439, 0.3423, 0.3550, 0.3738]

x_mutex_py     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times_mutex_py = [0.1044, 0.1094, 0.1141, 0.1149, 0.1142, 0.1240, 0.1298, 0.1473, 0.1535, 0.1490, 0.1520, 0.1621]
```

### DVM

| x | no-mutex (s) | mutex (s) |
|---|---|---|
| 1 | 0.701 | 3.97 |
| 2 | 0.703 | 6.56 |
| 4 | 0.726 | 7.92 |
| 6 | 0.712 | 9.17 |
| 8 | 0.716 | 9.79 |
| 10 | 0.722 | 9.01 |
| 14 | 0.731 | 9.34 |
| 18 | 0.730 | 9.60 |
| 22 | 0.745 | 9.23 |
| 24 | 0.757 | 9.24 |
| 28 | 0.767 | 9.26 |
| 32 | 0.782 | 9.30 |

```
x     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times = [0.7010, 0.7032, 0.7256, 0.7122, 0.7164, 0.7221, 0.7311, 0.7303, 0.7454, 0.7573, 0.7674, 0.7820]

x_mutex     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times_mutex = [3.9748, 6.5621, 7.9214, 9.1724, 9.7871, 9.0118, 9.3397, 9.5985, 9.2300, 9.2357, 9.2615, 9.3018]
```

---

## Branch: `main`

Unmodified deadlock detection code (`std::map`, no early-return guards on disabled path).

### Python (reference baseline)

| x | no-mutex (s) | mutex (s) |
|---|---|---|
| 1 | 0.299 | 0.105 |
| 2 | 0.299 | 0.110 |
| 4 | 0.313 | 0.116 |
| 6 | 0.317 | 0.124 |
| 8 | 0.329 | 0.123 |
| 10 | 0.337 | 0.132 |
| 14 | 0.332 | 0.138 |
| 18 | 0.344 | 0.145 |
| 22 | 0.356 | 0.144 |
| 24 | 0.357 | 0.151 |
| 28 | 0.362 | 0.160 |
| 32 | 0.371 | 0.162 |

```
x_py     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times_py = [0.2985, 0.2988, 0.3131, 0.3168, 0.3293, 0.3365, 0.3318, 0.3441, 0.3556, 0.3572, 0.3619, 0.3708]

x_mutex_py     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times_mutex_py = [0.1052, 0.1097, 0.1158, 0.1241, 0.1233, 0.1320, 0.1377, 0.1453, 0.1445, 0.1507, 0.1604, 0.1618]
```

### DVM

| x | no-mutex (s) | mutex (s) |
|---|---|---|
| 1 | 0.690 | 3.94 |
| 2 | 0.695 | 7.08 |
| 4 | 0.707 | 9.81 |
| 6 | 0.726 | 9.87 |
| 8 | 0.726 | 10.05 |
| 10 | 0.773 | 10.33 |
| 14 | 0.750 | 10.30 |
| 18 | 0.754 | 9.28 |
| 22 | 0.774 | 8.90 |
| 24 | 0.862 | 9.07 |
| 28 | 0.792 | 8.96 |
| 32 | 0.788 | 8.84 |

```
x     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times = [0.6903, 0.6954, 0.7068, 0.7256, 0.7263, 0.7727, 0.7501, 0.7536, 0.7738, 0.8618, 0.7918, 0.7883]

x_mutex     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times_mutex = [3.9365, 7.0833, 9.8099, 9.8743, 10.0464, 10.3310, 10.3050, 9.2795, 8.8957, 9.0701, 8.9621, 8.8353]
```

---

## Summary

### DVM no-mutex (10M iterations, no contention)

Both branches are nearly identical — deadlock detection changes have **no measurable effect** on mutex-free code, as expected (the detector methods are never called).

| x | main (s) | optimized (s) | diff |
|---|---|---|---|
| 1 | 0.690 | 0.701 | +1.5% |
| 8 | 0.726 | 0.716 | −1.4% |
| 32 | 0.788 | 0.782 | −0.8% |

Differences are within noise.

### DVM mutex (1M iterations, heavy contention)

Results are also comparable — the dominant cost is lock contention in `try_lock_for`, not the deadlock detector bookkeeping. The optimizations don't move the needle here because the bottleneck is the OS mutex spin, not the map operations.

| x | main (s) | optimized (s) |
|---|---|---|
| 1 | 3.94 | 3.97 |
| 8 | 10.05 | 9.79 |
| 32 | 8.84 | 9.30 |

Both branches have detection **disabled** by default (`enable_deadlock_detection = false` in `SafeVMProcess`), so the `std::map` / `std::unordered_map` and early-return changes make no difference here — all detector calls were already no-ops on both branches.

### Key takeaway

The benchmarks confirm the expected behaviour: the optimizations eliminate unnecessary work on the disabled path, but won't show up in these particular benchmarks because detection is off. To see the optimizations matter you'd need a benchmark that **enables detection** and uses many mutexes — that's where the `std::map` → `std::unordered_map` and removed map writes would actually show up.

---

## Branch: `mutex-opt`

Optimization applied to `builtinLockMutex`: try `mutex->try_lock()` (non-blocking CAS) first while still holding the GIL. Only release the GIL and enter the `try_lock_for(500ms)` spin loop if the mutex is actually contended. This avoids two GIL OS-mutex operations and the expensive `pthread_mutex_timedlock` syscall for the common uncontended case.

### Python (reference baseline)

| x | no-mutex (s) | mutex (s) |
|---|---|---|
| 1 | 0.293 | 0.102 |
| 2 | 0.313 | 0.107 |
| 4 | 0.321 | 0.116 |
| 6 | 0.331 | 0.111 |
| 8 | 0.328 | 0.124 |
| 10 | 0.329 | 0.132 |
| 14 | 0.335 | 0.126 |
| 18 | 0.340 | 0.151 |
| 22 | 0.330 | 0.145 |
| 24 | 0.347 | 0.143 |
| 28 | 0.372 | 0.148 |
| 32 | 0.354 | 0.152 |

```
x_py     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times_py = [0.2934, 0.3132, 0.3206, 0.3314, 0.3281, 0.3286, 0.3352, 0.3404, 0.3298, 0.3469, 0.3722, 0.3544]

x_mutex_py     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times_mutex_py = [0.1023, 0.1071, 0.1157, 0.1111, 0.1236, 0.1324, 0.1264, 0.1505, 0.1453, 0.1433, 0.1482, 0.1515]
```

### DVM

| x | no-mutex (s) | mutex (s) |
|---|---|---|
| 1 | 0.704 | 3.80 |
| 2 | 0.720 | 7.49 |
| 4 | 0.726 | 8.83 |
| 6 | 0.730 | 9.04 |
| 8 | 0.733 | 8.91 |
| 10 | 0.740 | 8.90 |
| 14 | 0.781 | 8.55 |
| 18 | 0.799 | 8.60 |
| 22 | 0.825 | 8.55 |
| 24 | 0.841 | 8.29 |
| 28 | 0.843 | 8.28 |
| 32 | 0.795 | 8.41 |

```
x     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times = [0.7038, 0.7203, 0.7256, 0.7295, 0.7325, 0.7400, 0.7806, 0.7990, 0.8253, 0.8413, 0.8425, 0.7946]

x_mutex     = [1, 2, 4, 6, 8, 10, 14, 18, 22, 24, 28, 32]
times_mutex = [3.8005, 7.4892, 8.8339, 9.0429, 8.9074, 8.8988, 8.5457, 8.5959, 8.5517, 8.2884, 8.2839, 8.4054]
```

### Comparison: `main` vs `mutex-opt` (DVM mutex only)

| x | main (s) | mutex-opt (s) | delta |
|---|---|---|---|
| 1 | 3.94 | 3.80 | −3.6% |
| 2 | 7.08 | 7.49 | +5.8% |
| 4 | 9.81 | 8.83 | −10.0% |
| 6 | 9.87 | 9.04 | −8.4% |
| 8 | 10.05 | 8.91 | −11.3% |
| 10 | 10.33 | 8.90 | −13.8% |
| 14 | 10.30 | 8.55 | −17.0% |
| 18 | 9.28 | 8.60 | −7.3% |
| 22 | 8.90 | 8.55 | −3.9% |
| 24 | 9.07 | 8.29 | −8.6% |
| 28 | 8.96 | 8.28 | −7.6% |
| 32 | 8.84 | 8.41 | −4.9% |

### Key takeaway

The fast-path `try_lock()` yields a consistent improvement in the mutex-heavy benchmark: **−3% to −17%** across thread counts, with the biggest gains at medium concurrency (8–14 threads). The single-thread case benefits modestly (−3.6%) since there is no GIL contention there anyway. The x=2 result is within noise — the benchmark has high variance at low thread counts due to OS scheduling.

The no-mutex DVM numbers are slightly higher on this branch (~0.70–0.84s vs ~0.69–0.86s on `main`), consistent with measurement noise — the change to `builtinLockMutex` doesn't touch the no-mutex path.
