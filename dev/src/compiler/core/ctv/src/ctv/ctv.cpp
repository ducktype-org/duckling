#include "ctv.hpp"

#include "typesystem/higher/symbol_type.hpp"

#include <typesystem/higher/queries/types.hpp>

#include "base/extend_cpp/variant_match.hpp"

#include <query_framework/context.hpp>

#include <type_traits>

namespace compiler::ctv {

#define DEFINE_DEFAULT_CTV_GETTER(NAME, TYPE)                  \
	base::Optional<TYPE> CompileTimeValue::get##NAME() const { \
		variant_match(value) {                                 \
			variant_case(TYPE, val) { return val; }            \
		}                                                      \
		return {};                                             \
	}

	CompileTimeValue::CompileTimeValue() = default;

	const CompileTimeValue::Storage& CompileTimeValue::getStorage() const { return value; }

	// std::string CompileTimeValue::toString2() const {
	// 	VARIANT_VISIT(
	// 		value,
	// 		VISIT_CASE(bool, val, { return val ? "true" : "false"; })
	// 			VISIT_CASE(tsh::SymbolType<>, val, { return val.toString(); })
	// 				VISIT_CASE(auto, val, code)
    //                 // TODOP: Can we do betteR?


	// 	);
	// }

	std::string CompileTimeValue::toString() const {
		return std::visit(
			[](auto&& val) -> std::string {
				using T = std::decay_t<decltype(val)>;
				if constexpr (std::is_same_v<T, bool>)
					return val ? "true" : "false";
				else if constexpr (std::is_same_v<T, UnitCTV>)
					return "()";
				else if constexpr (std::is_same_v<T, tsh::SymbolType<>>)
					return val.toString();
				else if constexpr (std::is_floating_point_v<T>)
					// Casting to float since no overload for f16 exists in std::to_string()
					return base::toString(static_cast<float>(val));
				else if constexpr (std::is_integral_v<T>)
					return base::toString(val);
				else
					throw base::NotYetImplemented("Converting other CTV types to string");
			},
			value
		);
	}

	DEFINE_DEFAULT_CTV_GETTER(Unit, CompileTimeValue::UnitCTV);
	DEFINE_DEFAULT_CTV_GETTER(I16, i16);
	DEFINE_DEFAULT_CTV_GETTER(I32, i32);
	DEFINE_DEFAULT_CTV_GETTER(I64, i64);
	DEFINE_DEFAULT_CTV_GETTER(I128, i128);
	DEFINE_DEFAULT_CTV_GETTER(F16, f16);
	DEFINE_DEFAULT_CTV_GETTER(F32, f32);
	DEFINE_DEFAULT_CTV_GETTER(F64, f64);
	DEFINE_DEFAULT_CTV_GETTER(Bool, bool);

	base::Optional<tsh::SymbolType<>> CompileTimeValue::getType(query::Context& ctx) const {
		variant_match(value) {
			variant_case(tsh::SymbolType<>, val) { return val; }
			variant_case_novalue(UnitCTV) {
				// Lift unit value to symbol type.
				// Assume direct, mutable. Other options require modifiers which
				// would force conversion to symbol type and invoke the previous branch.
				return tsh::SymbolType<>{
					ctx.query<tsh::QueryUnitType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
		}
		return {};
	}

	base::Optional<f128> CompileTimeValue::asF128() const {
		return std::visit(
			[](auto&& val) -> base::Optional<f128> {
				using T = std::decay_t<decltype(val)>;
				if constexpr (std::is_floating_point_v<T>) return static_cast<f128>(val);
				return {};
			},
			value
		);
	}

	base::Optional<i64> CompileTimeValue::asI64() const {
		return std::visit(
			[](auto&& val) -> base::Optional<i64> {
				using T = std::decay_t<decltype(val)>;
				if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
					return static_cast<i64>(val);
				return {};
			},
			value
		);
	}

	base::Optional<i128> CompileTimeValue::asI128() const {
		return std::visit(
			[](auto&& val) -> base::Optional<i128> {
				using T = std::decay_t<decltype(val)>;
				if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
					return static_cast<i128>(val);
				return {};
			},
			value
		);
	}
}
