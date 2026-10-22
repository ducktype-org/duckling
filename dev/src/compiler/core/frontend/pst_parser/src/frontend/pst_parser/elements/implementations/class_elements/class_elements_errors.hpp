// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "class_elements_errors.hpp"

#include <diagnostic/message.hpp>

namespace pst {
	class NonEmptyError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "destructor_arguments_error" };
		}

	public:
		NonEmptyError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class NoSpecifierError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_access_specifier" };
		}

	public:
		NoSpecifierError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};
}
