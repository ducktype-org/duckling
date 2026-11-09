// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file scope_id.hpp
 * @brief This file contains the definition of the ScopeID structure,
 * which is used to represent HELIOS-scopes across the compiler.
 */
#pragma once

#include <base/pointers/ref.hpp>

namespace compiler::helios {
	// Forwards:
	struct ScopeData;

	/**
	 * @brief Scope Identifier. Used to represent HELIOS Scope across the compiler.
	 */
	struct ScopeID final {
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;

		/**
		 * @note == is needed despite the existence of <=> because
		 * only defaulted <=> generates all 6 comparison operators.
		 */
		bool operator==(const ScopeID&) const;

		/**
		 * @note < is needed despite the existence of <=> because
		 * only defaulted <=> generates all 6 comparison operators.
		 */
		bool operator<(const ScopeID& other) const;

		std::strong_ordering operator<=>(const ScopeID& other) const;

		/**
		 * @brief Debug function to print scope and its parents IDs.
		 * Useful for debugging weird scope bugs.
		 * @param os The stream to print to.
		 */
		void debugPrintScopeAndParents(std::ostream& os) const;

	private:
		/**
		 * @brief Reference to scope data.
		 * @note There might be multiple ScopeData objects in memory
		 * for the same logical scope. ScopeData should be distinguished
		 * using their unstable_id perfect hash, not their memory address.
		 * See ScopeData::perfectClone() and the implementation of QueryPrimaryCodeScopeFor for more
		 * details.
		 */
		CRef<ScopeData> ref;

		ScopeID(const CRef<ScopeData> ref): ref(ref) {}
		friend struct ScopeAccess_Functor;
		friend struct ImplementationOf_QueryRootScopeOf;
		friend struct ImplementationOf_QueryPrimaryCodeScopeFor;
		friend struct ImplementationOf_QuerySymbolsInScope;
		friend struct ImplementationOf_QueryLookupInScopeAndParents;
	};
}
