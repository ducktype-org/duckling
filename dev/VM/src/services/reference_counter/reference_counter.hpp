#pragma once

#include "../services.hpp"

namespace vm {
	/**
	 * @brief Service that counts references to objects. (Not implemented)
	 */
	class ReferenceCounter {
		template<class... DynamicServices>
		ReferenceCounter(ServiceManagerDef<DynamicServices...>& /* serviceManager */) {}

	public:
		template<class... DynamicServices>
		friend class ServiceManagerDef;
	};
}
