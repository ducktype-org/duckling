### Tree evaluation.
const SIMPLE_REF = ref i64;
const SIMPLE_BOX = box i64;
const SIMPLE_CONST = const i64;
const SIMPLE_VARIANT = i32 | f64;
const SIMPLE_TUPLE: type = (i32, f64);
const CMP_1 = (const i32) == createConst(i32);
const CMP_2 = i32 != i64;

### Function evaluation.
fun createUnit() -> type = ();
fun createBox(t: type) -> type = box t;
fun createRef(t: type) -> type = ref t;
# @TODO: #1727 Change the following, when this gets fixed.
fun createConst(t: type) -> type = (const t);
fun createVariant(a: type, b: type, c: type, d: type) -> type = a | b | c | d;
fun createTuple(a: type, b: type, c: type, d: type) -> type = (a, b, c, d);
fun megaType(a: type) -> type = (createUnit(), createBox(a), i32, (i16, f16), i64 | (f64, f128));

const A = createUnit();
const B = createBox(i32);
const C = createRef(i32);
const D = createConst(i32);
const E = createVariant(i16, i32, i64, i128);
const F = createTuple(i16, i32, i64, i128);
const megaGigaType = megaType(i128);

fun fib(n: i64) -> i64 = {
   if (n < 2) { return n; }
   return fib(n - 1) + fib(n - 2);
}

fun complexLogic(n: i64) -> type = {
    if (fib(n) < 15) {
        return createBox(i16);
    } else {
        return createRef(i64);
    }
}

const FIRST = complexLogic(5);
const SECOND = complexLogic(10);

fun isMegaWithI128(a: type) -> bool = {
    if (a == (i16, i32, i64, i128)) {
        return true;
    } else {
        return false;
    }
}
const IS_MEGA = isMegaWithI128((i16, i32, i64, i128));
const NOT_IS_MEGA = isMegaWithI128((i16, f32, i64, i128));


fun isI32(a: type) -> bool = {
    if (a == i32) {
        return true;
    } else {
        return false;
    }
}
const REAL = isI32(i32);
const FAKE = isI32(i64);

