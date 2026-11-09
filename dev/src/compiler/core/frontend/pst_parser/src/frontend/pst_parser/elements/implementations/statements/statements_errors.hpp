// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <diagnostic/message.hpp>

namespace pst {
	class BadSpecifierCallError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_specifier_call" };
		}

	public:
		BadSpecifierCallError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class InvalidExternContentWarning final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "warning",
				     .family        = "parser",
				     .name          = "invalid_extern_content" };
		}

	public:
		InvalidExternContentWarning(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};
}
