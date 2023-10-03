#pragma once

#include <base/named_id.hpp>

namespace symtable {
	class Scope;
	class Symbol;

	// Symbol ID is currently left only for TS (@TODO: substitute for SymbolRef?)
	// Once we remove SymbolId, does this file make sense? Should it be removed
	// altogether?
	typedef base::NamedId<Symbol> SymbolId;
}  // namespace symtable
