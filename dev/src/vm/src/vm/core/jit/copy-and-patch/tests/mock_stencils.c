#include <stdlib.h>
#include <stdio.h>

__attribute__((weak)) extern int patchable;

__attribute__((noinline)) int simple_function_plus_1(int x) {
	if (x == 1234567) {
		x += patchable;
	}

	return x + 1;
}

int calling_simple_odd(int x) {
	return simple_function_plus_1(2 * x);
}

int recursive_fibonacci(int x) {
	if (x == 1234567) {
		x += patchable;
	}

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
	if (x == 1234567) {
		x += patchable;
	}
	
	int* output = calloc(sizeof(int), x);
	for (int i = 0; i < x; ++i) {
		output[i] = i;
	}
	return output;
}
