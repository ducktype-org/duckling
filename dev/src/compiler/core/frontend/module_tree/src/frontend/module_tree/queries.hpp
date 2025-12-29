#pragma once

#include "access.hpp"
#include "file_id.hpp"
#include "module_id.hpp"

#include <frontend/pst_parser/pst.hpp>

#include <base/collections/maps.hpp>
#include <base/pointers/ref.hpp>

#include <query_framework/query_input.hpp>
#include <query_framework/query_int.hpp>

namespace compiler::frontend {

	base::StrID moduleName(ModuleID);
	std::string printModuleTree(ModuleID);

	/**
	 * @brief Query parent of a module.
	 * @return parent module, none for root-module.
	 *
	 * @ingroup query_thread_safe
	 */
	DECLARE_QUERY(QueryParentModule, ModuleID, base::Optional<ModuleID>, ({ .uses_qresult = false }))

	/**
	 * @brief Query main source file of a module.
	 *
	 * @ingroup query_thread_safe
	 */
	DECLARE_QUERY(QueryMainSourceFile, ModuleID, FileID, ({ .uses_qresult = false }))

	/**
	 * @brief Query sources files of a module (without main source file).
	 *
	 * @ingroup query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QuerySourceFiles, ModuleID, CRef<std::vector<FileID>>, ({ .uses_qresult = false }))


	using QuerySubmodules_Result = CRef<base::HashMap<base::StrID, ModuleID>>;
	/**
	 * @brief Query map of children modules aka submodules
	 * of given module.
	 *
	 * @ingroup query_thread_safe
	 */
	DECLARE_QUERY(QuerySubmodules, ModuleID, QuerySubmodules_Result, ({ .uses_qresult = false }))


	/**
	 * @brief Query PST of given file.
	 *
	 * \parallel reads file content and creates PST; PST creation must be thread-safe; also uses \ref root_element_file_back_map (no cache)
	 * @ingroup query_not_thread_safe
	 */
	DECLARE_QUERY(QueryFilePST, FileID, CRef<pst::PST<>>, ({ .uses_qresult = false }))

	/**
	 * @brief Side input query for module dependency.
	 * Key is ModuleID.
	 */
	DECLARE_QUERY_SIDE_INPUT(QueryModuleSideInput, KeyOf_ModuleSideInput)

	/**
	 * @brief Side input query for file dependency.
	 * Key is FileID.
	 */
	DECLARE_QUERY_SIDE_INPUT(QueryFileSideInput, KeyOf_FileSideInput)

	/**
	 * @brief Returns ModuleID
	 * Assumes that @p element is a TopLevel element of some File parsed with interface of Frontend
	 * module.
	 */
	ModuleID extendQueryModuleIDOfPST(query::Context&, pst::AccessLocked<pst::LangElement> element);

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
	base::Optional<ModuleID> getRelativeModule(
		query::Context&, ModuleID from, const std::vector<base::StrID>& path
	);
}
