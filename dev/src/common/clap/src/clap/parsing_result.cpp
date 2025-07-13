/**
 * @file parsing_result.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "parsing_result.hpp"

#include <iostream>

namespace clap {
	ParsingResult::ParsingResult(std::string file_path, std::string args):
		  file_path(std::move(file_path)),
		  args(std::move(args)) {}

	const std::string& ParsingResult::getFilePath() const { return file_path; }

	const std::string& ParsingResult::getArgs() const { return args; }

	base::Optional<usize> ParsingResult::getID(char name) const {
		if (short_names_to_id.contains(name)) return short_names_to_id.at(name);
		return {};
	}

	base::Optional<usize> ParsingResult::getID(const base::RawView& name) const {
		if (long_names_to_id.contains(name)) return long_names_to_id.at(name);
		return {};
	}

	usize ParsingResult::insertQueryID(const Parameter& parameter) {
		usize id = 0;
		if_opt_some(parameter.getShortName(), name) {
			if (short_names_to_id.contains(name))
				id = short_names_to_id.at(name);
			else {
				short_names_to_id.put(name, id_counter);
				id = id_counter++;
			}
		}
		if_opt_some(parameter.getLongName(), name) {
			if (id != 0) {
				if (!long_names_to_id.contains(name)) long_names_to_id.put(name, id);
			} else {
				// id == 0 => parameter.getShortName() has no value
				if (long_names_to_id.contains(name))
					id = long_names_to_id.at(name);
				else {
					long_names_to_id.put(name, id_counter);
					id = id_counter++;
				}
			}
		}
		return id;
	}

	void ParsingResult::insertFlag(const Parameter& parameter) {
		flags.insert(insertQueryID(parameter));
	}

	void ParsingResult::insertParameterValue(const Parameter& parameter, const ParsedValue& value) {
		id_to_value.put(insertQueryID(parameter), value);
	}

	void ParsingResult::insertPositional(const ParsedValue& value) {
		positional_values.push_back(value);
	}

	bool ParsingResult::hasParam(const Parameter& parameter) {
		auto id = insertQueryID(parameter);
		if (flags.contains(id)) return true;
		if (id_to_value.contains(id)) return true;
		return false;
	}

	void ParsingResult::insertExtra(const ParsedValue& value) { extra_values.push_back(value); }

	void ParsingResult::setMatchedCommand(CRef<Command> cmd, std::vector<CRef<Command>> path) {
		command      = cmd;
		command_path = std::move(path);
	}

	usize ParsingResult::getPositionalParameterCount() const { return positional_values.size(); }

	usize ParsingResult::getExtraParameterCount() const { return extra_values.size(); }

	usize ParsingResult::getFlagCount() const { return flags.size(); }

	usize ParsingResult::getNamedParameterCount() const { return id_to_value.size(); }

	CRef<Command> ParsingResult::getMatchedCommand() const {
		// TODOP: What about empty optionals?
		return command.value();
	}

	const std::vector<CRef<Command>>& ParsingResult::getCommandPath() const { return command_path; }

	ParsingResult& ParsingResult::operator=(const ParsingResult& other) {
		file_path         = other.file_path;
		args              = other.args;
		id_counter        = other.id_counter;
		positional_values = other.positional_values;
		extra_values      = other.extra_values;
		flags             = other.flags;
		for (const auto& elem: other.short_names_to_id)
			short_names_to_id.put(elem.first, elem.second);
		for (const auto& elem: other.long_names_to_id)
			long_names_to_id.put(elem.first, elem.second);
		for (const auto& elem: other.id_to_value) id_to_value.put(elem.first, elem.second);
		return *this;
	}

	void ParsingResult::dPrint() {
		std::cout << "ParsingResult dump:\n";
		std::cout << "  File path: " << file_path << "\n";
		std::cout << "  Args: " << args << "\n";

		// Matched command path
		std::cout << "  Command path: ";
		if (command_path.empty()) {
			std::cout << "(none)\n";
		} else {
			for (size_t i = 0; i < command_path.size(); ++i) {
				std::cout << command_path[i]->getName();
				if (i + 1 < command_path.size()) std::cout << " -> ";
			}
			std::cout << "\n";
		}

		// Flags
		std::cout << "  Flags (" << flags.size() << "): ";
		if (flags.empty()) {
			std::cout << "(none)\n";
		} else {
			for (auto id: flags) std::cout << id << " ";
			std::cout << "\n";
		}

		// Named parameters
		std::cout << "  Named parameters (" << id_to_value.size() << "):\n";
		for (const auto& [id, val]: id_to_value) {
			std::cout << "    id=" << id << ", value=";
			try {
				std::cout << std::any_cast<std::string>(val.value);
			} catch (...) { std::cout << "<non-string>"; }
			std::cout << ", raw='" << val.raw_source << "'\n";
		}

		// Positional values
		std::cout << "  Positional values (" << positional_values.size() << "):\n";
		for (size_t i = 0; i < positional_values.size(); ++i) {
			std::cout << "    [" << i << "]: ";
			try {
				std::cout << std::any_cast<std::string>(positional_values[i].value);
			} catch (...) { std::cout << "<non-string>"; }
			std::cout << ", raw='" << positional_values[i].raw_source << "'\n";
		}

		// Extra values
		std::cout << "  Extra values (" << extra_values.size() << "):\n";
		for (size_t i = 0; i < extra_values.size(); ++i) {
			std::cout << "    [" << i << "]: ";
			try {
				std::cout << std::any_cast<std::string>(extra_values[i].value);
			} catch (...) { std::cout << "<non-string>"; }
			std::cout << ", raw='" << extra_values[i].raw_source << "'\n";
		}
	}
}
