\page vm-old-andrzej-raport Raport Andrzeja

## Test 5.06

| name | input | VM time (no debug) | VM time (with debug) | Java time | Java no jit | JS |  JS no jit | Python |
| :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: 
|collatz | 10^5 | 0.182s | 5.041s | 0.079s | 0.283s | 0.063s | 0.532s | 1.202s |
|collatz | 10^6 | 2.094s | 61.335s | 0.288s | 2.540s | 1.125s | 4.589s | 14.314s |
|collatz | 10^7 | 24.732s | 723.313s | 2.857s | 29.527s | 14.066s | 53.433s | 168.621s |