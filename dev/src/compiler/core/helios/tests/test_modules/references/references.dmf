class RefPoint { x: ref i32; y: ref i32; }

fun test_simple_ref() -> i32 = {
    var x: i32 = 10;
    var r: ref i32 = &x;
    r = 42;                     # Deref of lhs.
    var y: i32 = r;             # Deref of rhs.
    var r2: ref i32 = &y;
    r = r2;                     # Reference rebinding -> no derefs.
    r = r + 1;                  # Deref of both sides.
    dummy(r);                   # Deref in arguments.
    var z: i32 = -r;            # Deref in unary.
    var p: i32 = r + r2;        # Deref in binary.

    var point = RefPoint(&x, &y);
    var ref_point = &point;
    var val: i32 = ref_point.x; # Two derefs should appear. One for `ref_point`, one for `x`.
    
    return r;                   # Deref in return.
}

fun dummy(x: i32) -> i32 = x;
