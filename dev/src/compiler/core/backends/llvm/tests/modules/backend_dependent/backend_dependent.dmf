@backend_dependent
fundecl getValue() -> i32;

@dvm_only_impl
fun getValue() = {
	return 10;
}

@native_only_impl
fun getValue() = {
	return 20;
}

fun main() -> i32 = {
	# The LLVM (native) backend must compile the `@native_only_impl` implementation
	# (returning 20), not the `@dvm_only_impl` one (returning 10).
	return getValue();
}
