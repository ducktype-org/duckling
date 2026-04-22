#pragma once

#include <diagnostic_interactive/message.hpp>

namespace compiler::helios {
	// Used for more descriptive errors when trying to look up a shadowed variable.
	// Shadowing of unused variables gets detected at the MIR validation stage.
	class ShadowedVariableLookupError: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lookup",
				     .name          = "shadowed_variable_lookup" };
		}

	public:
		// @TODO: #2521 change this to stable position after fix
		ShadowedVariableLookupError(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ShadowingDeclarationNote final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "lookup",
				     .name          = "lookup_shadowing_declaration" };
		}

	public:
		ShadowingDeclarationNote(dia_int::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};
}
