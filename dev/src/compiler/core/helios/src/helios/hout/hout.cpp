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

	HOUTFunctionDeclaration::HOUTFunctionDeclaration(SymID symbol, tsh::SymbolType<> ret_type,const std::shared_ptr<std::vector<code::Parameter>>& parameters):
		  original_symbol(symbol),
		  original_name(name(original_symbol)),
		  return_type(ret_type),
		  parameters(parameters) {
		CORE_ASSERT(kind(symbol) == SymbolKind::Function, "Symbol is not a function");
	}

	u64 HOUTFunctionDeclaration::queryUnstablePerfectHash() const {
		return original_symbol.queryUnstablePerfectHash();
	}

	std::string HOUTFunctionDeclaration::debugPrint() const {
		std::stringstream out;
		out << "fun ";
		out << original_name.strView() << " : ";
		out << "Return type: ";
		out << this->return_type.toString() << "\n";
		out << "Parameters: \n";
		if (parameters->empty()) out << "  none\n";
		for (auto& param: *parameters) {
			out << "  " << param.name.strView() << " : ";
			out << param.type.toString();
			if (param.initial_value.has_value()) {
				out << " = ";
				param.initial_value.value()->debugPrint(out);
			}
			out << "\n";
		}
		return out.str();
	}

	u64 HOUTFunction::queryUnstablePerfectHash() const {
		// @note for now it doesn't depend on body
		return declaration.queryUnstablePerfectHash();
	}

	std::string HOUTFunction::debugPrint() const {
		std::stringstream out;
		out << "{\n";
		for (auto& stmt: body->statements) stmt->debugPrint(out, 1);
		out << "}\n";
		return declaration.debugPrint() + out.str();
	}

	HOUTFunction::HOUTFunction(HOUTFunctionDeclaration other, const std::shared_ptr<const code::CodeBlock>& body): declaration(std::move(other)), body(body) {}

	std::string HOUTGlobalData::debugPrint() const {
		std::stringstream out;
		variant_match(value) {
			variant_case(HOUTGlobalConst, const_value) {
				out << "const " << original_name.strView() << " = " << const_value.value.toString()
					<< '\n';
			}
			variant_case(HOUTGlobalVariable, val) {
				val.initial_value.get()->ref()->debugPrint(out);
				out << '\n';
			}
		}
		return out.str();
	}

	HOUTGlobalData::HOUTGlobalData(SymID symbol, query::Context& ctx, HOUTGlobalDataType data_type):
		  helios_symbol(symbol),
		  original_name(name(symbol)),
		  data_type(data_type),
		  value([&]() -> std::variant<HOUTGlobalConst, HOUTGlobalVariable> {
			  switch (data_type) {
			  case HOUTGlobalDataType::Variable:
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
			  case HOUTGlobalDataType::Constant:
				  return HOUTGlobalConst{ ctx.query<QueryConstValueOf>(symbol).expect(
					  "Handling errors in HOUT is not supported yet"
				  ) };
			  default:
				  CORE_PANIC("Unhandled HOUTGlobalDataType");
			  }
		  }()),
		  type(ctx.query<QueryTypeOfSymbol>(symbol)->expect(
			  "Handling errors in HOUT is not supported yet"
		  )) {}
}
