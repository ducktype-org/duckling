#pragma once

namespace base {
	/**
	 * @brief A utility type that has only one possible value.
	 * @note It is useful for example as compile-time marker members inside classes.
	 */
	struct Monostate final {};
}
