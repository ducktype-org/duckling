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
		ParsingResult() = default;

		ParsingResult(const ParsingResult& other) noexcept = default;
		ParsingResult(ParsingResult&& other) noexcept      = default;

		ParsingResult(std::string file_path, std::string args);

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
			if_opt_some(getId(name), id) return castValue<T>(id_to_value.at(id).value);
			return {};
		}

		template<class N>
		base::Optional<std::string> getRaw(const N& name) const {
			if_opt_some(getId(name), id) return id_to_value.at(id).raw_source;
			return {};
		}

		template<class T>
		[[nodiscard]]
		T getPositional(usize position) const {
			return castValue<T>(positional_values[position].value);
		}

		template<class T>
		[[nodiscard]]
		base::Optional<T> getExtra(usize position) const {
			if (position < getExtraParameterCount())
				return castValue<T>(extra_values[position].value);
			return {};
		}

		template<class T>
		bool isFlag(const T& name) const {
			if_opt_some(getId(name), id) return flags.contains(id);
			return false;
		}

		template<class T>
		bool isParam(const T& name) const {
			if_opt_some(getId(name), id) return id_to_value.contains(id);
			return false;
		}

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

		ParsingResult& operator=(const ParsingResult& other);

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

		// Values parsed with default value parser - that is
		// they were passed additionally.
		std::vector<ParsedValue> extra_values;

		std::unordered_set<usize> flags;

		template<class T>
		T castValue(const std::any& value) const {
			try {
				return std::any_cast<T>(value);
			} catch (std::bad_any_cast&) {
				throw base::LogicError(base::strConcat(
					"Bad any_cast: Value is of ",
#ifndef __GNUC__
					"type : \"",
					value.type().name(),  // This function works very poorly in gcc - displays only
				                          // the first letter of the type - for i64 == 'l'
					"\" instead of \"",
#else
					"different type than \"",
#endif
					base::type_name<T>(),
					"\""
				));
			}
		}
	};
}
