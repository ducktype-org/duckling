#pragma once
#include "command.hpp"

namespace clap {
	Command::Command(std::string name, std::string description):
		  name(std::move(name)),
		  description(std::move(description)),
		  default_value_parser(StringParser::make()) {}

	// Adds a specific option/flag for this command.
	Command&& Command::add(Parameter&& param) {
		parameters.push_back(std::move(param));
		return std::move(*this);
	}

	// Adds a positional argument.
	Command&& Command::addPositional(Box<ValueParser> parser) {
		positional_parameters.push_back(std::move(parser));
		return std::move(*this);
	}

	// Adds a subcommand to this command.
	Command&& Command::addSubcommand(Command&& sub_command) {
		subcommands.push_back(std::move(sub_command));
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

	// TODOP: Maybe this should be a map.
	// const base::HashMap<base::StrID, MRef<Command>>& Command::getSubcommands() const { return
	// subcommands; }
	const std::vector<Command>& Command::getSubcommands() const { return subcommands; }

	const std::vector<Parameter>& Command::getParameters() const { return parameters; }

	const std::vector<Box<ValueParser>>& Command::getPositionalParameters() const {
		return positional_parameters;
	}

	const Command::Handler& Command::getHandler() const { return handler; }

	MCRef<ValueParser> Command::getDefaultValueParser() const { return default_value_parser.ref(); }
}
