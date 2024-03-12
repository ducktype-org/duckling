# Wersje maszyn:
JavaScript: 20.2.0 z https://nodejs.org/en
Python: 3.11.3 z https://www.python.org/downloads/release/python-3113/
Java 20.0.1 z https://jdk.java.net/20/

python: ./configure --enable-optimizations --with-lto i później 

# Wykonanie:
Pamiętajcie o używaniu dobrej ścieżki do najnowszej wersji, JDK i nodejs przychodzą już skompilowane, python u mnie zainstalował się jako python3.11

Ustawcie komputer na najwyższe obroty jakie pozwala tak sensownie odpalić. Bez niepotrzebnych programów włączonych w tle i odpalajcie z czystego terminala.

## JavaScript:
Dwie wersje uruchomienia
1. Z JIT-em: `time node {plik}.js < input{n}.in > dump.out`
2. Bez JIT-a: `time node --jitless {plik}.js < input{n}.in > dump.out`
## Python:
`time python3 {plik}.py < input{n}.in > dump.out`
## Java:
`javac {plik}.java`
Dwie wersje uruchomienia
1. Z JIT-em: `time java {plik} < input{n}.in > dump.out`
2. Bez JIT-a: `time java -Xint {plik} < input{n}.in > dump.out`

## RiftVM:
Dwie wersje:
1. RiftVM + debug:  
W src/services/executor_f8/op_case_config.hpp:
```
constexpr bool IGNORE_EXECUTION_STRATEGY = false;
// #define USE_COMPUTED_GOTO
// #define USE_FLAT_FRAME
```
`time ./RiftVM -f {plik}.rbc < input{n}.in > dump.out`
2. RiftVM:  
W src/services/executor_f8/op_case_config.hpp:
```
constexpr bool IGNORE_EXECUTION_STRATEGY = true;
#define USE_COMPUTED_GOTO
// #define USE_FLAT_FRAME
```
`time ./RiftVM -f {plik}.rbc < input{n}.in > dump.out`

# Raportowanie
W rozdziale 7 w pracy trzeba wpisać swój procesor i ilość i rodzaj (DDR3/DDR4 itp) ramu i swój system operacyjny. Dalej wypełnijcie odpowiednie tabelki z prędkością w sekundach zaokrąglone do 2 miejsc po przecinku. Bierzemy czas "real"

Dobrze znaleźć kogoś kto ma ARMa na kompie, bo to może być ciekawe