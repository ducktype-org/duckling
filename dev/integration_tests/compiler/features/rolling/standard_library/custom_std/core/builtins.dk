namespace dvm {
    extern("DVM") {
        fundecl builtin_output_char(x: char) -> i64;
    }
}

namespace clib {
    extern("C") {
        fundecl putchar(c: i32) -> i32;
    }
}


@backend_dependent
fundecl builtin_output_i64(x: i64) -> i64;

@dvm_only_impl
fun builtin_output_i64(x: i64) -> i64 = {
    dvm.builtin_output_char('x');
    dvm.builtin_output_char('\n');
    return 2;
}

@native_only_impl
fun builtin_output_i64(x: i64) -> i64 = {
    clib.putchar('x' as i32);
    clib.putchar('\n' as i32);
    return 2;
}
