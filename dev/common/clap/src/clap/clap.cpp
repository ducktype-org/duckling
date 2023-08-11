#include <iostream>
#include <sstream>
#include "clap.hpp"
#include "error.hpp"

namespace clap {
	Config::Config() {
		add(ParameterConfig("help")
			.short_name('h')
			.description("Show this help message"));
	}

	option<const ParameterConfig&> Config::get_parameter_config(base::StrId long_name) {
		for (const ParameterConfig& parameter : parameters) {
			if (parameter.get_long_name()
				.map([long_name](base::StrId x) { return long_name == x; } )
				.value_or(false)) {
				return some<const ParameterConfig&>(parameter);
			}
		}
		return none<const ParameterConfig&>();
	}
	option<const ParameterConfig&> Config::get_parameter_config(char short_name) {
		for (const ParameterConfig& parameter : parameters) {
			if (parameter.get_short_name()
				.map([short_name](char x) { return short_name == x; } )
				.value_or(false)) {
				return some<const ParameterConfig&>(parameter);
			}
		}
		return none<const ParameterConfig&>();
	}

	Config& Config::add(ParameterConfig parameter_config) {
		parameters.push_back(std::forward<ParameterConfig&&>(parameter_config));
		return *this;
	}
	Config& Config::add_positional(const base::RawView& parameter_name) {
		positional_parameters_names.push_back(base::StrId(parameter_name));
		return *this;
	}
	
	result<ParametersMap, ClapParsingError> Config::parse_internal(CLIArgs args) {
		ParametersMap out;
		usize id = 0;
		i32 i = 1;
		usize positional_parameters_count = 0;
		usize found_required_parameters_count = 0;

		auto add_param = [&i, &out, &id, &found_required_parameters_count, &args](auto parameter_name, option<const ParameterConfig&> parameter_option) -> result<void, ClapParsingError> {
			if (parameter_option.has_error()) {
				return failure(UnexpectedParameter(parameter_name));
			}
			const ParameterConfig& config = parameter_option.value();
			if (out.contains(parameter_name)) {
				return failure(DuplicatedParameter(config));
			}

			const auto& long_name = config.get_long_name();
			if (long_name.has_value()) {
				out.long_names_to_id.put(long_name.value(), id);
			}
			const auto& short_name = config.get_short_name();
			if (short_name.has_value()) {
				out.short_names_to_id.put(short_name.value(), id);
			}
			if (config.requires_argument()) {
				if (i + 1 == args.argc) {
					if (config.is_required()) {
						return failure(MissingParameterArgument(config));
					}
					else {
						out.parameters.put(id, config.get_default_value().value());
					}
				}
				else {
					base::RawView param = args.argv[i + 1];
					if (!config.is_required() && param.size() >= 1 && param[0] == byte('-')) {
						out.parameters.put(id, config.get_default_value().value());
					} else {
						i++;
						out.parameters.put(id, base::StrId(param));
					}
				}
			} else {
				out.flags.insert(id);
			}
			if (config.is_required()) {
				found_required_parameters_count++;
			}
			id++;
			return result<void, ClapParsingError>();
		};
		
		for (; i < args.argc; i++) {
			base::RawView what = args.argv[i];
			if (what.size() > 2 && what[0] == byte('-') && what[1] == byte('-')) {
				base::StrId parameter_name = base::StrId(what.subSuffix(2));
				if (parameter_name.str() == "help") {
					return failure(HelpMessage{});
				}
				auto result = add_param(parameter_name, get_parameter_config(parameter_name));
				if (result.has_error()) {
					return failure(result.error());
				}
			} else if (what.size() == 2 && what[0] == byte('-')) {
				byte parameter_name = what[1];
				if (parameter_name == byte('h')) {
					return failure(HelpMessage{});
				}
				auto result = add_param(parameter_name, get_parameter_config(char(parameter_name)));
				if (result.has_error()) {
					return failure(result.error());
				}
			} else {
				if (positional_parameters_count == positional_parameters_names.size()) {
					return failure(PositionalParametersCountError{positional_parameters_names.size(), positional_parameters_count + 1});
				}
				out.parameters.put(positional_parameters_names[positional_parameters_count], base::StrId(what));
				positional_parameters_count++;
			}
		}

		for (const ParameterConfig& parameter : parameters) {
			if (parameter.is_required() && !(
				parameter.get_long_name()
					.map([&out](const base::StrId& long_name) { return out.contains(long_name); })
					.value_or(false) ||
				parameter.get_short_name()
					.map([&out](const char& short_name) { return out.contains(short_name); })
					.value_or(false))) {
				return failure(ClapParsingError{MissingRequiredParameter{parameter}});
			}
		}

		if (positional_parameters_count != positional_parameters_names.size()) {
			return failure(PositionalParametersCountError{positional_parameters_names.size(), positional_parameters_count});
		}

		return out;
	}
	
	ParametersMap Config::parse(CLIArgs args) {
		file_name = base::StrId(args.argv[0]);
		auto result = parse_internal(args);
		if (result.has_error()) {
			std::visit([](auto&& arg){
				std::cout << arg.print() << "\n";
			}, result.error());
			std::cout << help_message() << "\n";
			exit(0);
		}
		return result.value();
	}

	std::string Config::help_message() const {
		std::stringstream builder;

		builder << base::strConcat("Help message of ", file_name, "\n");
		builder << base::strConcat("Usage: ", file_name, " ");
		for (auto& param : parameters) {
			builder << param.short_help_message() << " ";
		}
		builder << "\n";
		for (auto& param : parameters) {
			builder << param.long_help_message() << "\n";
		}

		return builder.str();
	}
}
