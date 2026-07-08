#pragma once

#include "generated_symbol_data.hpp"
#include "pst_symbol_data.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/symbols/attributes.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/type_interface.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/context/context_fd.hpp>
#include <string_id/string_id.hpp>
#include <typeinfo>

namespace compiler::helios {
	/**
	 * @brief The unique ID of a symbol that is always increasing.
	 *
	 * The SymbolID should use this ID to implement its queryUnstablePerfectHash  so that it always
	 * generates a unique ID for each symbol. The previous approach of using the pointer to the
	 * SymbolData in cache was not working, because when we deallocate a SymbolData (LS can do that)
	 * some new SymbolData can be allocated at the same address.
	 */
	STRONG_TYPEDEF_ID(SymbolDataID);

	/**
	 * Symbol data shared by all symbols.
	 */
	struct CommonSymbolData final {
		/**
		 * Symbol name.
		 *
		 * @note In the future there might also be anonymous symbols (symbols with no name), like:
		 * `let _ = 5;`, lambdas, `using a.*`, etc.
		 * For now, we work around it, as all things that could be anonymous are also a wildcard.
		 * Those symbols will also need to have mangled name.
		 */
		base::StrID name;

		/**
		 * Symbol kind, determines what kind of symbol it is.
		 */
		SymbolKind kind;

		/**
		 * Whether the symbol is a wildcard symbol.
		 * When lookup encounter a wildcard symbol it
		 * looks-up into that symbol instead of considering the symbol itself.
		 * e.g.: `using a.*`
		 */
		bool is_wildcard = false;

		/**
		 * Whether the symbol is an alias.
		 * Aliases are symbols that are not "real" symbols, but are just a reference to another
		 * symbol. e.g.: `using a = b;`
		 */
		bool is_alias = false;

		/** Whether the symbol is a dependent symbol.
		 * Dependent symbols are symbols that can't be used in actual execution without some
		 * context, e.g. class fields.
		 */
		bool dependent = false;

		/**
		 * When this is true, the symbol is ignored by the lookup.
		 */
		bool is_ignored_by_lookup = false;

		/**
		 * List of attributes on a symbol.
		 */
		std::vector<Attribute> attributes = {};
	};

	/**
	 * @brief Stores generic symbol data.
	 * @note Symbols and their associated SymbolData are created by HELIOS via queries.
	 * SymbolData is by design a "read-only" structure.
	 */
	struct SymbolData final {
		using SymbolSemantics
			= std::variant<PstImplementedSemantics, BuiltinSemantics, GENERATED_SYMBOL_SEMANTICS_LIST>;

		SymbolData(CommonSymbolData common, SymbolSemantics other);

		CommonSymbolData common;
		SymbolSemantics  other;
		SymbolDataID     id;

		static SymbolData makeBuiltinSymbolData(
			CommonSymbolData common_data, BuiltinSemantics builtin_data
		);

		static SymbolData makePSTSymbolData(
			CommonSymbolData common_data, PstImplementedSemantics pst_data
		);

		static SymbolData makeGeneratedSymbolData(
			base::StrID name, defgen::GeneratedSymbolDataVariant generated_data
		);

		[[nodiscard]]
		base::Optional<ScopeID> getScope() const;

		template<class T>
		[[nodiscard]]
		CRef<T> getData() const {
			CORE_ASSERT(
				std::holds_alternative<T>(other),
				"SymbolData does not hold the requested type. ",
				"Symbol: ",
				common.name.strView(),
				" Requested type: ",
				typeid(T).name()
			);
			return &std::get<T>(other);
		}

		template<class T>
		[[nodiscard]]
		base::Optional<CRef<T>> getDataOpt() const {
			if (auto ptr = std::get_if<T>(&other)) return CRef<T>{ ptr };
			return std::nullopt;
		}

		[[nodiscard]]
		base::Optional<pst::AccessLocked<pst::LangElement>> maybePstElement() const;

		/**
		 * Return associated pst_element cast to Stmt.
		 * Panics if element is not a statement or if symbol is not associated with PST element.
		 */
		[[nodiscard]]
		base::Optional<pst::Access<pst::Stmt>> stmtCast(query::Context& ctx) const;
	};

	/**
	 * @brief Helper struct used to access private SymID data.
	 * It is used by HELIOS only. It is a struct so SymID can friend it.
	 */
	struct GetSymRef_Functor final {
		static auto get(const SymID id) { return id.ref; }

		static SymID make(const CRef<SymbolData> ref) { return SymID{ ref }; }
	};

	inline auto getSymRef(const SymID id) { return GetSymRef_Functor::get(id); }
}
