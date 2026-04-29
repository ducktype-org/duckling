#pragma once

#include <helios/scope_id.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <variant>

namespace compiler::helios::defgen {
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
		 * Represents a compiler-generated default constructor for a class.
		 *
		 * The default constructor is a function that takes no parameters and initializes all class
		 * fields with their initial values or default values if initial values where not provided.
		 * Returns the initialized class.
		 */
		struct DefaultClassConstructor final {
			SymID class_symbol;  // The symbol of the class this constructor belongs to.

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		/**
		 * Represents a compiler-generated default constructor for a static array type.
		 *
		 * The default constructor is a function that doesn't takes any parameters and loops through
		 * the static array initializing its fields with a default value (which may mean a call to
		 * another constructor). Returns the initialized static array value.
		 */
		struct DefaultStaticArrayConstructor final {
			tsh::AbstractType array_type;

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		struct BuiltinOperator final {
			// The type of the builtin operator this symbol represents.
			tsh::FunctionAbstractType operator_type;

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
		 * Represents a compiler-generated `self` parameter of a class method.
		 */
		struct SelfParameter final {
			SymID   method_symbol;
			ScopeID scope;

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

		/**
		 * Represents a compiler-generated function wrapper for REPL expressions.
		 * This is used to wrap single REPL expressions in a synthetic function.
		 * @note this does not store any function data, since this symbol is created when
		 * programmatically generating the function HOUT via QueryReplExpressionWrapper.
		 *
		 * @warning counter must never be reused with a different return_type.
		 * The mangled name is based only on counter, so reusing counter with different return_type
		 * will produce linker symbol collisions. The REPL code path (ReplSession::executeInput)
		 * enforces this by incrementing m_line_counter per statement, but any manual wrapper
		 * construction must preserve this rule.
		 */
		struct ReplExpressionWrapper final {
			u64 counter;  // A unique counter to distinguish different REPL expression wrappers.
			tsh::SymbolType<> return_type;

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		/**
		 * Represents a compiler-generated function wrapper for REPL instructions.
		 * This is used to wrap a single REPL instruction (if/while/for/block) in a
		 * synthetic void function so the DVM can execute it via runFunction.
		 */
		struct ReplInstructionWrapper final {
			u64 counter;

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		/**
		 * Represents a compiler-generated entry point for script execution.
		 * This symbol is synthetic and exists only to orchestrate script statements.
		 */
		struct ScriptMainWrapper final {
			/**
			 * Stable per-script identity used as one component of QueryGeneratedSymbol key hashing.
			 *
			 * @note Full ScriptMainWrapper unstable hash also includes `scope` (via
			 * `scope.queryUnstablePerfectHash()`), so final unstable identity is scope-dependent.
			 *
			 * @note This does NOT define the emitted linker symbol name.
			 * The emitted entry name is still `main` (set separately as symbol name).
			 */
			base::StrID script_id;

			/**
			 * Root scope assigned to the generated script `main` symbol.
			 *
			 * This scope is required so `isGlobalFun` recognizes the symbol as global.
			 *
			 * @note For detailed explanation see docs for the buildScriptMainWrapper function in
			 * helios/repl_utils/script_helpers.hpp.
			 *
			 * @TODO: #895 When entry points become explicit (not inferred from global `main`),
			 * reevaluate whether this stored scope is still required for ScriptMainWrapper.
			 */
			ScopeID scope;

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		using GeneratedSymbolDataVariant = std::variant<
			ImplicitConstructor,
			DefaultClassConstructor,
			DefaultStaticArrayConstructor,
			BuiltinOperator,
			Parameter,
			SelfParameter,
			Variable,
			ReplExpressionWrapper,
			ReplInstructionWrapper,
			ScriptMainWrapper>;
		GeneratedSymbolDataVariant data;

		explicit GeneratedSymbolData(const GeneratedSymbolDataVariant& data);

		[[nodiscard]]
		base::Bit256                          queryUnstablePerfectHash() const;
		tsh::SymbolType<>                     getType(query::Context& ctx) const;
		[[nodiscard]] ScopeID                 getScope() const;
		[[nodiscard]] base::Optional<ScopeID> maybeScope() const;

		/**
		 * @brief Whether GeneratedSymbolData stores a generated default constructor.
		 * @return True if the inner variant stores a default constructor, false otherwise.
		 */
		[[nodiscard]] bool isDefaultConstructor() const {
			variant_match(data) {
				variant_case_novalue(DefaultClassConstructor) { return true; }
				variant_case_novalue(DefaultStaticArrayConstructor) { return true; }
				variant_default { return false; }
			}
		}
	};
}
