#include <base/string_id.hpp>
#include <diagnostic/message.hpp>

namespace vm::preprocessor {
	class RepeatedLabelNote final: public dia::NoteWithPosition {
	public:
		constexpr static std::string_view ERR_MSG = "Previous declaration here.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		RepeatedLabelNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};

	class DuplicatedTypeError final: public dia::Error {
	public:
		constexpr static std::string_view ERR_MSG = "Duplicated type.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		DuplicatedTypeError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class DuplicatedTypeNote final: public dia::NoteWithPosition {
	public:
		constexpr static std::string_view ERR_MSG = "Previous type declaration here.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		DuplicatedTypeNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};

	class UnknownType final: public dia::Error {
		base::StrID type_name;

	public:
		constexpr static std::string_view ERR_MSG = "Unknown type: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, type_name);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		UnknownType(dia::SourcePosition pos, base::StrID type_name):
			  dia::Error(pos),
			  type_name(type_name) {}
	};

	class UnknownFunction final: public dia::Error {
		base::StrID func_name;

	public:
		constexpr static std::string_view ERR_MSG = "Function does not exist: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, func_name);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		UnknownFunction(dia::SourcePosition pos, base::StrID func_name):
			  dia::Error(pos),
			  func_name(func_name) {}
	};

	class DuplicateFunctionDefinitionError final: public dia::Error {
	public:
		constexpr static std::string_view ERR_MSG = "Function with this name already exists.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		DuplicateFunctionDefinitionError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class DuplicatedFunctionDefinitionNote final: public dia::NoteWithPosition {
	public:
		constexpr static std::string_view ERR_MSG = "Previous function declaration here.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		DuplicatedFunctionDefinitionNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};
}
