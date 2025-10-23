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
		using Storage
			= std::variant<UnitCTV, i16, i32, i64, f16, f32, f64, f128, bool, tsh::SymbolType<>>;
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

		// /**
		//  * @brief Retrieves the integer value from the CTV, converting smaller integers if
		//  necessary.
		//  * @return A i128 value or an empty optional if the CTV didn't store an integer.
		//  */
		// [[nodiscard]] base::Optional<i128> getI128() const;

		/**
		 * @brief Retrieves the float value from the CTV.
		 * @return A f80 value or an empty optional if the CTV didn't store a float.
		 */
		[[nodiscard]] base::Optional<f16> getF16() const;

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
		 * TODOP: Needed?
		 */
		[[nodiscard]] base::Optional<f80> getF80() const;

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
		 * @brief Retrieves the float value from the CTV.
		 * @return A i64 value or an empty optional if the CTV didn't store a float.
		 */
		[[nodiscard]] base::Optional<i64> asI64() const;

		// /**
		//  * @brief Retrieves the float value from the CTV.
		//  * @return A i128 value or an empty optional if the CTV didn't store a float.
		//  */
		// [[nodiscard]] base::Optional<i128> asI128() const;
	};

	/**
	 * @brief Creates a CompileTimeValue with the smallest possible integer type to hold the value.
	 */
	// TODOP: Maybe this should be a class member.
	inline CompileTimeValue makeMinimizedCTV(i64 val) {
		if (val >= std::numeric_limits<i16>::min() && val <= std::numeric_limits<i16>::max())
			return CompileTimeValue{ static_cast<i16>(val) };
		else if (val >= std::numeric_limits<i32>::min() && val <= std::numeric_limits<i32>::max())
			return CompileTimeValue{ static_cast<i32>(val) };
		else if (val >= std::numeric_limits<i64>::min() && val <= std::numeric_limits<i64>::max())
			return CompileTimeValue{ static_cast<i64>(val) };
		else
			return CompileTimeValue{ val };  // i64 is the smallest safe type here
	}

	/**
	 * @brief Creates a CompileTimeValue with the smallest possible float type to hold the value
	 * without losing precision.
	 */
	inline CompileTimeValue makeMinimizedCTV(f128 val) {
		if (static_cast<f128>(static_cast<f16>(val)) == val)
			return CompileTimeValue{ static_cast<f16>(val) };
		if (static_cast<f128>(static_cast<f32>(val)) == val)
			return CompileTimeValue{ static_cast<f32>(val) };
		else if (static_cast<f128>(static_cast<f64>(val)) == val)
			return CompileTimeValue{ static_cast<f64>(val) };
		return CompileTimeValue{ val };  // Full precision needed
	}
}
