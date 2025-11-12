#include "numeric_value.hpp"

#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <type_traits>

namespace compiler::numeric_value {
	const NumericValue::Storage& NumericValue::getStorage() const { return value; }

	[[nodiscard]] std::string NumericValue::toString() const {
		return std::visit([&](auto&& value) { return base::toString(value); }, value);
	}

	tsh::SymbolType<> NumericValue::getTypeOfStoredValue(query::Context& ctx) const {
		using namespace tsh;
		return std::visit(
			[&](auto&& val) -> tsh::SymbolType<> {
				using T                    = std::decay_t<decltype(val)>;
				constexpr bool IS_INTEGRAL = std::is_integral_v<T>;
				constexpr bool IS_SIGNED   = std::is_signed_v<T>;

				if constexpr (IS_INTEGRAL && IS_SIGNED) {
					return SymbolType{
						ctx.query<QueryIntegralType>({ sizeof(T) * 8,
					                                   IntegralAbstractType::Signedness::Signed }),
						ReferenceKind::Direct,
						Mutability::Immutable,
					};
				} else if constexpr (IS_INTEGRAL && !IS_SIGNED) {
					return SymbolType{
						ctx.query<QueryIntegralType>({ sizeof(T) * 8,
					                                   IntegralAbstractType::Signedness::Unsigned }),
						ReferenceKind::Direct,
						Mutability::Immutable,
					};
				} else {  // Floating point.
					return SymbolType{
						ctx.query<QueryFloatType>({ sizeof(T) * 8 }),
						ReferenceKind::Direct,
						Mutability::Immutable,
					};
				}
			},
			value
		);
	}
}
