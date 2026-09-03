#pragma once

#include "declarations_errors.hpp"

#include <diagnostic/message.hpp>

namespace pst {
	class PatternArgumentCountError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "pattern_argument_count" };
		}

	public:
		PatternArgumentCountError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class PatternBracketError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "pattern_bracket_error" };
		}

	public:
		PatternBracketError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class ForBracketError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "for_bracket_error" };
		}

	public:
		ForBracketError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};
}
