#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int32_t builtin_output_i64(int64_t v) { return printf("%ld\n", v); }

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

int32_t builtin_output_f64(double v) { return printf("%lf\n", v); }

double builtin_input_f64() {
	double v;
	if (scanf("%lf", &v) != 1) exit(1);
	return v;
}

struct DucklingString {
	// Pointer to the data of the string proper.
	char* data;
	// The length of the string proper.
	uint64_t length;
	// The difference between the pointer to the data and
	// the beginning of the allocated memory (always non-negative).
	uint64_t memory_begin_offset;
	// The difference between the end of the allocated memory and
	// the pointer to the data (always non-negative). The total size
	// of the allocated buffer is thus memory_begin_offset + memory_end_offset.
	uint64_t memory_end_offset;
};

DucklingString builtin_input_string() {
	char*   line = nullptr;
	size_t  len  = 0;
	ssize_t read = getline(&line, &len, stdin);

	if (read == -1) {
		// In case of error or EOF, return an empty string.
		// getline might have allocated memory, so free it.
		free(line);
		return DucklingString{
			.data = nullptr, .length = 0, .memory_begin_offset = 0, .memory_end_offset = 0
		};
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

	return DucklingString{
		.data                = new_buffer,
		.length              = uint64_t(read),
		.memory_begin_offset = 0,
		.memory_end_offset   = uint64_t(read),
	};
}

int64_t builtin_output_string(DucklingString s) {
	if (s.data == NULL || s.length == 0) {
		printf("\n");
		return 0;
	}
	// Use fwrite to handle non-null-terminated strings and binary data safely.
	int64_t written = int64_t(fwrite(s.data, sizeof(char), s.length, stdout));
	printf("\n");
	return written;
}

void builtin_free_string(DucklingString& s) {
	if (s.data != NULL) {
		// The data pointer might not be the start of the allocation.
		// Adjust back by the offset to get the real start.
		free(s.data - s.memory_begin_offset);
		s.data = NULL;
		s.length = 0;
		s.memory_begin_offset = 0;
		s.memory_end_offset = 0;
	}
}
