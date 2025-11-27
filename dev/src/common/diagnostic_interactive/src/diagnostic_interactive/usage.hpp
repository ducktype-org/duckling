#include "diagnostic.hpp"

namespace dia_int {
	class DiagnosticWithCodeFragment: public DiagnosticBase {
	protected:
		DiagnosticWithCodeFragment(dia::SourcePosition source_position) {
			addArgument<CodeArgument>("code", source_position);
			addArgument<CodeLocationArgument>("code_location", source_position);
		}
	};

	class DiagnosticWithCodeFragmentAndCause: public DiagnosticBase {
	protected:
		DiagnosticWithCodeFragmentAndCause(dia::SourcePosition source_position) {
			addArgument<CodeArgument>("code", source_position);
			addArgument<CodeLocationArgument>("code_location", source_position);
			addPointerMessage({ "cause", source_position });
		}
	};

	class TodoHeaderError: public DiagnosticBase {
		dia_file::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "misc",
				     .name          = "todo_header" };
		}

	public:
		TodoHeaderError(std::string header_message): DiagnosticBase() {
			addArgument<TextArgument>("header_message", std::move(header_message));
		}
	};

	class TodoCodeError final: public DiagnosticBase {
		dia_file::Metadata getMetadata() const final {
			return {
				.template_type = "message", .type = "error", .family = "misc", .name = "todo_code"
			};
		}

	public:
		TodoCodeError(
			std::string                 header_message,
			std::string                 description,
			dia::SourcePosition         source_position,
			base::Optional<std::string> pointer_message_content = {}
		):
			  DiagnosticBase() {
			addArgument<TextArgument>("header_message", std::move(header_message));
			addArgument<TextArgument>("description", std::move(description));

			addArgument<CodeArgument>("code", source_position);
			addArgument<CodeLocationArgument>("code_location", source_position);
			addPointerMessage("cause", source_position);
			addArgument<TextArgument>("pointer_message", "");

			if_opt_some(pointer_message_content, val) {
				addArgument<TextArgument>("pointer_message_content", std::move(val));
			}
			else { addArgument<TextArgument>("pointer_message_content", ""); }
		}
	};

	class FunctionOverloadResolutionFailed {};
}