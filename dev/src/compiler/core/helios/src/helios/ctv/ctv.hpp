#pragma once

#include <typesystem/higher/symbol_type.hpp>

#include <base/variant.hpp>

#include <string>
#include <variant>

namespace compiler::helios {
	// TODOP: What about the VmValue.
	// using CompileTimeValue = std::variant<i64, bool, tsh::SymbolType<>>;

	/**
	 * @brief Represents a value known at compile time.
	 */
	class CompileTimeValue {
	private:
		using Storage = std::variant<i64, bool, tsh::SymbolType<>>;
		Storage value;

	public:
		// TODOP: Template constructor?
		CompileTimeValue();
		CompileTimeValue(i64 val);
		CompileTimeValue(bool val);
		CompileTimeValue(tsh::SymbolType<> val);

		/**
		 * @brief Retruns a constant reference to the CTVs internal value storage.
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
		 * @brief Retrieves the value of type i64 from the CTV.
		 * @return A i64 value or an empty optional if the CTV didn't store a value of type i64.
		 */
		[[nodiscard]] base::Optional<i64> asI64() const;

		/**
		 * @brief Retrieves the value of type bool from the CTV.
		 * @return A bool value or an empty optional if the CTV didn't store a value of type bool.
		 */
		[[nodiscard]] base::Optional<bool> asBool() const;

		/**
		 * @brief Retrieves the value of type from the CTV.
		 * @return A type value or an empty optional if the CTV didn't store a type.
		 */
		[[nodiscard]] base::Optional<tsh::SymbolType<>> asType() const;
	};


}
