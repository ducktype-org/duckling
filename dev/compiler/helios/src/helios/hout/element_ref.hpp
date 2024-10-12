#pragma once

#include <base/unique_pointer.hpp>

namespace compiler::helios::code {
	template<typename T>
	using ElementRef = base::unique_ptr<T>;
}
