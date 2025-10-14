#pragma once

#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_id.hpp>

namespace compiler::frontend {

	class SourceFile;

	/**
	 * @brief Helper struct used to access private FileID data.
	 * If you use this, you must know what you are doing. It is a struct so FileID can friend it.
	 */
	struct GetFileID_Functor final {
		static auto get(FileID id) { return base::CRef<SourceFile>{ id.ref }; }

		static auto getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(FileID id) {
			return id.ref;
		}

		static auto fromRef(base::Ref<SourceFile> ref) { return FileID(ref); }
	};

	inline auto getFileRef(FileID id) { return GetFileID_Functor::get(id); }

	/**
	 * @brief Helper struct used to access private ModuleID data.
	 * If you use this, you must know what you are doing. It is a struct so ModuleID can friend it.
	 */
	struct GetModuleID_Functor final {
		static auto get(ModuleID id) { return base::CRef<ModuleTree>{ id.ref }; }

		static auto getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(ModuleID id) {
			return id.ref;
		}

		static auto fromRef(base::Ref<ModuleTree> ref) { return ModuleID(ref); }
	};

	inline auto getModuleRef(ModuleID id) { return GetModuleID_Functor::get(id); }
}
