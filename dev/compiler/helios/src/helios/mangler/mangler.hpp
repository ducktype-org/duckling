#include <helios/scope_symbol_id.hpp>
#include <base/string_id.hpp>
#include <query_framework/query_int.hpp>


namespace compiler::helios::mangler {

    struct KeyOf_MangledSymbol {
        SymID symbol;
        u64 mangling_scheme_version = 0;
        base::Optional<std::string> additional_metadata = std::nullopt;

        auto operator<=>(const KeyOf_MangledSymbol& rhs) const {
            if( symbol != rhs.symbol )
                return symbol <=> rhs.symbol;
            if( mangling_scheme_version != rhs.mangling_scheme_version )
                return mangling_scheme_version <=> rhs.mangling_scheme_version;
            if( additional_metadata != rhs.additional_metadata ) {
                if( !additional_metadata.has_value() && !rhs.additional_metadata.has_value() )
                    return std::strong_ordering::equal;
                if( additional_metadata.has_value() && rhs.additional_metadata.has_value() )
                    return additional_metadata.value() <=> rhs.additional_metadata.value();
                return additional_metadata.has_value() ? std::strong_ordering::greater : std::strong_ordering::less;
            }
            return std::strong_ordering::equal;
        }
        
        bool operator==(const KeyOf_MangledSymbol& rhs) const = default;

        [[nodiscard]]
        u64 queryUnstablePerfectHash() const {
            static base::Map<KeyOf_MangledSymbol, u64> hashes{};
            
            if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;
            
            u64 result = hashes.size();
            hashes.put(*this, result);
            return result;
        }
    };

    /**
     * @brief Gets the mangled name of a symbol.
     */
    DECLARE_QUERY(QueryMangledSymbol, KeyOf_MangledSymbol, base::Optional<std::string>);

} // namespace compiler::helios::mangler
