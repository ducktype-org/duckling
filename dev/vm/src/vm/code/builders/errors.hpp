#include <base/exceptions.hpp>
#include <utility>

namespace vm::code::builders {
	class BuilderError: public base::LogicError {
	public:
		BuilderError(std::string reason): base::LogicError(std::move(reason)) {}
	};
}
