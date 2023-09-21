#include "emitter_manager.hpp"

namespace vm {
	Emitter<MemoryEvent>& EmitterManager::getMemoryEventEmitter() noexcept {
		return get().memory_event_emitter;
	}
	
	Emitter<FunctionCallEvent>& EmitterManager::getFunctionCallEventEmitter() noexcept {
		return get().function_call_event_emitter;
	}
}