#include "hout.hpp"

#include "../symbols/simple.hpp"
#include "elements.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/scopes/scopes.hpp>  // for parent
#include <helios_private/symbols/symbols.hpp>
#include <pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <query_framework/context.hpp>

#include <memory>
#include <sstream>

namespace compiler::helios {
	std::string HOUTUnit::debugPrint() const {
		std::string out;

		out += "HOUT UNIT:\n\n";

		out += "Constants:\n";
		for (auto& const_: glob_data) out += const_.debugPrint();

		out += "\nFunctions:\n";
		for (auto& func: functions) {
			out += func.debugPrint();
			out += "\n";
		}

		return out;
	}

	std::string HOUTFunction::debugPrint() const {
		std::stringstream out;
		out << "fun ";
		out << original_name.strView() << " : ";
		out << this->type.toString() << "\n";
		out << "Parameters: \n";
		if (content.parameters->empty()) out << "  none\n";
		for (auto& param: *content.parameters) {
			out << "  " << param.name.strView() << " : ";
			out << param.type.toString();
			if (param.initial_value.has_value()) {
				out << " = ";
				param.initial_value.value()->debugPrint(out);
			}
			out << "\n";
		}
		out << "{\n";
		for (auto& stmt: content.body->statements) stmt->debugPrint(out, 1);
		out << "}\n";
		return out.str();
	}

	u64 HOUTFunction::queryUnstablePerfectHash() const {
		// @note: see
		// https://github.com/orgs/ducktype-org/projects/8/views/1?pane=issue&itemId=70870558
		return original_symbol.queryUnstablePerfectHash();
	}

	HOUTFunction::HOUTFunction(SymID symbol, query::Context& ctx):
		  original_symbol(symbol),
		  original_name(name(original_symbol)),
		  type(ctx.query<QueryTypeOfSymbol>(original_symbol)
	               ->expect("Handling errors in HOUT is not supported yet")
	               .getType()),
		  top_lifetime_scope(parent(scope(symbol)).value()) {
		CORE_ASSERT(kind(symbol) == SymbolKind::Function, "Symbol is not a function");
	}

	std::string HOUTGlobalData::debugPrint() const {
		std::stringstream out;
		if (std::holds_alternative<HOUTGlobalConst>(value)) {
			auto const_value = std::get<HOUTGlobalConst>(value).value;
			out << "const " << original_name.strView() << " = " << const_value << "\n";
		} else if (std::holds_alternative<HOUTGlobalVariable>(value)) {
			out << "var " << original_name.strView() << " = ";
			std::get<HOUTGlobalVariable>(value).initial_value.get()->ref()->debugPrint(out);
			out << "\n";
		}
		return out.str();
	}

	HOUTGlobalData::HOUTGlobalData(SymID symbol, query::Context& ctx, HOUTGlobalDataType data_type):
		  helios_symbol(symbol),
		  original_name(name(symbol)),
		  data_type(data_type),
		  value([&]() -> std::variant<HOUTGlobalConst, HOUTGlobalVariable> {
			  if (data_type == HOUTGlobalDataType::Variable) {
				  return HOUTGlobalVariable{ std::make_shared<Box<code::Expr>>(
					  std::move(ctx.query<QueryHoutOfExpr>(stmt(ctx, symbol)
				                                               ->dynamicCast<pst::Variable>()
				                                               .value()
				                                               ->getValue()
				                                               .value()
				                                               .unlock(ctx)
				                                               ->getExpr())
				                    .expect("Handling errors in HOUT is not supported yet"))
				  ) };
			  } else {
				  return HOUTGlobalConst{ ctx.query<QueryConstValueOf>(symbol).expect(
					  "Handling errors in HOUT is not supported yet"
				  ) };
			  }
		  }()),
		  type(ctx.query<QueryTypeOfSymbol>(symbol)->expect(
			  "Handling errors in HOUT is not supported yet"
		  )) {}
}
