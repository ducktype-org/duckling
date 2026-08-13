import core.builtins.*;

extern("C") {
    # In this file SSE register is floating-point register.
    # Small (8 bytes)  -> x86-64: single INTEGER reg, coerced to `{ i64 }`.
    class Small { x: i32 = 0; y: i32 = 0; }

    # Mid nested {{i32, f32}, f32} (12 bytes) -> x86-64: two eightbytes;
    # eightbyte 0 = INTEGER (i32+f32 mixed), eightbyte 1 = SSE, coerced to `{ i64, float }`.
    class MidInner { i: i32 = 0; f: f32 = 0.0f32; }
    class Mid { inner: MidInner = MidInner(); g: f32 = 0.0f32; }

    # Mid2 homogeneous {f64, f64, f64, f64} (32 bytes) -> x86-64: > 16 bytes, passed in
    # memory (`byval`) / returned via `sret`. (AArch64: HFA in 4 SSE regs.)
    class Mid2 { a: f64 = 0.0; b: f64 = 0.0; c: f64 = 0.0; d: f64 = 0.0; }

    # Padded {{i64, i8}, i8}: `PadInner` is 9 bytes of payload rounded up to 16 (align 8), so
    # `Pad.tail` sits at offset 16, after `inner`'s tail padding - not at offset 9.
    class PadInner { a: i64 = 0i64; b: i8 = 0i8; }
    class Pad { inner: PadInner = PadInner(); tail: i8 = 0i8; }

    # Big (24 bytes) -> x86-64: memory, `byval` argument / `sret` return.
    class Big { a: i64 = 0i64; b: i64 = 0i64; c: i64 = 0i64; }

    fundecl take_small(s: Small) -> i64;
    fundecl take_mid(s: Mid) -> i64;
    fundecl take_mid2(s: Mid2) -> i64;
    fundecl take_big(s: Big) -> i64;

    fundecl make_small(seed: i64) -> Small;
    fundecl make_mid(seed: i64) -> Mid;
    fundecl make_mid2(seed: i64) -> Mid2;
    fundecl make_big(seed: i64) -> Big;

    # Sub-word integer args -> x86-64: passed sign-extended to 32 bits by the caller.
    # Declared in a namespace nested inside the extern("C") block: the C ABI (and therefore the
    # unmangled symbol name) has to propagate through the namespace, otherwise linking fails.
    namespace narrow_ints {
        fundecl add_shorts(a: i16, b: i16) -> i32;
        fundecl add_bytes(a: i8, b: i8) -> i32;
    }
}

fun main() -> i64 = {
    builtin_output_i64(take_small(Small(3, 4)));                       # 7
    builtin_output_i64(take_mid(Mid(MidInner(3, 4.0f32), 5.0f32)));    # 12
    builtin_output_i64(take_mid2(Mid2(10.0, 20.0, 30.0, 40.0)));       # 100
    builtin_output_i64(take_big(Big(100i64, 200i64, 300i64)));         # 600

    var s = make_small(1i64);
    builtin_output_i64(s.x);                                           # 1
    builtin_output_i64(s.y);                                           # 2

    # `Small` is two adjacent i32s, so indexing a `cptr` to its first field walks onto the
    # second one - the stride comes from the pointee type.
    let sp = &s.x as cptr i32;
    builtin_output_i64(sp[1]);                                         # 2
    sp[0] = 9;
    builtin_output_i64(s.x);                                           # 9

    # Field access through a `cptr` to a struct must use the same offsets as a direct field
    # access on the value: `tail` is at offset 16, so a read through the pointer that reused
    # the unpadded offset 9 would come back as a byte of `inner`'s padding.
    var pad: Pad;
    pad.inner.a = 5i64;
    pad.inner.b = 6i8;
    pad.tail = 77i8;
    let pad_p = &pad as cptr Pad;
    builtin_output_i64((*pad_p).inner.a);                              # 5
    builtin_output_i64((*pad_p).inner.b as i64);                       # 6
    builtin_output_i64((*pad_p).tail as i64);                          # 77
    (*pad_p).tail = 78i8;
    builtin_output_i64(pad.tail as i64);                               # 78

    let m = make_mid(5i64);
    builtin_output_i64(m.inner.i);                                     # 5
    builtin_output_i64(m.inner.f as i64);                              # 10
    builtin_output_i64(m.g as i64);                                    # 15

    let m2 = make_mid2(7i64);
    builtin_output_i64(m2.a as i64);                                   # 7
    builtin_output_i64(m2.b as i64);                                   # 14
    builtin_output_i64(m2.c as i64);                                   # 21
    builtin_output_i64(m2.d as i64);                                   # 28

    let b = make_big(7i64);
    builtin_output_i64(b.a);                                           # 7
    builtin_output_i64(b.b);                                           # 14
    builtin_output_i64(b.c);                                           # 21

    # 0x0000ABCD: positive as i32, but -21555 once truncated to i16 (clean-zero upper bits).
    let raw_s: i32 = 43981i32;
    builtin_output_i64(narrow_ints::add_shorts(raw_s as i16, 1i16));  # -21554

    # 0x000000C8: -56 as i8; without signext the clang callee reads 200 and prints 201.
    let raw_b: i32 = 200i32;
    builtin_output_i64(narrow_ints::add_bytes(raw_b as i8, 1i8));     # -55

    return 0i64;
}
