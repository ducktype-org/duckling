# `ptrof` always lowers to an `AddressOf`, whatever the reference kind of its operand is.
fun ptr_of(m: manyptr i32) -> () = {
    var a: i32 = 1;
    var b: box i32 = new a;

    var p_direct = ptrof a;    # AddressOf of a plain local.
    var p_box = ptrof b;       # AddressOf of the box itself, `&b` would forward it instead.
    var p_elem = ptrof m[1];   # AddressOf of an indexed place.
}
