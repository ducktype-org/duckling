fun main() -> i64 = {
    let x: i64 = 1;
    let y: i64 = 1;

    if (1 > 2) { 
        builtin_output_i64(1); # not printed
    }

    if (2i64 != 1i64 == x >= -1i64) {
        builtin_output_i64(2); # 2
    }

    if (x <= (y + 2i64) < 10i64) {
        builtin_output_i64(3); # 3
    }

    let a: i64 = 1;
    let b: i64 = 2;

    if (a < b <= b == b != b >= b > a) { 
        builtin_output_i64(4); # not printed
    }

    return 0i64;
}
