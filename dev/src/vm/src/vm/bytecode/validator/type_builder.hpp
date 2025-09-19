#pragma once

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_context.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code::detail {
	/**
	 * @brief Builds TypeMetadata from TypeContext without validation. Assumes that the passed
	 * TypeContext was validated by `vm::code::detail::validateTypes()` beforehand (in particular,
	 * there are no cycles in the inheritance hierarchy).
	 */
	Box<TypeMetadata> buildTypeMetadata(const TypeContext& ctx);

	/**
	 * @brief Expands the existing `type_metadata` with new_types.
	 * @param type_metadata A reference TypeMetadata to fill with new types.
	 * @param new_types New types to add to the existing type_metadata.
	 * TODOP: Update that comment because TypeContext
	 *
	 * @note This function does not invalidate the old references in the given type_metadata
	 * @note Assumes that the newly added types won't invalidate the state. Before calling this
	 * function you should use `vm::code::detail::revalidateTypes()`.
	 */
	void rebuildTypeMetadata(
		Ref<TypeMetadata> type_metadata, const TypeContext& ctx
	);
}
