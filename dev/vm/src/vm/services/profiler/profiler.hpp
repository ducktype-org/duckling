#pragma once

#include <vm/events/function_call_event.hpp>
#include <vm/events/memory_event.hpp>

#include "../services.hpp"
#include <listener/listener.hpp>

namespace vm {
	/**
	 * @brief Service that profiles the execution of the VM.
	 *
	 * It listens to the events emitted by different services.
	 * Currently nothing emits events and this service is not implemented!
	 */
	class Profiler: public Listener<MemoryEvent>, public Listener<FunctionCallEvent> {
		template<class... DynamicServices>
		Profiler(ServiceManagerDef<DynamicServices...>& /* serviceManager */) {}

	public:
		~Profiler() noexcept override = default;

		void onEvent(const MemoryEvent& event) noexcept override;
		void onEvent(const FunctionCallEvent& event) noexcept override;

		template<class... DynamicServices>
		friend class ServiceManagerDef;
	};
}
