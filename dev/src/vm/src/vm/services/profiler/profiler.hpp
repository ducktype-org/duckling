#pragma once

#include "../services.hpp"

#include <vm/events/function_call_event.hpp>
#include <vm/events/memory_event.hpp>

namespace vm {
	/**
	 * @brief Service that profiles the execution of the VM.
	 *
	 * It listens to the events emitted by different services.
	 * Currently nothing emits events and this service is not implemented!
	 */
	class Profiler {
		template<class... DynamicServices>
		Profiler(ServiceManagerDef<DynamicServices...>& /* serviceManager */) {}

	public:

		/**
		 * @note This function is not implemented yet. If we decide on a different implementation
		 * of the profiler or events, it should be deleted.
		 */
		void onEvent(const MemoryEvent& event) noexcept;

		/**
		 * @note This function is not implemented yet. If we decide on a different implementation
		 * of the profiler or events, it should be deleted.
		 */
		void onEvent(const FunctionCallEvent& event) noexcept;

		template<class... DynamicServices>
		friend class ServiceManagerDef;
	};
}
