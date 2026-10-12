// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/except/exceptions.hpp>

#include <string>

namespace vm {
	class KillProcessException: public base::Exception {
		std::string message;

	public:
		KillProcessException(std::string message = "Process killed"): message(std::move(message)) {
			this->message += '\0';
		}

		[[nodiscard]]
		const char* what() const noexcept override {
			return message.data();
		}
	};
}
