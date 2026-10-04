#pragma once

#include <diagnostic/message.hpp>

namespace pst::error {
	class BlockStartError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "block_start_error" };
		}

	public:
		BlockStartError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class DuplicateSemicolon final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "warning",
				     .family        = "parser",
				     .name          = "duplicate_semicolon" };
		}

	public:
		DuplicateSemicolon(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};
}
