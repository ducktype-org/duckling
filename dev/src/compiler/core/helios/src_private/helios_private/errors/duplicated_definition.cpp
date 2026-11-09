// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
