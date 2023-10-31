#pragma once

#include <string>
#include <vector>
#include <base/string_id.hpp>
#include <base/optional.hpp>
#include "error.hpp"
#include "parameters_map.hpp"
#include "parameter_config.hpp"
#include "cli_args.hpp"

namespace clap {
	class Config {
	private:
		base::StrId file_name;

		std::vector<ParameterConfig> parameters;
		usize                        required_parameters_count;

		std::vector<base::StrId> positional_parameters_names;

		cpp::result<ParametersMap, ClapParsingError> parse_internal(CLIArgs args);

		base::Optional<const ParameterConfig&> get_parameter_config(base::StrId long_name);
		base::Optional<const ParameterConfig&> get_parameter_config(char short_name);

	public:
		Config();

		Config& add(ParameterConfig parameter_config);
		Config& add_positional(const base::RawView& parameter_name);

		ParametersMap parse(CLIArgs args);

		std::string help_message() const;
	};
}
