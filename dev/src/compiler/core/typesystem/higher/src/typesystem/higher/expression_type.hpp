/**
 * @file expression_type.hpp
 * @brief ExpressionType implementation
 */

#pragma once

#include "abstract_type.hpp"
#include "symbol_type.hpp"
#include "value_category.hpp"

#include <concepts>

namespace compiler::tsh {
	/**
	 * @brief The ExpressionType class contains information about a type,
	 * expanded with information about a value of that type.
	 *
	 * An ExpressionType<AbstractType> object will contain information about any type,
	 * while a ExpressionType<IntegralAbstractType> object is guaranteed to contain information
	 * about some Integral type described with an IntegralAbstractType object.
	 *
	 * An ExpressionType object describes the value of an expression. That value has a type, as well
	 * as mutability, referential access, uniqueness, and leakage specifiers. These are specified
	 * in the symbol_type field. However, the value of an expression is also described by its
	 * value category and associated semantics, like copy and move capability — these are specified
	 * in the value_category field.
	 *
	 * @tparam ABSTRACT_TYPE The underlying class from the AbstractType hierarchy.
	 */
	template<std::derived_from<AbstractType> ABSTRACT_TYPE = AbstractType>
	class ExpressionType {
	public:
		/**
		 * @brief Constructs the ExpressionType from another ExpressionType.
		 *
		 * The source ExpressionType must hold a symbol type (and by extension an abstract type)
		 * which is dynamically convertible to the one expected by the target ExpressionType.
		 * Otherwise, the dynamic cast, and then the constructor, will fail.
		 *
		 * @tparam OTHER_ABSTRACT_TYPE The kind of the source abstract type,
		 * from the AbstractType hierarchy.
		 * @param other The source ExpressionType.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		ExpressionType(ExpressionType<OTHER_ABSTRACT_TYPE> other):
			  symbol_type(other.getSymbolType()),
			  value_category(other.getValueCategory()) {}

		/**
		 * @brief Constructs the ExpressionType directly from its contents.
		 * @param symbol_type The source symbol type.
		 * @param value_category The value category of the described value.
		 */
		ExpressionType(
			const SymbolType<ABSTRACT_TYPE> symbol_type, const ValueCategory value_category
		):
			  symbol_type(symbol_type),
			  value_category(value_category) {}

		ExpressionType(const ExpressionType& other) = default;

		/**
		 * @brief Gets the underlying abstract type.
		 * @return The underlying abstract type.
		 */
		[[nodiscard]]
		ABSTRACT_TYPE getType() const {
			return symbol_type.getType();
		}

		/**
		 * @brief Gets the underlying symbol type.
		 * @return The underlying symbol type.
		 */
		[[nodiscard]]
		const SymbolType<ABSTRACT_TYPE>& getSymbolType() const {
			return symbol_type;
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
			if (auto symbol_cmp = symbol_type <=> other.getSymbolType(); symbol_cmp != 0)
				return symbol_cmp;
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

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(symbol_type, value_category);
		}

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const ExpressionType& t
		) noexcept {
			addToHash(h, t.queryUnstablePerfectHash());
		}

	private:
		SymbolType<ABSTRACT_TYPE> symbol_type;
		ValueCategory             value_category;
	};
}
