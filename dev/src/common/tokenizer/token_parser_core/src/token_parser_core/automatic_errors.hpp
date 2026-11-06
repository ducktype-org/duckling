// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <diagnostic/message.hpp>
#include <diagnostic/source_position.hpp>
#include <lang_definitions/key_spec_op.hpp>

namespace tpc {
	class BadKeywordError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_keyword" };
		}

	public:
		BadKeywordError(dia::SourcePosition pos, lang_def::Keyword key):
			  MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("keyword", lang_def::keywordToStr(key).str());
		}
	};

	class BadSpecialError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_special" };
		}

	public:
		BadSpecialError(dia::SourcePosition pos, lang_def::Special spec):
			  MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("special", lang_def::specialToStr(spec).str());
		}
	};

	class BadOperatorError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_operator" };
		}

	public:
		BadOperatorError(dia::SourcePosition pos, lexer::Operator opr):
			  MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("operator", opr.str());
		}
	};

	class NoIdentifierError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return {
				.template_type = "message",
				.type          = "error",
				.family        = "parser",
				.name          = "no_identifier",
			};
		}

	public:
		NoIdentifierError(dia::SourcePosition pos, std::string_view but_got);
	};

	class NoKeywordError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return {
				.template_type = "message",
				.type          = "error",
				.family        = "parser",
				.name          = "no_keyword",
			};
		}

	public:
		NoKeywordError(dia::SourcePosition pos, std::string_view but_got);
	};

	class NoOperatorError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return {
				.template_type = "message",
				.type          = "error",
				.family        = "parser",
				.name          = "no_operator",
			};
		}

	public:
		NoOperatorError(dia::SourcePosition pos, std::string_view but_got);
	};

	class NoStringError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_string" };
		}

	public:
		NoStringError(dia::SourcePosition pos): MessageWithCodeFragmentAndCause(pos) {}
	};

	class NoNumericValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_numeric_value" };
		}

	public:
		NoNumericValueError(dia::SourcePosition pos): MessageWithCodeFragmentAndCause(pos) {}
	};

}
