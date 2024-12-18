#pragma once

#include <core/process/type_metadata/type.hpp>
#include "pointer.hpp"

namespace vm {
	class TypedPointer {
	private:
		TypeRef type;
		Pointer pointer;
	};
}
