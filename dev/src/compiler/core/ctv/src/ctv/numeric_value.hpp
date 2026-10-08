// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/tsh/symbol_type.hpp>

#include <base/collections/optional.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/numeric/numeric_utils.hpp>
#include <base/types/floats.hpp>
#include <base/types/ints.hpp>

#include <query_framework/context/context.hpp>

#include <type_traits>
#include <variant>

namespace compiler::numeric_value {
	/**
	 * @brief Represents a numeric value representing a numeric literal.
	 * @TODO: #1498 Add support for f16, f128, i128.
	 */
	class NumericValue final {
	public:
		// @note: std::uint8_t and std::int8_t are used here instead of our `STRONG_TYPEDEF_INT` u8
		// and i8 so the generic code operating on numeric value won't get too complicated (we would
		// have to implement basically every numeric trait from the std to include our u8 and i8).
		// Since the risk of implicit uint8_t to char conversions is close to none when using
		// numeric value it's valid to drop the "strongly typed" requirement.
		using Storage
			= std::variant<std::int8_t, i16, i32, i64, std::uint8_t, u16, u32, u64, f32, f64>;

	private:
		Storage value;

	public:
		NumericValue() = delete;

		/**
		 * @brief Template constructor for all types which exist in the Storage variant.
		 */
		template<typename T>
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>) constexpr NumericValue(T val): value(val) {}

		/**
		 * @brief Factory method for creating a NumericValue with the minimal needed type to store
		 * the given value.
		 * @return The NumericValue storing the minimized type.
		 */
		template<typename T>
		requires(std::is_arithmetic_v<T>)
		[[nodiscard]] static constexpr NumericValue createMinimized(T value) {
			if constexpr (std::is_integral_v<T>) {
				// Prioritize signed types as they're more general.
				// @note: For now, the smallest deduced type is `i32`. We may decide to deduce `i8`
				// and `i16` in the future as well.
				if (base::fitsIn<i32>(value)) return NumericValue{ static_cast<i32>(value) };
				if (base::fitsIn<u32>(value)) return NumericValue{ static_cast<u32>(value) };
				if (base::fitsIn<i64>(value)) return NumericValue{ static_cast<i64>(value) };
				if (base::fitsIn<u64>(value)) return NumericValue{ static_cast<u64>(value) };
				return NumericValue{ static_cast<i64>(value) };
			} else if constexpr (std::is_floating_point_v<T>) {
				f64 high_prec = static_cast<f64>(value);
				if (static_cast<f64>(static_cast<f32>(high_prec)) == high_prec)
					return NumericValue{ static_cast<f32>(high_prec) };
				// Highest precision needed.
				return NumericValue{ high_prec };
			}
		}

		/**
		 * @brief Factory method for creating a NumericValue from a specific value, of the given
		 * tsh::SymbolType.
		 * @return A new NumericValue or an empty optional if the value does not fit in the target
		 * type.
		 */
		template<typename T = i64>
		requires(std::is_arithmetic_v<T>)
		[[nodiscard]] static base::Optional<NumericValue> createOfType(
			const tsh::AbstractType& type, T value = 0
		) {
			NumericValue initial = createMinimized(value);
			return initial.castTo(type);
		}

		/**
		 * @brief Returns a constant reference to the NumericValues internal value storage.
		 * @return A constant reference to the NumericValues value storage.
		 */
		[[nodiscard]] const Storage& getStorage() const;

		/**
		 * @brief Transforms the value stored in the NumericValue to a string representation. Used
		 * for debug purposes.
		 * @return A string representation of the value stored in the NumericValue.
		 */
		[[nodiscard]] std::string toString() const;

		/**
		 * @brief Performs a safe static_cast between values stored in the NumericValue.
		 * First, checks if the cast is safe in terms of underflow/overflow/nan to int conversions
		 * and then performs the static_cast.
		 * @tparam The type to cast the stored value to.
		 * @return The casted value or an empty optional if any of the checks failed.
		 */
		template<typename TargetType>
		requires(std::is_arithmetic_v<TargetType>)
		[[nodiscard]] base::Optional<TargetType> coerceTo() const {
			return std::visit(
				[&](auto&& stored_val) -> base::Optional<TargetType> {
					if constexpr (std::is_same_v<TargetType, bool>)
						return stored_val != 0;
					else if (base::fitsIn<TargetType>(stored_val))
						return static_cast<TargetType>(stored_val);
					return {};
				},
				value
			);
		}

		/**
		 * @brief Retrieves the value of the given type from the NumericValue.
		 * @return A stored value or an empty optional if the NumericValue didn't store the
		 * requested type.
		 */
		template<typename T>
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>)
		[[nodiscard]] base::Optional<T> get() const {
			variant_match(value) {
				variant_case(T, val) { return val; }
			}
			return {};
		}

		/**
		 * @brief Returns the compiler::tsh::SymbolType based on the value stored in the NumericValue
		 * @return The compiler::tsh::SymbolType of the value stored in the NumericValue.
		 */
		[[nodiscard]] tsh::SymbolType<> getTypeOfStoredValue(query::Context& ctx) const;


		/**
		 * @brief Performs a safe cast of this NumericValue to a new type specified by target_type.
		 * Checks for overflows and underflows.
		 * @param target_type The target symbol type for the cast.
		 * @return A new NumericValue with the casted value, or an empty optional if the
		 *         cast failed (e.g., overflow).
		 */
		[[nodiscard]] base::Optional<NumericValue> castTo(const tsh::AbstractType& target_type
		) const;

		/**
		 * @brief Whether the NumericValue stores an integer value.
		 * @return True if CTV stores an integer, false otherwise.
		 */
		[[nodiscard]] bool isIntegral() const;
	};
}
