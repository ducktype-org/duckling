#include "hout.hpp"
#include "elements.hpp"
#include <query_framework/query_impl.hpp>
#include <sstream>

namespace compiler::helios {
	std::string HOUTUnit::debugPrint() const {
		std::string out;

		out += "HOUT UNIT:\n\n";

		out += "Constants:\n";
		for (auto& const_: glob_data) out += const_.debugPrint();

		out += "\nFunctions:\n";
		for (auto& func: functions) out += func.debugPrint();

		return out;
	}

	std::string HOUTFunction::debugPrint() const {
		std::stringstream out;
		// @TODO

		out << "fun ";
		out << original_name.strView();
		out << " ( @TODO ) -> @TODO {\n";
		for (auto&& stmt: body.body->statements) stmt->debugPrint(out, 1);
		out << "}\n";
		return out.str();
	}

	base::HashT HOUTFunction::customPerfectHash() const {
		// @note: see
		// https://github.com/orgs/ducktype-org/projects/8/views/1?pane=issue&itemId=70870558
		return base::perfectHash(original_symbol);
	}

	HOUTFunction::HOUTFunction(SymID symbol, query::Context& ctx):
		  original_symbol(symbol),
		  original_name(name(original_symbol)),
		  type(ctx.query<QueryTypeOfSymbol>(original_symbol)
	               ->expect("Handling errors in HOUT is not supported yet")),
		  top_lifetime_scope(parent(scope(symbol)).value()) {}

	std::string HOUTGlobalData::debugPrint() const {
		return base::strConcat(
			"const ",
			original_name,
			" (",
			"Symbol ",
			helios_symbol.customPerfectHash(),
			")"
			" := ",
			value,
			"\n"
		);
	}

	HOUTGlobalData::HOUTGlobalData(SymID symbol, query::Context& ctx):
		  helios_symbol(symbol),
		  original_name(name(symbol)),
		  value(ctx.query<QueryConstValueOf>(symbol)->expect(
			  "Handling errors in HOUT is not supported yet"
		  )),
		  type(ctx.query<QueryTypeOfSymbol>(symbol)->expect(
			  "Handling errors in HOUT is not supported yet"
		  )) {}
}
