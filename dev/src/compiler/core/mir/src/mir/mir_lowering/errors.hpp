#pragma once

#include <diagnostic_interactive/message.hpp>

namespace compiler::mir {
	// Used for reporting unused shadowed variables.
	// Shadowing of used variables gets detected earlier, in HELIOS.
	class VariableShadowingError: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lookup",
				     .name          = "variable_shadowing" };
		}

	public:
		VariableShadowingError(dia_int::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ShadowedDeclarationNote final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "lookup",
				     .name          = "shadowed_declaration" };
		}

	public:
		ShadowedDeclarationNote(dia_int::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class IllegalComptimeTypeError final : public dia_int::MessageWithCodeFragmentAndCause {
    dia_int::Metadata getMetadata() const final {
        return { .template_type = "message",
                 .type          = "error",
                 .family        = "type_system",  
                 .name          = "illegal_comptime_type" };
    }

public:
    IllegalComptimeTypeError(dia::SourcePosition source_position):
          MessageWithCodeFragmentAndCause(source_position) {}
};
}
