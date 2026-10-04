#pragma once

#include <diagnostic/message.hpp>

namespace compiler::mir {
	// Used for reporting unused shadowed variables.
	// Shadowing of used variables gets detected earlier, in HELIOS.
	class VariableShadowingError: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lookup",
				     .name          = "variable_shadowing" };
		}

	public:
		VariableShadowingError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ShadowedDeclarationNote final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "lookup",
				     .name          = "shadowed_declaration" };
		}

	public:
		ShadowedDeclarationNote(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};
}
