import core.builtins.*;

template(a: i64)
namespace Number {
    const inner = a;
}

const one = Number:{0i64}.inner + Number:{1i64}.inner;
const two = Number:{2i64}.inner;

const three_1 = Number:{3i64}.inner;
const three_2 = Number:{1i64}.inner + Number:{2i64}.inner;


template(T: type)
fun identity(t: T) = {
    return t;
}

fun getType() = i64;

const out = identity:{getType()}(2);

fun main() -> i64 = {
    builtin_output_i64(one);
    builtin_output_i64(two);
    builtin_output_i64(three_1);
    builtin_output_i64(three_2);

    builtin_output_i64(out);

    return 0;
}
