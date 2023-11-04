/**
 * @file parsing_result.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "parsing_result.hpp"
#include "clap.hpp"

namespace clap {
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
		usize id = insertQueryId(parameter);
		if (!id_to_value.contains(id)) id_to_value.put(id, {});
		id_to_value.at(id).push_back(value);
	}

	void ParsingResult::insertPositional(const ParsedValue& value) {
		positional_values.push_back(value);
	}

	base::Optional<usize> ParsingResult::queryId(const ClapParameter& parameter) const {
		if_opt_some(parameter.getShortName(), name) return short_names_to_id.at(name);
		if_opt_some(parameter.getLongName(), name) return long_names_to_id.at(name);
		return {};
	}

	bool ParsingResult::hasParam(const ClapParameter& parameter) const {
		return queryId(parameter)
		    .map([this](auto id) {
				if (flags.contains(id)) return true;
				if (id_to_value.contains(id)) return true;
				return false;
			})
		    .value_or(false);
	}


}
