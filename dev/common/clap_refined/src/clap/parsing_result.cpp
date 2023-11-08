/**
 * @file parsing_result.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "parsing_result.hpp"

namespace clap {
	//	ParsingResult::ParsingResult(ParsingResult& other) noexcept:
	//		  file_path(other.file_path),
	//		  args(other.args),
	//		  id_counter(other.id_counter),
	//		  positional_values(other.positional_values),
	//		  extra_values(other.extra_values),
	//		  flags(other.flags) {
	//		short_names_to_id.merge(other.short_names_to_id);
	//		long_names_to_id.merge(other.long_names_to_id);
	//		id_to_value.merge(other.id_to_value);
	//	}

	ParsingResult::ParsingResult(std::string file_path, std::string args):
		  file_path(std::move(file_path)),
		  args(std::move(args)) {}

	const std::string& ParsingResult::getFilePath() const { return file_path; }

	const std::string& ParsingResult::getArgs() const { return args; }

	base::Optional<usize> ParsingResult::getId(char name) const {
		if (short_names_to_id.contains(name)) return short_names_to_id.at(name);
		return {};
	}

	base::Optional<usize> ParsingResult::getId(const base::RawView& name) const {
		if (long_names_to_id.contains(name)) return long_names_to_id.at(name);
		return {};
	}

	usize ParsingResult::insertQueryId(const ClapParameter& parameter) {
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

	void ParsingResult::insertFlag(const ClapParameter& parameter) {
		flags.insert(insertQueryId(parameter));
	}

	void ParsingResult::insertParameterValue(
		const ClapParameter& parameter, const ParsedValue& value
	) {
		id_to_value.put(insertQueryId(parameter), value);
	}

	void ParsingResult::insertPositional(const ParsedValue& value) {
		positional_values.push_back(value);
	}

	bool ParsingResult::hasParam(const ClapParameter& parameter) {
		auto id = insertQueryId(parameter);
		if (flags.contains(id)) return true;
		if (id_to_value.contains(id)) return true;
		return false;
	}

	void ParsingResult::insertExtra(const ParsedValue& value) { extra_values.push_back(value); }

	usize ParsingResult::getPositionalParameterCount() const { return positional_values.size(); }

	usize ParsingResult::getExtraParameterCount() const { return extra_values.size(); }

	usize ParsingResult::getFlagCount() const { return flags.size(); }

	usize ParsingResult::getNamedParameterCount() const { return id_to_value.size(); }

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

}
