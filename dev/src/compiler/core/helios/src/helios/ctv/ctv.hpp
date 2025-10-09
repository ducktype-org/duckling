#pragma once

#include <typesystem/higher/symbol_type.hpp>

#include <base/type_traits.hpp>
#include <base/variant.hpp>

#include <string>

namespace compiler::helios {
	/**
	 * @brief Represents a value known at compile time.
	 */
	class CompileTimeValue {
	public:
		class UnitCTV {};

	private:
		using Storage = std::variant<UnitCTV, i64, bool, tsh::SymbolType<>>;
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
		[[nodiscard]] base::Optional<UnitCTV> asUnit() const;

		/**
		 * @brief Retrieves the value of type i64 from the CTV.
		 * @return An i64 value or an empty optional if the CTV didn't store a value of type i64.
		 */
		[[nodiscard]] base::Optional<i64> asI64() const;

		/**
		 * @brief Retrieves the value of type bool from the CTV.
		 * @return A bool value or an empty optional if the CTV didn't store a value of type bool.
		 */
		[[nodiscard]] base::Optional<bool> asBool() const;

		/**
		 * @brief Retrieves the value of type from the CTV.
		 * @note Possibly converts tuple and unit values to types. @TODO: #1373 reconsider this.
		 * @param ctx The query context for lifting unit value to unit type.
		 * @return A type value or an empty optional if the CTV didn't store a type.
		 */
		[[nodiscard]] base::Optional<tsh::SymbolType<>> asType(query::Context& ctx) const;
	};
}
