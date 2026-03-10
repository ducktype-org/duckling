#include <diagnostic_interactive/logger_fwd.hpp>
#include <base/pointers/ref.hpp>
#include <base/pointers/box.hpp>

namespace global_state {

	/**
	 * Returns a global dia-int logger that can be used outside query framework.
	 */
	Ref<dia_int::Logger> getRootCollection();

	/**
	 * Returns true if the global logger has been set.
	 */
	bool hasGlobalLogger();

	namespace setters {
		/**
		 * This should only be called by the driver.
		 */
		void setGlobalLogger(Box<dia_int::Logger>);
	}
}
