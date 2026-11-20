// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "global_logger.hpp"

#include <diagnostic/logger.hpp>
#include <query_framework/context/context.hpp>

namespace global_state {

	namespace {
		constinit MBox<dia::Logger> global_logger;
	}

	Ref<dia::Logger> getGlobalLogger() {
		CORE_ASSERT(!query::Context::areWeInsideQuery(), "getGlobalLogger used within query!");
		CORE_ASSERT(global_logger.toOpt().has_value(), "Global logger is not set!");
		return global_logger.toOpt().value();
	}

	bool hasGlobalLogger() { return global_logger.toOpt().has_value(); }

	namespace setters {
		void setGlobalLogger(Box<dia::Logger> logger) {
			CORE_ASSERT(global_logger.toOpt().empty(), "Global logger is already set!");
			global_logger = std::move(logger);
		}
	}
}
