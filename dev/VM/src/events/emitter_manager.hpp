/**
 * @file emitter_manager.hpp
 */
#pragma once

#include <listener/emitter.hpp>
#include "memory_event.hpp"
#include "function_call_event.hpp"

namespace vm {
	using EmitterManager = std::tuple<FunctionCallEvent, MemoryEvent>;
}
