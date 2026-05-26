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
		 * Represents a compiler-generated implicit constructor for a class (or other type such as a
		 * tuple).
		 *
		 * The implicit constructor is a function that takes parameters for each field of the type
		 * and returns an instance of the type with those fields initialised accordingly.
		 *
		 * @note Different types, such as tuples, might have an implicit ctor as well.
		 */
		struct ImplicitConstructor final {
			tsh::AbstractType target_type;  // The type of the object this constructor belongs to.

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
		 * The default constructor is a function that doesn't take any parameters and loops through
		 * the static array initializing its fields with a default value (which may mean a call to
		 * another constructor). Returns the initialized static array value.
		 */
		struct DefaultStaticArrayConstructor final {
			tsh::AbstractType array_type;

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		struct ToStringMethod final {
			tsh::AbstractType owner_type;

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		struct DefaultDestructor final {
			tsh::AbstractType owner_type;

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
		 * Represents a compiler-generated field in a type. That type does not need to be a class.
		 * For example, the `_1`, `_2`, etc. fields in tuples.
		 */
		struct Field final {
			tsh::AbstractType parent_type;  // The type that the field belongs to
			// @TODO: #2515 Remove this
			u64 index;  // The index of the generated field

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		/**
		 * Represents a compiler-generated variable (not parameter) in a function inside a compiler
		 * generated function, such as the `ImplicitConstructor`. This variable is uniquely
		 * identified by it's index and belongs to a generated function.
		 * @note When inserting variables into regular (not generated functions), where getting a
		 * `variable_index` is unachievable use `ControlFlowLocal`.
		 */
		struct GeneratedFunctionVariable final {
			SymID function_symbol;  // The symbol of the function this variable belongs to.
			u64   variable_index;   // The index of the variable in the function's body.
			// @TODO: #2515 Remove this
			tsh::SymbolType<> type;  // The type of the variable.

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		/**
		 * @brief Represents a compiler-generated local variable injected into a specific scope
		 * during desugaring.
		 *
		 * Unlike `Variable`, which is used for synthesizing whole function bodies,
		 * `ControlFlowLocal` is used when lowering complex statements (like `for` loops) into
		 * simpler building blocks. It represents auxiliary variables (e.g., iterators, hidden
		 * collection references, loop counters) that live within a specific scope.
		 *
		 * These variables identified by the scope they belong to and their `role` (typically their
		 * name).
		 */
		struct ControlFlowLocal final {
			ScopeID     owning_scope;  /// Scope that this local belongs to.
			base::StrID role;  /// Role of the variable (typically it's name). Needed to distinguish
			                   /// between many Locals inserted into the same scope.
			// @TODO: #2515 Remove this
			tsh::SymbolType<> type;  /// The type of the variable;

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
			ToStringMethod,
			DefaultDestructor,
			BuiltinOperator,
			Parameter,
			SelfParameter,
			Field,
			GeneratedFunctionVariable,
			ControlFlowLocal,
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
				variant_case_novalue(DefaultClassConstructor, DefaultStaticArrayConstructor) {
					return true;
				}
				variant_default { return false; }
			}
		}
	};
}
