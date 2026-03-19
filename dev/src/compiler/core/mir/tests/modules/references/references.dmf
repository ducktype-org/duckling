class Point { x: i32; y: i32; }
class RefPoint { x: ref i32; y: ref i32; }
class RefWrapper { p: ref RefPoint; }

fun references(p: ref Point) -> () = {
    var x: i32 = 123;
    var y: i32 = 456;

    var rx = &x;
    var rpx = &p.x; # AddressOf from [Deref, FieldProjection].

    var point = RefPoint(&x, &y);
    var ref_point = &point;
    var wrapper = RefWrapper(&point);
    var ref_wrapper = &wrapper;
    
    ref_wrapper.p.x = 999; # [Deref, Field, Deref, Field, Deref]
}
