/**
 * @file type_desc.hpp
 * @brief TypeDesc implementation
 */

#pragma once

#include "type_info.hpp"
#include "value_category.hpp"

#include <concepts>

namespace ts {

	class TypeInfo;

	/**
	 * @TODO: we need to think if value category should be here
	 */

	template<std::derived_from<TypeInfo> TYPE_INFO = TypeInfo>
	class TypeDesc {
	public:
		template<std::derived_from<TypeInfo> OTHER_TYPE_INFO>
		TypeDesc(TypeDesc<OTHER_TYPE_INFO> other):
			  type_info(other.getType()),
			  valueCategory(other.getValueCategory()) {}

		TypeDesc(TYPE_INFO type_info, ValueCategory valueCategory = {}):
			  type_info(type_info),
			  valueCategory(valueCategory) {}

		[[nodiscard]]
		TYPE_INFO getType() const {
			return type_info;
		}

		[[nodiscard]]
		ValueCategory getValueCategory() const {
			return valueCategory;
		}

		template<std::derived_from<TypeInfo> OTHER_TYPE_INFO>
		[[nodiscard]]
		bool operator==(const TypeDesc<OTHER_TYPE_INFO>& other) const {
			return valueCategory == other.getValueCategory() && type_info == other.getType();
		}

		auto operator<=>(const TypeDesc<TYPE_INFO>& other) const = default;

		[[nodiscard]]
		bool isDescImplicitlyCoercible(const TypeDesc<>& to) const;

	private:
		TYPE_INFO     type_info;
		ValueCategory valueCategory;
	};
}
