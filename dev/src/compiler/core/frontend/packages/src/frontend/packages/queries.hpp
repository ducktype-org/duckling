#pragma once

#include <base/str/str_utils.hpp>

#include <query_framework/input_query/query_input.hpp>
#include <query_framework/utils/query_hash.hpp>
#include <string_id/string_id.hpp>

namespace compiler::frontend::packages {


	/**
	 * @brief Key for module child side input query.
	 * It stores the parent module hash, child name and whether the child was found.
	 * This is needed to be in the key to store the metadata, and recreate this input during driver
	 * initialization. For more info see QueryModuleChildSideInput query.
	 * @note This key is used as MetadataType and it's stored in metadata during the provide call.
	 * That's why it implements the serialize/deserialize methods.
	 * These methods are called during metadata serialization/deserialization.
	 */
	struct KeyOf_QueryPackageSideInput final {
		/**
		 * @brief The ID of the package.
		 */
		base::StrID package_id;

		[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const;
	};

	/**
	 * @brief Side input identifying parent->child edge for a submodule lookup by name.
	 * Key includes parent module hash, child name and whether the child was found.
	 * This is needed to properly register the dependency when looking up a submodule by name.
	 * This registers dependencies that queries rely on to determine whether a module has or doesn't
	 * have a child with a given name.
	 */
	DECLARE_QUERY_SIDE_INPUT(QueryPackageSideInput, KeyOf_QueryPackageSideInput);

}  // namespace compiler::frontend::packages
