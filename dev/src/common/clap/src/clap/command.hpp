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

		Command(std::string name, std::string description);
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
		// Sets a default value parser for this command. Default is the string parser.
		Command&& setDefaultValueParser(MBox<ValueParser> parser);

		[[nodiscard]]
		const std::string& getName() const;

		[[nodiscard]]
		const std::string& getDescription() const;

		// [[nodiscard]]
		// const base::HashMap<base::StrID, MRef<Command>>& getSubcommands() const;
		[[nodiscard]]
		const std::vector<Command>& getSubcommands() const;

		[[nodiscard]]
		const std::vector<Parameter>& getParameters() const;

		[[nodiscard]]
		const std::vector<Box<ValueParser>>& getPositionalParameters() const;

		[[nodiscard]]
		const Handler& getHandler() const;

		/**
		 * A default value parser is used to parse values, that are not directly specified
		 * in the Clap's specification.
		 * @return A pointer to the parser. Might be nullptr.
		 */
		[[nodiscard]]
		MCRef<ValueParser> getDefaultValueParser() const;


	private:
		std::string       name;
		std::string       description;
		MBox<ValueParser> default_value_parser;
		Handler           handler{};

		std::vector<Box<ValueParser>> positional_parameters;
		std::vector<Parameter>        parameters;
		std::vector<Command>          subcommands;
		// base::HashMap<base::StrID, MRef<Command>> subcommands; // TODOP: maybe it should be a map.
	};
};  // clap
