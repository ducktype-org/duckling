/**
 * @file usage.hpp
 * @author Wojciech Rzeplinski
 * @brief Predefined message usages for common diagnostic messages.
 *
 * @warning
 *
 * The classes should define constructors that set up the arguments
 * and pointer messages required by the message template.
 * In the constructor they should call the methods of MessageBase
 * to add arguments and pointer messages.
 *
 * The user should not call `addArgument` or `addPointerMessage` directly,
 * but use the provided constructors to ensure the type correctness.
 */
#pragma once

#include "message.hpp"

namespace dia_int {
	/**
	 * @brief This is a helper base class for messages.
	 * This is the same as MessageWithCodeFragmentAndCause, but without the cause pointer message.
	 */
	class MessageWithCodeFragment: public MessageBase {
	protected:
		MessageWithCodeFragment(dia::SourcePosition source_position) {
			addArgument<CodeArgument>("code", source_position);
			addArgument<CodeLocationArgument>("code_location", source_position);
		}
	};

	/**
	 * @brief This is a helper base class for messages.
	 * The `code` and `code_location` arguments are the same arguments as any other,
	 * they are not special in any way.
	 * But they are very commonly used together with the `cause` pointer message,
	 * so this base class adds them both based on the provided source position.
	 */
	class MessageWithCodeFragmentAndCause: public MessageBase {
	protected:
		MessageWithCodeFragmentAndCause(dia::SourcePosition source_position) {
			addArgument<CodeArgument>("code", source_position);
			addArgument<CodeLocationArgument>("code_location", source_position);
			addPointerMessage({ "cause", source_position });
		}
	};

	/**
	 * @brief A Placeholder message with a header only and no code snippet.
	 *
	 * Used when the developer is lazy and want's to have a fast error message.
	 */
	class PlaceholderHeaderError: public MessageBase {
		Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "misc",
				     .name          = "todo_header" };
		}

	public:
		PlaceholderHeaderError(std::string header_message): MessageBase() {
			addArgument<TextArgument>("header_message", std::move(header_message));
		}
	};

	/**
	 * @brief A Placeholder message with a code snippet and optional description and pointer message.
	 *
	 * Used when the developer is lazy and want's to have a fast error message.
	 *
	 * The content of the header message, description and pointer message
	 * can be customized and provided by the developer.
	 *
	 * This is not a recommended way of reporting errors to the user,
	 * the text content of the error message should be inside the template files as much as possible.
	 */
	class PlaceholderCodeError final: public MessageBase {
		Metadata getMetadata() const final {
			return {
				.template_type = "message", .type = "error", .family = "misc", .name = "todo_code"
			};
		}

	public:
		PlaceholderCodeError(
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
