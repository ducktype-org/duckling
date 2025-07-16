#pragma once

#include "exceptions.hpp"
#include "parameter.hpp"
#include "value_parser.hpp"

#include "base/maps.hpp"
#include "base/optional.hpp"
#include "base/string_id.hpp"
#include <base/ints.hpp>

#include <functional>
#include <string>
#include <vector>

namespace clap {
	// Forward declaration.
	class ParsingResult;
	class ParsingState;

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

		[[nodiscard]]
		const std::vector<Command>& getSubcommands() const;

		[[nodiscard]]
		const std::vector<Parameter>& getParameters() const;

		[[nodiscard]]
		const std::vector<Box<ValueParser>>& getPositionalParameters() const;

		[[nodiscard]]
		const Handler& getHandler() const;

		[[nodiscard]] bool isLeaf() const;

		[[nodiscard]]
		base::Optional<CRef<Command>> getSubcommand(const std::string& subcommand_name) const;


		/**
		 * A default value parser is used to parse values, that are not directly specified
		 * in the Clap's specification.
		 * @return A pointer to the parser. Might be nullptr.
		 */
		[[nodiscard]]
		MCRef<ValueParser> getDefaultValueParser() const;

		// Main parsing function for the command.
		// TODOP: Better docs.
		void parse(ParsingState& state) const;


	private:
		std::string name;
		std::string description;
		// Is it a leaf in the tree. Has no subcommands.
		bool              is_leaf;
		MBox<ValueParser> default_value_parser;
		Handler           handler{};

		std::vector<Box<ValueParser>> positional_parameters;
		std::vector<Parameter>        parameters;
		std::vector<Command>          subcommands;
	};
};  // clap
