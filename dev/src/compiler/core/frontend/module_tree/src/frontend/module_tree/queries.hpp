// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "access.hpp"
#include "file_id.hpp"
#include "module_id.hpp"

#include <frontend/pst_parser/parsed_pst.hpp>
#include <frontend/packages/access.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/bit256.hpp>

#include <query_framework/input_query/query_input.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_metadata/declare_metadata.hpp>
#include <string_id/string_id.hpp>

namespace compiler::frontend {

	base::StrID moduleName(ModuleID);
	std::string printModuleTree(ModuleID);

	/**
	 * @brief Query parent of a module.
	 * @return parent module, none for root-module.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryParentModule, ModuleID, base::Optional<ModuleID>, ({ .uses_qresult = false }))

	/**
	 * @brief Query parent package of a module.
	 * @return AccesLocked of a parent package.
	 * On unlock PackageAccessLocked registers QueryPackageSideInput dependency on the owning package.
	 */
	DECLARE_QUERY(
		QueryPackageOfModule, ModuleID, packages::PackageAccessLocked, ({ .uses_qresult = false })
	);

	/**
	 * @brief Query whether the module was produced by the REPL pipeline.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryIsReplModule, ModuleID, bool, ({ .uses_qresult = false }))

	/**
	 * @brief Query parent link in the REPL module chain (if any).
	 *
	 * Returns empty optional for non-REPL modules as well as for the first REPL module.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(
		QueryReplModuleParent, ModuleID, base::Optional<ModuleID>, ({ .uses_qresult = false })
	)

	/**
	 * @brief Query main source file of a module.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryMainSourceFile, ModuleID, FileID, ({ .uses_qresult = false }))

	using QuerySubmodules_Result = CRef<base::HashMap<base::StrID, ModuleID>>;
	/**
	 * @brief Query map of children modules aka submodules
	 * of given module.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QuerySubmodules, ModuleID, QuerySubmodules_Result, ({ .uses_qresult = false }))

	/**
	 * @brief Side input controlling dependency on number of submodules in a module.
	 * Key includes module path hash and submodule count.
	 * This is needed to properly invalidate queries that depend on the number of submodules
	 * when some query will need access to submodules list (eg. getSubmodules).
	 */
	DECLARE_QUERY_SIDE_INPUT(QuerySubmoduleCountSideInput, KeyOf_SubmoduleCountSideInput)


	/**
	 * @brief Side input query for module dependency.
	 * Key is ModuleID.
	 * It registers a dependency on the module when some query needs to access it.
	 */
	DECLARE_QUERY_SIDE_INPUT(QueryModuleSideInput, KeyOf_ModuleSideInput)

	/**
	 * @brief Side input query for file dependency.
	 * Key is FileID.
	 * It registers a dependency on the file when some query needs to access it.
	 */
	DECLARE_QUERY_SIDE_INPUT(QueryFileSideInput, KeyOf_FileSideInput)

	/**
	 * @brief Side input identifying parent->child edge for a submodule lookup by name.
	 * Key includes parent module hash, child name and whether the child was found.
	 * This is needed to properly register the dependency when looking up a submodule by name.
	 * This registers dependencies that queries rely on to determine whether a module has or doesn't
	 * have a child with a given name.
	 */
	DECLARE_QUERY_SIDE_INPUT(QueryModuleChildSideInput, KeyOf_ModuleChildSideInput)

	/**
	 * @brief Module Lookup metadata
	 * The whole KeyOf_ModuleChildSideInput is stored as metadata
	 * And serialized/deserialized accordingly
	 * This is needed to recreate the side input during driver initialization.
	 * In particular it stores the string of the child name and whether the child was found.
	 * Thanks to that the driver can check if there is a submodule with given name or not, and add
	 * the dependency accordingly. For more info see driver initialization
	 * @note This metadata is added during the provide call of QueryModuleChildSideInput query
	 */
	DECLARE_METADATA(ModuleLookup, KeyOf_ModuleChildSideInput);

	/**
	 * @brief Returns the parse tree of a source file.
	 * \parallel reads file content and creates PST; PST creation must be thread-safe;
	 */
	CRef<pst::ParsedPST<>> getFilePST(::query::Context& ctx, FileID file_id);


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

	/**
	 * @brief Get the module by its absolute path, which includes the package name and the path of
	 * submodules. It's used to lookup the language primitives.
	 *
	 * @warning Since this ignores the dependencies between modules, maybe it should never be used
	 * in other cases than standard library packages.
	 */
	base::Optional<ModuleID> getModuleByAbsolutePath(
		query::Context& ctx, base::StrID package_name, const std::vector<base::StrID>& path
	);
}
