#include <diagnostic/message.hpp>
#include <string_id/string_id.hpp>

#include <string_view>

namespace vm::loader {
	class DuplicatedTypeError final: public dia::Error {
		base::StrID type_name;

	public:
		constexpr static std::string_view ERR_MSG = "Duplicated type: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG.data(), type_name);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::StaticVerification;
		}

		DuplicatedTypeError(dia::SourcePosition pos, base::StrID type_name):
			  dia::Error(pos),
			  type_name(type_name) {}
	};

	class DuplicatedTypeNote final: public dia::NoteWithPosition {
		base::StrID type_name;

	public:
		constexpr static const std::string_view ERR_MSG = "Previous type declaration here.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		DuplicatedTypeNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};

	class DuplicatedFunctionError final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG = "Function with this name already exists.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::StaticVerification;
		}

		DuplicatedFunctionError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class DuplicatedFunctionNote final: public dia::NoteWithPosition {
	public:
		constexpr static const std::string_view ERR_MSG = "Previous function declaration here.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		DuplicatedFunctionNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};

	class SomeValidationError final: public dia::Error {
		std::string error_message;

	public:
		constexpr static const std::string_view ERR_MSG = "A validation error occurred: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, error_message);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::StaticVerification;
		}

		SomeValidationError(dia::SourcePosition pos, std::string_view error_message):
			  dia::Error(pos),
			  error_message(error_message) {}
	};

	class SomeValidationNote final: public dia::NoteWithPosition {
		std::string error_message;

	public:
		constexpr static const std::string_view ERR_MSG = "The error attached a note: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, error_message);
		}

	public:
		SomeValidationNote(dia::SourcePosition pos, std::string_view error_message):
			  dia::NoteWithPosition(pos),
			  error_message(error_message) {}
	};

	class DuplicatedGlobalDataError final: public dia::Error {
		base::StrID global_data_name;

	public:
		constexpr static const std::string_view ERR_MSG = "This global data is duplicated: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, global_data_name);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::StaticVerification;
		}

		DuplicatedGlobalDataError(dia::SourcePosition pos, base::StrID global_data_name):
			  dia::Error(pos),
			  global_data_name(global_data_name) {}
	};

	class DuplicatedGlobalDataNote final: public dia::NoteWithPosition {
	public:
		constexpr static const std::string_view ERR_MSG = "Previous declaration here.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		DuplicatedGlobalDataNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};
}
