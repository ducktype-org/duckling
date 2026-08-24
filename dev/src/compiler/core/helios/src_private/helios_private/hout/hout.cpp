#include "hout.hpp"

#include <helios/hout/elements.hpp>

#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>

namespace compiler::helios {

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
				auto err = makeBox<dia::PlaceholderError>(
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
				err->addAttachedMessage(makeBox<dia::PlaceholderNote>(
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
				auto err = makeBox<dia::PlaceholderError>(
					base::strConcat(
						"Function parameter count mismatch. ",
						"Function has ",
						a.parameters.size(),
						" parameter(s).",
						detailed_reason
					),
					a.origin.getStablePosition()
				);
				err->addAttachedMessage(makeBox<dia::PlaceholderNote>(
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
					auto err = makeBox<dia::PlaceholderError>(
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
					err->addAttachedMessage(makeBox<dia::PlaceholderNote>(
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
					auto err = makeBox<dia::PlaceholderError>(
						base::strConcat(
							"Parameter named `", pa.name, "` does not match `", pb.name, "`."
						),
						pa.origin.getStablePosition(),
						detailed_reason
					);
					err->addAttachedMessage(makeBox<dia::PlaceholderNote>(
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
				auto err = makeBox<dia::PlaceholderError>(
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
