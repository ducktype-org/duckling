#include <helios/scope_symbol_id.hpp>
#include <query_framework/query_int.hpp>

#include <base/string_id.hpp>

namespace compiler::helios::mangler {

	struct KeyOf_MangledSymbol final {
		SymID                       symbol;
		u64                         mangling_scheme_version = 0;
		base::Optional<std::string> additional_metadata     = std::nullopt;

		constexpr auto operator<=>(const KeyOf_MangledSymbol& other) const;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Gets the mangled name of a symbol from SymID.
	 */
	DECLARE_QUERY(QueryMangledSymbol, KeyOf_MangledSymbol, base::StrID);

	base::StrID getSimpleMangledName(query::Context& ctx, SymID sym_id);

}
