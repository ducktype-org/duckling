import core.builtins.*;

template(a: i64)
namespace N {
    var b = a;
}

template(a: i64)
fun get() = a;

fun main() -> i64 = {
    builtin_output_i64(N:{1}.b);
    builtin_output_i64(N:{2}.b);
    builtin_output_i64(N:{1}.b);
    builtin_output_i64(get:{1}());
    builtin_output_i64(get:{2}());
    builtin_output_i64(get:{1}());

    return 0;
}
