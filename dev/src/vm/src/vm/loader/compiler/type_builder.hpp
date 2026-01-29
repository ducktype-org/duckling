#pragma once

#include <base/pointers/box.hpp>

#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code::detail {
	/**
	 * @brief Builds TypeMetadata from TypeContext without validation. Assumes that
	 * provided types are validated.
	 */
	Box<TypeMetadata> buildTypeMetadata(const ObjIdNameMap<TypeOfData>& types);

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
	void rebuildTypeMetadata(Ref<TypeMetadata> type_metadata, const ObjIdNameMap<TypeOfData>& types);
}
