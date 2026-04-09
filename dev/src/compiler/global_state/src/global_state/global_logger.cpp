
#include "global_logger.hpp"

#include <diagnostic_interactive/logger.hpp>

#include <query_framework/context/context.hpp>

namespace global_state {

	namespace {
		constinit MBox<dia_int::Logger> global_logger;
	}

	Ref<dia_int::Logger> getGlobalLogger() {
		// @TODO: #1933 change it to something better, remove or relax query->global state
		// dependency if possible. Note that this assertion now only works because global logger is
		// not used concurrently with query-workers, but it is logically incorrect.
		CORE_ASSERT(
			query::Context::getState().activeQueryCount() == 0, "getGlobalLogger used within query!"
		);
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
