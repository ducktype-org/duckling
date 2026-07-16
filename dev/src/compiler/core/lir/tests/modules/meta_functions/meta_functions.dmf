fun createBox(t: type) -> type = box t;
fun createRef(t: type) -> type = ref t;
fun createPtr(t: type) -> type = ptr t;
fun createCPtr(t: type) -> type = cptr t;
fun createManyPtr(t: type) -> type = manyptr t;
fun createSlice(t: type) -> type = slice t;
# @TODO: #1727 Change the following, when this gets fixed.
fun createConst(t: type) -> type = (const t);
fun createVariant(a: type, b: type, c: type, d: type) -> type = a | b | c | d;
fun createTuple(a: type, b: type, c: type, d: type) -> type = (a, b, c, d);
fun megaType(a: type) -> type = ((), a, createBox(i32), (i16, f16), i64 | (f64, f128));
