#include <debug_info/debug_info.hpp>
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
	/** @brief A FilePosition in simple.dk, the way the compiler emits one. */
	debug_info::SourcePosition at(u64 start_line, u64 start_column, u64 end_line, u64 end_column) {
		return debug_info::SourcePosition{ debug_info::FilePosition{
			.file_path    = "simple.dk",
			.start_line   = start_line,
			.start_column = start_column,
			.end_line     = end_line,
			.end_column   = end_column,
		} };
	}

	/** @brief One named variable - a parameter, or a `let` at some instruction offset. */
	std::pair<u64, debug_info::VariableMetadata> var(
		u64 offset, std::string name, debug_info::SourcePosition position
	) {
		return { offset,
			     debug_info::VariableMetadata{ .name = std::move(name), .position = position } };
	}

	/** @brief One instruction offset and the source it came from. */
	std::pair<u64, debug_info::InstructionMetadata> instr(
		u64 offset, debug_info::SourcePosition position
	) {
		return { offset, debug_info::InstructionMetadata{ .position = std::move(position) } };
	}

	/** @brief The four functions of simple.dk, with their offsets and variables. */
	debug_info::DebugInfo simpleDebugInfo() {
		debug_info::DebugInfo info{
			.target                = debug_info::Target::DBC,
			.module_path           = "package_dvm.dbc",
			.source_positions_type = debug_info::SourcePositionsType::LineColumn,
		};

		info.functions.put(
			"_Q_M6simpleG3addFi64i64i64E1a1bE",
			debug_info::FunctionMetadata{
				.function_name = "add",
				.position      = at(1, 1, 3, 1),
				.parameter_indexes_to_metadata
				= { var(0, "a", at(1, 9, 1, 14)), var(1, "b", at(1, 17, 1, 22)) },
				.instr_offsets_to_metadata      = { instr(4, at(2, 5, 2, 17)) },
				.instr_offsets_to_variable_init = {},
			}
		);

		info.functions.put(
			"_Q_M6simpleG11addAndPrintFi64i64i64E1a1bE",
			debug_info::FunctionMetadata{
				.function_name = "addAndPrint",
				.position      = at(5, 1, 9, 1),
				.parameter_indexes_to_metadata
				= { var(0, "a", at(5, 17, 5, 22)), var(1, "b", at(5, 25, 5, 30)) },
				.instr_offsets_to_metadata      = { instr(2, at(6, 5, 6, 20)),
		                                            instr(5, at(7, 5, 7, 27)),
		                                            instr(14, at(8, 5, 8, 15)) },
				.instr_offsets_to_variable_init = { var(1, "out", at(6, 5, 6, 20)) },
			}
		);

		info.functions.put(
			"_Q_M6simpleG6noArgsFi64EE",
			debug_info::FunctionMetadata{
				.function_name                  = "noArgs",
				.position                       = at(11, 1, 13, 1),
				.parameter_indexes_to_metadata  = {},
				.instr_offsets_to_metadata      = { instr(6, at(12, 5, 12, 13)) },
				.instr_offsets_to_variable_init = {},
			}
		);

		/*
		 * The offsets of `main` are the ones the assertions below name: 1 is the `let sum`,
		 * 59 the call on line 17.
		 */
		info.functions.put(
			"main",
			debug_info::FunctionMetadata{
				.function_name                  = "main",
				.position                       = at(15, 1, 19, 1),
				.parameter_indexes_to_metadata  = {},
				.instr_offsets_to_metadata      = { instr(10, at(16, 15, 16, 22)),
		                                            instr(14, at(16, 41, 16, 41)),
		                                            instr(18, at(16, 38, 16, 38)),
		                                            instr(22, at(16, 26, 16, 42)),
		                                            instr(30, at(16, 15, 16, 42)),
		                                            instr(32, at(16, 53, 16, 53)),
		                                            instr(36, at(16, 50, 16, 50)),
		                                            instr(40, at(16, 46, 16, 54)),
		                                            instr(48, at(16, 5, 16, 55)),
		                                            instr(59, at(17, 5, 17, 27)),
		                                            instr(71, at(18, 5, 18, 13)) },
				.instr_offsets_to_variable_init = { var(1, "sum", at(16, 5, 16, 55)) },
			}
		);

		info.types.put("i32", debug_info::TypeMetadata{ .name = "i32" });
		info.types.put("i64", debug_info::TypeMetadata{ .name = "i64" });

		return info;
	}

	/**
	 * @brief Writes the mapping above to a fresh temporary directory and returns the file.
	 * @note Binary mode, because the payload is bytes and a `\n` in it is not a line ending.
	 */
	fs::File writeMappingFile() {
		const fs::File temp_dir = fs::FileManager::createRandomTempDirectory();
		const auto     di_path  = temp_dir.getFilePath().getPath() / "package_dvm.di";

		std::ofstream out(di_path, std::ios::binary);
		debug_info::saveToStream(simpleDebugInfo(), out);
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
		/**
		 * @brief Normally debug symbols contain absolute paths but it's impossible to use them
		 * simply in tests, so let's unify relative ones
		 */
		auto original_path = std::filesystem::current_path();
		std::filesystem::current_path(path(""));

		vm::debugger::Mapper mapper;

		ASSERT_NO_VALUE(mapper.mainFile());
		ASSERT_FALSE(mapper.containsFile("abc"));

		/*
		 * The mapping is produced by this test and read back through the same format the
		 * compiler writes.
		 */
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
