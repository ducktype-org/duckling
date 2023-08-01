#include <vector>
#include <set>
#include <any>
#include <base/exceptions.hpp>
#include <base/raw_view.hpp>
#include <base/smart_pointers.hpp>

#include "config.hpp"
#include "params_configuration.hpp"
#include "parsing_result.hpp"

namespace config {

	class ArgParser {
		base::HashMap<base::RawView, size_t> long_options_map;
		base::HashMap<base::RawView, size_t> short_options_map;
		ConfigOptions options;

		std::set<size_t> option_was;
		base::Map<size_t, base::RawView> option_values;

		std::vector<base::RawView> values;

		void reset() {
			long_options_map.clear();
			short_options_map.clear();
			option_was.clear();
			option_values.clear();
			values.clear();

			for (int i = 0; i < options.options.size(); i++) {
				long_options_map.put(options.options[i].long_version, i);

				if (options.options[i].has_short) {
					short_options_map.put(options.options[i].short_version, i);
				}
			}
		}

	public:
		ArgParser(ConfigOptions&& opts): options(std::move(opts)) {
			// @TODO: validate options
		}

		ParsingResult parse(const std::vector<base::RawView>& arg_values) {
			reset();
			const size_t arg_count = arg_values.size();

			for (int i = 0; i < arg_count; i++) {

				base::RawView what = arg_values[i];
				size_t option_index = -1;

				if (what.size() >= 2 and what[0] == byte('-') and what[1] == byte('-')) {
					// long option
					auto opt_name = what.subSuffix(2);
					if (long_options_map.find(opt_name) == long_options_map.end()) {
						// @TODO: error
						continue;
					}
					option_index = long_options_map.at(opt_name);

				}
				else if (what.size() >= 1 and what[0] == byte('-')) {
					// short option
					auto opt_name = what.subSuffix(1);
					if (short_options_map.find(opt_name) == short_options_map.end()) {
						// @TODO: error
						continue;
					}
					option_index = short_options_map.at(opt_name);
				}
				else {
					values.push_back(what);
					continue;
				}

				// @TODO: repeated options?
				RIFT_ASSERT(option_index != -1, "CLI parsing critical error");

				option_was.insert(option_index);

				if (options.options[option_index].has_param) {
					if (options.options[option_index].param_type == ParamType::Always) {
						i++;
						// @TODO: make decent error handling:
						if (i >= arg_count) { throw base::Panic("Config", "No value provided for option"); }
						else {
							option_values.put(option_index, arg_values[i]);
						}
					}
					else {
						throw base::NotYetImplemented("Option ParamType different then Always");
					}
				}
			}

			ParsingResult out;

			out.non_option_values = std::move(values);

			for (const auto& [name, index]: long_options_map) {
				out.all_options.insert(name);
				if (option_was.contains(index)) {
					out.option_was.insert(name);

					if (option_values.contains(index)) {
						out.name_to_raw_value.put(name, option_values[index]);
						out.name_to_value.put(
							name,
							options.options[index].value_parser->parse(option_values[index])
						);
					}
				}
			}

			return out;
		}
	};


	std::vector<base::RawView> cliArgsToVec(CLIArgs args) {
		std::vector<base::RawView> out;

		// we skip first because its a program name
		for (size_t i = 1; i < args.argc; i++) {
			out.emplace_back(args.argv[i]);
		}

		return out;
	}

	ParsingResult parse(ConfigOptions&& config, CLIArgs args) {
		return parse(std::move(config), cliArgsToVec(args));
	}

	ParsingResult parse(ConfigOptions&& config, const std::vector<base::RawView>& args) {
		ArgParser parser{std::move(config)};
		return parser.parse(args);
	}
}
