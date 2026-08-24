#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <diagnostic/logger_fwd.hpp>

namespace global_state {

	/**
	 * Returns a global dia-int logger that can be used outside the query framework.
	 */
	Ref<dia::Logger> getGlobalLogger();

	/**
	 * Returns true if the global logger has been set.
	 */
	bool hasGlobalLogger();

	namespace setters {
		/**
		 * This should only be called by the driver.
		 */
		void setGlobalLogger(Box<dia::Logger>);
	}
}
