N = 10000

def make_duck_fun(i):
    return f"fun big_fun_{i}(x: i64) -> i64 = {{ return x + {i}; }}"

def make_cpp_fun(i):
    return f"long big_fun_{i}(long x) {{ return x + {i}; }}"

with open("big/big.dmf", "w") as duck:
    for i in range(N):
        duck.write(make_duck_fun(i) + "\n")

with open("big/big.cpp", "w") as cpp:
    for i in range(N):
        cpp.write(make_cpp_fun(i) + "\n")


