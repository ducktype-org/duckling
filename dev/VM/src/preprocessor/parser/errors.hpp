#include <utility>

#include <base/str_utils.hpp>
#include <base/string_id.hpp>
#include <diagnostic/message.hpp>

namespace vm::parser {
	class ExpectedSemicolonAfterError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected `;` after here";
		}

	public:
		[[nodiscard]] Domain getDomain() const override { return Domain::Parser; }

		ExpectedSemicolonAfterError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnknownOpCodeError final: public dia::Error {
		base::StrID opcode;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat("Given OpCode does not exist: ", opcode);
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

	class InvalidLabel final: public dia::Error {
		base::StrID reason;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat("Invalid label: ", reason);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		InvalidLabel(dia::SourcePosition pos, std::string_view reason):
			  dia::Error(pos),
			  reason(base::StrID(reason.data())) {}
	};

	class RepeatedLabelNote final: public dia::NoteWithPosition {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Previous declaration here.";
		}

	public:
		RepeatedLabelNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};

	class NoMainError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Provided program does not have `main` function.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		NoMainError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class DuplicatedTypeError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Duplicated type here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		DuplicatedTypeError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class DuplicatedTypeNote final: public dia::NoteWithPosition {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Duplicated type.";
		}

	public:
		DuplicatedTypeNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};

	class InvalidType final: public dia::Error {
		base::StrID reason;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat("Invalid type: ", reason);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		InvalidType(dia::SourcePosition pos, std::string_view reason):
			  dia::Error(pos),
			  reason(base::StrID(reason.data())) {}
	};

	class InvalidFunction final: public dia::Error {
		base::StrID reason;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat("Invalid function: ", reason);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		InvalidFunction(dia::SourcePosition pos, std::string_view reason):
			  dia::Error(pos),
			  reason(base::StrID(reason.data())) {}
	};

	class InvalidLiteral final: public dia::Error {
		base::StrID reason;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat("Invalid literal: ", reason);
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
