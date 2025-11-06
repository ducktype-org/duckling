#pragma once

#include <ctv/numeric_value.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <string>

namespace compiler::ctv {
	using numeric_value::NumericValue;

	/**
	 * @brief Represents a value known at compile time.
	 */
	class CompileTimeValue {
	public:
		class UnitCTV {};

	private:
		using Storage = std::variant<UnitCTV, NumericValue, bool, tsh::SymbolType<>>;
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
		 * @brief Retrieves the value of the given type from the CTV.
		 * @return A stored value or an empty optional if the CTV didn't store the requested type.
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
		 * @brief Retrieves the value of type from the CTV.
		 * @note Possibly converts tuple and unit values to types. @TODO: #1373 reconsider this.
		 * @param ctx The query context for lifting unit value to unit type.
		 * @return A type value or an empty optional if the CTV didn't store a type.
		 */
		[[nodiscard]] base::Optional<tsh::SymbolType<>> getType(query::Context& ctx) const;
	};
}
