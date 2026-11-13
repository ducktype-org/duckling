// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <cstdint>

namespace vm::fast {
	/**
	 * @brief Why a fast-mode thread stopped executing before reaching `exit`.
	 * Fast mode never throws on the execution path; see D-1 and D-11 in core/fast/readme.md.
	 */
	enum class Trap : std::uint8_t {
		None,
		DivisionByZero,
		StackOverflow,
		VTableUnset,
		NullFunctionPointer,
		Stopped,    ///< A stop request was observed; the thread ends as Stopped, not Panicked.
		HostError,  ///< A helper caught an exception; the message is in the runtime state.
	};

	constexpr const char* trapMessage(Trap trap) noexcept {
		switch (trap) {
		case Trap::None:
			return "No trap";
		case Trap::DivisionByZero:
			return "Division by zero";
		case Trap::StackOverflow:
			return "Stack overflow";
		case Trap::VTableUnset:
			return "Method called on an object without a vtable";
		case Trap::NullFunctionPointer:
			return "Called a null function pointer";
		case Trap::Stopped:
			return "Stopped";
		case Trap::HostError:
			return "Host error";
		}
		return "Unknown trap";
	}
}
