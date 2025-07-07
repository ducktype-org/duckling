#pragma once

#include "parameter.hpp"
#include "parsing_result.hpp"
#include "value_parser.hpp"

#include "base/maps.hpp"
#include "base/string_id.hpp"
#include <base/ints.hpp>

#include <functional>
#include <string>
#include <vector>

namespace clap {

	/**
	 * @brief Structure representing a single command.
	 * Can have subcommands and be a subcommand.
	 */
	class Command {
	public:
		using Handler = std::function<int(const ParsingResult&)>;

		Command(const std::string& name, const std::string& description);
		Command(Command&&) noexcept            = default;
		Command& operator=(Command&&) noexcept = default;

		// Adds a specific option/flag for this command.
		Command&& add(Parameter&& param);
		// Adds a positional argument.
		Command&& addPositional(Box<ValueParser> parser);
		// Adds a subcommand to this command.
		Command&& addSubcommand(Command&& sub_command);
		// Sets a function to be run.
		Command&& setHandler(Handler handler);

		const std::string& getName() const { return name; }

		const std::string& getDescription() const { return description; }

		const base::HashMap<base::StrID, MRef<Command>>& getSubcommands() const {
			return subcommands;
		}

		const std::vector<Parameter>& getParameters() const { return parameters; }

		const std::vector<Box<ValueParser>>& getPositionalParameters() const {
			return positional_parameters;
		}

		const Handler& getHandler() const { return handler; }
	private:
		std::string name;
		std::string description;
		Handler     handler{};

		MBox<ValueParser>                         default_value_parser;
		std::vector<Box<ValueParser>>             positional_parameters;
		std::vector<Parameter>                    parameters;
		base::HashMap<base::StrID, MRef<Command>> subcommands;
	};
};  // clap
