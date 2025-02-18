#include <utility>

#include <base/str_utils.hpp>
#include <base/string_id.hpp>
#include <diagnostic/message.hpp>

namespace vm::parser {
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
		std::string_view reason;

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
			  reason(reason) {}
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
		std::string_view reason;

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
			  reason(reason) {}
	};

	class InvalidFunction final: public dia::Error {
		std::string_view reason;

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
			  reason(reason) {}
	};

	// class FileError final: public dia::Error {
	// 	std::string           err;
	// 	std::filesystem::path path;

	// protected:
	// 	[[nodiscard]]
	// 	std::string toStringBrief() const override {
	// 		return base::strConcat("Problem with a file `", path.c_str(), "`, reason: ", err);
	// 	}

	// public:
	// 	[[nodiscard]]
	// 	Domain getDomain() const override {
	// 		return Domain::Parser;
	// 	}

	// 	FileError(dia::SourcePosition pos, fs::FilePath& file, std::string err):
	// 		  dia::Error(pos),
	// 		  err(std::move(err)),
	// 		  path(file.absolutePath()) {}
	// };
}
