
#include "global_logger.hpp"

#include <diagnostic_interactive/logger.hpp>

#include <query_framework/context/context.hpp>

namespace global_state {

	namespace {
		constinit MBox<dia_int::Logger> global_logger;
	}

	Ref<dia_int::Logger> getGlobalLogger() {
		CORE_ASSERT(!query::Context::areWeInsideQuery(), "getGlobalLogger used within query!");
		CORE_ASSERT(global_logger.toOpt().has_value(), "Global logger is not set!");
		return global_logger.toOpt().value();
	}

	bool hasGlobalLogger() { return global_logger.toOpt().has_value(); }

	namespace setters {
		void setGlobalLogger(Box<dia_int::Logger> logger) {
			CORE_ASSERT(global_logger.toOpt().empty(), "Global logger is already set!");
			global_logger = std::move(logger);
		}
	}
}
