#include "ctv.hpp"

#include <typesystem/higher/queries/types.hpp>

#include <query_framework/context.hpp>

#include <string>

namespace compiler::ctv {
	CompileTimeValue::CompileTimeValue() = default;

	const CompileTimeValue::Storage& CompileTimeValue::getStorage() const { return value; }

	std::string CompileTimeValue::toString() const {
		variant_match(value) {
			variant_case_novalue(UnitCTV) { return "()"; }
			variant_case(NumericValue, val) { return val.toString(); }
			variant_case(bool, val) { return val ? "true" : "false"; }
			variant_case(tsh::SymbolType<>, val) { return val.toString(); }
			variant_default {
				throw base::NotYetImplemented("Converting other CTV types to string");
			}
		}
		return "";
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
