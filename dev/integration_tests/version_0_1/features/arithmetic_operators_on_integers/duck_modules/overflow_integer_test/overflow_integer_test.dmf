# Test unsigned integer overflow (wraparound)

fun main() -> i64 = {
    # Unsigned integer wraparound
    var zero: u64 = 0;
    var max_u16: u16 = 65535;
    var max_u32: u32 = 4294967295;
    var max_u64: u64 = 18446744073709551615;

    # Overflow addition (unsigned)
    builtin_output_u64(max_u64 + 1u64);   # 0 (wraparound)
    builtin_output_u64(max_u32 + 1u32);   # 0 (wraparound)
    builtin_output_u64(max_u16 + 1u16);   # 0 (wraparound)

    # Overflow multiplication (unsigned)
    builtin_output_u64(max_u64 * 2);     # 18446744073709551614 (wraparound)
    builtin_output_u64(max_u32 * 2);     # 4294967294 (wraparound)
    builtin_output_u64(max_u16 * 2);     # 65534 (wraparound)

    # Overflow subtraction (unsigned)
    builtin_output_u64(zero - 1);      # 18446744073709551615 (wraparound)
    builtin_output_u64(zero - max_u32);  # 18446744069414584321 (wraparound)
    builtin_output_u64(zero - max_u16);  # 18446744073709486081 (wraparound)

    # Signed integer wraparound
    var zero_s: i64 = 0;
    var max_i16: i16 = 32767;
    var min_i16: i16 = -32768;
    var max_i32: i32 = 2147483647;
    var min_i32: i32 = -2147483648;
    var max_i64: i64 = 9223372036854775807;
    var min_i64: i64 = -9223372036854775808;

    # Overflow addition (signed)
    builtin_output_i64(max_i64 + 1i64);   # -9223372036854775808 (wraparound)
    builtin_output_i64(max_i32 + 1i32);   # -2147483648 (wraparound)
    builtin_output_i64(max_i16 + 1i16);   # -32768 (wraparound)

    # Underflow subtraction (signed)
    builtin_output_i64(min_i64 - 1i64);   # 9223372036854775807 (wraparound)
    builtin_output_i64(min_i32 - 1i32);   # 2147483647 (wraparound)
    builtin_output_i64(min_i16 - 1i16);   # 32767 (wraparound)

    # Overflow multiplication (signed)
    builtin_output_i64(max_i64 * 2);     # -2 (wraparound)
    builtin_output_i64(max_i32 * 2);     # -2 (wraparound)
    builtin_output_i64(max_i16 * 2);     # -2 (wraparound)

    # Underflow multiplication (signed)
    builtin_output_i64(min_i64 * 2);     # 0 (wraparound)
    builtin_output_i64(min_i32 * 2);     # 0 (wraparound)
    builtin_output_i64(min_i16 * 2);     # 0 (wraparound)

    return 0;
}

