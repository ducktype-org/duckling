#include "command.hpp"

#include "clap/exceptions.hpp"
#include "parsing_state.hpp"
// #include "parsing_result.hpp"

#include "base/optional.hpp"

#include <string>

/**
 * Basic helper functions.
 */
namespace {
	/**
	 * @brief Checks if a token represents a negative number.
	 * @param token The token to check.
	 * @return True if it's a negative number.
	 */
	bool isNegativeNumber(const std::string& token) {
		if (token.empty() || token[0] != '-') return false;
		if (token.length() == 1) return false;

		// Check if all of the chars are digits and contain at most one dot.
		bool has_dot = false;
		for (usize i = 1; i < token.length(); ++i) {
			if (!std::isdigit(token[i])) {
				if (token[i] == '.' && !has_dot)
					has_dot = true;
				else
					return false;
			}
		}
		return true;
	}
}

namespace clap {
	Command::Command(std::string name, std::string description):
		  name(std::move(name)),
		  description(std::move(description)),
		  is_leaf(true),
		  default_value_parser(StringParser::make()) {}

	// Adds a subcommand to this command.
	Command&& Command::addSubcommand(Command&& sub_command) {
		if (!positional_parameters.empty())
			throw clap::exceptions::CoexistingPositionalAndSubcommand(name);
		is_leaf = false;

		// Check for duplicates.
		// TODOP: Map.
		for (const auto& subcmd: subcommands)
			if (subcmd.getName() == sub_command.getName())
				throw clap::exceptions::DuplicateSubcommand(sub_command.getName(), name);

		subcommands.emplace_back(std::move(sub_command));
		return std::move(*this);
	}

	// Adds a specific option/flag for this command.
	Command&& Command::add(Parameter&& param) {
		parameters.push_back(std::move(param));
		return std::move(*this);
	}

	// Adds a positional argument.
	Command&& Command::addPositional(Box<ValueParser> parser) {
		if (!is_leaf) throw clap::exceptions::CoexistingPositionalAndSubcommand(name);
		positional_parameters.push_back(std::move(parser));
		return std::move(*this);
	}

	// Sets a function to be run.
	Command&& Command::setHandler(Handler handl) {
		handler = std::move(handl);
		return std::move(*this);
	}

	// Sets a default parser for parsing arguments if none where specified.
	Command&& Command::setDefaultValueParser(MBox<ValueParser> parser) {
		default_value_parser = std::move(parser);
		return std::move(*this);
	}

	const std::string& Command::getName() const { return name; }

	const std::string& Command::getDescription() const { return description; }

	const std::vector<Command>& Command::getSubcommands() const { return subcommands; }

	const std::vector<Parameter>& Command::getParameters() const { return parameters; }

	const std::vector<Box<ValueParser>>& Command::getPositionalParameters() const {
		return positional_parameters;
	}

	const Command::Handler& Command::getHandler() const { return handler; }

	bool Command::isLeaf() const { return is_leaf; }

	base::Optional<CRef<Command>> Command::getSubcommand(const std::string& subcommand_name) const {
		// TODOP: Map.
		for (const auto& cmd: subcommands)
			if (cmd.getName() == subcommand_name) return &cmd;
		return {};
	}

	MCRef<ValueParser> Command::getDefaultValueParser() const { return default_value_parser.ref(); }

	void Command::parse(ParsingState& st, const Command& root_command) const {
		// Add `this` command to the result path.
		st.result.addToPath(this);

		while (st.hasMoreArgs()) {
			const std::string& token              = st.peekToken();
			bool               is_negative_number = isNegativeNumber(token);

			// Parameter
			if (token.starts_with('-') && !is_negative_number)
				st.parseParameter(parameters, root_command.getParameters());
			else {  // Subcommand or positional.
				auto maybe_subcmd = getSubcommand(token);
				if_opt_some(maybe_subcmd, subcmd) {
					// It's a subcommand, go down the tree.
					st.consumeToken();
					subcmd->parse(st, root_command);
					return;
				}

				if (!is_leaf) {
					// If it's not subcommand (its a positional) and this subcommand is not a leaf
					// then we throw an error, since the subcommand is not specified.
					throw exceptions::UnknownSubcommand(token, name);
				}

				// If not found a "-" parse using default value parser.
				// Check if value is positional or extra.
				usize positional_count = st.result.getPositionalParameterCount();
				if (positional_count < getPositionalParameters().size()) {
					const auto& value_parser = getPositionalParameters()[positional_count];
					st.parsePositional(*value_parser);
				} else {                    // To many positional arguments.
					auto parser = getDefaultValueParser();
					if (parser == nullptr)  // Extra arguments and no default value parser.
						throw exceptions::NoDefaultValueParser((i32) st.parsing_position, st.args);
					st.parseExtra(*parser);
				}
			}
		}
	}
}
