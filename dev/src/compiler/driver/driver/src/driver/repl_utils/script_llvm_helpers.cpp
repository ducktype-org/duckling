#include "script_llvm_helpers.hpp"

#include <base/str/str_utils.hpp>

#include <hashing/component_hash.hpp>

namespace compiler::repl {
	base::StrID getLLVMScriptModuleID(const fs::File& script_file) {
		auto path_string = script_file.getFilePath().string();
		auto path_id     = base::StrID(path_string.c_str());
		auto script_hash = hashing::ComponentHash(path_id);
		// Stable per-script id used to name the merged LLVM module and its artifacts
		return base::StrID(base::strConcat("script_module_", script_hash.hash.toStringHex()).c_str()
		);
	}

	void appendLLVMLIRModuleData(driver::LIRModuleData& merged, const driver::LIRModuleData& chunk) {
		merged.functions.insert(
			merged.functions.end(), chunk.functions.begin(), chunk.functions.end()
		);
		merged.globals.insert(merged.globals.end(), chunk.globals.begin(), chunk.globals.end());
	}

}  // namespace compiler::repl
