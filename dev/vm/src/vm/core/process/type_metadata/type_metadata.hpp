#pragma once

#include "type.hpp"
#include <deque>

namespace vm {
	namespace parser {
		struct ParsedProgram;
	}

	/**
	 * @brief Holds metadata about all types in the VCPU.
	 *
	 * This class is used to store and access all types
	 * that are used in the VCPU.
	 */
	class TypeMetadata {
	private:
		enum class TypeMetadataState { AddingTypes, Finalized };

		/**
		 * @note We are using std::deque here, because references its data are always valid. (Do not
		 * become dangling).
		 * We also assume that we **never pop** from this structure.
		 */
		std::deque<Type>               types{};
		std::vector<TypeID>            types_ids{};
		base::Map<base::StrID, TypeID> names_to_type{};

		TypeMetadataState state{ TypeMetadata::TypeMetadataState::AddingTypes };

	public:
		TypeMetadata() = default;

		TypeMetadata(const TypeMetadata&)            = default;
		TypeMetadata(TypeMetadata&&) noexcept        = default;
		TypeMetadata& operator=(const TypeMetadata&) = default;
		TypeMetadata& operator=(TypeMetadata&&)      = default;
		TypeRef       addType(Type&& type);

		/**
		 * @brief Finalize adding types.
		 */
		void finalize();

		[[nodiscard]]
		TypeCRef getType(TypeID id) const;

		[[nodiscard]]
		base::Optional<TypeCRef> getTypeSafe(TypeID id) const;

		// @TODO: This function is currently used by parser, but
		// should be deleted in the future
		[[nodiscard]]
		base::Optional<TypeCRef> getTypeByName(base::StrID name) const;
	};
}
