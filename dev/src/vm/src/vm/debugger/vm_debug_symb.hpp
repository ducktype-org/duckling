#pragma once

#include <base/types/ints.hpp>
#include <base/collections/maps.hpp>
#include <filesystem/file.hpp>


namespace vm::debugger {

	struct FileDebuggerContext {
		// map between lines of the file and position in the code <func_id, instr no>
		std::map<usize, std::pair<u64, u64>> lines;
		/// @todo: check what other things in debugger are file-related
	};

	struct FunctionDebuggerContext {
		/// label IDs used for setting named breakpoints.
		base::HashMap<base::StrID, usize> label_to_offset{};
		/// A mapping from a local variable's name to its offset on the function's local stack.
		base::HashMap<base::StrID, usize> local_offset_map{};
		/// @todo: Consider adding mapping from offset to variable name
	};

	/**
	* @brief Stores an optional debug symbols between fat bytecode to microcode
	*/
	struct DebugContext {
		base::HashMap<fs::File, FileDebuggerContext> files_ctx;
		base::HashMap<u64, FunctionDebuggerContext> funcs_ctx;
	};
}
