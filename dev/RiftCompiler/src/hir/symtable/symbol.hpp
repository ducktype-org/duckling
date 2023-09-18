#pragma once

#include "lookup_result.hpp"
#include "scope_symbol_id.hpp"
#include "symbol_ref.hpp"

#include <base/string_id.hpp>
#include <optional>
#include <typesystem/typesystem.hpp>
#include <vector>

namespace hir {
	class AnalysisState;
}

namespace symtable {

	enum class SymbolKind {
		Basic,
		Namespace,
		Function,
		CompilationUnit,
		Const,
		Struct,

		TestSymbol,
		// ...
	};

	class Symbol {
		// Update constructors when adding fields here

	protected:
		// symbol identification:
		ScopeRef                      scope;
		base::StrId                   name;
		bool                          anonymous;

		// wildcard are anonymous symbols, that behave in special way in lookup
		bool                          wildcard = false;

		// aliases are "pointers" to other symbols
		bool                          is_alias = false;

		// Common symbol data:
		bool                          is_static;

		// Scope the symbol represent in default lookup context:
		// @TODO: should be `not yet done`/`done value`/`error error`
		std::optional<ScopeRef>       linked_lookup_scope;

		// Symbol position relative to other symbols:
		usize                         relative_position;

		// symbol is dependent if it can't be used independently
		// example: class fields
		bool                          dependent = false;


		std::optional<ts::TypeDesc<>> type;
		SymbolKind                    kind;

		Symbol(ScopeRef scope, base::StrId name, bool anonymous, bool is_static, SymbolKind kind);


		virtual void calculateType() = 0;

		virtual void calculateLinkedLookup(hir::AnalysisState&);

		void         getAll(hir::AnalysisState& state) {
            getKind();
            getType();
            getLinkedLookupScope(state);
		}

	public:
		friend class SymbolData;

		// This delete is important, to prevent any copy of symbol data:
		Symbol(const Symbol&)                               = delete;

		Symbol(Symbol&& other)                              = default;
		Symbol&                   operator=(const Symbol&)  = default;
		Symbol&                   operator=(Symbol&& other) = default;

		ScopeRef                  getScope() const { return scope; }

		base::StrId               getName() const { return name; }

		bool                      getIsStatic() const { return is_static; }

		usize                     getRelativePosition() const { return relative_position; }

		bool                      isAnonymous() const { return anonymous; }

		bool                      isWildcard() const { return wildcard; }

		// @TODO: current design forces this function, to take
		// hir::AnalysisState&, which results in
		// symtable.lookup needing it as well
		// Is should be changed somehow
		// Ideas: 1. move lookup to hir::AnalysisState
		//        2. change SymbolId to SymbolRef
		//        3. add member hir::AnalysisState& to symbol
		ScopeRef                  getLinkedLookupScope(hir::AnalysisState&);

		ts::TypeDesc<>            getType();

		SymbolKind                getKind() const { return kind; }

		virtual SymbolChain       getUniqueDeAlias(hir::AnalysisState&);
		virtual ChainLookupResult getDeAlias(hir::AnalysisState&);

		virtual void              analyzeAll(hir::AnalysisState&) = 0;

		virtual ~Symbol()                                         = default;

		LookupResult lookupIn(hir::AnalysisState&, base::StrId name);
	};

}
