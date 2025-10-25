#pragma once

#include <typesystem/higher/symbol_type.hpp>

#include "base/types/floats.hpp"
#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <string>

// TODOP: All Comment in this file.
// TODOP: Macrofy this
namespace compiler::ctv {
	/**
	 * @brief Represents a value known at compile time.
	 */
	class CompileTimeValue {
	public:
		class UnitCTV {};

	private:
		// @TODO: Add Support for 128 bit integers
		// TODOP: Add issue?
		using Storage = std::variant<
			UnitCTV,
			i8,
			i16,
			i32,
			i64,
			u8,
			u16,
			u32,
			u64,
			f32,
			f64,
			f128,
			bool,
			tsh::SymbolType<>>;
		Storage value;

	public:
		CompileTimeValue();

		/**
		 * @brief Template constructor of CTV for all types which exist in the Storage variant.
		 */
		template<typename T>
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>) CompileTimeValue(T val): value(val) {}

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

		/**
		 * @brief Retrieves the value of type UnitCTV from the CTV.
		 * @return A UnitCTV or an empty optional if the CTV didn't store a value of type UnitCTV.
		 */
		[[nodiscard]] base::Optional<UnitCTV> getUnit() const;

		/**
		 * @brief Retrieves the integer value from the CTV, converting smaller integers if necessary.
		 * @return A i8 value or an empty optional if the CTV didn't store an integer.
		 */
		[[nodiscard]] base::Optional<i8> getI8() const;

		/**
		 * @brief Retrieves the integer value from the CTV, converting smaller integers if necessary.
		 * @return A i16 value or an empty optional if the CTV didn't store an integer.
		 */
		[[nodiscard]] base::Optional<i16> getI16() const;

		/**
		 * @brief Retrieves the integer value from the CTV, converting smaller integers if necessary.
		 * @return A i32 value or an empty optional if the CTV didn't store an integer.
		 */
		[[nodiscard]] base::Optional<i32> getI32() const;

		/**
		 * @brief Retrieves the integer value from the CTV, converting smaller integers if necessary.
		 * @return A i64 value or an empty optional if the CTV didn't store an integer.
		 */
		[[nodiscard]] base::Optional<i64> getI64() const;

		/**
		 * @brief Retrieves the integer value from the CTV, converting smaller integers if necessary.
		 * @return A i8 value or an empty optional if the CTV didn't store an integer.
		 */
		[[nodiscard]] base::Optional<u8> getU8() const;

		/**
		 * @brief Retrieves the integer value from the CTV, converting smaller integers if necessary.
		 * @return A i16 value or an empty optional if the CTV didn't store an integer.
		 */
		[[nodiscard]] base::Optional<u16> getU16() const;

		/**
		 * @brief Retrieves the integer value from the CTV, converting smaller integers if necessary.
		 * @return A i32 value or an empty optional if the CTV didn't store an integer.
		 */
		[[nodiscard]] base::Optional<u32> getU32() const;

		/**
		 * @brief Retrieves the integer value from the CTV, converting smaller integers if necessary.
		 * @return A i64 value or an empty optional if the CTV didn't store an integer.
		 */
		[[nodiscard]] base::Optional<u64> getU64() const;

		/**
		 * @brief Retrieves the float value from the CTV.
		 * @return A f80 value or an empty optional if the CTV didn't store a float.
		 */
		[[nodiscard]] base::Optional<f32> getF32() const;

		/**
		 * @brief Retrieves the float value from the CTV.
		 * @return A f80 value or an empty optional if the CTV didn't store a float.
		 */
		[[nodiscard]] base::Optional<f64> getF64() const;

		/**
		 * @brief Retrieves the float value from the CTV.
		 * @return A f80 value or an empty optional if the CTV didn't store a float.
		 * TODOP: This approach is temporary. Write about that.
		 */
		[[nodiscard]] base::Optional<f128> getF128() const;

		/**
		 * @brief Retrieves the value of type bool from the CTV.
		 * @return A bool value or an empty optional if the CTV didn't store a value of type bool.
		 */
		[[nodiscard]] base::Optional<bool> getBool() const;

		/**
		 * @brief Retrieves the value of type from the CTV.
		 * @note Possibly converts tuple and unit values to types. @TODO: #1373 reconsider this.
		 * @param ctx The query context for lifting unit value to unit type.
		 * @return A type value or an empty optional if the CTV didn't store a type.
		 */
		[[nodiscard]] base::Optional<tsh::SymbolType<>> getType(query::Context& ctx) const;

		/**
		 * @brief Retrieves the int value from the CTV.
		 * @return A i64 value or an empty optional if the CTV didn't store a float.
		 */
		[[nodiscard]] base::Optional<i64> asI64() const;

		/**
		 * @brief Retrieves the float value from the CTV.
		 * @return A f128 value or an empty optional if the CTV didn't store a float.
		 */
		[[nodiscard]] base::Optional<f128> asF128() const;
	};
}
