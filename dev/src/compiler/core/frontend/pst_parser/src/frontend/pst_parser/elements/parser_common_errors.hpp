#pragma once

#include <diagnostic_interactive/message.hpp>

namespace pst::error {
	class BlockStartError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "block_start_error" };
		}

	public:
		BlockStartError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class DuplicateSemicolon final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "warning",
				     .family        = "parser",
				     .name          = "duplicate_semicolon" };
		}

	public:
		DuplicateSemicolon(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};
}
