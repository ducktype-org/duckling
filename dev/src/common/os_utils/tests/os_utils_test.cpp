#include <os_utils/memory.hpp>
#include <os_utils/dynamic_library.hpp>
#include <os_utils/executable_path.hpp>
#include <tester/tester.hpp>

#include <dlfcn.h>
#include <fstream>
#include <string>
#include <vector>

class OSUtilsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OSUtilsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(pageSizeTest);
		TESTER_ADD_TEST(allocateFreePagesTest);
		TESTER_ADD_TEST(markExecutableTest);
		TESTER_ADD_TEST(openCloseLibraryTest);
		TESTER_ADD_TEST(findSymbolTest);
		TESTER_ADD_TEST(openLibraryFromMemoryTest);
		TESTER_ADD_TEST(executablePathTest);
	}

private:
	void pageSizeTest() {
		auto result = os_utils::getPageSize();
		assertTrue(result.has_value(), "getPageSize should succeed");
		assertTrue(*result > 0, "Page size should be positive");
	}

	void allocateFreePagesTest() {
		auto page_size = os_utils::getPageSize();
		assertTrue(page_size.has_value(), "getPageSize should succeed");

		auto memory = os_utils::allocatePages(*page_size);
		assertTrue(memory.has_value(), "allocatePages should succeed");
		assertTrue(*memory != nullptr, "Allocated memory should not be null");

		// Write to verify the memory is usable.
		(*memory)[0] = byte{ 42 };

		os_utils::freePages(*memory, *page_size);
	}

	void markExecutableTest() {
		auto page_size = os_utils::getPageSize();
		assertTrue(page_size.has_value(), "getPageSize should succeed");

		auto memory = os_utils::allocatePages(*page_size);
		assertTrue(memory.has_value(), "allocatePages should succeed");

		auto result = os_utils::markExecutable(*memory, *page_size);
		assertTrue(result.has_value(), "markExecutable should succeed");

		os_utils::freePages(*memory, *page_size);
	}

	void openCloseLibraryTest() {
		auto lib = os_utils::openLibrary(nullptr);
		assertTrue(lib.has_value(), "openLibrary(nullptr) should succeed on all POSIX systems");
		os_utils::closeLibrary(*lib);
	}

	void findSymbolTest() {
		auto lib = os_utils::openLibrary(nullptr);
		assertTrue(lib.has_value(), "openLibrary(nullptr) should succeed");

		auto sym = os_utils::findSymbol(*lib, "strlen");
		assertTrue(sym.has_value(), "findSymbol should succeed");
		assertTrue(*sym != nullptr, "strlen symbol should be found");

		// Verify the symbol is callable and correct.
		using StrlenFunc = unsigned long (*)(const char*);
		auto strlen_func = reinterpret_cast<StrlenFunc>(*sym);
		ASSERT_EQUAL(5UL, strlen_func("hello"));

		os_utils::closeLibrary(*lib);
	}

	void openLibraryFromMemoryTest() {
		// 1. Use dladdr to find the absolute path to libc on THIS platform.
		//    strlen is in libc, which is loaded into every process.
		auto main_handle = os_utils::openLibrary(nullptr);
		assertTrue(main_handle.has_value(), "main program handle should be valid");
		auto strlen_addr = os_utils::findSymbol(*main_handle, "strlen");
		assertTrue(strlen_addr.has_value(), "strlen must be locatable in main program");
		os_utils::closeLibrary(*main_handle);

		Dl_info info{};
		int     ret = dladdr(*strlen_addr, &info);
		assertTrue(ret != 0, "dladdr must succeed for strlen");
		const char* libc_path = info.dli_fname;

		// 2. Read the library file into memory.
		std::ifstream file(libc_path, std::ios::binary | std::ios::ate);
		assertTrue(file.is_open(), "must be able to open the detected libc");
		auto file_size = static_cast<usize>(file.tellg());
		file.seekg(0, std::ios::beg);
		std::vector<byte> buffer(file_size);
		file.read(
			reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size)
		);
		assertTrue(
			static_cast<usize>(file.gcount()) == file_size, "must read entire libc file"
		);

		// 3. Load the library from memory — the function under test.
		auto lib = os_utils::openLibraryFromMemory(buffer);
		assertTrue(lib.has_value(), "openLibraryFromMemory should succeed");

		// 4. Look up a symbol — verify dlsym works on memory-loaded libraries.
		auto sym = os_utils::findSymbol(*lib, "strlen");
		assertTrue(sym.has_value(), "strlen should be found in memory-loaded lib");
		assertTrue(*sym != nullptr, "strlen symbol should not be null");

		// 5. Call the symbol — verify the loaded code is actually executable.
		using StrlenFunc = unsigned long (*)(const char*);
		auto func        = reinterpret_cast<StrlenFunc>(*sym);
		ASSERT_EQUAL(5UL, func("hello"));

		// 6. Close — verify cleanup doesn't crash.
		os_utils::closeLibrary(*lib);
	}

	void executablePathTest() {
		auto path = os_utils::getExecutablePath();
		assertTrue(!path.empty(), "Executable path should not be empty");
	}
};

TESTER_COMMON_MAIN("/src/common/os_utils/tests/")
