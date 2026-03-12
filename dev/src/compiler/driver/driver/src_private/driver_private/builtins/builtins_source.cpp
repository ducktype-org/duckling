/**
 * @file builtins_source.cpp
 * @brief Implementation of built-ins for the Duckling programming language.
 * This file contains the definitions of built-in functions, including input and output,
 * in a human-friendly language (C++), instead of LLVM IR. It is then compiled to LLVM
 * bitcode, the bytes are embedded into the compiler, and linked into the final executable.
 */

// NOLINTBEGIN

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Definition of the Duckling string representation.
struct str {
	// Pointer to the data of the string proper.
	char* data;
	// The length of the string proper.
	uint64_t length;
	// The difference between the pointer to the data and
	// the beginning of the allocated memory (always non-negative).
	uint64_t memory_begin_offset;
	// The difference between the past-the-end implicit sentinel of the allocated
	// memory and the pointer to the data (always non-negative). The total size
	// of the allocated buffer is thus memory_begin_offset + memory_end_offset.
	uint64_t memory_end_offset;
};

// Here, we declare the entire interface as extern "C" to avoid name mangling.
// The definitions will be given below.
extern "C" {
	// Basic small I/O
	int32_t  builtin_output_char(char c);
	char     builtin_input_char();
	int64_t  builtin_output_i64(int64_t v);
	int64_t  builtin_input_i64();
	int32_t  builtin_output_u64(uint64_t v);
	uint64_t builtin_input_u64();
	int32_t  builtin_output_f64(double v);
	double   builtin_input_f64();

	// String I/O
	int64_t builtin_output_string(str s);
	str     builtin_input_string();
	void    builtin_free_string(str s);

	// Runtime Allocators
	void* builtin_alloc(uint64_t size);
	void  builtin_dealloc(void* ptr);
}

int32_t builtin_output_char(char c) { return printf("%c", c); }

char builtin_input_char() {
	char c;
	if (scanf(" %c", &c) != 1) exit(1);
	return c;
}

// @TODO: #1782 change return type to i32 when updating builtins in VM.
int64_t builtin_output_i64(int64_t v) { return printf("%ld\n", v); }

int64_t builtin_input_i64() {
	int64_t v;
	if (scanf("%ld", &v) != 1) exit(1);
	return v;
}

int32_t builtin_output_u64(uint64_t v) { return printf("%lu\n", v); }

uint64_t builtin_input_u64() {
	uint64_t v;
	if (scanf("%lu", &v) != 1) exit(1);
	return v;
}

int32_t builtin_output_f64(double v) { return printf("%.4lg\n", v); }

double builtin_input_f64() {
	double v;
	if (scanf("%lg", &v) != 1) exit(1);
	return v;
}

str builtin_input_string() {
	char*   line = nullptr;
	size_t  len  = 0;
	ssize_t read = getline(&line, &len, stdin);

	if (read == -1) {
		// In case of error or EOF, return an empty string.
		// getline might have allocated memory, so free it.
		free(line);
		return str{ .data = nullptr, .length = 0, .memory_begin_offset = 0, .memory_end_offset = 0 };
	}

	// Strip trailing newline if present
	if (read > 0 && line[read - 1] == '\n') {
		line[read - 1] = '\0';
		read--;
	}

	// Allocate exactly the required memory
	char* new_buffer = (char*) malloc(size_t(read));
	if (!new_buffer) {
		free(line);
		exit(1);
	}
	memcpy(new_buffer, line, size_t(read));
	free(line);

	return str{
		.data                = new_buffer,
		.length              = uint64_t(read),
		.memory_begin_offset = 0,
		.memory_end_offset   = uint64_t(read),
	};
}

int64_t builtin_output_string(str s) {
	// Use fwrite to handle non-null-terminated strings and binary data safely.
	return int64_t(fwrite(s.data, sizeof(char), s.length, stdout));
}

void builtin_free_string(str s) {
	if (s.data != NULL) {
		// The data pointer might not be the start of the allocation.
		// Adjust back by the offset to get the real start.
		free(s.data - s.memory_begin_offset);
		s.data                = NULL;
		s.length              = 0;
		s.memory_begin_offset = 0;
		s.memory_end_offset   = 0;
	}
}

// This is an intended abstraction over the allocation. In the future, different allocators for
// different architectures will be supported here. For now we just malloc.
void* builtin_alloc(uint64_t size) {
	void* ptr = malloc(size);
	if (ptr == nullptr) exit(1);
	return ptr;
}

void builtin_dealloc(void* ptr) { free(ptr); }

// NOLINTEND
