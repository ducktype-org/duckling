#pragma once

#include "access.hpp"

#include <query_framework/input_query/query_input.hpp>
#include <query_framework/query_metadata/declare_metadata.hpp>

namespace compiler::frontend::packages {

	/**
	 * @brief Side input query registering a dependency on a package.
	 * Key is the (pre-computed) package hash. See KeyOf_PackageSideInput.
	 */
	DECLARE_QUERY_SIDE_INPUT(QueryPackageSideInput, KeyOf_PackageSideInput)

	/**
	 * @brief Side input query registering a dependency on a package's dependency-count.
	 * Triggered by PackageDependenciesAccessLocked::unlock.
	 */
	DECLARE_QUERY_SIDE_INPUT(
		QueryPackageDependencyCountSideInput, KeyOf_PackageDependencyCountSideInput
	)

	/**
	 * @brief Side input query registering a dependency on a (package, alias) lookup edge.
	 * Records whether the alias resolves to a dependency in the owner package and which package
	 * id it points to (if any). Used by PackageInfo::getPackageDependencyByAlias.
	 */
	DECLARE_QUERY_SIDE_INPUT(
		QueryPackageDependencyAliasSideInput, KeyOf_PackageDependencyAliasSideInput
	)

	/**
	 * @brief Metadata storing KeyOf_PackageDependencyAliasSideInput for incremental driver init.
	 * Added during the provide call of QueryPackageDependencyAliasSideInput.
	 */
	DECLARE_METADATA(PackageDependencyAliasLookup, KeyOf_PackageDependencyAliasSideInput);

}  // namespace compiler::frontend::packages
