#include "ctv.hpp"

#include <ctv/numeric_value.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <query_framework/context.hpp>

#include <sstream>
#include <string>

namespace compiler::ctv {
	CompileTimeValue::CompileTimeValue() = default;

	const CompileTimeValue::Storage& CompileTimeValue::getStorage() const { return value; }

	std::string CompileTimeValue::toString() const {
		variant_match(value) {
			variant_case(bool, val) { return val ? "true" : "false"; }
			variant_case(NumericValue, val) { return val.toString(); }
			variant_case_novalue(UnitCTV) { return "()"; }
			variant_case(TupleCTV, tuple) {
				std::stringstream ss;
				ss << "(" << tuple.getElements().at(0).toString();
				for (usize i = 1; i < tuple.getElements().size(); i++)
					ss << ", " << tuple.getElements().at(i).toString();
				ss << ")";
				return ss.str();
			}
			variant_case(tsh::SymbolType<>, val) { return val.toString(); }
			variant_default {
				throw base::NotYetImplemented("Converting other CTV types to string");
			}
		}
		return "";
	}

	tsh::SymbolType<> CompileTimeValue::getTypeOfStoredValue(query::Context& ctx) const {
		variant_match(value) {
			variant_case_novalue(bool) {
				return tsh::SymbolType<>{
					ctx.query<tsh::QueryBoolType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
			variant_case(NumericValue, numeric) { return numeric.getTypeOfStoredValue(ctx); }
			variant_case_novalue(UnitCTV) {
				return tsh::SymbolType<>{
					ctx.query<tsh::QueryUnitType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
			variant_case(TupleCTV, tuple) {
				std::vector<tsh::SymbolType<>> component_types;
				component_types.reserve(tuple.getElements().size());
				for (const auto& element: tuple.getElements())
					component_types.push_back(element.getTypeOfStoredValue(ctx));

				return tsh::SymbolType<>{
					ctx.query<tsh::QueryTupleType>({ std::move(component_types) }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}

			variant_case(tsh::SymbolType<>, val) {
				return tsh::SymbolType<>{
					ctx.query<tsh::QueryMetaType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
		}
		CORE_UNREACHABLE();
	}

	// @TODO: #1618 Remove
	bool CompileTimeValue::canBeType() const {
		variant_match(value) {
			variant_case_novalue(tsh::SymbolType<>) { return true; }
			variant_case_novalue(UnitCTV) { return true; }
			variant_case(TupleCTV, tuple) { return tuple.canBeType(); }
			variant_default { return false; }
		}
		CORE_UNREACHABLE();
	}

	// @TODO: #1618 Remove
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
			variant_case(TupleCTV, tuple) {
				if (!tuple.canBeType()) return {};
				// Lift tuple value to symbol type.
				// Assume direct, mutable. Other options require modifiers which
				// would force conversion to symbol type and invoke the previous branch.
				std::vector<tsh::SymbolType<>> subtypes;
				for (const auto& element: tuple.getElements())
					subtypes.push_back(element.getType(ctx).value());
				return tsh::SymbolType<>{
					ctx.query<tsh::QueryTupleType>({ std::move(subtypes) }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
		}
		return {};
	}
}
