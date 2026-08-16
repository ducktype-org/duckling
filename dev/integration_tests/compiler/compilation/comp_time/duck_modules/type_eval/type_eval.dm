# This test tests the compile time evaluation of meta types. 
# Note that for more complex types we create bool constants which 
# we check at runtime to assert their correctness, since some of 
# the used types are not implemented.

# ========== Helpers ==========
class T {}
class U {}

# A class with a non-trivial layout: i8 at offset 0, i64 (align 8) at offset 8,
# i16 at offset 16 -> 18 bytes of payload rounded up to the 8-byte alignment,
# so size 24 bytes, alignment 8 bytes.
class Mixed { a: i8; b: i64; c: i16; }

fun createUnit() -> type = ();
fun createBox(t: type) -> type = box t;
fun createRef(t: type) -> type = ref t;
fun createPtr(t: type) -> type = ptr t;
fun createCPtr(t: type) -> type = cptr t;
fun createManyPtr(t: type) -> type = manyptr t;
fun createSlice(t: type) -> type = slice t;
fun createVariant(a: type, b: type, c: type, d: type) -> type = a | b | c | d;
fun createTuple(a: type, b: type, c: type, d: type) -> type = (a, b, c, d);
fun createShortTuple(a: type, b: type) -> type = (a, b);
fun createMegaType(a: type) -> type = (createUnit(), createBox(a), i32, (i16, f16), i64 | (f64, f128));
fun createInt() -> type = i32;
fun createBigInt() -> type = i64;
fun getT() -> type = T;
fun getU() -> type = U;

fun typesEqual(a: type, b: type) -> bool = a == b;
fun typesNotEqual(a: type, b: type) -> bool = a != b;

fun getTuple(order: i32, a: type, b: type) -> type = {
    if (order == 1) {
        return (a, b, createRef(a));
    } else {
        return (a, b, createRef(b));
    }
}

fun getVariant(order: i32, a: type, b: type) -> type = {
    if (order == 1) {
        return a | b | createRef(a);
    } else {
        return a | b | createRef(b);
    }
}

# ========== Simple Types ==========
const I32_TYPE = i32;
const I64_TYPE = i64;
const BOX_I32 = createBox(i32);
const REF_I32 = createRef(i32);
const MEGA_TYPE_I128 = createMegaType(i128);
const MEGA_TYPE_MANUAL: type = ((), box i128, i32, (i16, f16), i64 | (f64, f128));
const MEGA_TYPE_DIFFERENT = createMegaType(i64);
const TUPLE_A = createShortTuple(i32, f32);
const TUPLE_B: type = (i32, f32);
const A = createInt();
const B: A = 123;

# ========== Pointer / slice constructors ==========
const PTR_I32 = createPtr(i32);
const CPTR_I32 = createCPtr(i32);
const MANYPTR_I32 = createManyPtr(i32);
const SLICE_I32 = createSlice(i32);

# ========== size_of / alignment_of ==========
const MIXED_SIZE: i64 = size_of(Mixed);
const MIXED_ALIGN: i64 = alignment_of(Mixed);

# ========== Complex Types ==========
const T_TYPE = T;
const U_TYPE = U;
const T2_TYPE = getT();
const T_ARRAY = getT()[5];      # T[5]
const T_LIST = List[getT()];    # List[T]
const T_IN_TUPLE: type = (i32, T, T, U);
const T_IN_TUPLE_MANUAL: type = (i32, getT(), getT(), getU());

