#pragma once

#include <diagnostic_interactive/message.hpp>

namespace pst {


	class EmptyStatementError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "empty_statement_error" };
		}

	public:
		EmptyStatementError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

}
