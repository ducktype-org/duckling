/**
 * @file scope_symbol_id.hpp
 * @brief This file contains definitions of SymbolID
 * and ScopeID structures, that are used to represent HELIOS-symbols
 * and HELIOS-scopes across the compiler.
 */
#pragma once

#include <base/perfect_hash.hpp>
#include <base/ref.hpp>

namespace compiler::helios {
	// Forward:
	// @TODO: put in detail namespace
	struct SymbolData;
	struct ScopeData;

	/**
	 * @brief Symbol Identifier. Used to represent HELIOS Symbol across the compiler.
	 */
	struct SymID final {
		// @FUTURE: add some mangling, so valgrind will not get confused
		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const SymID&) const = default;

		auto operator<=>(const SymID& other) const { return ref.get() <=> other.ref.get(); }

	private:
		Ref<SymbolData> ref;

		SymID(Ref<SymbolData> ref): ref(ref) {}
		friend struct ImplementationOf_QuerySymbolOfSTMT;
		friend struct ImplementationOf_QueryLookupInSymbol;
		friend struct GetSymRef_Functor;
		friend struct ImplementationOf_QueryLinkedScope;
		friend struct ImplementationOf_QueryClassSymbolData;
	};

	/**
	 * @brief Scope Identifier. Used to represent HELIOS Scope across the compiler.
	 */
	struct ScopeID final {
		// @FUTURE: add some mangling, so valgrind will not get confused
		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const ScopeID&) const = default;

		bool operator<(const ScopeID& other) const { return ref < other.ref; }

	private:
		Ref<ScopeData> ref;

		ScopeID(Ref<ScopeData> ref): ref(ref) {}
		friend struct ImplementationOf_QueryRootScopeOf;
		friend struct ImplementationOf_QueryPrimaryCodeScopeFor;
		friend struct ImplementationOf_QuerySymbolsInScope;
		friend struct ImplementationOf_QueryLookupInScopeAndParents;
		friend struct GetScopeRef_Functor;
	};

}
