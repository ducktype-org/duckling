#include "module_flags.hpp"

namespace logger {
	// Dev logs are disabled by default.
	constinit bool enable_dev_logs = false;

	// User logs are enabled by default.
	constinit bool enable_user_logs = true;
}
