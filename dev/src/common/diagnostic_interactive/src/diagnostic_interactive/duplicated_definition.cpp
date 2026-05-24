#include "duplicated_definition.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>

namespace dia_int {

	DuplicatedDefinitionError::DuplicatedDefinitionError(
		const std::string&                      symbol_name,
		base::Optional<dia_int::StablePosition> source_position,
		const std::string&                      pointer_message_content
	):
		  MessageBase() {
		addArgument<TextArgument>("symbol_name", symbol_name);

		if_opt_some(source_position, pos) {
			addArgument<CodeArgument>("code", pos);
			addArgument<CodeLocationArgument>("code_location", pos);
			addPointerMessage("cause", pos);
		}

		addArgument<TextArgument>("pointer_message_content", pointer_message_content);
	}

}
