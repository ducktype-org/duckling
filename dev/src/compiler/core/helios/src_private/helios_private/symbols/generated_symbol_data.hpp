#pragma once

#include <ctv/ctv.hpp>
#include <helios/hout/hout.hpp> // PR: try to relax it, its just for Operatoriness, we could move it elsewhere
#include <helios/scope_id.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>

#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <variant>

namespace compiler::helios::defgen {

	/**
	 * Represents a compiler-generated constructor for a type. The concrete constructor is
	 * distinguished by `kind`:
	 *
	 * - `Implicit`: takes parameters for each field of the type and returns an instance of the type
	 *   with those fields initialised accordingly. Different types, such as tuples, might have an
	 *   implicit ctor as well.
	 *
	 * - `Default`: for a class/array/tuple (in general more complex type). The default constructor
	 *   for a class takes no parameters and initializes all class fields with their initial values
	 *   or default values if initial values where not provided; returns the initialized class. For
	 *   the array, it takes no parameters and loops through the static array initializing its
	 * fields with a default value (which may mean a call to another constructor); returns the
	 * initialized static array value.
	 *
	 * - `Copy`: the compiler-generated default copy constructor. It takes a single `const ref T`
	 *   source and returns a new `T` that copies each member (trivially-copyable members are
	 *   byte-copied, non-trivially-copyable members are copied via their own copy constructor).
	 */
	struct Constructor final {
		enum class Kind {
			Implicit,
			Default,
			Copy,
		};

		tsh::AbstractType type;  // The type this constructor belongs to.
		Constructor::Kind kind;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * Represents a compiler-generated method shared by all types (the destructor, `length`,
	 * `toString`, and dynamic-array `push`/`pop`). The concrete method is distinguished by `kind`.
	 */
	struct Method final {
		enum class Kind {
			DefaultDestructor,
			LengthMethod,
			ToString,
			Push,  // Dynamic array `push` method.
			Pop,   // Dynamic array `pop` method.
		};

		tsh::AbstractType owner_type;
		Method::Kind      kind;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	struct BuiltinOperator final {
		// The type of the builtin operator this symbol represents.
		tsh::FunctionAbstractType operator_type;

		HOUTFunctionDeclaration::Operatoriness operatoriness;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	struct GeneratedConstant final {
		ctv::CompileTimeValue value;

		/**
		 * @TODO: #3099 maybe remove this, it will allow to make baking logic simpler.
		 */
		ScopeID scope;

		GeneratedConstant(ctv::CompileTimeValue value, ScopeID scope);

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * Represents a compiler-generated parameter of a function. This function may itself be
	 * compiler-generated, such as the implicit `Constructor`.
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
	 * generated function, such as the implicit `Constructor`. This variable is uniquely
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
	 * Unlike `GeneratedFunctionVariable`, which is used for synthesizing whole function bodies,
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

#define GENERATED_SYMBOL_SEMANTICS_LIST                                                          \
	defgen::Constructor, defgen::Method, defgen::BuiltinOperator, defgen::Parameter,             \
		defgen::SelfParameter, defgen::Field, defgen::GeneratedFunctionVariable,                 \
		defgen::ControlFlowLocal, defgen::ReplExpressionWrapper, defgen::ReplInstructionWrapper, \
		defgen::ScriptMainWrapper, defgen::GeneratedConstant

	using GeneratedSymbolDataVariant = std::variant<GENERATED_SYMBOL_SEMANTICS_LIST>;

	[[nodiscard]]
	base::Bit256 generatedSymbolUnstablePerfectHash(const GeneratedSymbolDataVariant&);
}
