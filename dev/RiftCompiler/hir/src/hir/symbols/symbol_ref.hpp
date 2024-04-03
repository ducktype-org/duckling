#pragma once

#include <base/smart_pointers.hpp>
#include <base/stable_container.hpp>

// @TODO: move scope_ref to: hir/scopes/scope_ref

namespace symtable {
	class Scope;
	class Symbol;

	/**
	 * @brief Reference to HIR-symbol
	 */
	using SymbolRef = base::borrow_ptr<Symbol>;

	/**
	 * @brief Const reference to HIR-symbol
	 */
	using SymbolCRef = base::c_borrow_ptr<Symbol>;

	namespace detail {
		using ScopesList = base::StableIntList<Scope>;

		// @TODO: For SymbolsList: StableList of pointers is needed (this is just vector of
		// pointers) For ease of use UPtrStableList might be added
	}

	/**
	 * @brief Reference to HIR-scope
	 */
	using ScopeRef = detail::ScopesList::Ref;

	/**
	 * @brief Const reference to HIR-scope
	 */
	using ScopeCRef = detail::ScopesList::CRef;
}
