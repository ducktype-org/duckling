#include "ctv.hpp"

#include <ctv/numeric_value.hpp>
#include <helios/tsh/queries/types.hpp>

#include <query_framework/context/context.hpp>
#include <string_id/string_id.hpp>

#include <sstream>
#include <string>

namespace compiler::ctv {
	const CompileTimeValue::Storage& CompileTimeValue::getStorage() const { return value; }

	std::string CompileTimeValue::toString() const {
		variant_match(value) {
			variant_case(bool, val) { return val ? "true" : "false"; }
			variant_case(NumericValue, val) { return val.toString(); }
			variant_case(char, c) { return "'" + base::escapeString(std::string{ c }) + "'"; }
			variant_case(base::StrID, val) { return "\"" + base::escapeString(val.str()) + "\""; }
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
					tsh::getBoolType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
			variant_case(NumericValue, numeric) { return numeric.getTypeOfStoredValue(ctx); }
			variant_case_novalue(char) {
				return tsh::SymbolType<>{
					tsh::getCharType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
			variant_case_novalue(base::StrID) {
				return tsh::SymbolType<>{
					tsh::getCharSliceType(ctx),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				};
			}
			variant_case_novalue(UnitCTV) {
				return tsh::SymbolType<>{
					tsh::getUnitType(),
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
					tsh::getMetaType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
		}
		CORE_UNREACHABLE();
	}
}
