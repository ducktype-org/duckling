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
		static auto get(FileID id) {
			SourceFile::checkDanglingReference(id.ref);
			return base::CRef<SourceFile>{ id.ref };
		}

		static auto getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(FileID id) {
			SourceFile::checkDanglingReference(id.ref);
			return id.ref;
		}
	};

	inline auto getFileRef(FileID id) { return GetFileID_Functor::get(id); }

	/**
	 * @brief Helper struct used to access private ModuleID data.
	 * If you use this, you must know what you are doing. It is a struct so ModuleID can friend it.
	 */
	struct GetModuleID_Functor final {
		static auto get(ModuleID id) {
			ModuleTree::checkDanglingReference(id.ref);
			return base::CRef<ModuleTree>{ id.ref };
		}

		static auto getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(ModuleID id) {
			ModuleTree::checkDanglingReference(id.ref);
			return id.ref;
		}
	};

	inline auto getModuleRef(ModuleID id) { return GetModuleID_Functor::get(id); }
}
