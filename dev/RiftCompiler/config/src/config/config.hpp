/**
 * @file config.hpp
 * @brief utility functions for handling set of compiler options not associated with concrete task
 * @todo: add a way to customize what options are included and how.
 */

#pragma once

#include <clap/clap.hpp>

namespace config {

	/**
	 * @brief Generate Clap instance with all standard compiler parameters.
	 * See source code for list of parameters.
	 * @return clap::Clap
	 */
	clap::Clap standardOptions();

	/**
	 * @brief Parses arguments with @p clap and performs
	 * configuration of the program based on standard options.
	 * @note it assumes that @p clap has parameters
	 * added by standardOptions.
	 */
	clap::ParsingResult configureWith(clap::Clap& clap, clap::CLIArgs args);
}
