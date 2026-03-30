#pragma once

#include <helios/symbols/symbol_id.hpp>
#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <string_id/string_id.hpp>

namespace compiler::helios::mangler {

	namespace special_symbol_keys {
		struct LIRModuleID {
			base::StrID id;

			constexpr auto operator<=>(const LIRModuleID& other) const = default;
		};
	}

	/**
	 * @brief Special symbols may require different keys
	 */
	enum class ManglingSymbolKind {
		Standard,                   // helios::SymID
		ModuleConstructor,          // special_symbol_keys::LIRModuleID
		ModuleDestructor,           // special_symbol_keys::LIRModuleID
		GlobalVariableConstructor,  // helios::SymID
		GlobalVariableDestructor,   // helios::SymID
	};

	using ManglingSymbolKey = std::variant<SymID, special_symbol_keys::LIRModuleID>;

	struct KeyOf_MangledSymbol final {
		ManglingSymbolKey           symbol_key;
		ManglingSymbolKind          kind                    = ManglingSymbolKind::Standard;
		u64                         mangling_scheme_version = 0;
		base::Optional<std::string> additional_metadata     = std::nullopt;

		/**
		 * @TODO: #2027 likely remove, it is used only by the hash map.
		 */
		constexpr auto operator==(const KeyOf_MangledSymbol& other) const;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Gets the mangled name of a symbol from SymID.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryMangledSymbol, KeyOf_MangledSymbol, base::StrID, ({ .uses_qresult = false }));

	/**
	 * @brief Gets the mangled name of a type. The type can either be AbstractType or SymbolType.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryMangledType, tsh::SymbolType<>, CRef<query::QResult<base::StrID>>, ({}));

	base::StrID getSimpleMangledName(query::Context& ctx, SymID sym_id);

	template<ManglingSymbolKind Kind, class SpecialSymbolKey>
	base::StrID getSpecialMangledName(query::Context& ctx, SpecialSymbolKey key);

	template<>
	base::StrID getSpecialMangledName<ManglingSymbolKind::ModuleConstructor>(
		query::Context& ctx, special_symbol_keys::LIRModuleID mod_id
	);

	template<>
	base::StrID getSpecialMangledName<ManglingSymbolKind::ModuleDestructor>(
		query::Context& ctx, special_symbol_keys::LIRModuleID mod_id
	);

	template<>
	base::StrID getSpecialMangledName<ManglingSymbolKind::GlobalVariableConstructor>(
		query::Context& ctx, SymID sym_id
	);

	template<>
	base::StrID getSpecialMangledName<ManglingSymbolKind::GlobalVariableDestructor>(
		query::Context& ctx, SymID sym_id
	);
}

/**
 * Hashed used for the perfect hash of KeyOf_MangledSymbol.
 * @TODO: #2027 likely remove.
 */
template<>
struct std::hash<compiler::helios::mangler::KeyOf_MangledSymbol> final {
	std::size_t operator()(const compiler::helios::mangler::KeyOf_MangledSymbol& key) const;
};
