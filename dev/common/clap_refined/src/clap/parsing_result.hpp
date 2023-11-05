/**
 * @file parsing_result.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once
#include <string>
#include <utility>
#include <vector>
#include <any>
#include <unordered_set>
#include "base/ints.hpp"
#include "base/optional.hpp"
#include "base/maps.hpp"
#include "clap_parameter.hpp"

namespace clap {
	class Clap;

	struct ParsedValue {
		std::any    value;
		std::string raw_source;
	};

	class ParsingResult {
	public:
		ParsingResult(std::string file_path, std::string args):
			  file_path(std::move(file_path)),
			  args(std::move(args)) {}

		[[nodiscard]]
		const std::string& getFilePath() const;
		[[nodiscard]]
		const std::string& getArgs() const;

		void insertFlag(const ClapParameter& parameter);
		void insertParameterValue(const ClapParameter& parameter, const ParsedValue& value);
		void insertPositional(const ParsedValue& value);
		void insertExtra(const ParsedValue& value);

		template<class T, class N>
		base::Optional<T> getValue(const N& name) const {
			if_opt_some(getId(name), id) return std::any_cast<T>(id_to_value.at(id).value);
			return {};
		}

		template<class N>
		base::Optional<std::string> getRaw(const N& name) const {
			if_opt_some(getId(name), id) return id_to_value.at(id).raw_source;
			return {};
		}

		template<class T>
		[[nodiscard]]
		base::Optional<T> getPositional(usize position) const {
			if (position < getPositionalParameterCount())
				return std::any_cast<T>(positional_values[position].value);
			return {};
		}

		template<class T>
		[[nodiscard]]
		base::Optional<T> getExtra(usize position) const {
			if (position < getExtraParameterCount())
				return std::any_cast<T>(extra_values[position].value);
			return {};
		}

		template<class N>
		[[nodiscard]]
		bool isFlag(const N& name) const;

		[[nodiscard]]
		bool hasParam(const ClapParameter& parameter);

		[[nodiscard]]
		usize getPositionalParameterCount() const;
		[[nodiscard]]
		usize getExtraParameterCount() const;
		[[nodiscard]]
		usize getFlagCount() const;
		[[nodiscard]]
		usize getNamedParameterCount() const;

	private:
		std::string file_path;
		std::string args;

		usize insertQueryId(const ClapParameter& parameter);

		base::Optional<usize> getId(char name) const;
		base::Optional<usize> getId(const base::RawView& name) const;

		usize                               id_counter = 1;
		base::HashMap<char, usize>          short_names_to_id;
		base::HashMap<base::RawView, usize> long_names_to_id;

		base::HashMap<usize, ParsedValue> id_to_value;
		std::vector<ParsedValue>          positional_values;
		std::vector<ParsedValue> extra_values;  // Values parsed with default value parser - that is
		                                        // they were passed additionally.

		std::unordered_set<usize> flags;
	};
}
