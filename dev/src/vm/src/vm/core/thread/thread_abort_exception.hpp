#pragma once

#include <base/except/exceptions.hpp>

namespace vm {
	class ThreadAbortException: public base::Exception {
	public:
		[[nodiscard]] const char* what() const noexcept override { return "Thread aborted"; }
	};
}