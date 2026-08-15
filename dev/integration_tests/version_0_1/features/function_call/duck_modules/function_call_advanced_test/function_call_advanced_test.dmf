# Test advanced function call features
# Focus: default parameters and named arguments

# Functions with default parameters
fun withOneDefault(a: i64, b: i64 = 100) -> i64 = a + b;

fun withTwoDefaults(a: i64, b: i64 = 10, c: i64 = 20) -> i64 = a + b + c;

fun allDefaults(x: i64 = 1, y: i64 = 2, z: i64 = 3) -> i64 = x * 100 + y * 10 + z;

# Functions for named argument testing
fun position(x: i64, y: i64, z: i64) -> i64 = x * 100 + y * 10 + z;

fun rectangle(width: i64, height: i64) -> i64 = width * height;

fun main() -> i64 = {
    # Test single default parameter
    builtin_output_i64(withOneDefault(5));       # 5 + 100 = 105
    builtin_output_i64(withOneDefault(5, 15));   # 5 + 15 = 20

    # Test multiple default parameters
    builtin_output_i64(withTwoDefaults(1));      # 1 + 10 + 20 = 31
    builtin_output_i64(withTwoDefaults(1, 2));   # 1 + 2 + 20 = 23
    builtin_output_i64(withTwoDefaults(1, 2, 3));# 1 + 2 + 3 = 6

    # Test all defaults
    builtin_output_i64(allDefaults());           # 123

    # Test named arguments - reordering
    builtin_output_i64(position(1, 2, 3));       # 123
    builtin_output_i64(position(z = 1, y = 2, x = 3)); # 321
    builtin_output_i64(position(y = 5, x = 1, z = 9)); # 159

    # Test named arguments with partial ordering
    builtin_output_i64(position(1, z = 3, y = 2));  # 123

    # Test named arguments for clarity
    builtin_output_i64(rectangle(width = 5, height = 4));  # 20
    builtin_output_i64(rectangle(height = 4, width = 5));  # 20

    # Test named arguments combined with defaults
    builtin_output_i64(allDefaults(z = 9));      # 1*100 + 2*10 + 9 = 129
    builtin_output_i64(allDefaults(x = 5, z = 7)); # 5*100 + 2*10 + 7 = 527

    return 0;
}
