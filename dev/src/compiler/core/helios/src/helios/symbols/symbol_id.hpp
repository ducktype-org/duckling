/**
 * @file symbol_id.hpp
 * @brief This file contains the definition of the SymbolID structure,
 * which is used to represent HELIOS-symbols across the compiler.
 */
#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/elements_list.hpp>
#include <helios/scope_id.hpp>
#include <helios/symbols/attributes.hpp>
#include <helios/symbols/symbol_kind.hpp>

#include <base/pointers/ref.hpp>

#include <hashing/add_to_hash.hpp>
#include <string_id/string_id.hpp>

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

namespace compiler::helios {
	/**
	 * @return is given symbol a wildcard symbol (e.g. using a.*)
	 */
	bool isWildcard(SymID);

	bool isAlias(SymID);

	/**
	 * @return name of the symbol
	 */
	base::StrID name(SymID);

	/**
	 * @return whether SymID is a global function.
	 * @note This function iterates through parents of the PST elements of the symbol.
	 */
	bool isGlobalFun(SymID);

	/**
	 * @return whether SymID is a global variable.
	 * @note This function iterates through parents of the PST elements of the symbol to obtain this
	 * information. It might be changed in the future, especially when more kinds of global
	 * variables will appear (for example analog to C++ static variables).
	 */
	bool isGlobalVar(query::Context&, SymID);

	/**
	 * Whether a symbol is ignored by lookup.
	 */
	bool isIgnoredByLookup(SymID);

	/**
	 * @return kind of the symbol
	 */
	SymbolKind kind(SymID);

	/**
	 * @return scope that given symbol was defined within.
	 * Throws in symbol doesn't have a scope.
	 */
	ScopeID scope(SymID);

	/**
	 * @brief Symbol emission policy, whether it should
	 * be inserted into every module, or we should keep the definition to
	 * the module where the definition is located.
	 * This also translates to LLVM weak/strong linkage.
	 */
	enum class EmissionPolicy {
		OwnerOnly,
		Replicated,
	};
	EmissionPolicy emissionPolicy(SymID id);

	/**
	 * @brief Whether the symbol can be called
	 * with QueryCodeOfFun.
	 * For example `fundecl abc(...)` returns false,
	 * but a builtin can return true.
	 * Non functional symbols return false.
	 */
	bool implementsQueryCodeOfFun(SymID id);

	/**
	 * Gets scope that given symbol was defined within.
	 * Returns empty optional if the symbol doesn't have a scope.
	 * E.g. builtin functions don't have a scope.
	 */
	base::Optional<ScopeID> maybeScope(SymID);

	/**
	 * Check if a symbol has attribute of the given type.
	 */
	template<typename Attribute>
	bool hasAttribute(SymID);

	/**
	 * @return PST Stmt element symbol was created from.
	 * Panics if the element was not a statement.
	 * implementation
	 */
	base::Optional<pst::Access<pst::Stmt>> stmt(query::Context&, SymID);

	/**
	 * @return PST element symbol was created from,
	 * or empty optional if the symbol was not created from a PST element.
	 */
	base::Optional<pst::AccessLocked<pst::LangElement>> maybeSymbolPst(SymID id);

	/**
	 * @brief Pretty prints the symbol.
	 */
	std::string prettyDebugPrint(SymID, query::Context&);
}
