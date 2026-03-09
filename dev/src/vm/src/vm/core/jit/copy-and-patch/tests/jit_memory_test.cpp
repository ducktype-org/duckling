#include "../memory/memory.cpp"  // for easy invocation
#include "../memory/memory.hpp"
#include "../stencils/import_stencils.hpp"

#include <dlfcn.h>
#include <unistd.h>

#include <base/pointers/box.hpp>
#include <base/preproc/diagnostics.hpp>

#include <tester/tester.hpp>

#include <cstring>
#include <string>

class JitMemoryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS JitMemoryTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSimple);
		TESTER_ADD_TEST(testRecursive);
		TESTER_ADD_TEST(testCallingSimple);
		TESTER_ADD_TEST(testCallingRecursive);
		TESTER_ADD_TEST(testCallingLibc);
	}

private:
	PUSH_DIAGNOSTIC
	ALLOW_EXTENSIONS
	constexpr static char binary[] = {
#embed "mock_stencils-text" suffix(, )
	};
	POP_DIAGNOSTIC

	constexpr static vm::jit::Stencils stencils
		= vm::jit::Stencils{ .binary    = std::to_array(binary),
		                     .functions = {
#include "mock_stencils-nm"
							 } };

	void printBinary() {
		std::cerr << "Binary:\n";
		for (char c: stencils.binary) std::cerr << std::hex << (int) (unsigned char) c << ' ';
		std::cerr << "\nFunctions:\n";
		for (vm::jit::LLVM_nm_data data: stencils.functions) std::cerr << data.name << '\n';
	}

	constexpr static auto find_func = [](auto name) {
		for (vm::jit::LLVM_nm_data data: stencils.functions)
			if (data.name == name) return data;
		CORE_PANIC("No function with that name");
	};

	void testSimple() {
		auto foo_code = find_func("simple_function_plus_1");

		auto memory = JitMemory::allocate(foo_code.size);
		std::ranges::copy(stencils.stencil_binary(foo_code), memory.memory);
		memory.mark_executable();
		auto simple = memory.into_func<int(int)>();
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), i + 1);
	}

	void testRecursive() {
		auto foo_code = find_func("recursive_fibonacci");

		auto memory = JitMemory::allocate(foo_code.size);
		std::ranges::copy(stencils.stencil_binary(foo_code), memory.memory);
		memory.mark_executable();
		auto fibonacci = memory.into_func<int(int)>();
		ASSERT_EQUAL(std::invoke(fibonacci, 0), 1);
		ASSERT_EQUAL(std::invoke(fibonacci, 1), 1);
		ASSERT_EQUAL(std::invoke(fibonacci, 2), 2);
		ASSERT_EQUAL(std::invoke(fibonacci, 3), 3);
		ASSERT_EQUAL(std::invoke(fibonacci, 4), 5);
		ASSERT_EQUAL(std::invoke(fibonacci, 5), 8);
	}

	constexpr static char full_elf[] = {
#embed "stencils.so" suffix(, )
	};

	void testCallingSimple() {
		int fd = memfd_create("lib", 0);
		if (fd == -1) {
			perror("memfd_create");
			return;
		}

		// 2. Write the library bytes to the memory file
		if (write(fd, full_elf, sizeof(full_elf)) != (ssize_t) sizeof(full_elf)) {
			perror("write");
			close(fd);
			return;
		}
		lseek(fd, 0, SEEK_SET);

		char path[64];
		sprintf(path, "/proc/self/fd/%d", fd);
		void* handle = dlopen(path, RTLD_LAZY);
		if (!handle) {
			fprintf(stderr, "dlopen failed: %s\n", dlerror());
			close(fd);
			return;
		}

		void* sym_loc = dlsym(handle, "calling_simple_odd");
		if (!sym_loc) {
			fprintf(stderr, "dlopen failed: %s\n", dlerror());
			close(fd);
			return;
		}

		auto simple = reinterpret_cast<int (*)(int)>(sym_loc);
		for (int i = 0; i < 10; ++i) ASSERT_EQUAL(std::invoke(simple, i), 2 * i + 1);
	}

	void testCallingRecursive() {
		int fd = memfd_create("lib", 0);
		if (fd == -1) {
			perror("memfd_create");
			return;
		}

		// 2. Write the library bytes to the memory file
		if (write(fd, full_elf, sizeof(full_elf)) != (ssize_t) sizeof(full_elf)) {
			perror("write");
			close(fd);
			return;
		}
		lseek(fd, 0, SEEK_SET);

		char path[64];
		sprintf(path, "/proc/self/fd/%d", fd);
		void* handle = dlopen(path, RTLD_LAZY);
		if (!handle) {
			fprintf(stderr, "dlopn failed: %s\n", dlerror());
			close(fd);
			return;
		}

		void* sym_loc = dlsym(handle, "calling_fibonacci_sum");
		if (!sym_loc) {
			fprintf(stderr, "dlopen failed: %s\n", dlerror());
			close(fd);
			return;
		}

		auto fibonacci_sum = reinterpret_cast<int (*)(int)>(sym_loc);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 0), 1);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 1), 2);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 2), 6);
		ASSERT_EQUAL(std::invoke(fibonacci_sum, 3), 15);
	}

	void testCallingLibc() {
		int fd = memfd_create("lib", 0);
		if (fd == -1) {
			perror("memfd_create");
			return;
		}

		// 2. Write the library bytes to the memory file
		if (write(fd, full_elf, sizeof(full_elf)) != (ssize_t) sizeof(full_elf)) {
			perror("write");
			close(fd);
			return;
		}
		lseek(fd, 0, SEEK_SET);

		char path[64];
		sprintf(path, "/proc/self/fd/%d", fd);
		void* handle = dlopen(path, RTLD_LAZY);
		if (!handle) {
			fprintf(stderr, "dlopn failed: %s\n", dlerror());
			close(fd);
			return;
		}

		void* sym_loc = dlsym(handle, "calling_libc");
		if (!sym_loc) {
			fprintf(stderr, "dlopen failed: %s\n", dlerror());
			close(fd);
			return;
		}

		auto calling_libc    = reinterpret_cast<int* (*) (int)>(sym_loc);
		auto from_jit_memory = base::Box<int>::fromPointer(std::invoke(calling_libc, 100));
		for (int i = 0; i < 100; ++i) ASSERT_EQUAL(from_jit_memory.get()[i], i);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/jit/");
