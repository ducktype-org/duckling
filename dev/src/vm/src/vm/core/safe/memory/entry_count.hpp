#pragma once

#include <base/types/ints.hpp>

#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/kinds/data.hpp>
#include <vm/core/safe/type_metadata/type.hpp>

#include <cstddef>
#include <type_traits>

namespace vm {
	struct ShadowEntry;

	/**
	 * @brief Number of `EntryT` entries an object of `type` occupies. The single place that maps a
	 * type to an entry count per memory flavour: bytes for the data memory, shadow entries for the
	 * shadow memory. Every new `EntryT` has to add its own mapping here.
	 */
	template<typename EntryT>
	[[nodiscard]] usize entryCountFor(TypeCRef type) {
		if constexpr (std::is_same_v<EntryT, byte>)
			return type->getSize().asInt();
		else if constexpr (std::is_same_v<EntryT, ShadowEntry>)
			return type->getShadowSize();
		else
			static_assert(false, "No entry count mapping for this EntryT");
	}

	/**
	 * @brief Offset, in `EntryT` entries, of a field inside its data type. See `entryCountFor`.
	 */
	template<typename EntryT>
	[[nodiscard]] usize fieldEntryOffsetFor(const kind::FieldDesc& field) {
		if constexpr (std::is_same_v<EntryT, byte>)
			return field.offset.asInt();
		else if constexpr (std::is_same_v<EntryT, ShadowEntry>)
			return field.shadow_offset;
		else
			static_assert(false, "No entry offset mapping for this EntryT");
	}
}
