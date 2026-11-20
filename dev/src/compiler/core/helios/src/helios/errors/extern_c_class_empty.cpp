// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "extern_c_class_empty.hpp"

#include <diagnostic/core/diagnostic_arguments.hpp>

namespace compiler::helios {

	ExternCClassEmptyError::ExternCClassEmptyError(
		dia::StablePosition source_position, std::string class_name
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<dia::TextArgument>("class_name", std::move(class_name));
	}

}