# ========== Tests ==========
const CMP1 = typesEqual(I32_TYPE, i32);
const CMP2 = typesNotEqual(I32_TYPE, I64_TYPE);
const CMP3 = typesEqual(MEGA_TYPE_I128, MEGA_TYPE_MANUAL);
const CMP4 = typesNotEqual(MEGA_TYPE_I128, MEGA_TYPE_DIFFERENT);
const CMP5 = typesNotEqual(BOX_I32, REF_I32);
const CMP6 = typesEqual(TUPLE_A, TUPLE_B);
const CMP7 = typesNotEqual(I32_TYPE, I64_TYPE);
const CMP8 = typesEqual(BOX_I32, createBox(i32));
const CMP9 = typesEqual(TUPLE_A, TUPLE_B);
const CMP10 = typesEqual(MEGA_TYPE_I128, MEGA_TYPE_MANUAL);
const CMP11 = typesNotEqual(MEGA_TYPE_I128, MEGA_TYPE_DIFFERENT);
const CMP12 = typesEqual(T, T);
const CMP13 = typesEqual(T_TYPE, T2_TYPE);
const CMP14 = typesNotEqual(T, U);
const CMP15 = typesNotEqual(T, i32);
const CMP16 = typesEqual(T_ARRAY, T[5]);
const CMP17 = typesEqual(T_LIST, List[T]);
const CMP18 = typesEqual(T_IN_TUPLE, T_IN_TUPLE_MANUAL);
const CMP19 = typesEqual(T_IN_TUPLE, createTuple(i32, T, T, U));
const CMP20 = typesEqual((T, U, ref T), getTuple(1, T, U));
const CMP21 = typesEqual((T, U, ref U), getTuple(0, T, U));
const CMP22 = typesEqual((T, U, ref U), getTuple(0, T, U));
const CMP23 = typesEqual(T | U | ref U, getVariant(0, T, U));
const CMP24 = typesEqual(T | U | ref T, getVariant(1, T, U));

# Pointer / slice constructors produce the same type as the literal operators.
const CMP27 = typesEqual(PTR_I32, ptr i32);
const CMP28 = typesNotEqual(createPtr(i32), ptr i64);
const CMP29 = typesEqual(CPTR_I32, cptr i32);
const CMP30 = typesEqual(MANYPTR_I32, manyptr i32);
const CMP31 = typesEqual(SLICE_I32, slice i32);
const CMP32 = typesNotEqual(SLICE_I32, slice i64);
# ptr, cptr and manyptr of the same pointee are all distinct types.
const CMP33 = typesNotEqual(PTR_I32, MANYPTR_I32);
const CMP34 = typesNotEqual(PTR_I32, CPTR_I32);

# size_of / alignment_of of a class with a non-trivial layout.
const CMP35 = MIXED_SIZE == 24;
const CMP36 = MIXED_ALIGN == 8;
const CMP37 = size_of(i32) == 4;
const CMP38 = alignment_of(i64) == 8;

fun main() -> i64 = {
    if (not CMP1) { builtin_output_i64(1); }
    if (not CMP2) { builtin_output_i64(2); }
    if (not CMP3) { builtin_output_i64(3); }
    if (not CMP4) { builtin_output_i64(4); }
    if (not CMP5) { builtin_output_i64(5); }
    if (not CMP6) { builtin_output_i64(6); }
    if (not CMP7) { builtin_output_i64(7); }
    if (not CMP8) { builtin_output_i64(8); }
    if (not CMP9) { builtin_output_i64(9); }
    if (not CMP10) { builtin_output_i64(10); }
    if (not CMP11) { builtin_output_i64(11); }
    if (not CMP12) { builtin_output_i64(12); }
    if (not CMP13) { builtin_output_i64(13); }
    if (not CMP14) { builtin_output_i64(14); }
    if (not CMP15) { builtin_output_i64(15); }
    if (not CMP16) { builtin_output_i64(16); }
    if (not CMP17) { builtin_output_i64(17); }
    if (not CMP18) { builtin_output_i64(18); }
    if (not CMP19) { builtin_output_i64(19); }
    if (not CMP20) { builtin_output_i64(20); }
    if (not CMP21) { builtin_output_i64(21); }
    if (not CMP22) { builtin_output_i64(22); }
    if (not CMP23) { builtin_output_i64(23); }
    if (not CMP24) { builtin_output_i64(24); }

    let sum: createInt() = 2147483647;
    let sum2: A = 2147483647;

    # If a correct type was evaluated this should be an overflow. So the result should be < 0.
    if (not (sum + 1 < 0)) { builtin_output_i64(25); }
    if (not (sum2 + 1 < 0)) { builtin_output_i64(26); }

    if (not CMP27) { builtin_output_i64(27); }
    if (not CMP28) { builtin_output_i64(28); }
    if (not CMP29) { builtin_output_i64(29); }
    if (not CMP30) { builtin_output_i64(30); }
    if (not CMP31) { builtin_output_i64(31); }
    if (not CMP32) { builtin_output_i64(32); }
    if (not CMP33) { builtin_output_i64(33); }
    if (not CMP34) { builtin_output_i64(34); }
    if (not CMP35) { builtin_output_i64(35); }
    if (not CMP36) { builtin_output_i64(36); }
    if (not CMP37) { builtin_output_i64(37); }
    if (not CMP38) { builtin_output_i64(38); }

    return 0;
}

