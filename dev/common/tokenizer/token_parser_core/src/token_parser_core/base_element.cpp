#include "base_element.hpp"
#include <base/exceptions.hpp>

namespace tpc {
	Element::~Element() = default;

	bool Element::trailingSemicolon() {
		// @IDEA: not Panic
		CORE_PANIC("trailingSemicolon called on illegal object");
	}
}
