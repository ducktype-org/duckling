#pragma once

#include <base/box.hpp>

namespace compiler::helios::code {
	template<typename T>
	using ElementRef = base::Box<T>;
}
