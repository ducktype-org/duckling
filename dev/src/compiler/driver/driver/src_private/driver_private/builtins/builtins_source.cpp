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

// Definition of the Duckling dynamic list representation.
struct list {
	// Pointer to the data of the list.
	char* data;
	// The length of the list.
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
	// Basic small I/O @TODO: #2635 move to Duckling, probably
	int32_t  builtin_output_char(char c);
	char     builtin_input_char();
	int64_t  builtin_output_i64(int64_t v);
	int64_t  builtin_input_i64();
	int32_t  builtin_output_u64(uint64_t v);
	uint64_t builtin_input_u64();
	int32_t  builtin_output_f64(double v);
	double   builtin_input_f64();

	// String I/O @TODO: #2636 move to Duckling
	int64_t print(str s);
	str     builtin_input_string();
	void    builtin_free_string(str s);
	str     builtin_string_prepended(char c, str s);
	str     builtin_string_appended(str s, char c);
	str     builtin_string_concatenated(str s, str t);

	// Runtime Allocators
	void* builtin_alloc(uint64_t size);
	void  builtin_dealloc(void* ptr);

	// List
	void     builtin_list_push(list* list, void* element_ptr, uint64_t element_size);
	void     builtin_list_pop(list* list, uint64_t count, uint64_t element_size);
	uint64_t builtin_list_len(list* list);
	void     builtin_list_free(list* list);

	// Stringification @TODO: #2634 move to Duckling, probably
	str builtin_stringify_i64(int64_t v);
	str builtin_stringify_u64(uint64_t v);
	str builtin_stringify_f64(double v);
	str builtin_stringify_char(char c);
	str builtin_stringify_bool(bool b);
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

int64_t print(str s) {
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

str builtin_string_appended(str s, char c) {
	char* new_data = (char*) malloc(s.length + 1);
	if (!new_data) exit(1);
	memcpy(new_data, s.data, s.length);
	new_data[s.length] = c;

	return str{
		.data                = new_data,
		.length              = s.length + 1,
		.memory_begin_offset = 0,
		.memory_end_offset   = s.length + 1,
	};
}

str builtin_string_prepended(char c, str s) {
	char* new_data = (char*) malloc(s.length + 1);
	if (!new_data) exit(1);
	new_data[0] = c;
	memcpy(new_data + 1, s.data, s.length);

	return str{
		.data                = new_data,
		.length              = s.length + 1,
		.memory_begin_offset = 0,
		.memory_end_offset   = s.length + 1,
	};
}

str builtin_string_concatenated(str s, str t) {
	uint64_t new_length = s.length + t.length;
	char*    new_data   = (char*) malloc(new_length);
	if (!new_data) exit(1);

	memcpy(new_data, s.data, s.length);
	memcpy(new_data + s.length, t.data, t.length);

	return str{
		.data                = new_data,
		.length              = new_length,
		.memory_begin_offset = 0,
		.memory_end_offset   = new_length,
	};
}

// This is an intended abstraction over the allocation. In the future, different allocators for
// different architectures will be supported here. For now we just malloc.
void* builtin_alloc(uint64_t size) {
	void* ptr = malloc(size);
	if (ptr == nullptr) exit(1);
	return ptr;
}

void builtin_dealloc(void* ptr) { free(ptr); }

// push(vec: ref List[T], value: T, sizeof(T)) -> ()
void builtin_list_push(list* list, void* element_ptr, uint64_t element_size) {
	uint64_t required_end_space = (list->length + 1) * element_size;

	// Reallocate if needed.
	if (required_end_space > list->memory_end_offset) {
		uint64_t current_data_size = list->length * element_size;

		// New capacity of the list is twice the size of the old list length.
		uint64_t new_number_of_elements = list->length > 1 ? list->length * 2 : 4;
		uint64_t new_total_size         = new_number_of_elements * element_size;

		char* real_data_start = list->data - list->memory_begin_offset;
		char* new_start       = (char*) realloc(real_data_start, new_total_size);
		if (!new_start) exit(1);

		// Center the data in the new buffer, for cheap `push_front` operations.
		uint64_t new_begin_offset               = (new_total_size - current_data_size) / 2;
		char*    new_data_location              = new_start + new_begin_offset;
		char*    old_data_location_in_new_block = new_start + list->memory_begin_offset;
		memmove(new_data_location, old_data_location_in_new_block, current_data_size);

		list->data                = new_data_location;
		list->memory_begin_offset = new_begin_offset;
		list->memory_end_offset   = new_total_size - new_begin_offset;
	}

	// Insert the new element.
	char* dest = list->data + (list->length * element_size);
	memcpy(dest, element_ptr, element_size);
	list->length++;
}

// pop(vec: ref List[T], count: u64, sizeof(T)) -> ()
void builtin_list_pop(list* list, uint64_t count, uint64_t element_size) {
	if (list->length == 0) return;
	// Calculate the maximum size of elements to remove if the count is bigger then the length.
	uint64_t to_remove = count < list->length ? count : list->length;
	list->length -= to_remove;
}

// len(vec: List[T]) -> u64
uint64_t builtin_list_len(list* list) { return list->length; }

void builtin_list_free(list* list) {
	if (list->data != NULL) {
		// The data pointer might not be the start of the allocation.
		// Adjust back by the offset to get the real start.
		free(list->data - list->memory_begin_offset);
		list->data                = NULL;
		list->length              = 0;
		list->memory_begin_offset = 0;
		list->memory_end_offset   = 0;
	}
}

str builtin_stringify_i64(int64_t v) {
	char stringified[21];
	snprintf(stringified, sizeof(stringified), "%ld", v);
	uint64_t length = strlen(stringified);

	auto result = str{
		.data                = (char*) malloc(length),
		.length              = uint64_t(length),
		.memory_begin_offset = 0,
		.memory_end_offset   = uint64_t(length),
	};

	memcpy(result.data, stringified, result.length);
	return result;
}

str builtin_stringify_u64(uint64_t v) {
	char stringified[21];
	snprintf(stringified, sizeof(stringified), "%lu", v);
	uint64_t length = strlen(stringified);

	auto result = str{
		.data                = (char*) malloc(length),
		.length              = uint64_t(length),
		.memory_begin_offset = 0,
		.memory_end_offset   = uint64_t(length),
	};

	memcpy(result.data, stringified, result.length);
	return result;
}

str builtin_stringify_f64(double v) {
	char stringified[32];
	snprintf(stringified, sizeof(stringified), "%g", v);
	uint64_t length = strlen(stringified);

	auto result = str{
		.data                = (char*) malloc(length),
		.length              = uint64_t(length),
		.memory_begin_offset = 0,
		.memory_end_offset   = uint64_t(length),
	};

	memcpy(result.data, stringified, result.length);
	return result;
}

str builtin_stringify_char(char c) {
	auto result = str{
		.data                = (char*) malloc(1),
		.length              = 1,
		.memory_begin_offset = 0,
		.memory_end_offset   = 1,
	};
	result.data[0] = c;
	return result;
}

str builtin_stringify_bool(bool b) {
	const char*    stringified = b ? "true" : "false";
	const uint64_t length      = b ? 4 : 5;

	auto result = str{
		.data                = (char*) malloc(length),
		.length              = length,
		.memory_begin_offset = 0,
		.memory_end_offset   = length,
	};

	memcpy(result.data, stringified, length);
	return result;
}

// NOLINTEND
