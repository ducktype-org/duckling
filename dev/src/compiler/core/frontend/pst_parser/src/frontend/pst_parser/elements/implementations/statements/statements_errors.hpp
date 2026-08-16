#pragma once

#include <diagnostic/message.hpp>

namespace pst {
	class AliasStarError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "alias_star_error" };
		}

	public:
		AliasStarError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadSpecifierCallError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_specifier_call" };
		}

	public:
		BadSpecifierCallError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class InvalidExternContentWarning final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "warning",
				     .family        = "parser",
				     .name          = "invalid_extern_content" };
		}

	public:
		InvalidExternContentWarning(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};
}
