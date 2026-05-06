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

#include <diagnostic_interactive/stable_position.hpp>

namespace dia_int {

	/**
	 * @brief A Placeholder message with an optional code snippet
	 * and optional description and pointer message.
	 *
	 * Used when the developer is lazy and want's to have a fast error message.
	 *
	 * The content of the header message, description and pointer message
	 * can be customized and provided by the developer.
	 *
	 * This is not a recommended way of reporting errors to the user,
	 * the text content of the error message should be inside the template files as much as possible.
	 */
	class PlaceholderError final: public MessageBase {
		Metadata getMetadata() const final {
			return {
				.template_type = "message",
				.type          = "error",
				.family        = "misc",
				.name          = "placeholder",
			};
		}

	public:
		PlaceholderError(
			std::string                             header_message,
			base::Optional<dia_int::StablePosition> source_position         = {},
			std::string                             description             = "",
			base::Optional<std::string>             pointer_message_content = "here"
		);

		/**
		 * @brief Should be used when we can't access the stable position only.
		 */
		PlaceholderError(
			std::string                         header_message,
			base::Optional<dia::SourcePosition> source_position         = {},
			std::string                         description             = "",
			base::Optional<std::string>         pointer_message_content = "here"
		);

		/**
		 * @brief Alias because not providing a source_position leads to ambiguity.
		 */
		PlaceholderError(std::string header_message, std::string description = "");
	};

	/**
	 * @brief A Placeholder Not-Yet-Implemented message with a header and code snippet. Used to
	 * indicate that a feature is not yet implemented.
	 *
	 * This acts as an alternative to throwing panics or `base::NotYetImplemented` exceptions that
	 * provides better user experience and testing possibilities.
	 *
	 * @note This should be treated as any other error, and the query that logs this error should
	 * generally return a failed result.
	 */
	class NotYetImplementedCodeError final: public MessageBase {
		Metadata getMetadata() const final {
			return {
				.template_type = "message",
				.type          = "error",
				.family        = "misc",
				.name          = "not_yet_implemented_code",
			};
		}

		void addStacktraceArgument();

	public:
		NotYetImplementedCodeError(
			std::string                         header_message,
			base::Optional<dia::SourcePosition> source_position,
			std::string                         description             = "",
			base::Optional<std::string>         pointer_message_content = "here"
		);

		NotYetImplementedCodeError(
			std::string                             header_message,
			base::Optional<dia_int::StablePosition> source_position,
			std::string                             description             = "",
			base::Optional<std::string>             pointer_message_content = "here"
		);

		/**
		 * @brief Alias because not providing a source_position leads to ambiguity.
		 */
		NotYetImplementedCodeError(std::string header_message, std::string description = "");
	};

	/**
	 * @brief A Placeholder message with an optional code snippet and optional description and
	 * pointer message.
	 */
	class PlaceholderNote final: public MessageBase {
		Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "misc",
				     .name          = "placeholder" };
		}

	public:
		PlaceholderNote(
			std::string                         header_message,
			base::Optional<dia::SourcePosition> source_position         = {},
			std::string                         description             = "",
			base::Optional<std::string>         pointer_message_content = "here"
		);

		PlaceholderNote(
			std::string                             header_message,
			base::Optional<dia_int::StablePosition> source_position         = {},
			std::string                             description             = "",
			base::Optional<std::string>             pointer_message_content = "here"
		);

		/**
		 * @brief Alias because not providing a source_position leads to ambiguity.
		 */
		PlaceholderNote(std::string header_message, std::string description = "");
	};

	class PlaceholderWarning final: public MessageBase {
		Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "warning",
				     .family        = "misc",
				     .name          = "placeholder" };
		}

	public:
		PlaceholderWarning(
			std::string                         header_message,
			base::Optional<dia::SourcePosition> source_position         = {},
			std::string                         description             = "",
			base::Optional<std::string>         pointer_message_content = "here"
		);

		PlaceholderWarning(
			std::string                             header_message,
			base::Optional<dia_int::StablePosition> source_position         = {},
			std::string                             description             = "",
			base::Optional<std::string>             pointer_message_content = "here"
		);

		/**
		 * @brief Alias because not providing a source_position leads to ambiguity.
		 */
		PlaceholderWarning(std::string header_message, std::string description = "");
	};

}
