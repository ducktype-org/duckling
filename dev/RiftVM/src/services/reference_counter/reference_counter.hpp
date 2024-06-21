#pragma once

#include "../services.hpp"

namespace vm {
	class ReferenceCounter {
		template<class... DynamicServices>
		ReferenceCounter(ServiceManagerDef<DynamicServices...>&) {}

	public:
		template<class... DynamicServices>
		friend class ServiceManagerDef;
	};
}
