#pragma once

#include <vector>
#include <typesystem/typesystem.hpp>
#include <exec/ctv.hpp>
#include <base/string_id.hpp>

#include <base/optional.hpp>

#include "scope_symbol_id.hpp"
#include "symbol_ref.hpp"
#include "lookup_result.hpp"

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
		Alias,

		TestSymbol,
		// ...
	};

	/**
	 * @brief Symbol is a base class for implementation of all symbols.
	 * It defines common interface and data of all HIR-symbols.
	 * 
	 * There are 3 types of operations on symbols:
	 *  
	 *  * calculateXYZ -- internal method used to alter symbols state.
	 *    Usually used in request implementations.
	 *  
	 *  * requestXYZ -- external request to do/get something.
	 *    May cause cascade of other operations.
	 *    Will usually cause heavy computations only during first call.
	 *    Will usually be implemented as: if (not available) calculate(); return access();
	 *  
	 *  * accessXYZ -- external request to do/get something, that will not spawn heavy computation.
	 *    In particular accessXYZ may assert that value that you want to access is already available.
	 *  
	 *  * getXYZ, isXYZ -- simple getters, setters
	 * 
	 * In order to implement a new type of symbol,
	 * one must create a new class inheriting from this one.
	 */
	class Symbol {
		// Update constructors when adding fields here

	protected:
		// state:
		// @TODO: use it instead of passing state everywhere
		hir::AnalysisState& analysis_state;

		// symbol identification:
		ScopeRef    scope;
		base::StrId name;
		bool        anonymous;

		// wildcard are anonymous symbols, that behave in special way in lookup
		bool wildcard = false;

		// aliases are "pointers" to other symbols
		bool is_alias = false;

		// Common symbol data:
		bool is_static;

		// Scope the symbol represent in default lookup context:
		// @TODO: should be `not yet done`/`done value`/`error error`
		base::Optional<ScopeRef> linked_lookup_scope;

		// Symbol position relative to other symbols:
		usize relative_position;

		// symbol is dependent if it can't be used independently
		// example: class fields
		bool dependent = false;


		base::Optional<ts::TypeDesc<>> type;
		SymbolKind                     kind;

		// lookup lock:
		bool lock_lookup = false;

		Symbol(
			hir::AnalysisState& state,
			ScopeRef            scope,
			base::StrId         name,
			bool                anonymous,
			bool                is_static,
			SymbolKind          kind
		);


		virtual void calculateType() = 0;

		bool         symbol_in_done = false;
		virtual void getSymbolsIn();
		virtual void calculateLinkedLookup();

		void getAll() {
			getKind();
			requestType();
			requestLinkedLookupScope();
			getSymbolsIn();
		}

	public:
	// @TODO: here we need a vary clear separation of what can be used where
	
		friend class SymbolData;

		// This delete is important, to prevent any copy of symbol data:
		Symbol(const Symbol&) = delete;

		Symbol(Symbol&& other)            = default;
		Symbol& operator=(const Symbol&)  = default;
		Symbol& operator=(Symbol&& other) = default;

		// Get-s/Is-s are simple getters:

		ScopeRef getScope() const { return scope; }
		base::StrId getName() const { return name; }
		bool getIsStatic() const { return is_static; }
		usize getRelativePosition() const { return relative_position; }
		bool isAnonymous() const { return anonymous; }
		bool isWildcard() const { return wildcard; }
		SymbolKind getKind() const { return kind; }

		// request are operations that can spawn entire compilation processes, and require cycle control

		ScopeRef requestLinkedLookupScope();
		ts::TypeDesc<> requestType();


		// @TODO: decide if value should be kept in Symbol itself
		virtual exec::CTV requestValue();

		virtual SymbolChain       requestUniqueDeAlias();
		virtual ChainLookupResult requestDeAlias();

		LookupResult requestLookupIn(base::StrId name);
		
		virtual void analyzeAll() = 0;
		virtual ~Symbol() = default;

		bool unlockedLookup() const;
	};

}
