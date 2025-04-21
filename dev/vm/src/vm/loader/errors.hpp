#include <diagnostic/message.hpp>

#include <base/string_id.hpp>

#include <string_view>

namespace vm::loader {
	class UnknownLabelError final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG = "Label does not exist.";

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

		UnknownLabelError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class RepeatedLabelError final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG = "Repeated label.";

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

		RepeatedLabelError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class RepeatedLabelNote final: public dia::NoteWithPosition {
	public:
		constexpr static const std::string_view ERR_MSG = "Previous declaration here.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		RepeatedLabelNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};

	class DuplicatedTypeError final: public dia::Error {
		base::StrID type_name;

	public:
		constexpr static const std::string_view ERR_MSG = "Duplicated type: ";

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
		DuplicatedTypeNote(dia::SourcePosition pos, base::StrID type_name):
			  dia::NoteWithPosition(pos),
			  type_name(type_name) {}
	};

	class UnknownTypeError final: public dia::Error {
		base::StrID type_name;

	public:
		constexpr static const std::string_view ERR_MSG = "Unknown type: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, type_name);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::StaticVerification;
		}

		UnknownTypeError(dia::SourcePosition pos, base::StrID type_name):
			  dia::Error(pos),
			  type_name(type_name) {}
	};

	class UnknownFunctionError final: public dia::Error {
		base::StrID func_name;

	public:
		constexpr static const std::string_view ERR_MSG = "Function does not exist: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, func_name);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::StaticVerification;
		}

		UnknownFunctionError(dia::SourcePosition pos, base::StrID func_name):
			  dia::Error(pos),
			  func_name(func_name) {}
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

	class StackStructureMismatchError final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG
			= "This instruction invalidates stack structure.";

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

		StackStructureMismatchError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class StackStructureMismatchNote final: public dia::NoteWithPosition {
	public:
		constexpr static const std::string_view ERR_MSG = "Some stack structure here.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		StackStructureMismatchNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};

	class UseAfterDeinit final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG
			= "This instruction tries to dereference a pointer to a deinitialized value.";

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

		UseAfterDeinit(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class DeferenceTypeMismatchError final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG
			= "The dereferenced pointer's type does not match the target variable's type.";

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

		DeferenceTypeMismatchError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class LocalUsedAsPointerError final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG
			= "This instruction tries to use a primitive as a pointer.";

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

		LocalUsedAsPointerError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class OpCodeTypeMismatchError final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG
			= "This instruction tries to be constructed with arguments of wrong types.";

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

		OpCodeTypeMismatchError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UninitializedLocalError final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG
			= "Tried to access an uninitizlized local variable.";

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

		UninitializedLocalError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class StackOffsetError final: public dia::Error {
	public:
		constexpr static const std::string_view ERR_MSG
			= "Tried to dereference a stack variable with an invalid offset.";

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

		StackOffsetError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnknownSubtypeError final: public dia::Error {
		base::StrID subtype_name;

	public:
		constexpr static const std::string_view ERR_MSG = "This subtype is not defined anywhere: ";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return base::strConcat(ERR_MSG, subtype_name);
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::StaticVerification;
		}

		UnknownSubtypeError(dia::SourcePosition pos, base::StrID subtype_name):
			  dia::Error(pos),
			  subtype_name(subtype_name) {}
	};

	class SomeBuilderError final: public dia::Error {
		std::string error_message;

	public:
		constexpr static const std::string_view ERR_MSG = "An error occurred during building: ";

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

		SomeBuilderError(dia::SourcePosition pos, std::string_view error_message):
			  dia::Error(pos),
			  error_message(error_message) {}
	};

	constexpr const std::string_view NO_MAIN_ERR
		= "Provided program does not have `main` function.";

	constexpr const std::string_view WRONG_MAIN_RET_VAL_ERR
		= "Main function has to return an 8 byte primitive type.";
}
