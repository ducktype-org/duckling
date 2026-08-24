# Test types as first-class citizens
# Focus: type aliases and functions taking and returning types

# Note: Per docs, type aliases use const (types are values)
# However, alias can be used for symbol aliasing

const my_i64 = i64;
const another_i64 = i64;

class A {
	var a: i64;
	fun foo() = {
		print("A");
		builtin_output_i64(a);
	}
}

class B {
	var b: i64 = 1;
	fun foo() = {
		print("B");
		builtin_output_i64(b);
	}
}

class C {
	var c: i64;
	fun foo() = {
		print("C");
		builtin_output_i64(c);
	}
}

fun foo(n: i64) -> type = {
	if (n % 3 == 0) {
		return A;
	} else if (n % 3 == 1) {
		return B;
	} else {
		return C;
	}
}

fun make_reverse_tuple(t1: type, t2: type) -> type = (t2, t1);

fun main() -> i64 = {
	var a: my_i64 = 1;
	builtin_output_i64(a);  # 1

	const b = my_i64 == another_i64;
	builtin_output_i64(b);  # 1

	var x: if false then A else B;
	x.foo();  # B1

	var y: foo(6);
	y.foo();  # A0

	var z = foo(8)(8);  # Constructor of type C
	z.foo();  # C8

	var tup: make_reverse_tuple(i64, bool) = (true, 42);
	print(tup.toString());  # (true, 42);

    return 0;
}
