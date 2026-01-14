#pragma once

#include "class_elements_errors.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst {
	class NonEmptyError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "destructor_arguments_error" };
		}

	public:
		NonEmptyError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class NoSpecifierError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_access_specifier" };
		}

	public:
		NoSpecifierError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};
}
