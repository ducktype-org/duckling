#pragma once

#include <listener/listener.hpp>

#include "../services.hpp"
#include "events/function_call_event.hpp"
#include "events/memory_event.hpp"

namespace vm {
	class Profiler: public Listener<MemoryEvent>, public Listener<FunctionCallEvent> {
	private:
		template<class... DynamicServices>
		Profiler(ServiceManagerDef<DynamicServices...>& serviceManager) {}

	public:
		virtual ~Profiler() noexcept = default;

		void onEvent(const MemoryEvent& event) noexcept override;
		void onEvent(const FunctionCallEvent& event) noexcept override;

		template<class... DynamicServices>
		friend class ServiceManagerDef;
	};
}  // namespace vm
