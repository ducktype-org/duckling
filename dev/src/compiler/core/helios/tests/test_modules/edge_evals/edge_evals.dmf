const M1: i32 = 10 / 2 / 5;
const M2: i32 = 120 / 5 / 4;

# This does not parse well.
# const M3: i64 = 1 * -1;
# const M5

namespace NS1 {
    namespace NS2 {
        const A: i64 = 7;
    }
}

const O1: i64 = (NS1).NS2.A;
const O2: i64 = ((NS1).NS2).A;

# These do not work anymore (which is intended):
# const O3: i64 = NS1.(NS2).A;
# const O4: i64 = (NS1).(NS2).A;

# Comparison chains are evaluated lazily: `2 < 1` is false, so the division by zero in
# the next comparison is never evaluated.
const P1: bool = 2 < 1 < 3 / 0;

# The untaken ternary branch is not evaluated either.
const P2: i32 = if false then 1 / 0 else 7;
