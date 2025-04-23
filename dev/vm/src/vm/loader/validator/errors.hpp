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
