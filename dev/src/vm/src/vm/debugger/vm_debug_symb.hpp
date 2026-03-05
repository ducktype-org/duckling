#pragma once

#include <base/collections/maps.hpp>
#include <base/types/ints.hpp>

#include <diagnostic/source_position.hpp>
#include <filesystem/file.hpp>

namespace vm::loader {
	enum class LoadMode : uint8_t {
		NORMAL,
		DEBUG_DBC,
	};
};

namespace vm::loader::compiler {
	/**
	 * @brief Stores an debug symbols between fat bytecode to microcode
	 */
	struct FatMicroMapping {
		// { func_id, intr_no }
		using micro_pos = std::pair<usize, usize>;

		struct FunctionCtx {
			/// label IDs used for setting named breakpoints.
			base::HashMap<base::StrID, usize> label_to_offset{};
			/// A mapping from a local variable's name to its offset on the function's local stack.
			base::HashMap<base::StrID, usize> local_offset_map{};
			/// @todo: Consider adding mapping from offset to variable name
		};

		struct FileCtx {
			// map between line and column of the file to micro_position
			std::map<std::pair<usize, usize>, micro_pos> from_fat_to_micro;
			/// @todo: check what other things in debugger are file-related
		};

		std::map<micro_pos, dia::SourcePosition> from_micro_to_fat;
		base::HashMap<fs::File, FileCtx>         files_ctx;
		base::HashMap<u64, FunctionCtx>          funcs_ctx;
	};
}
