import core.builtins.*;

template(a: i64)
const c = a;

template(v: i64)
const b = c:{v} + 1;

fun main() -> i64 = {
    builtin_output_i64(b:{1i64});
    return 0;
}
