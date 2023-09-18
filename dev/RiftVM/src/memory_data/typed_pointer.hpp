#pragma once

#include "pointer.hpp"

#include <services_data/type_metadata/type.hpp>

namespace vm {
	class TypedPointer {
	private:
		TypeRef type;
		Pointer pointer;
	};
}
