class Point {
    x: i32;
    y: i32;
}

fun by_ref(p : ref Point) -> i32 = {
    return p.x;
}

fun by_val(p : Point) -> i32 = {
    return p.x;
}

fun take_int_ref(x: ref i32) -> i64 = {
    return x;
}

fun test_boxes() -> i64 = {
    var b_int: box i32 = new 42;    # `box_alloc` should be inserted.
    var b_point: box Point = new Point(10, 20);

    take_int_ref(&b_int);
    
    var x: i32 = b_point.x;     # Deref on the rhs.
    var y: i32 = b_point.y;
    b_point.y = 99;             # Deref on the lhs.

    by_val(b_point);            # Box -> Direct coercion. Should copy
    by_ref(&b_point);      # Box -> Ref.
    
    return b_int;               # Box -> Direct coercion;
}
