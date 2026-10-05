// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "field_not_c_compatible.hpp"

#include <helios_private/errors/dia_interactive_elements.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>

namespace compiler::helios {

	FieldNotCCompatibleError::FieldNotCCompatibleError(
		query::Context&     ctx,
		dia::StablePosition source_position,
		std::string         field_name,
		tsh::SymbolType<>   field_type,
		std::string         reason
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<dia::TextArgument>("field_name", std::move(field_name));
		addArgument<dia::InteractiveArgument>(
			"field_type", makeBox<InteractiveType>(ctx, field_type)
		);
		addArgument<dia::TextArgument>("reason", std::move(reason));
	}

}
