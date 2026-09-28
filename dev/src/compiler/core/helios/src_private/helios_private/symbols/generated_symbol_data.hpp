#pragma once

#include <ctv/ctv.hpp>
#include <frontend/pst_parser/pst_config.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/hout.hpp>  // @TODO: #404 try to relax it, it's just for Operatoriness, we could move it elsewhere
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
	 * Represents a compiler-generated method shared by all types (the destructor, `length` and
	 * `toString`). The concrete method is distinguished by `kind`.
	 */
	struct Method final {
		enum class Kind {
			DefaultDestructor,
			LengthMethod,
			ToString,
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

	/**
	 * Represents a compiler-generated builtin function templated on a single type argument. The
	 * concrete builtin is distinguished by `kind`.
	 *
	 * `MoveIn` is declaration-only, its call is replaced by an instruction while lowering to LIR.
	 * @note This is not related with language template implementation.
	 */
	class BuiltinTemplatedSymbol final {
	public:
		enum class Kind {
			MoveIn  //< `move_in(ptr T, T)` - in place construct T by bytecopy
		};

		// The type argument the builtin is templated on.
		tsh::SymbolType<>            type;
		BuiltinTemplatedSymbol::Kind kind;

		[[nodiscard]] BuiltinKind getBuiltinKind() const {
			switch (kind) {
			case Kind::MoveIn:
				return BuiltinKind::MoveIn;
			default:
				CORE_PANIC("Unhandled builtin case");
			}
		}

		explicit BuiltinTemplatedSymbol(tsh::SymbolType<> type, BuiltinTemplatedSymbol::Kind kind):
			  type(type),
			  kind(kind) {}

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
	 * Represents a compiler-generated function wrapper for REPL expressions,
	 * instructions and global initializers. It's a synthetic function wrapper around them.
	 */
	struct ReplInputWrapper final {
		/**
		 * @brief A PST statement, compiled into a unit-returning body.
		 */
		struct Instruction {
			pst::HashType stmt;
		};

		/**
		 * @brief A HOUT expression, returned from the wrapper.
		 */
		struct Expression {
			SharedBox<code::Expr> expr;
		};

		using ElementVariant = std::variant<Instruction, Expression>;
		ElementVariant element;

		ReplInputWrapper(ElementVariant element): element(std::move(element)) {}

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Represents the storage of a REPL/script global variable, stripped of its initial
	 * value.
	 */
	struct ReplEmptyVariable final {
		SymID original_variable;

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

#define GENERATED_SYMBOL_SEMANTICS_LIST                                                           \
	defgen::Constructor, defgen::Method, defgen::BuiltinOperator, defgen::BuiltinTemplatedSymbol, \
		defgen::Parameter, defgen::SelfParameter, defgen::Field,                                  \
		defgen::GeneratedFunctionVariable, defgen::ControlFlowLocal, defgen::ReplInputWrapper,    \
		defgen::ReplEmptyVariable, defgen::ScriptMainWrapper, defgen::GeneratedConstant

	using GeneratedSymbolDataVariant = std::variant<GENERATED_SYMBOL_SEMANTICS_LIST>;

	[[nodiscard]]
	base::Bit256 generatedSymbolUnstablePerfectHash(const GeneratedSymbolDataVariant&);
}
