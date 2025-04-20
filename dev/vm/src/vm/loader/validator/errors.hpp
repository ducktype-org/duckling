#pragma once
#include <diagnostic/message.hpp>

namespace vm::loader::validator {
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

	constexpr const std::string_view NO_MAIN_ERR
		= "Provided program does not have `main` function.";
}
// @TODOB
#if 0
#pragma once
<<<<<<< HEAD:dev/vm/src/vm/preprocessor/validator/errors.hpp

=======
>>>>>>> inheritance-metadata:dev/vm/src/vm/loader/validator/errors.hpp
#include <diagnostic/message.hpp>

namespace vm::loader::validator {
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

<<<<<<< HEAD:dev/vm/src/vm/preprocessor/validator/errors.hpp
	DEFINE_VALIDATOR_ERROR(NoMainError, "Provided program does not have `main` function.");
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
		InvalidStackOperation,
		"Trying to pop an empty stack or call a function with invalid arguments"
	);
	DEFINE_VALIDATOR_ERROR(InvalidStackOffset, "Trying to use uninitialised memory");
	DEFINE_VALIDATOR_ERROR(UninstantiableValue, "This type cannot be instantiated");
	DEFINE_VALIDATOR_ERROR(InvalidUpcast, "The source does not inherit from the destination type");
	DEFINE_VALIDATOR_ERROR(InvalidImplements, "An interface/class can implement only interfaces");
	DEFINE_VALIDATOR_ERROR(InvalidExtends, "An class can extend only nonfinal classes");
	DEFINE_VALIDATOR_ERROR(
		MissingVtablePointer,
		"The implementation of an interface/class has to contain a VTable pointer"
	);
	DEFINE_VALIDATOR_ERROR(
		InstanceDataInInterface,
		"The implementation of an interface should only contain a VTable pointer"
	);
	DEFINE_VALIDATOR_ERROR(
		MissingAncestorField, "The implementation of a class has to contain all ancestors' fields"
	);
=======
	constexpr const std::string_view NO_MAIN_ERR
		= "Provided program does not have `main` function.";
>>>>>>> inheritance-metadata:dev/vm/src/vm/loader/validator/errors.hpp
}
#endif
