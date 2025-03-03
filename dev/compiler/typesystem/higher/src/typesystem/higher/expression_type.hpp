/**
 * @file expression_type.hpp
 * @brief ExpressionType implementation
 */

#pragma once

#include "abstract_type.hpp"
#include "value_category.hpp"

#include <concepts>

namespace tsh {
	/**
	 * @brief The ExpressionType class contains information about a type,
	 * expanded with information about a value of that type.
	 *
	 * A ExpressionType<AbstractType> object will contain information about any type,
	 * while a ExpressionType<IntegralInfo> object is guaranteed to contain information
	 * about some Integral type described with an IntegralInfo object.
	 *
	 * A ExpressionType object describes a value. That value has a type, but it
	 * may have additional properties, like immutability. Information about
	 * the type is held in the AbstractType field, and everything else is
	 * stored in the valueCategory field.
	 *
	 * @tparam ABSTRACT_TYPE The underlying class from the AbstractType hierarchy.
	 */
	template<std::derived_from<AbstractType> ABSTRACT_TYPE = AbstractType>
	class ExpressionType {
	public:
		/**
		 * @brief Constructs the ExpressionType from another ExpressionType.
		 *
		 * The source ExpressionType must hold a type description which is dynamically convertible
		 * to the type description expected by the target ExpressionType. Otherwise,
		 * the dynamic cast, and then the constructor, will fail.
		 *
		 * @tparam OTHER_ABSTRACT_TYPE The type of the source type description,
		 * from the AbstractType hierarchy.
		 * @param other The source ExpressionType.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		ExpressionType(ExpressionType<OTHER_ABSTRACT_TYPE> other):
			  abstract_type(other.getType()),
			  value_category(other.getValueCategory()) {}

		/**
		 * @brief Constructs the ExpressionType directly from its contents.
		 * @param abstract_type The source type description, from the AbstractType hierarchy.
		 * @param value_category The value category of the described value.
		 */
		ExpressionType(const ABSTRACT_TYPE abstract_type, const ValueCategory value_category):
			  abstract_type(abstract_type),
			  value_category(value_category) {}

		ExpressionType(const ExpressionType& other) = default;

		/**
		 * @brief Gets the underlying abstract type.
		 * @return The underlying abstract type.
		 */
		[[nodiscard]]
		ABSTRACT_TYPE getType() const {
			return abstract_type;
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
		 * @brief Three-way comparison with another ExpressionType.
		 *
		 * This comparison is arbitrary (in this case, lexicographical),
		 * and just as is the case with AbstractType and ValueCategory, should
		 * only be used to index ordered data structures or compare for equality.
		 *
		 * @tparam OTHER_ABSTRACT_TYPE The type of the source type description,
		 * from the AbstractType hierarchy.
		 * @param other The other ExpressionType.
		 * @return Result of comparison, as std::strong_ordering.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		[[nodiscard]]
		auto operator<=>(const ExpressionType<OTHER_ABSTRACT_TYPE>& other) const {
			if (auto type_cmp = abstract_type <=> other.getType(); type_cmp != 0) return type_cmp;
			return value_category <=> other.getValueCategory();
		}

		/**
		 * @brief Template equality operator for ease of use, because a template <=> operator
		 * does not work very well for deducing other comparison operators.
		 * @tparam OTHER_ABSTRACT_TYPE The type of the source type description,
		 * from the AbstractType hierarchy.
		 * @param other The other ExpressionType.
		 * @return Result of comparison, as bool.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		[[nodiscard]]
		bool operator==(const ExpressionType<OTHER_ABSTRACT_TYPE>& other) const {
			return *this <=> other == 0;
		}

	private:
		ABSTRACT_TYPE abstract_type;
		ValueCategory value_category;
	};
}
