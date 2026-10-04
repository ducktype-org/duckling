#pragma once

#include <base/pointers/box.hpp>

#include <vm/bytecode/validator/valid_type/type_map.hpp>
#include <vm/core/safe/type_metadata/type_metadata.hpp>

namespace vm::code::detail {
	/**
	 * @brief Expands the existing `type_metadata` with new_types.
	 * @param type_metadata A reference TypeMetadata to fill with new types.
	 * @param types Set of types containing both the old and new types. New types from this
	 * stucture will be added to the type metadata.
	 *
	 * @note This function does not invalidate the old references in the given `type_metadata`
	 *
	 * @note One could wonder why pass the whole `TypeContext` instead of only a vector of
	 * new_types. The answer is we need the full type context in order to build vtables for
	 * inheritable types.
	 *
	 * @note Assumes that the newly added types won't invalidate the state. Assumes types are
	 * validated.
	 */
	void rebuildTypeMetadata(Ref<TypeMetadata> type_metadata, const valid_type::ValidTypeMap& types);
}
