/**
 * @file emitter_manager.hpp
 */
#pragma once

#include "function_call_event.hpp"
#include "memory_event.hpp"

#include <tuple>

namespace vm {
	using EmitterManager = std::tuple<FunctionCallEvent, MemoryEvent>;
}
