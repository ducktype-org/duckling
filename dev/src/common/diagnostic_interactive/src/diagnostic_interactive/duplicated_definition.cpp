#include "duplicated_definition.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>

namespace dia_int {

	DuplicatedDefinitionError::DuplicatedDefinitionError(
		std::string                             symbol_name,
		base::Optional<dia_int::StablePosition> source_position,
		std::string                             pointer_message_content
	):
		  MessageBase() {
		addArgument<TextArgument>("symbol_name", std::move(symbol_name));

		if_opt_some(source_position, pos) {
			addArgument<CodeArgument>("code", pos);
			addArgument<CodeLocationArgument>("code_location", pos);
			addPointerMessage("cause", pos);
		}

		addArgument<TextArgument>("pointer_message_content", std::move(pointer_message_content));
	}

}
