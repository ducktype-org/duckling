/**
 * @file type_desc.hpp
 * @brief TypeDesc implementation
 */

#pragma once

#include "value_category.hpp"

#include <concepts>

namespace tsh {

	class TypeInfo;

	/**
	 * @brief The TypeDesc class contains information about a type,
	 * expanded with information about a value of that type.
	 *
	 * A TypeDesc<TypeInfo> object will contain information about any type,
	 * while a TypeDesc<IntegralInfo> object is guaranteed to contain information
	 * about some Integral type described with an IntegralInfo object.
	 *
	 * A TypeDesc object describes a value. That value has a type, but it
	 * may have additional properties, like immutability. Information about
	 * the type is held in the typeInfo field, and everything else is
	 * stored in the valueCategory field.
	 *
	 * @tparam TYPE_INFO The underlying class from the TypeInfo hierarchy.
	 */
	template<std::derived_from<TypeInfo> TYPE_INFO = TypeInfo>
	class TypeDesc {
	public:
		/**
		 * @brief Constructs the TypeDesc from another TypeDesc.
		 *
		 * The source TypeDesc must hold a type description which is dynamically convertible
		 * to the type description expected by the target TypeDesc. Otherwise,
		 * the dynamic cast, and then the constructor, will fail.
		 *
		 * @tparam OTHER_TYPE_INFO The type of the source type description,
		 * from the TypeInfo hierarchy.
		 * @param other The source TypeDesc.
		 */
		template<std::derived_from<TypeInfo> OTHER_TYPE_INFO>
		TypeDesc(TypeDesc<OTHER_TYPE_INFO> other):
			  type_info(other.getType()),
			  value_category(other.getValueCategory()) {}

		/**
		 * @brief Constructs the TypeDesc directly from its contents.
		 * @param type_info The source type description, from the TypeInfo hierarchy.
		 * @param value_category The value category of the described value.
		 */
		TypeDesc(const TYPE_INFO type_info, const ValueCategory value_category):
			  type_info(type_info),
			  value_category(value_category) {}

		TypeDesc(const TypeDesc& other) = default;

		/**
		 * @brief Gets the underlying type information.
		 * @return The underlying type information.
		 */
		[[nodiscard]]
		TYPE_INFO getType() const {
			return type_info;
		}

		/**
		 * @brief Gets the underlying value category.
		 * @return The underlying value category.
		 */
		[[nodiscard]]
		ValueCategory getValueCategory() const {
			return value_category;
		}

		/**
		 * @brief Three-way comparison with another TypeDesc.
		 *
		 * This comparison is arbitrary (in this case, lexicographical),
		 * and just as is the case with TypeInfo and ValueCategory, should
		 * only be used to index ordered data structures or compare for equality.
		 *
		 * @tparam OTHER_TYPE_INFO The type of the source type description,
		 * from the TypeInfo hierarchy.
		 * @param other The other TypeDesc.
		 * @return Result of comparison, as std::strong_ordering.
		 */
		template<std::derived_from<TypeInfo> OTHER_TYPE_INFO>
		[[nodiscard]]
		auto operator<=>(const TypeDesc<OTHER_TYPE_INFO>& other) const {
			if (auto type_cmp = type_info <=> other.getType(); type_cmp != 0) return type_cmp;
			return value_category <=> other.getValueCategory();
		}

		/**
		 * @brief Template equality operator for ease of use, because a template <=> operator
		 * does not work very well for deducing other comparison operators.
		 * @tparam OTHER_TYPE_INFO The type of the source type description,
		 * from the TypeInfo hierarchy.
		 * @param other The other TypeDesc.
		 * @return Result of comparison, as bool.
		 */
		template<std::derived_from<TypeInfo> OTHER_TYPE_INFO>
		[[nodiscard]]
		bool operator==(const TypeDesc<OTHER_TYPE_INFO>& other) const {
			return *this <=> other == 0;
		}

	private:
		TYPE_INFO     type_info;
		ValueCategory value_category;
	};
}
