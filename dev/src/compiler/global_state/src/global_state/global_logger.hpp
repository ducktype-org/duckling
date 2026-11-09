// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

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
