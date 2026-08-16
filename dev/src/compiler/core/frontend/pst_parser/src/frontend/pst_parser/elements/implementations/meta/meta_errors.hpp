#pragma once

#include <diagnostic/message.hpp>

namespace pst {


	class EmptyStatementError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "empty_statement_error" };
		}

	public:
		EmptyStatementError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

}
