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
		~Profiler() noexcept = default;

		void onEvent(const MemoryEvent& event) noexcept;
		void onEvent(const FunctionCallEvent& event) noexcept;

		template<class... DynamicServices>
		friend class ServiceManagerDef;
	};
}
