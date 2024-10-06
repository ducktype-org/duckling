/**
 * @file emitter_manager.hpp
 */
#pragma once

#include <listener/emitter.hpp>
#include "memory_event.hpp"
#include "function_call_event.hpp"

namespace vm {
	/**
	* @brief Manages emitters for VM events
	*
	* @warning This class is not used anywhere in the code. The events are not implemented yet.
	 */
	class EmitterManager {
	private:
		EmitterManager() = default;

		static EmitterManager& get() {
			static EmitterManager instance;
			return instance;
		}

		Emitter<MemoryEvent>       memory_event_emitter;
		Emitter<FunctionCallEvent> function_call_event_emitter;

	public:
		static Emitter<MemoryEvent>&       getMemoryEventEmitter() noexcept;
		static Emitter<FunctionCallEvent>& getFunctionCallEventEmitter() noexcept;
	};
}
