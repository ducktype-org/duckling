#include <os_utils/memory.hpp>
#include <os_utils/dynamic_library.hpp>
#include <os_utils/executable_path.hpp>
#include <tester/tester.hpp>

#include <cmath>
#include <string>

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
		auto lib = os_utils::openLibrary("libm.so.6");
		assertTrue(lib.has_value(), "openLibrary should succeed");
		os_utils::closeLibrary(*lib);
	}

	void findSymbolTest() {
		auto lib = os_utils::openLibrary("libm.so.6");
		assertTrue(lib.has_value(), "openLibrary should succeed");

		auto sym = os_utils::findSymbol(*lib, "sin");
		assertTrue(sym.has_value(), "findSymbol should succeed");
		assertTrue(*sym != nullptr, "sin symbol should be found");

		// Verify the symbol is callable.
		using SinFunc = double (*)(double);
		auto sin_func  = reinterpret_cast<SinFunc>(*sym);
		double result  = sin_func(0.0);
		assertTrue(std::abs(result) < 0.0001, "sin(0) should be ~0");

		os_utils::closeLibrary(*lib);
	}

	void executablePathTest() {
		auto path = os_utils::getExecutablePath();
		assertTrue(!path.empty(), "Executable path should not be empty");
	}
};

TESTER_COMMON_MAIN("/src/common/os_utils/tests/")
