# Simple self-contained test for empty classes.
# This test checks that empty classes are handled similarly to unit types
# and other information-less types, i.e. their instances are entirely removed
# from the generated code. The only place where they are kept are in function
# return types, where they are converted to LLVM's void type anyway.

class Unit {}

let a: Unit = Unit();

fun foo(u: Unit) -> Unit = {
    return u;
}

fun main() -> i64 = {
    foo(a);
    return 0i64;
}
