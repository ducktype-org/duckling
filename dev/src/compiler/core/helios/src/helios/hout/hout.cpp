#include "hout.hpp"

#include "elements.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/symbols/symbol_id_utils.hpp>

#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>

#include <memory>

namespace compiler::helios {
	void HOUTUnit::debugPrint(query::Context& ctx, std::ostream& out) const {
		out << "HOUT UNIT:\n\n";

		out << "Constants:\n";
		for (auto& const_gd: glob_data) const_gd->debugPrint(ctx, out);

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
				val.initial_value.ref()->debugPrint(out);
				out << '\n';
			}
		}
	}

	bool compareFunctionDeclarations(
		query::Context&                  ctx,
		const HOUTFunctionDeclaration&   a,
		const HOUTFunctionDeclaration&   b,
		FunctionDeclarationCompareParams params
	) {
		// We can make it a template function based on the provided parameters.
		const bool do_log          = params.log_error_on_mismatch;
		auto       detailed_reason = params.reason.copyValueOr("");

		if (params.compare_initial_values) CORE_PANIC("Comparing initial values is not supported");

		if (a.return_type != b.return_type) {
			if (do_log) {
				auto err = makeBox<dia_int::PlaceholderError>(
					base::strConcat(
						"Return type `",
						a.return_type.toString(),
						"` does not match `",
						b.return_type.toString(),
						"`."
					),
					a.origin.getStablePosition(),
					detailed_reason
				);
				err->addAttachedMessage(makeBox<dia_int::PlaceholderNote>(
					"Other function declared here",
					b.origin.getStablePosition(),
					base::strConcat("Return type is `", b.return_type.toString(), "`.")
				));
				ctx.logInt(std::move(err));
			}
			return false;
		}

		if (a.parameters.size() != b.parameters.size()) {
			if (do_log) {
				auto err = makeBox<dia_int::PlaceholderError>(
					base::strConcat(
						"Function parameter count mismatch. ",
						"Function has ",
						a.parameters.size(),
						" parameter(s).",
						detailed_reason
					),
					a.origin.getStablePosition()
				);
				err->addAttachedMessage(makeBox<dia_int::PlaceholderNote>(
					base::strConcat("Other function has ", b.parameters.size(), " parameter(s)."),
					b.origin.getStablePosition()
				));
				ctx.logInt(std::move(err));
			}
			return false;
		}

		for (usize i = 0; i < a.parameters.size(); ++i) {
			const auto& pa = a.parameters[i];
			const auto& pb = b.parameters[i];

			if (pa.type != pb.type) {
				if (do_log) {
					auto err = makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"Parameter type `",
							pa.type.toString(),
							"` does not match `",
							pb.type.toString(),
							"`."
						),
						pa.origin.getStablePosition(),
						detailed_reason
					);
					err->addAttachedMessage(makeBox<dia_int::PlaceholderNote>(
						"Corresponding parameter declared here",
						pb.origin.getStablePosition(),
						base::strConcat("Parameter type is `", pb.type.toString(), "`.")
					));
					ctx.logInt(std::move(err));
				}
				return false;
			}

			if (params.compare_parameter_names and pa.name != pb.name) {
				if (do_log) {
					auto err = makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"Parameter named `", pa.name, "` does not match `", pb.name, "`."
						),
						pa.origin.getStablePosition(),
						detailed_reason
					);
					err->addAttachedMessage(makeBox<dia_int::PlaceholderNote>(
						"Corresponding parameter declared here",
						pb.origin.getStablePosition(),
						base::strConcat("Parameter is named `", pb.name, "`.")
					));
					ctx.logInt(std::move(err));
				}
				return false;
			}
		}

		return true;
	}

	base::OkBad ensureFunctionDeclarationNoInitialValue(
		query::Context& ctx, const HOUTFunctionDeclaration& fun, const std::string& reason
	) {
		for (auto& param: fun.parameters) {
			if_opt_some(param.initial_value, expr) {
				auto err = makeBox<dia_int::PlaceholderError>(
					base::strConcat("Initial value is not allowed here. ", reason),
					expr->origin.getStablePosition()
				);
				ctx.logInt(std::move(err));
				return base::BAD;
			}
		}
		return base::OK;
	}
}
