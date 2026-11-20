// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <diagnostic/message.hpp>

namespace compiler::helios {
	// Used for more descriptive errors when trying to look up a shadowed variable.
	// Shadowing of unused variables gets detected at the MIR validation stage.
	class ShadowedVariableLookupError: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lookup",
				     .name          = "shadowed_variable_lookup" };
		}

	public:
		ShadowedVariableLookupError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	// Used when the looked-up name exists in the interface of a type, but its visibility hides it
	// from the scope the lookup was written in.
	class InaccessibleSymbolLookupError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lookup",
				     .name          = "inaccessible_symbol_lookup" };
		}

	public:
		InaccessibleSymbolLookupError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class InaccessibleDeclarationNote final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "lookup",
				     .name          = "lookup_inaccessible_declaration" };
		}

	public:
		InaccessibleDeclarationNote(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ShadowingDeclarationNote final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "lookup",
				     .name          = "lookup_shadowing_declaration" };
		}

	public:
		ShadowingDeclarationNote(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};
}
