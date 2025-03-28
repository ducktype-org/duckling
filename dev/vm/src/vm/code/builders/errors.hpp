#include <base/exceptions.hpp>
#include <utility>

namespace vm::code::builders {
	class BuilderError: public base::LogicError {
	public:
		BuilderError(std::string reason): base::LogicError(std::move(reason)) {}
	};

	class DuplicatedTypeError: public BuilderError {
		using BuilderError::BuilderError;
	};

	class StackStateError: public BuilderError {
		using BuilderError::BuilderError;
	};
}
