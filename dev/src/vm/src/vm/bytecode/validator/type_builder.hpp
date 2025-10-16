#pragma once

#include <base/box.hpp>

#include <vm/bytecode/validator/type_context.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

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
	 * @param new_ctx New set of types containing both the old and new types. New types from this
	 * stucture will be added to the type metadata.
	 *
	 * @note This function does not invalidate the old references in the given `type_metadata`
	 *
	 * @note One could wonder why pass the whole `TypeContext` instead of only a vector of new_types.
	 * The answer is we need the full type context in order to build vtables for inheritable types.
	 *
	 * @note Assumes that the newly added types won't invalidate the state. Before calling this
	 * function you should use `vm::code::detail::validateTypesIntegrity()` on the whole context and
	 * `vm::code::detail::validateType()` on every new type.
	 */
	void rebuildTypeMetadata(Ref<TypeMetadata> type_metadata, const TypeContext& new_ctx);
}
