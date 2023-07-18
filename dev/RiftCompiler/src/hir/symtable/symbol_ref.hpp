#pragma once

#include <base/smart_pointers.hpp>
#include <base/stable_container.hpp>
#include "scope_symbol_id.hpp"

namespace symtable {
	class Scope;
	class Symbol;

	using SymbolRef = base::borrow_ptr<Symbol>;
	using SymbolCRef = base::c_borrow_ptr<Symbol>;
	
	namespace detail {
		using ScopesList = base::StableIntList<Scope>;

		// @TODO: For SymbolsList: StableList of pointers is needed (this is just vector of pointers)
		// For ease of use UPtrStableList might be added	
	}

	using ScopeRef = detail::ScopesList::Ref;
	using ScopeCRef = detail::ScopesList::CRef;
}
