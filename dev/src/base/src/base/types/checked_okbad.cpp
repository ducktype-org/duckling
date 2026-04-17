#include "checked_okbad.hpp"
#include <base/except/exceptions.hpp>

namespace base {
	CheckedOkBad::CheckedOkBad(base::OkBad result): result(result) {}

	CheckedOkBad::~CheckedOkBad() {
		CORE_ASSERT_NOEXCEPT(checked, "Initialization status was not checked, use status method!");
	}

	[[nodiscard]]
	base::OkBad CheckedOkBad::status() {
		checked = true;
		return result;
	}
}
