# Test namespace declaration
# Per docs: namespace is a construct to group code elements into a common scope

namespace Math {
    var counter: i64 = 0;

    fun increment() -> i64 = {
        counter = counter + 1;
        return counter;
    }

    fun add(a: i64, b: i64) -> i64 = a + b;

    fun multiply(a: i64, b: i64) -> i64 = a * b;
}

# Test nested namespaces (per docs: outer.inner.K)
namespace Outer {
    namespace Inner {
        fun getValue() -> i64 = 42;

        var data: i64 = 100;
    }

    fun outerFunc() -> i64 = 10;
}

# Test namespace with access specifiers (public/private)
namespace Protected {
    public var publicVar: i64 = 50;
    # private var privateVar: i64 = 60;  # Would not be accessible outside

    public fun publicFunc() -> i64 = publicVar * 2;
}

fun main() -> i64 = {
    # Test namespace variable and function
    builtin_output_i64(Math.increment()); # 1
    builtin_output_i64(Math.increment()); # 2

    # Test namespace function with arguments
    builtin_output_i64(Math.add(10, 20)); # 30
    builtin_output_i64(Math.multiply(3, 4)); # 12

    # Test nested namespace access
    builtin_output_i64(Outer.Inner.getValue()); # 42
    builtin_output_i64(Outer.Inner.data);       # 100
    builtin_output_i64(Outer.outerFunc());      # 10

    # Test public access specifier
    builtin_output_i64(Protected.publicVar);    # 50
    builtin_output_i64(Protected.publicFunc()); # 100

    return 0;
}
