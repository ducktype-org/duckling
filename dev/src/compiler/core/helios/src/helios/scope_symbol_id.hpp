/**
 * @file scope_symbol_id.hpp
 * @brief This file contains definitions of SymbolID
 * and ScopeID structures, that are used to represent HELIOS-symbols
 * and HELIOS-scopes across the compiler.
 */
#pragma once

#include <base/pointers/ref.hpp>

namespace compiler::helios {
	// Forward:
	// @TODO: put in internal namespace
	struct SymbolData;
	struct ScopeData;

	/**
	 * @brief Symbol Identifier. Used to represent HELIOS Symbol across the compiler.
	 */
	struct SymID final {
		// @FUTURE: add some mangling, so valgrind will not get confused
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const SymID&) const = default;

		auto operator<=>(const SymID& other) const { return ref.get() <=> other.ref.get(); }

	private:
		CRef<SymbolData> ref;

		SymID(const CRef<SymbolData> ref): ref(ref) {}
		friend struct GetSymRef_Functor;
		friend struct ImplementationOf_QuerySymbolOfSTMT;
		friend struct ImplementationOf_QueryLookupInSymbol;
		friend struct ImplementationOf_QueryLinkedScope;
		friend struct ImplementationOf_QueryClassSymbolData;
	};

	/**
	 * @brief Scope Identifier. Used to represent HELIOS Scope across the compiler.
	 */
	struct ScopeID final {
		// @FUTURE: add some mangling, so valgrind will not get confused
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const ScopeID&) const = default;

		bool operator<(const ScopeID& other) const { return ref < other.ref; }

		/**
		 * @brief Debug function to print scope and its parents IDs.
		 * Useful for debugging weird scope bugs.
		 * @param os The stream to print to.
		 */
		void debugPrintScopeAndParents(std::ostream& os) const;

	private:
		Ref<ScopeData> ref;

		ScopeID(const Ref<ScopeData> ref): ref(ref) {}
		friend struct ScopeAccess_Functor;
		friend struct ImplementationOf_QueryRootScopeOf;
		friend struct ImplementationOf_QueryPrimaryCodeScopeFor;
		friend struct ImplementationOf_QuerySymbolsInScope;
		friend struct ImplementationOf_QueryLookupInScopeAndParents;
	};

}
