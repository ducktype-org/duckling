#include "base_element.hpp"
#include <base/exceptions.hpp>

namespace tpc {
	Element::~Element() {}
	
	bool Element::trailingSemicolon() {
		// @IDEA: not Panic
		RIFT_PANIC("trailingSemicolon called on illegal object");
	}
}