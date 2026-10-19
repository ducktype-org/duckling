#pragma once

#include <debug_info/debug_info.hpp>

#include <string>
#include <utility>

/**
 * @file
 * @brief The debug info of simple.dk, built in code - the tests write it out as a `.di` file
 * through the same format the compiler uses.
 */

namespace debugger_test_data {
	/** @brief A FilePosition in `file_path`, the way the compiler emits one. */
	inline debug_info::SourcePosition positionIn(
		const std::string& file_path, u64 start_line, u64 start_column, u64 end_line, u64 end_column
	) {
		return debug_info::SourcePosition{ debug_info::FilePosition{
			.file_path    = file_path,
			.start_line   = start_line,
			.start_column = start_column,
			.end_line     = end_line,
			.end_column   = end_column,
		} };
	}

	/** @brief One named variable - a parameter, or a `let` at some instruction offset. */
	inline std::pair<u64, debug_info::VariableMetadata> var(
		u64 offset, std::string name, debug_info::SourcePosition position
	) {
		return { offset,
			     debug_info::VariableMetadata{ .name = std::move(name), .position = position } };
	}

	/** @brief One instruction offset and the source it came from. */
	inline std::pair<u64, debug_info::InstructionMetadata> instr(
		u64 offset, debug_info::SourcePosition position
	) {
		return { offset, debug_info::InstructionMetadata{ .position = std::move(position) } };
	}

	/**
	 * @brief The four functions of simple.dk, with their offsets and variables.
	 * @param source_path the path the positions name - relative in the mapper test, absolute
	 * where the debugger has to find the file on disk.
	 */
	inline debug_info::DebugInfo simpleDebugInfo(const std::string& source_path = "simple.dk") {
		const auto at = [&](u64 start_line, u64 start_column, u64 end_line, u64 end_column) {
			return positionIn(source_path, start_line, start_column, end_line, end_column);
		};
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

		// The offsets of `main` are the ones the assertions below name: 1 is the `let sum`,
		// 59 the call on line 17.
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
}  // namespace debugger_test_data
