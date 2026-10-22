// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/source_file.hpp>

namespace compiler::frontend {

	/**
	 * @brief Helper struct used to access private FileID data.
	 * If you use this, you must know what you are doing. It is a struct so FileID can friend it.
	 */
	struct GetFileID_Functor final {
		static auto get(FileID id) { return base::CRef<SourceFile>{ id.resolve() }; }

		static auto getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(FileID id) {
			return id.resolve();
		}
	};

	inline auto getFileRef(FileID id) { return GetFileID_Functor::get(id); }

	/**
	 * @brief Helper struct used to access private ModuleID data.
	 * If you use this, you must know what you are doing. It is a struct so ModuleID can friend it.
	 */
	struct GetModuleID_Functor final {
		static auto get(ModuleID id) { return base::CRef<ModuleTree>{ id.resolve() }; }

		static auto getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(ModuleID id) {
			return id.resolve();
		}
	};

	inline auto getModuleRef(ModuleID id) { return GetModuleID_Functor::get(id); }
}
