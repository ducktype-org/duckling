#pragma once

#include <query_framework/query_int.hpp>
#include <filesystem/file.hpp>
#include <pst_parser/pst.hpp>
#include <base/maps.hpp>

// @TODO: this dependency can be relaxed by separating ModuleID and FileID
#include "module_tree.hpp"

namespace compiler::frontend {


	// clang-format off
	DECLARE_QUERY(QueryModuleTree,     fs::FilePath, ModuleId)
	DECLARE_QUERY(QueryParentModule,   ModuleId,     ModuleId)
	DECLARE_QUERY(QueryMainSourceFile, ModuleId,     FileId)
	DECLARE_QUERY(QuerySourceFiles,    ModuleId,     const std::vector<FileId>&)
	DECLARE_QUERY(QuerySubmodules,     ModuleId,     const base::HashMap<std::string COMMA ModuleId>&)

	DECLARE_QUERY(QueryFileID,         fs::FilePath, FileId)
	DECLARE_QUERY(QueryFilePST,        FileId,       const pst::PST&)
	// clang-format on
}
