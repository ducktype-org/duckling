import submodule_a as A;
import submodule_b as B;
import submodule_b.submodule_c as C;

fun foo(a: i64) -> i64 = {
    return 1000;
}

fun main() -> i64 = {

    builtin_output_i64(foo(0));
    builtin_output_i64(A.foo(0));
    builtin_output_i64(A.foo());
    builtin_output_i64(B.foo(0));
    builtin_output_i64(B.foo(0.));
    builtin_output_i64(C.foo(a=0));
    builtin_output_i64(C.foo(b=0));
    
    return 0;
}
