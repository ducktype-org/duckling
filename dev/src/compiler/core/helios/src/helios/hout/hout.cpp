#include "hout.hpp"

#include "elements.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_code_generation/default_constructors.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/context/context.hpp>

#include <memory>

namespace compiler::helios {
	void HOUTUnit::debugPrint(query::Context& ctx, std::ostream& out) const {
		out << "HOUT UNIT:\n\n";

		out << "Constants:\n";
		for (auto& const_gd: glob_data) const_gd.debugPrint(ctx, out);

		out << "\nFunctions:\n";
		for (auto& func: functions) {
			func->debugPrint(out);
			out << "\n";
		}
	}

	HOUTFunctionDeclaration::HOUTFunctionDeclaration(
		const SymID                  symbol,
		const tsh::SymbolType<>      ret_type,
		std::vector<code::Parameter> parameters,
		code::ElementOrigin          origin
	):
		  original_symbol(symbol),
		  original_name(name(original_symbol)),
		  return_type(ret_type),
		  parameters(std::move(parameters)),
		  origin(origin) {
		CORE_ASSERT(
			kind(symbol) == SymbolKind::Function or kind(symbol) == SymbolKind::FunctionDeclaration
				or kind(symbol) == SymbolKind::Method,
			"Symbol is not a function, function declaration nor method"
		);
	}

	u64 HOUTFunctionDeclaration::queryUnstablePerfectHash() const {
		return original_symbol.queryUnstablePerfectHash();
	}

	void HOUTFunctionDeclaration::debugPrint(std::ostream& out) const {
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
	}

	u64 HOUTFunction::queryUnstablePerfectHash() const {
		// @note for now it doesn't depend on body
		return declaration->queryUnstablePerfectHash();
	}

	void HOUTFunction::debugPrint(std::ostream& out) const {
		declaration->debugPrint(out);
		out << "{\n";
		for (auto& stmt: body->statements) stmt->debugPrint(out, 1);
		out << "}\n";
	}

	HOUTFunction::HOUTFunction(

		code::ElementOrigin                           origin,
		const CRef<HOUTFunctionDeclaration>           other,
		const std::shared_ptr<const code::CodeBlock>& body
	):
		  origin(origin),
		  declaration(other),
		  body(body) {}

	void HOUTGlobalData::debugPrint(query::Context& ctx, std::ostream& out) const {
		variant_match(value) {
			variant_case(HOUTGlobalConst, const_value) {
				out << "const " << prettyDebugPrint(helios_symbol, ctx) << " : " << type.toString()
					<< " = " << const_value.value.toString() << '\n';
			}
			variant_case(HOUTGlobalVariable, val) {
				out << (type.getMutability() == tsh::Mutability::Mutable ? "var   " : "let   ");
				out << prettyDebugPrint(helios_symbol, ctx) << " : " << type.toString() << " = ";
				val.initial_value.get()->ref()->debugPrint(out);
				out << '\n';
			}
		}
	}

	HOUTGlobalData::HOUTGlobalData(
		query::Context& ctx, const SymID symbol, const HOUTGlobalDataType data_type
	):
		  helios_symbol(symbol),
		  origin(code::pstOrigin(stmt(ctx, symbol).value())),
		  original_name(name(symbol)),
		  data_type(data_type),
		  value([&]() -> std::variant<HOUTGlobalConst, HOUTGlobalVariable> {
			  switch (data_type) {
			  case HOUTGlobalDataType::Variable: {
				  auto var_decl = stmt(ctx, symbol)->dynamicCast<pst::Variable>().value();

				  // Get the initial value and type of the variable.
				  const auto variable_type = ctx.query<QueryTypeOfSymbol>(symbol)->valueOrThrow();
				  auto       get_initial_value = [&]() -> Box<code::Expr> {
                      if (auto maybe_initial_pst = var_decl->getValue()) {
                          auto initial_value_pst = maybe_initial_pst.value().unlock(ctx)->getExpr();
                          return getHoutOfExprWithExpectedType(ctx, initial_value_pst, variable_type)
                              .valueOrThrow();
                      } else {
                          return houtgen::getDefaultInitializerExpr(
                                     ctx, variable_type, origin.getSourcePosition().value()
                          )
                              .valueOrThrow();
                      }
				  };
				  auto initial_value = get_initial_value();

				  return HOUTGlobalVariable{
					  std::make_shared<Box<code::Expr>>(std::move(initial_value))
				  };
			  }
			  case HOUTGlobalDataType::Constant:
				  return HOUTGlobalConst{ ctx.query<QueryConstValueOf>(symbol).valueOrThrow() };
			  default:
				  CORE_PANIC("Unhandled HOUTGlobalDataType");
			  }
		  }()),
		  type(ctx.query<QueryTypeOfSymbol>(symbol)->valueOrThrow()) {}
}
