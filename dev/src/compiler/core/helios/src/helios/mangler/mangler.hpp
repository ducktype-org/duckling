#include <helios/scope_symbol_id.hpp>
#include <query_framework/query_int.hpp>

#include <base/string_id.hpp>

namespace compiler::helios::mangler {

	struct KeyOf_MangledSymbol {
		SymID                       symbol;
		u64                         mangling_scheme_version = 0;
		base::Optional<std::string> additional_metadata     = std::nullopt;

		constexpr auto operator<=>(const KeyOf_MangledSymbol& other) const {
			return std::tie(symbol, mangling_scheme_version, additional_metadata)
			   <=> std::tie(other.symbol, other.mangling_scheme_version, other.additional_metadata);
		}

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Gets the mangled name of a symbol from SymID.
	 */
	DECLARE_QUERY(QueryMangledSymbol, KeyOf_MangledSymbol, base::Optional<std::string>);

}  // namespace compiler::helios::mangler
