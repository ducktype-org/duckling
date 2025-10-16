#pragma once

#include <helios/scope_symbol_id.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <base/bit256.hpp>
#include <base/ints.hpp>

#include <variant>

namespace compiler::helios::houtgen {
	/**
	 * Represents any data associated with a compiler-generated symbol. See the inner classes.
	 */
	struct GeneratedSymbolData final {
		/**
		 * Represents a compiler-generated implicit constructor for a class.
		 *
		 * The implicit constructor is a function that takes parameters for each field of the class
		 * and returns an instance of the class with those fields initialised accordingly.
		 */
		struct ImplicitConstructor final {
			SymID class_symbol;  // The symbol of the class this constructor belongs to.

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		/**
		 * Represents a compiler-generated parameter of a function. This function may itself be
		 * compiler-generated, such as the `ImplicitConstructor`.
		 */
		struct Parameter final {
			SymID function_symbol;  // The symbol of the function this parameter belongs to.
			u64   parameter_index;  // The index of the parameter in the function's signature.

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		/**
		 * Represents a compiler-generated variable (not parameter) in a function. This function may
		 * itself be compiler-generated, such as the `ImplicitConstructor`.
		 */
		struct Variable final {
			SymID function_symbol;   // The symbol of the function this variable belongs to.
			u64   variable_index;    // The index of the variable in the function's body.
			tsh::SymbolType<> type;  // The type of the variable.

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		std::variant<ImplicitConstructor, Parameter, Variable> data;

		explicit GeneratedSymbolData(
			const std::variant<ImplicitConstructor, Parameter, Variable>& data
		);

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;

		tsh::SymbolType<> getType(query::Context& ctx) const;
	};
}
