#include "module_flags.hpp"

#include <rang.hpp>

namespace term_ui {
	bool use_color = true;

	void configureColoring(bool use) {
		use_color = use;
		// Always force rang so it emits escape codes even when printing
		// to stringstreams (which are not recognized as TTYs).
		rang::setControlMode(rang::control::Force);
	}
}
