#pragma once

#include <listener/emitter.hpp>
#include "memory_event.hpp"
#include "function_call_event.hpp"

namespace vm {
	class EmitterManager {
		private:
			EmitterManager() = default;

			static EmitterManager& get() {
				static EmitterManager instance;
				return instance;
			}
			
			Emitter<MemoryEvent> memory_event_emitter;
			Emitter<FunctionCallEvent> function_call_event_emitter;
		public:
			static Emitter<MemoryEvent>& getMemoryEventEmitter() noexcept;
			static Emitter<FunctionCallEvent>& getFunctionCallEventEmitter() noexcept;
	};
}
