
#include "global_logger.hpp"

#include <diagnostic_interactive/logger.hpp>

namespace global_state {

	namespace {
		constinit MBox<dia_int::Logger> global_logger;
	}

	Ref<dia_int::Logger> getGlobalLogger() { return global_logger.refMut().toOpt().value(); }

	bool hasGlobalLogger() { return global_logger.toOpt().has_value(); }

	namespace setters {
		/**
		 * This should only be called by the driver.
		 */
		void setGlobalLogger(Box<dia_int::Logger> logger) {
			CORE_ASSERT(global_logger.toOpt().empty(), "Global logger is already set!");
			global_logger = std::move(logger);
		}
	}
}
