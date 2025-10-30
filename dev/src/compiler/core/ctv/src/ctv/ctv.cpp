#include "ctv.hpp"

#include "typesystem/higher/symbol_type.hpp"

#include <typesystem/higher/queries/types.hpp>

#include "base/extend_cpp/variant_match.hpp"
#include "base/str/str_utils.hpp"

#include <query_framework/context.hpp>

#include <concepts>
#include <type_traits>

namespace compiler::ctv {
	CompileTimeValue::CompileTimeValue() = default;

	const CompileTimeValue::Storage& CompileTimeValue::getStorage() const { return value; }

	std::string CompileTimeValue::toString() const { 
		//clang-format off
		VARIANT_VISIT(value,
			VISIT_CASE(bool, val, return val ? "true" : "false")
			VISIT_CASE(UnitCTV, val, return "()")
			VISIT_CASE(NumericValue, val, return val.toString())
			VISIT_CASE(tsh::SymbolType<>, val, return val.toString())
		) 
		//clang-format off
}

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
}
