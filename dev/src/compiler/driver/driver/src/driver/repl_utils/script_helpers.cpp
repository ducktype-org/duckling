// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "script_helpers.hpp"

#include <base/extend_cpp/vector_utils.hpp>
#include <base/str/str_utils.hpp>

#include <hashing/component_hash.hpp>

namespace compiler::repl {
	base::StrID getScriptModuleID(const fs::File& script_file) {
		auto path_string = script_file.getFilePath().string();
		auto path_id     = base::StrID(path_string.c_str());
		auto script_hash = hashing::ComponentHash(path_id);
		// Stable per-script id used to name the merged script module and its artifacts
		return base::StrID(base::strConcat("script_module_", script_hash.hash.toStringHex()).c_str()
		);
	}

	void appendScriptLIRModuleData(
		driver::LIRUnitWithBackendName& merged, const driver::LIRUnitWithBackendName& chunk
	) {
		base::appendToVector(merged.lir_unit.lir_functions, chunk.lir_unit.lir_functions);
		base::appendToVector(merged.lir_unit.lir_globals, chunk.lir_unit.lir_globals);
	}

}  // namespace compiler::repl
