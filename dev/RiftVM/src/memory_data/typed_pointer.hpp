#pragma once

#include <services_data/type_metadata/type.hpp>
#include "pointer.hpp"

namespace vm {	
	class TypedPointer {
		private:
			TypeRef type;
			Pointer pointer;
	};
}
