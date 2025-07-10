#pragma once

#include <clap/clap.hpp>

/**
 * @brief Generate Clap instance with all standard compiler parameters
 * (i.e. parameters available in every command).
 * See source code for list of parameters.
 * @return clap::Clap
 */
clap::Clap standardOptions();
