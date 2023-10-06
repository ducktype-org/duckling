#pragma once

#include <filesystem/file.hpp>
#include <printer/printer.hpp>
#include <string>

#include "value_parser.hpp"
#include "parsing_result.hpp"
#include "params_configuration.hpp"
#include "cli_args.hpp"

// @TODO: perform memory copy

namespace config {

	/**
	 * @brief This function does not copy memory,
	 * meaning that argc and argv memory should be
	 * valid as long as cliArgsToVec return value
	 * is valid
	 */
	std::vector<base::RawView> cliArgsToVec(CLIArgs args);

	/**
	 * @brief This function does not copy memory,
	 * meaning that argc and argv memory should be
	 * valid as long as ParsingResult
	 * is valid
	 */
	ParsingResult parse(ConfigOptions&& config, CLIArgs args);


	/**
	 * @brief This function does not copy memory,
	 * meaning that argc and argv memory should be
	 * valid as long as ParsingResult
	 * is valid
	 */
	ParsingResult parse(ConfigOptions&& config, const std::vector<base::RawView>& args);

}
