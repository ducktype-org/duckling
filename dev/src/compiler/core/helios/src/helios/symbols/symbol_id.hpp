/**
 * @file symbol_id.hpp
 * @brief This file contains the definition of the SymbolID structure,
 * which is used to represent HELIOS-symbols across the compiler.
 */
#pragma once

#include <base/pointers/ref.hpp>

#include <hashing/add_to_hash.hpp>

namespace compiler::helios {
	// Forwards:
	struct SymbolData;

	namespace defgen {
		struct ImplementationOf_QueryGeneratedSymbol;
	}

	/**
	 * @brief Symbol Identifier. Used to represent HELIOS Symbol across the compiler.
	 */
	struct SymID final {
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;

		bool operator==(const SymID&) const = default;

		auto operator<=>(const SymID& other) const { return ref.get() <=> other.ref.get(); }

		friend constexpr void addToHash(hashing::hash_algorithm auto& hash_alg, const SymID& id) {
			hashing::addToHash(hash_alg, id.queryUnstablePerfectHash());
		}

	private:
		CRef<SymbolData> ref;

		SymID(const CRef<SymbolData> ref): ref(ref) {}
		friend struct GetSymRef_Functor;
		friend struct ImplementationOf_QuerySymbolOfSTMT;
		friend struct defgen::ImplementationOf_QueryGeneratedSymbol;
		friend struct ImplementationOf_QueryLookupInSymbol;
		friend struct ImplementationOf_QueryLinkedScope;
		friend struct ImplementationOf_QueryClassSymbolData;
	};
}

namespace std {
	/**
	 * @brief Hash template specialization so SymID can be used in std::unordered_set and
	 * base::HashMap.
	 */
	template<>
	struct hash<compiler::helios::SymID> {
		std::size_t operator()(const compiler::helios::SymID& s) const noexcept {
			return s.queryUnstablePerfectHash();
		}
	};
}
