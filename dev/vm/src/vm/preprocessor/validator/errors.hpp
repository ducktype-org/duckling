#include <base/str_utils.hpp>
#include <base/string_id.hpp>
#include <diagnostic/message.hpp>

namespace vm::validator {
	class ByteCodeValidatorError: public dia::Error {
	public:
		ByteCodeValidatorError(dia::SourcePosition pos): dia::Error(pos) {}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::StaticVerification;
		}
	};

#define DEFINE_VALIDATOR_ERROR(ErrorName, ErrorMessage)                    \
	class ErrorName final: public ByteCodeValidatorError {                 \
	public:                                                                \
		constexpr static std::string_view ERR_MSG = ErrorMessage;          \
                                                                           \
		[[nodiscard]]                                                      \
		Domain getDomain() const override {                                \
			return Domain::StaticVerification;                             \
		}                                                                  \
		ErrorName(dia::SourcePosition pos): ByteCodeValidatorError(pos) {} \
                                                                           \
	protected:                                                             \
		[[nodiscard]]                                                      \
		std::string toStringBrief() const override {                       \
			return ERR_MSG.data();                                         \
		}                                                                  \
	};

	DEFINE_VALIDATOR_ERROR(NoMainError, "Provided program does not have `main` function.");
	DEFINE_VALIDATOR_ERROR(
		CallerArgSizeMismatch,
		"Invalid Tailcall! Caller signature must have arg_size equal to next_arg_size"
	);
	DEFINE_VALIDATOR_ERROR(
		CalledArgSizeMismatch,
		"Invalid Tailcall! Called function signature must have arg_size == next_arg_size"
	);
	DEFINE_VALIDATOR_ERROR(
		CallerCalledArgSizeMismatch, "Invalid Tailcall! Caller and called arg size unmatched"
	);
	DEFINE_VALIDATOR_ERROR(
		CallerCalledStackSizeMismatch, "Invalid Tailcall! Caller and called stack size unmatched"
	);
	DEFINE_VALIDATOR_ERROR(
		CallerCalledRetSizeMismatch, "Invalid Tailcall! Caller and called ret size unmatched"
	);
	DEFINE_VALIDATOR_ERROR(
		JumpStackStructureMismatch,
		"Invalid Jump! Matching jump and label should have the same stack structures"
	);
	DEFINE_VALIDATOR_ERROR(
		InvalidDeinit, "Invalid Deinit! Deinit cannot be used on an empty stack"
	);
}
