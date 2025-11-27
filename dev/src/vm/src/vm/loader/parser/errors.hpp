#pragma once
#include <string_id/string_id.hpp>

#include <base/str/str_utils.hpp>

#include <diagnostic/message.hpp>

namespace vm::loader::parser {
	class ExpectedSemicolonAfterError final: public dia::Error {
	public:
		constexpr static std::string_view ERR_MSG = "Expected `;` after here";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		[[nodiscard]] Domain getDomain() const override { return Domain::Parser; }

		ExpectedSemicolonAfterError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnknownOpCodeError final: public dia::Error {
		base::StrID opcode;

	public:
		constexpr static std::string_view ERR_MSG = "Given OpCode does not exist: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, opcode);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		UnknownOpCodeError(dia::SourcePosition pos, base::StrID opcode):
			  dia::Error(pos),
			  opcode(opcode) {}
	};

	class InvalidLiteral final: public dia::Error {
		base::StrID reason;

	public:
		constexpr static std::string_view ERR_MSG = "Invalid literal: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, reason);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		InvalidLiteral(dia::SourcePosition pos, std::string_view reason):
			  dia::Error(pos),
			  reason(base::StrID(reason.data())) {}
	};

}
