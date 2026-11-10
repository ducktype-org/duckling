@backend_dependent
fundecl getValue() -> i64;

@dvm_only_impl
fun getValue() = {
	return 10i64;
}

@native_only_impl
fun getValue() = {
	return 20i64;
}

fun main() -> i64 = {
	# The DVM backend must select the `@dvm_only_impl` implementation (returning 10),
	# not the `@native_only_impl` one (returning 20).
	return getValue();
}
