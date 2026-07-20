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
	// Runtime Allocators
	void* builtin_alloc(uint64_t size);
	void  builtin_dealloc(void* ptr);

	// List
	void builtin_list_push(list* list, void* element_ptr, uint64_t element_size);
	void builtin_list_pop(list* list, uint64_t count, uint64_t element_size);
	void builtin_list_free(list* list);

	// Number formatting into a caller-provided buffer of `buffer_cap` bytes. Each one
	// writes the decimal representation of `v` into `p` followed by a terminating NUL and
	// returns how many characters it wrote (excluding the NUL), or `0` when the
	// representation plus its NUL does not fit into `buffer_cap` bytes.
	// These back `core.runtime` and must stay in sync with the DVM builtins of the same
	// names (see `vm::builtins::getBuiltinFunctions`).
	uint64_t float_to_string(double v, char* p, uint64_t buffer_cap);
	uint64_t u64_to_string(uint64_t v, char* p, uint64_t buffer_cap);
	uint64_t i64_to_string(int64_t v, char* p, uint64_t buffer_cap);
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

// Formats `value` with `format` directly into `p`, whose total capacity is `buffer_cap`
// bytes. Writes at most `buffer_cap - 1` characters followed by a terminating NUL, as
// `snprintf` does. Returns the number of characters written (excluding the NUL), or `0`
// when the representation plus its NUL does not fit; on a non-fit the buffer may hold a
// truncated result, so callers must ignore it when `0` is returned.
template<typename T>
static uint64_t write_formatted(char* p, uint64_t buffer_cap, const char* format, T value) {
	const int written = snprintf(p, buffer_cap, format, value);
	if (written < 0) exit(1);

	const uint64_t length = uint64_t(written);
	if (length >= buffer_cap) return 0;

	return length;
}

uint64_t float_to_string(double v, char* p, uint64_t buffer_cap) {
	return write_formatted(p, buffer_cap, "%g", v);
}

uint64_t u64_to_string(uint64_t v, char* p, uint64_t buffer_cap) {
	return write_formatted(p, buffer_cap, "%lu", v);
}

uint64_t i64_to_string(int64_t v, char* p, uint64_t buffer_cap) {
	return write_formatted(p, buffer_cap, "%ld", v);
}
