#pragma once

#include <base/collections/optional.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/types/floats.hpp>
#include <base/types/ints.hpp>

#include <cmath>
#include <variant>

// TODOP: All Comment in this file.
namespace compiler::numeric_value {
	/**
	 * @brief Represents a numeric value.
	 */
	// TODOP: Fix comments in file.
	// TODOP: Future BigInt.
	// TODOP: Support for 8, 128 bit integers.
	class NumericValue {
		using Storage = std::variant<i16, i32, i64, u16, u32, u64, f32, f64, f128>;
		Storage value;

	public:
		NumericValue();

		/**
		 * @brief Template constructor for all types which exist in the Storage variant.
		 */
		template<typename T>
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>) NumericValue(T val): value(val) {}

		/**
		 * @brief Returns a constant reference to the CTVs internal value storage.
		 * @return A constant reference to the CTV value storage.
		 */
		[[nodiscard]] const Storage& getStorage() const;

		/**
		 * @brief Transforms the value stored in the CTV to a string representation. Used for debug
		 * purposes.
		 * @return A string representation of the value stored in the CTV.
		 */
		[[nodiscard]] std::string toString() const;

		template<typename TargetType>
		requires(std::is_arithmetic_v<TargetType>)
		[[nodiscard]] base::Optional<TargetType> coerceTo() const {
			return std::visit(
				[&](auto&& stored_val) -> base::Optional<TargetType> {
					using StoredType = std::decay_t<decltype(stored_val)>;

					// Integer to integer.
					if constexpr (std::is_integral_v<StoredType> && std::is_integral_v<TargetType>) {
						if constexpr (std::is_signed_v<StoredType>
					                  && std::is_unsigned_v<TargetType>) {
							if (stored_val < 0) return {};
						}
						// Check for over/underflow.
						if (stored_val > std::numeric_limits<TargetType>::max()) return {};
						if (stored_val < std::numeric_limits<TargetType>::min()) return {};
					}
					// Floating point to integer.
					else if constexpr (std::is_floating_point_v<StoredType>
				                       && std::is_integral_v<TargetType>) {
						if (std::isnan(stored_val) || std::isinf(stored_val)) return {};
						if ((stored_val
					         > static_cast<StoredType>(std::numeric_limits<TargetType>::max()))
					        || (stored_val
					            < static_cast<StoredType>(std::numeric_limits<TargetType>::min()))) {
							return {};
						}
					}
					// Integer/Floating point to floating point.
					else if constexpr (std::is_floating_point_v<TargetType>) {
						if (std::isinf(stored_val)) {
						} else if (static_cast<f128>(stored_val)
					               > std::numeric_limits<TargetType>::max()) {
							return {};
						} else if (static_cast<f128>(stored_val)
					               < -std::numeric_limits<TargetType>::max()) {
							return {};
						}
					}

					// If we got here, than the conversion if safe.
					return static_cast<TargetType>(stored_val);
				},
				value
			);
		}

		/**
		 * TODOP: Comment
		 */
		template<typename T>
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>)
		[[nodiscard]] base::Optional<T> get() const {
			variant_match(value) {
				variant_case(T, val) { return val; }
			}
			return {};
		}
	};
}
