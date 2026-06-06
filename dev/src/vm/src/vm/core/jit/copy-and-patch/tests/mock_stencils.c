#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((weak)) extern int _value_to_patch_arg0;

__attribute__((noinline)) int simple_function_plus_1(int x) {
	if (x == 1234567) x += _value_to_patch_arg0;

	return x + 1;
}

int calling_simple_odd(int x) { return simple_function_plus_1(2 * x); }

int recursive_fibonacci(int x) {
	if (x == 1234567) x += _value_to_patch_arg0;  // tests if an unfilled relocation can go through compilation

	if (x <= 1)
		return 1;
	else
		return recursive_fibonacci(x - 1) + recursive_fibonacci(x - 2);
}

int calling_fibonacci_sum(int x) {
	int output = 0;

	for (int i = 0; i <= x; ++i) {
		int fib = recursive_fibonacci(i);
		output += fib * fib;
	}
	return output;
}

int* calling_libc(int x) {
	if (x == 1234567) x += _value_to_patch_arg0;  // tests if an unfilled relocation can go through compilation

	int* output = calloc(sizeof(int), x);
	for (int i = 0; i < x; ++i) output[i] = i;
	return output;
}

int must_patch(int x) { return x + (intptr_t) (&_value_to_patch_arg0); }

typedef int (*stencil_type)(int, int);

__attribute__((weak)) extern int _value_to_patch_continue_fn;

int mock_add(int a, int b) {
	a += b;

	stencil_type val = (stencil_type) &_value_to_patch_continue_fn;
	return (*val)(a, b);
}

int mock_mul(int a, int b) {
	a *= b;

	stencil_type val = (stencil_type) &_value_to_patch_continue_fn;
	return (*val)(a, b);
}

int mock_end(int a, int _) { return a; }
