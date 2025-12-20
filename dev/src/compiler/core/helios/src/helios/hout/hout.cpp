#include "hout.hpp"

#include "elements.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/context.hpp>

#include <memory>
#include <sstream>

namespace compiler::helios {
	std::string HOUTUnit::debugPrint(query::Context& ctx) const {
		std::string out;

		out += "HOUT UNIT:\n\n";

		out += "Constants:\n";
		for (auto& const_gd: glob_data) out += const_gd.debugPrint(ctx);

		out += "\nFunctions:\n";
		for (auto& func: functions) {
			out += func.debugPrint();
			out += "\n";
		}

		return out;
	}

	HOUTFunctionDeclaration::HOUTFunctionDeclaration(
		const SymID symbol, const tsh::SymbolType<> ret_type, std::vector<code::Parameter> parameters
	):
		  original_symbol(symbol),
		  original_name(name(original_symbol)),
		  return_type(ret_type),
		  parameters(std::move(parameters)) {
		CORE_ASSERT(
			kind(symbol) == SymbolKind::Function or kind(symbol) == SymbolKind::FunctionDeclaration,
			"Symbol is not a function or function declaration"
		);
	}

	u64 HOUTFunctionDeclaration::queryUnstablePerfectHash() const {
		return original_symbol.queryUnstablePerfectHash();
	}

	std::string HOUTFunctionDeclaration::debugPrint() const {
		std::stringstream out;
		out << "fun ";
		out << original_name.strView() << " (" << original_symbol.queryUnstablePerfectHash() << ")";
		out << " : " << "Return type: ";
		out << this->return_type.toString() << "\n";
		out << "Parameters: \n";
		if (parameters.empty()) out << "  none\n";
		for (auto& param: parameters) {
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
		return declaration->queryUnstablePerfectHash();
	}

	std::string HOUTFunction::debugPrint() const {
		std::stringstream out;
		out << "{\n";
		for (auto& stmt: body->statements) stmt->debugPrint(out, 1);
		out << "}\n";
		return declaration->debugPrint() + out.str();
	}

	HOUTFunction::HOUTFunction(
		const CRef<HOUTFunctionDeclaration> other, const std::shared_ptr<const code::CodeBlock>& body
	):
		  declaration(other),
		  body(body) {}

	std::string HOUTGlobalData::debugPrint(query::Context& ctx) const {
		std::stringstream out;
		variant_match(value) {
			variant_case(HOUTGlobalConst, const_value) {
				out << "const " << prettyDebugPrint(helios_symbol, ctx) << " : " << type.toString()
					<< " = " << const_value.value.toString() << '\n';
			}
			variant_case(HOUTGlobalVariable, val) {
				std::string decl
					= type.getMutability() == tsh::Mutability::Mutable ? "var   " : "let   ";
				out << decl << prettyDebugPrint(helios_symbol, ctx) << " : " << type.toString()
					<< " = ";
				val.initial_value.get()->ref()->debugPrint(out);
				out << '\n';
			}
		}
		return out.str();
	}

	HOUTGlobalData::HOUTGlobalData(
		const SymID symbol, query::Context& ctx, const HOUTGlobalDataType data_type
	):
		  helios_symbol(symbol),
		  original_name(name(symbol)),
		  data_type(data_type),
		  value([&]() -> std::variant<HOUTGlobalConst, HOUTGlobalVariable> {
			  switch (data_type) {
			  case HOUTGlobalDataType::Variable: {
				  // Get the initial value and type of the variable.
				  const auto initial_value_pst = stmt(ctx, symbol)
			                                         ->dynamicCast<pst::Variable>()
			                                         .value()
			                                         ->getValue()
			                                         .value()
			                                         .unlock(ctx)
			                                         ->getExpr();
				  const auto variable_type = ctx.query<QueryTypeOfSymbol>(symbol)->throwOnFail(
					  "Handling errors in HOUT is not supported yet 2a — " + name(symbol).str()
				  );
				  auto initial_value_hout
					  = ctx.query<QueryHoutOfExpr>(initial_value_pst)
			                .throwOnFail(
								"Handling errors in HOUT is not supported yet 2b — "
								+ name(symbol).str()
							);

				  // Apply necessary coercions.
				  const auto coercion = canCoerce(
					  ctx, initial_value_hout->expression_type.getSymbolType(), variable_type
				  );
				  const auto valid_coercion = coercion.throwOnFail(
					  "Handling errors in HOUT is not supported yet 2c — " + name(symbol).str()
				  );

				  // Return the coerced value.
				  return HOUTGlobalVariable{ std::make_shared<Box<code::Expr>>(
					  valid_coercion.coerce(ctx, std::move(initial_value_hout))
				  ) };
			  }
			  case HOUTGlobalDataType::Constant:
				  return HOUTGlobalConst{ ctx.query<QueryConstValueOf>(symbol).throwOnFail(
					  "Handling errors in HOUT is not supported yet 3 — " + name(symbol).str()
				  ) };
			  default:
				  CORE_PANIC("Unhandled HOUTGlobalDataType");
			  }
		  }()),
		  type(ctx.query<QueryTypeOfSymbol>(symbol)->throwOnFail(
			  "Handling errors in HOUT is not supported yet 4 — " + name(symbol).str()
		  )) {}
}
