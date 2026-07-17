#pragma once

#include <ctv/ctv.hpp>
#include <ctv/numeric_value.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/elements/expr.hpp>  // @todo relax this dependency, just expr is needed (#404)
#include <helios/scope_id.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/utils/symbol_list.hpp>

#include <filesystem/file.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <type_traits>

namespace compiler::helios::test_utils {
	/**
	 * Get the ModuleID and ScopeID of a module in the given directory.
	 * @param path The path to the module directory.
	 * @return The module's ModuleID and ScopeID.
	 */
	std::pair<frontend::ModuleID, ScopeID> getModule(const fs::File& path);

	/**
	 * Get the ScopeID of the main file of a module that was registered as a package (e.g. via the
	 * driver test utils), so that its `import`s of the standard library resolve.
	 * @param module The module's ModuleID.
	 * @return The module's main-file root ScopeID.
	 */
	ScopeID getModuleScope(frontend::ModuleID module);

	/**
	 * Get the SymIDs of all symbols in a chain in a given scope.
	 * @param chain The symbol chain to look up, e.g. `"N1.N2.X"`.
	 * @param scope The scope in which to perform the lookup, like the scope of a module.
	 * @return The symbols of all elements of a chain. In particular, the symbol of the last
	 * element in the chain is accessed with the `back()` method.
	 */
	SymbolList getChain(const std::string_view chain, ScopeID scope);

	/**
	 * Get the CTV representing a constant value of the last symbol in a symbol chain in a given
	 * scope.
	 * @note Used as a helper for `getConstValueAs` since we can't use QueryConstValueOf in this
	 * header file.
	 * @param chain The symbol chain to resolve.
	 * @param scope The scope in which to resolve.
	 * @return The CTV value of the last symbol in the chain.
	 */
	ctv::CompileTimeValue getConstValue(std::string_view chain, ScopeID scope);

	/**
	 * Get the value of type T of the last symbol in a symbol chain in a given scope.
	 * @tparam T The expected type of the value.
	 * @param chain The symbol chain to resolve.
	 * @param scope The scope in which to resolve.
	 * @return The value of the last symbol in the chain.
	 */
	template<typename T>
	T getConstValueAs(const std::string_view chain, ScopeID scope) {
		static_assert(
			std::is_constructible_v<ctv::CompileTimeValue, T>
				|| std::is_constructible_v<numeric_value::NumericValue, T>,
			"getConstValueAs was called with a type which doesn't exist in CTV and NumericValue"
		);

		auto ctv_result = getConstValue(chain, scope);

		base::Optional<T> maybe_value{};
		if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
			auto maybe_numeric_value = ctv_result.get<numeric_value::NumericValue>();
			maybe_value              = maybe_numeric_value->get<T>();
		} else {
			maybe_value = ctv_result.get<T>();
		}

		CORE_ASSERT(
			maybe_value.has_value(),

			base::strConcat("Constant '", chain, "' has a different type than expected")
		);
		return maybe_value.value();
	}

	/**
	 * Get the type of the value associated with last symbol in a symbol chain in a given scope.
	 * Use this to get the type of the variable `a` in `var a : i32`.
	 * @param chain The symbol chain to resolve.
	 * @param scope The scope in which to resolve.
	 * @return The symbol type of the last symbol in the chain.
	 */
	tsh::SymbolType<> getSymbolTypeOf(const std::string_view chain, ScopeID scope);

	/**
	 * Get the abstract type of the value associated with last symbol in a symbol chain
	 * in a given scope. Use this to get the type of the variable `a` in `var a : i32`.
	 * @param chain The symbol chain to resolve.
	 * @param scope The scope in which to resolve.
	 * @return The abstract type of the last symbol in the chain.
	 */
	tsh::AbstractType getTypeOf(const std::string_view chain, ScopeID scope);

	/**
	 * Get the type associated with the last symbol in a symbol chain in a given scope.
	 * Use this to get the type `T` of the symbol `T` in definition `struct T {}`.
	 * @param chain The symbol chain to resolve.
	 * @param scope The scope in which to resolve.
	 * @return The type of the last symbol in the chain.
	 */
	tsh::SymbolType<> getTypeFromDefinition(const std::string_view chain, ScopeID scope);

	/**
	 * @brief returns hout-expr of the initialization value of given const.
	 * For `const a = 42`, it returns the hout-expr of `42`.
	 * @note It's a hack-ish method, for easy testing only
	 */
	Box<code::Expr> getExprOfConst(SymID sym);

	/**
	 * @brief returns hout-expr of the initialization value of given variable.
	 * For `var a = 42`, it returns the hout-expr of `42`.
	 * @note It's a hack-ish method, for easy testing only
	 */
	Box<code::Expr> getExprOfVariable(SymID sym);

	/**
	 * @brief Gets function body scope of given function.
	 * @param sym The function symbol ID.
	 */
	helios::ScopeID getFunctionBodyScope(SymID sym);
}
