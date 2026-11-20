// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>

#include <string>

namespace compiler::helios {

	/**
	 * @brief Error reported when an `extern("C")` class has no fields.
	 *
	 * Empty structs are not legal in C, so such a class has no valid C-ABI
	 * layout.
	 */
	class ExternCClassEmptyError final: public dia::MessageWithCodeFragmentAndCause {
		[[nodiscard]] dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "extern_c_class_empty" };
		}

	public:
		ExternCClassEmptyError(dia::StablePosition source_position, std::string class_name);
	};

}
