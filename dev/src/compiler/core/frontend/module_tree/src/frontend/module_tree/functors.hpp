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
		static auto get(FileID id) { return id.ref; }

		static FileID make(CRef<SourceFile> ref) { return FileID{ ref }; }
	};

	inline auto getSymRef(FileID id) { return GetFileID_Functor::get(id); }

	/**
	 * @brief Helper struct used to access private ModuleID data.
	 * If you use this, you must know what you are doing. It is a struct so ModuleID can friend it.
	 */
	struct GetModuleID_Functor final {
		static auto get(ModuleID id) { return id.ref; }

		static ModuleID make(CRef<ModuleTree> ref) { return ModuleID{ ref }; }
	};

	inline auto getSymRef(ModuleID id) { return GetModuleID_Functor::get(id); }
}
