#pragma once

#include <diagnostic_interactive/message.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>

namespace compiler::helios {
	class VariableShadowingError: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "variable_shadowing" };
		}

	public:
		VariableShadowingError(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ShadowingDeclarationNote final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "type_check",
				     .name          = "shadowing_declaration" };
		}

	public:
		ShadowingDeclarationNote(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};
}
