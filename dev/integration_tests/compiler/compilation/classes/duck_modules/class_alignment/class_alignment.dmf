# This test checks that alignment is properly computed for classes, that is
# it is taken to be the maximum alignment of its members.
#
# The alignment of Inner is 4 bytes (driven by its i32 members, not its 8-byte total size).
# The alignment of Outer is therefore also 4 bytes:
#   max(align(a), align(b), align(c), align(inner)) = max(4, 4, 4, 4) = 4.
# The LLVM struct layout must agree with what the compiler computes.

class Inner {
    x: i32 = 0;
    y: i32 = 0;
}

class Outer {
    a: i32 = 0;
    b: i32 = 0;
    c: i32 = 0;
    inner: Inner = Inner(0, 0);
}

fun main() -> i64 = {
    # Construction
    var o = Outer(1, 2, 3, Inner(4, 5));
    builtin_output_i64(o.a);      # 1
    builtin_output_i64(o.b);      # 2
    builtin_output_i64(o.c);      # 3
    builtin_output_i64(o.inner.x); # 4
    builtin_output_i64(o.inner.y); # 5

    # Assignment to scalar field
    o.a = 10;
    builtin_output_i64(o.a);      # 10

    # Assignment to nested class field
    o.inner = Inner(20, 30);
    builtin_output_i64(o.inner.x); # 20
    builtin_output_i64(o.inner.y); # 30

    # Copy
    var p = o;
    p.b = 99;
    builtin_output_i64(o.b);      # 2  (original unchanged)
    builtin_output_i64(p.b);      # 99

    return 0i64;
}
