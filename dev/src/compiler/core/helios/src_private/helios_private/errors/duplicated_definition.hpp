#pragma once

#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>

namespace dia {

	/**
	 * @brief Error indicating that a symbol has been defined more than once.
	 */
	class DuplicatedDefinitionError final: public MessageBase {
		[[nodiscard]] Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "misc",
				     .name          = "duplicated_definition" };
		}

	public:
		DuplicatedDefinitionError(
			std::string_view    symbol_name,
			dia::StablePosition source_position,
			std::string_view    pointer_message_content = "here"
		);
	};

}
