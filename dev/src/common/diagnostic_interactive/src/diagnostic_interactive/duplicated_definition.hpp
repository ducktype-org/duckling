#pragma once

#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/stable_position.hpp>

#include <base/collections/optional.hpp>

#include <string>

namespace dia_int {

	/**
	 * @brief Error indicating that a symbol has been defined more than once.
	 */
	class DuplicatedDefinitionError final: public MessageBase {
		Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "misc",
				     .name          = "duplicated_definition" };
		}

	public:
		DuplicatedDefinitionError(
			std::string                             symbol_name,
			base::Optional<dia_int::StablePosition> source_position,
			std::string                             pointer_message_content = "here"
		);
	};

}
