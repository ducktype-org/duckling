#pragma once

#include <functional>
namespace concurrent {
	/**
	 * @brief Represents a task to be executed by a thread.
     */
    using Task = std::function<void()>;
}
