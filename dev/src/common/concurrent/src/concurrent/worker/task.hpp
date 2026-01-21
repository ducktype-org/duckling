#pragma once

#include <concurrent/worker/worker_data.hpp>

#include <functional>

namespace concurrent {
	/**
	 * @brief Represents a task to be executed by a worker.
	 */
	using Task = std::function<void(WDRef)>;
}
