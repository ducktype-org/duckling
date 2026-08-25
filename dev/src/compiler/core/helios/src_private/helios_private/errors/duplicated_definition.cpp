#include "duplicated_definition.hpp"

#include <diagnostic/core/diagnostic_arguments.hpp>

namespace dia {

	DuplicatedDefinitionError::DuplicatedDefinitionError(
		std::string_view    symbol_name,
		dia::StablePosition source_position,
		std::string_view    pointer_message_content
	):
		  MessageBase() {
		addArgument<TextArgument>("symbol_name", std::string(symbol_name));

		addArgument<CodeArgument>("code", source_position);
		addArgument<CodeLocationArgument>("code_location", source_position);
		addPointerMessage("cause", source_position);

		addArgument<TextArgument>("pointer_message_content", std::string(pointer_message_content));
	}

}
