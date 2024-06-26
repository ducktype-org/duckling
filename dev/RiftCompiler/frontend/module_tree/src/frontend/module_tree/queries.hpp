#pragma once

#include <query_framework/query_int.hpp>
#include <filesystem/file.hpp>
#include <pst_parser/pst.hpp>
#include <base/maps.hpp>

// @TODO: this dependency can be relaxed by separating ModuleID and FileID
#include "module_tree.hpp"

namespace compiler::frontend {

	base::StrId moduleName(ModuleId);

	// clang-format off
	DECLARE_QUERY(QueryModuleTree,     fs::FilePath, ModuleId)
	DECLARE_QUERY(QueryParentModule,   ModuleId,     base::Optional<ModuleId>)
	DECLARE_QUERY(QueryMainSourceFile, ModuleId,     FileId)
	DECLARE_QUERY(QuerySourceFiles,    ModuleId,     const std::vector<FileId>&)
	DECLARE_QUERY(QuerySubmodules,     ModuleId,     const base::HashMap<base::StrId COMMA ModuleId>&)

	DECLARE_QUERY(QueryFileID,         fs::FilePath, FileId)
	DECLARE_QUERY(QueryFilePST,        FileId,       const pst::PST<>&)
	// clang-format on


	/**
	 * @brief Query extension used to 
	 * determine ModuleID of relative import.
	 * Return none if no module was found.
	 */
	base::Optional<ModuleId> getRelativeModule(query::Context&, ModuleId from, const std::vector<base::StrId>& path);
}
