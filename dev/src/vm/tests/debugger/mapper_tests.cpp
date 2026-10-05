// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "simple_debug_info.hpp"

#include <debug_info/debug_info_io.hpp>

#include <base/misc/int_conv.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

#include <vm/debugger/mapper.hpp>

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define ASSERT_FALSE(actual) ASSERT_TRUE(!(actual))

namespace {
	/**
	 * @brief Writes the mapping above to a fresh temporary directory and returns the file.
	 * @note Binary mode, because the payload is bytes and a `\n` in it is not a line ending.
	 */
	fs::File writeMappingFile() {
		const fs::File temp_dir = fs::FileManager::createRandomTempDirectory();
		const auto     di_path  = temp_dir.getFilePath().getPath() / "package_dvm.di";

		std::ofstream out(di_path, std::ios::binary);
		debug_info::saveToStream(debugger_test_data::simpleDebugInfo(), out);
		out.close();

		return { fs::FilePath(di_path) };
	}
}

class VmDebugTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(mappingTest); }


private:
	/**
	 * @brief Checks mapping.
	 */
	void mappingTest() {
		// Normally debug symbols contain absolute paths but it's impossible to use them simply in
		// tests, so let's unify relative ones
		auto original_path = std::filesystem::current_path();
		std::filesystem::current_path(path(""));

		vm::debugger::Mapper mapper;

		ASSERT_NO_VALUE(mapper.mainFile());
		ASSERT_FALSE(mapper.containsFile("abc"));

		// The mapping is produced by this test and read back through the same format the
		// compiler writes.
		ASSERT_HAS_VALUE(mapper.loadMapping(writeMappingFile()));

		auto simple = fs::FilePath("simple.dk");
		ASSERT_TRUE(mapper.containsFile(simple));

		auto assert_mapping
			= [&](usize line, base::StrID function, usize index, bool bidirectional = false) {
				  {
					  auto map_response = mapper.mapSourcePositionToCodePosition(simple, line);

					  ASSERT_HAS_VALUE(map_response);
					  auto code_position = map_response.value();

					  ASSERT_EQUAL_PRINT(code_position.first, function);
					  ASSERT_EQUAL_PRINT(code_position.second, index);
				  }
				  if (bidirectional) {
					  auto map_response = mapper.mapCodePositionToSourcePosition(function, index);

					  ASSERT_HAS_VALUE(map_response);
					  auto source_position = map_response.value();

					  ASSERT_EQUAL_PRINT(source_position.getStartLineColumn().first, line);
				  }
			  };

		auto assert_no_maping = [&](usize line) {
			ASSERT_NO_VALUE(mapper.mapSourcePositionToCodePosition(simple, line));
		};

		auto main          = base::StrID("main");
		auto add_and_print = base::StrID("_Q_M6simpleG11addAndPrintFi64i64i64E1a1bE");

		assert_mapping(15, main, 0);
		assert_mapping(16, main, 1, true);
		assert_mapping(17, main, 59, true);
		assert_mapping(6, add_and_print, 1, true);
		assert_no_maping(4);
		assert_no_maping(20);

		std::filesystem::current_path(original_path);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
