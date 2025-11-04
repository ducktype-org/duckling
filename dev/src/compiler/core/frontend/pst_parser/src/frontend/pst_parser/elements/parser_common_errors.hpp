#pragma once

#include <diagnostic/message.hpp>

namespace pst::error {
	class BlockStartError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected a code block starting with `{`.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BlockStartError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class DuplicateSemicolon final: public dia::Warning {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Duplicate semicolon";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		DuplicateSemicolon(dia::SourcePosition pos): dia::Warning(pos) {}
	};
}
