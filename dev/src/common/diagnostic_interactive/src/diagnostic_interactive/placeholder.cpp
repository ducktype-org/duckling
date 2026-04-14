#include "placeholder.hpp"

#include <diagnostic_interactive/stable_position.hpp>

#include <logger/logger.hpp>

namespace dia_int {

	PlaceholderHeaderError::PlaceholderHeaderError(
		std::string header_message, std::string description
	):
		  MessageBase() {
		addArgument<TextArgument>("header_message", std::move(header_message));
		addArgument<TextArgument>("description", std::move(description));
	}

	PlaceholderCodeError::PlaceholderCodeError(
		std::string                 header_message,
		dia::SourcePosition         source_position,
		std::string                 description,
		base::Optional<std::string> pointer_message_content
	):
		  MessageBase() {
		addArgument<TextArgument>("header_message", std::move(header_message));
		addArgument<TextArgument>("description", std::move(description));

		addArgument<CodeArgument>("code", source_position);
		addArgument<CodeLocationArgument>("code_location", source_position);

		if_opt_some(pointer_message_content, val) {
			addArgument<TextArgument>("pointer_message_content", std::move(val));
		}
		if_opt_none(pointer_message_content) {
			addArgument<TextArgument>("pointer_message_content", "");
		}

		addPointerMessage("cause", source_position);
	}

	PlaceholderHeaderNote::PlaceholderHeaderNote(std::string header_message, std::string description):
		  MessageBase() {
		addArgument<TextArgument>("header_message", std::move(header_message));
		addArgument<TextArgument>("description", std::move(description));
	}

	PlaceholderCodeNote::PlaceholderCodeNote(
		std::string                 header_message,
		dia::SourcePosition         source_position,
		std::string                 description,
		base::Optional<std::string> pointer_message_content
	):
		  MessageBase() {
		addArgument<TextArgument>("header_message", std::move(header_message));
		addArgument<TextArgument>("description", std::move(description));

		addArgument<CodeArgument>("code", source_position);
		addArgument<CodeLocationArgument>("code_location", source_position);

		match_optional(pointer_message_content) {
			opt_some(val) { addArgument<TextArgument>("pointer_message_content", std::move(val)); }
			opt_none { addArgument<TextArgument>("pointer_message_content", ""); }
		}

		addPointerMessage("cause", source_position);
	}

	NotYetImplementedCodeError::NotYetImplementedCodeError(
		std::string                         header_message,
		base::Optional<dia::SourcePosition> source_position,
		std::string                         description,
		base::Optional<std::string>         pointer_message_content
	):
		  MessageBase() {
		addArgument<TextArgument>("header_message", std::move(header_message));
		addArgument<TextArgument>("description", std::move(description));

		if_opt_some(source_position, pos) {
			addArgument<CodeArgument>("code", pos);
			addArgument<CodeLocationArgument>("code_location", pos);
			addPointerMessage("cause", pos);
		}

		if_opt_some(pointer_message_content, val) {
			addArgument<TextArgument>("pointer_message_content", std::move(val));
		}
		if_opt_none(pointer_message_content) {
			addArgument<TextArgument>("pointer_message_content", "");
		}

		if (logger::isCategoryEnabled(logger::DevLogCategories::NYIStacktraces)) {
			addArgument<TextArgument>("stacktrace", base::getCurrentStackTrace());
		} else {
			addArgument<TextArgument>(
				"stacktrace",
				"Enable the NYIStacktraces dev-logs category to see the stacktrace for this "
				"not-yet-implemented error."
			);
		}
	}
}
