#pragma once

#include <vector>
#include <helios/scopes/scopes.hpp>
#include <helios/symbols/symbols.hpp>

// @todo relax this dependency, just expr is needed (#404)
#include <helios/hout/elements/expr.hpp>

namespace compiler::helios::test_utils {
	/**
	 * Get the ModuleID and ScopeID of a module in the given directory.
	 * @param path The path to the module directory.
	 * @return The module's ModuleID and ScopeID.
	 */
	std::pair<frontend::ModuleID, ScopeID> getModule(const fs::FilePath& path);

	/**
	 * Get the SymIDs of all symbols in a chain in a given scope.
	 * @param chain The symbol chain to look up, e.g. `"N1.N2.X"`.
	 * @param scope The scope in which to perform the lookup, like the scope of a module.
	 * @return The symbols of all elements of a chain. In particular, the symbol of the last
	 * element in the chain is accessed with the `back()` method.
	 */
	std::vector<SymID> getChain(const std::string_view chain, ScopeID scope);

	/**
	 * Get the integral value of the last symbol in a symbol chain in a given scope.
	 * @param chain The symbol chain to resolve.
	 * @param scope The scope in which to resolve.
	 * @return The value of the last symbol in the chain.
	 */
	i64 getValue(const std::string_view chain, ScopeID scope);

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
	tsh::AbstractType getTypeFromDefinition(const std::string_view chain, ScopeID scope);

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
}
