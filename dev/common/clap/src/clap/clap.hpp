#pragma once

#include "cli_args.hpp"
#include "error.hpp"
#include "parameter_config.hpp"
#include "parameters_map.hpp"
#include <base/option.hpp>
#include <base/string_id.hpp>
#include <string>
#include <vector>

namespace clap {
	class Config {
	private:
		base::StrId file_name;

		std::vector<ParameterConfig> parameters;
		usize                        required_parameters_count;

		std::vector<base::StrId> positional_parameters_names;

		result<ParametersMap, ClapParsingError> parse_internal(CLIArgs args);

		option<const ParameterConfig &> get_parameter_config(base::StrId long_name);
		option<const ParameterConfig &> get_parameter_config(char short_name);

	public:
		Config();

		Config &add(ParameterConfig parameter_config);
		Config &add_positional(const base::RawView &parameter_name);

		ParametersMap parse(CLIArgs args);

		std::string help_message() const;
	};
}
