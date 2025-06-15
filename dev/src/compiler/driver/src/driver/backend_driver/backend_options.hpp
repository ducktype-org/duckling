#pragma once

#include <base/string_id.hpp>

#include <vector>

namespace compiler::driver {

	enum class BackendType : std::uint8_t { LLVM, DVM };

	/**
	 * @brief Compilation options.
	 */
	struct BackendOptions final {
		BackendType backend_type;
		bool        compile_to_assembly;
		bool        dump_llvm_ir;

		/**
		 * Do not saves the compiled DBC to file.
		 * Useful when wanting to run the compiled bytecode.
		 */
		bool dvm_code_only_memory;
		bool add_builtin_library;
	};
}
