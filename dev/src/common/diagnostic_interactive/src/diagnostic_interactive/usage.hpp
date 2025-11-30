#pragma once

#include "message.hpp"
// #include <diagnostic_interactive/core/diagnostic_file.hpp>

namespace dia_int {
	class MessageWithCodeFragment: public MessageBase {
	protected:
		MessageWithCodeFragment(dia::SourcePosition source_position) {
			addArgument<CodeArgument>("code", source_position);
			addArgument<CodeLocationArgument>("code_location", source_position);
		}
	};

	class MessageWithCodeFragmentAndCause: public MessageBase {
	protected:
		MessageWithCodeFragmentAndCause(dia::SourcePosition source_position) {
			addArgument<CodeArgument>("code", source_position);
			addArgument<CodeLocationArgument>("code_location", source_position);
			addPointerMessage({ "cause", source_position });
		}
	};

	class TodoHeaderError: public MessageBase {
		Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "misc",
				     .name          = "todo_header" };
		}

	public:
		TodoHeaderError(std::string header_message): MessageBase() {
			addArgument<TextArgument>("header_message", std::move(header_message));
		}
	};

	class TodoCodeError final: public MessageBase {
		Metadata getMetadata() const final {
			return {
				.template_type = "message", .type = "error", .family = "misc", .name = "todo_code"
			};
		}

	public:
		TodoCodeError(
			std::string                 header_message,
			dia::SourcePosition         source_position,
			std::string                 description             = "",
			base::Optional<std::string> pointer_message_content = {}
		):
			  MessageBase() {
			addArgument<TextArgument>("header_message", std::move(header_message));
			addArgument<TextArgument>("description", std::move(description));

			addArgument<CodeArgument>("code", source_position);
			addArgument<CodeLocationArgument>("code_location", source_position);

			if_opt_some(pointer_message_content, val) {
				addArgument<TextArgument>("pointer_message_content", std::move(val));
			}
			{ addArgument<TextArgument>("pointer_message_content", ""); }

			addPointerMessage("cause", source_position);
		}
	};

	class FunctionOverloadResolutionFailed {};
}
