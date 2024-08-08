#pragma once

#include <query_framework/query_int.hpp>
#include <filesystem/file.hpp>
#include <pst_parser/pst.hpp>
#include <base/maps.hpp>

// @TODO: this dependency can be relaxed by separating ModuleID and FileID
#include "module_tree.hpp"

namespace compiler::frontend {

	base::StrId moduleName(ModuleId);
	std::string printModuleTree(ModuleId);

	/**
	 * @brief Query entire module tree build from given path.
	 * @return module id of root-module.
	 */
	DECLARE_QUERY(QueryModuleTree, fs::FilePath, ModuleId)

	/**
	 * @brief Query parent of a module.
	 * @return parent module, none for root-module.
	 */
	DECLARE_QUERY(QueryParentModule, ModuleId, base::Optional<ModuleId>)

	/**
	 * @brief Query main source file of a module.
	 */
	DECLARE_QUERY(QueryMainSourceFile, ModuleId, FileId)

	/**
	 * @brief Query sources files of a module (without main source file).
	 */
	DECLARE_QUERY(QuerySourceFiles, ModuleId, const std::vector<FileId>&)

	/**
	 * @brief Query map of children modules aka submodules
	 * of given module.
	 */
	DECLARE_QUERY(QuerySubmodules, ModuleId, const base::HashMap<base::StrId COMMA ModuleId>&)


	/**
	 * @brief Query PST of given file.
	 */
	DECLARE_QUERY(QueryFilePST, FileId, const pst::PST<>&)

	/**
	 * @brief Returns ModuleID
	 * Assumes that @p element is a TopLevel element of some File parsed with interface of Frontend
	 * module.
	 */
	ModuleId
		extendQueryModuleIDOfPST(query::Context&, pst::ParserCBorrowRef<pst::DucklingElement> element);

	/**
	 * @brief Query extension used to
	 * determine ModuleID of relative import.
	 *
	 * It represent following operation:
	 *  `import path[0].path[1].path[2]...path[n] as ...;`
	 * inside module @p from
	 *
	 * The component "@path[0]" is looked up from @p from children and ancestors,
	 * while other components are always looked up from children of previous
	 * component result.
	 *
	 * @note @todo: it currently will return some module even if
	 * there is an ambiguity.
	 *
	 * @return Found module, none if no matching module was found.
	 */
	base::Optional<ModuleId>
		getRelativeModule(query::Context&, ModuleId from, const std::vector<base::StrId>& path);
}
