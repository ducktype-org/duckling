#include "numeric_value.hpp"

#include "typesystem/higher/types.hpp"

#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include "base/except/exceptions.hpp"
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

	base::Optional<NumericValue> NumericValue::castTo(const tsh::SymbolType<>& target_type) const {
		using namespace tsh;
		const auto target_abstract_type = target_type.getType();

		auto cast = [&]<typename TargetType>() -> base::Optional<NumericValue> {
			if (auto maybe_casted = this->coerceTo<TargetType>())
				return NumericValue{ maybe_casted.value() };
			return {};
		};


		switch (target_abstract_type.getKind()) {
		case Kind::Integral: {
			IntegralAbstractType int_type(target_abstract_type);
			usize                width = usize(int_type.getSize());

			if (int_type.getSignedness() == IntegralAbstractType::Signedness::Signed) {
				switch (width) {
				case 16:
					return cast.template operator()<i16>();
				case 32:
					return cast.template operator()<i32>();
				case 64:
					return cast.template operator()<i64>();
				default:
					CORE_PANIC("Unsupported signed integer size in compile-time cast");
				}
			} else {
				switch (width) {
				case 16:
					return cast.template operator()<u16>();
				case 32:
					return cast.template operator()<u32>();
				case 64:
					return cast.template operator()<u64>();
				default:
					CORE_PANIC("Unsupported signed integer size in compile-time cast");
				}
			}
		}
		case tsh::Kind::Float: {
			tsh::FloatAbstractType float_type(target_abstract_type);
			usize                  width = usize(float_type.getSize());
			switch (width) {
			case 32:
				return cast.template operator()<f32>();
			case 64:
				return cast.template operator()<f64>();
			default:
				CORE_PANIC("Unsupported float width in compile-time cast");
			}
		}
		default:
			CORE_PANIC("Invalid compile-time cast to a non-numeric type");
		}
	}
}
