#pragma once

#include <query_framework/query_int.hpp>
#include <filesystem/file.hpp>
#include <pst_parser/pst.hpp>
#include <base/maps.hpp>

// @TODO: this dependency can be relaxed by separating ModuleID and FileID
#include "module_tree.hpp"

namespace compiler::frontend {

	/**
	 * @TODO @FIXME !!! here fs::FilePath hash is not collision free which invalidates
	 * conditions required by query framework.
	 * It might be hard to maintain this condition and ensure it in a long run.
	 *
	 * I see two options:
	 *  * create hash system independent of std::hash that we will have to implement
	 *  * relax the condition in query framework (seams better but is not that simple to implement
	 * in staticly typed lang) One way to do it is to add `static base::HashMap` in
	 * QueryImplementation struct that maps keys to ints. The problem is that is is extremely
	 * inefficient for keys like int. Simillar way would be probably to require `hashKey` function
	 * in QueryImplementation struct that could be easily defaulted to std::hash (or identity) for
	 * all obvious types.
	 *
	 */
	// clang-format off
	DECLARE_QUERY(QueryModuleTree,     fs::FilePath, ModuleId)
	DECLARE_QUERY(QueryParentModule,   ModuleId,     ModuleId)
	DECLARE_QUERY(QueryMainSourceFile, ModuleId,     FileId)
	DECLARE_QUERY(QuerySourceFiles,    ModuleId,     const std::vector<FileId>&)
	DECLARE_QUERY(QuerySubmodules,     ModuleId,     const base::HashMap<std::string COMMA ModuleId>&)

	DECLARE_QUERY(QuerySourceFile,     fs::FilePath, FileId)
	DECLARE_QUERY(QueryFilePST,        FileId,       const pst::PST&)
	// clang-format on
}
