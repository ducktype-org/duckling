// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/except/exceptions.hpp>

#include <string_view>

namespace vm {
	/**
	 * Exception thrown when a VM operation is not implemented.
	 */
	class VMNotImplemented final: public base::Exception {
		std::string message;

	public:
		VMNotImplemented(): message("VM Operation not implemented") {}

		VMNotImplemented(std::string_view msg): message(msg) {}

		[[nodiscard]]
		constexpr const char* what() const noexcept override {
			return message.c_str();
		}
	};
}
