/**
 * @file parsing_result.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief This is a container class for the result of a parsing.
 */

#pragma once
#include "parameter.hpp"

#include <base/anycast.hpp>
#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>

#include <any>
#include <string>
#include <unordered_set>
#include <vector>

namespace clap {
	class Clap;

	struct ParsedValue final {
		std::any    value;
		std::string raw_source;
	};

	/**
	 * This class contains data parsed by clap::Clap. If obtained via Clap::parse() it holds valid
	 * and correct data in terms of clap::Clap specification. It is also possible to obtain it
	 * through some exceptions, like the HelpException, to gain some context during help message
	 * generation. In this case, you can normally access its data, but you cannot depend on
	 * clap::Clap's specification - user could have passed anything and a help flag.
	 *
	 * Throughout the docs, "X was passed" is meant to suggest the user has typed "X" into the
	 * command-line arguments of the program.
	 */
	class ParsingResult {
	public:
		ParsingResult() = default;

		ParsingResult(const ParsingResult& other) noexcept = default;
		ParsingResult(ParsingResult&& other) noexcept      = default;

		/**
		 *
		 * @param file_path path to the executed file - argv[0] from "int main" without "./".
		 * @param args merged argv[1..] into a single string
		 */
		ParsingResult(std::string file_path, std::string args);

		ParsingResult& operator=(const ParsingResult& other);

		/**
		 * @return Full path to the executed file.
		 */
		[[nodiscard]]
		const std::string& getFilePath() const;
		/**
		 * @return Provided arguments as a single string.
		 */
		[[nodiscard]]
		const std::string& getArgs() const;

		/**
		 * Inserts a flag to a collection of flags.
		 * @param parameter clap::Parameter flag object that should be inserted.
		 */
		void insertFlag(const Parameter& parameter);

		/**
		 * Inserts a value linked to the parameter.
		 * @param parameter clap::Parameter object identifying the value.
		 * @param value A value parsed by the parser.
		 */
		void insertParameterValue(const Parameter& parameter, const ParsedValue& value);

		/**
		 * Inserts a positional value.
		 * @param value A value parsed by the parser.
		 */
		void insertPositional(const ParsedValue& value);
		/**
		 * Inserts an extra value.
		 * @param value A value parsed by the default parser.
		 */
		void insertExtra(const ParsedValue& value);

		/**
		 * The main way to retrieve a value of a parameter from ParsingResult.
		 * @TODO: better explanation
		 * @tparam T Type of the returned value.
		 * @tparam N Type of the name. (std::string/char)
		 * @param name Name of the parameter identifying the value.
		 * @return An optional holding the queried value.
		 */
		template<class T, class N>
		base::Optional<T> getValue(const N& name) const {
			if_opt_some(getID(name), id) return base::anyCast<T>(id_to_value.at(id).value);
			return {};
		}

		/**
		 * An accessor to a raw string responsible for producing a value.
		 * @tparam N Type of the name.
		 * @param name Name of the parameter.
		 * @return the source characters if parameter exists and was passed.
		 */
		template<class N>
		base::Optional<std::string> getRaw(const N& name) const {
			if_opt_some(getID(name), id) return id_to_value.at(id).raw_source;
			return {};
		}

		/**
		 * An accessor the the positional value at a position. Since positional values are required,
		 * this method never fails so long the position is compliant with the clap::Clap
		 * specification used to create this object.
		 * @tparam T Type of the value.
		 * @param position Index of the positional value.
		 * @return The value at index position.
		 */
		template<class T>
		[[nodiscard]]
		T getPositional(usize position) const {
			return base::anyCast<T>(positional_values.at(position).value);
		}

		/**
		 * An accessor to the extra arguments if provided.
		 * @tparam T Type of the value returned by the default value parser.
		 * @param position Index of the value.
		 * @return An optional holding the value at a given index.
		 */
		template<class T>
		[[nodiscard]]
		base::Optional<T> getExtra(usize position) const {
			if (position < getExtraParameterCount())
				return base::anyCast<T>(extra_values[position].value);
			return {};
		}

		/**
		 * Checks if a flag was passed.
		 * @tparam N Type of the name.
		 * @param name The name of a flag.
		 * @return True if flag was passed, false otherwise.
		 */
		template<class N>
		bool isFlag(const N& name) const {
			if_opt_some(getID(name), id) return flags.contains(id);
			return false;
		}

		/**
		 * Checks if a named Parameter was passed.
		 * @tparam N Type of the name.
		 * @param name The name of the parameter.
		 * @return True if the parameter was passed, false otherwise.
		 */
		template<class N>
		bool isParam(const N& name) const {
			if_opt_some(getID(name), id) return id_to_value.contains(id);
			return false;
		}

		/**
		 * Another way to check if a Parameter was passed.
		 * @param parameter A Parameter object.
		 * @return True if the parameter was passed, false otherwise.
		 */
		[[nodiscard]]
		bool hasParam(const Parameter& parameter);

		/**
		 * @return Number of the positional parameters passed.
		 */
		[[nodiscard]]
		usize getPositionalParameterCount() const;
		/**
		 * @return Number of the extra arguments passed.
		 */
		[[nodiscard]]
		usize getExtraParameterCount() const;
		/**
		 * @return Number of the flags passed.
		 */
		[[nodiscard]]
		usize getFlagCount() const;

		/**
		 * @return Number of named parameters passed.
		 */
		[[nodiscard]]
		usize getNamedParameterCount() const;

	private:
		std::string file_path;
		std::string args;

		/**
		 * Internally, ParsingResult addresses each parameter with an id.
		 * This method is used to insert and query or just query the id of
		 * a parameter depending if the parameter has already been inserted
		 * previously or not.
		 * @param parameter The parameter to be identified.
		 * @return id
		 */
		usize insertQueryID(const Parameter& parameter);

		/**
		 * A const accessor to the id of a parameter.
		 * @param name the parameter's short name.
		 * @return Optional holding the id if parameter with
		 * according name has been inserted before or not.
		 */
		base::Optional<usize> getID(char name) const;

		/**
		 * A const accessor to the id of a parameter.
		 * @param name the parameter's long name.
		 * @return Optional holding the id if parameter with
		 * according name has been inserted before or not.
		 */
		base::Optional<usize> getID(const base::RawView& name) const;

		usize                               id_counter = 1;
		base::HashMap<char, usize>          short_names_to_id;
		base::HashMap<base::RawView, usize> long_names_to_id;

		base::HashMap<usize, ParsedValue> id_to_value;
		std::vector<ParsedValue>          positional_values;

		// Values parsed with default value parser - that is
		// they were passed additionally.
		std::vector<ParsedValue> extra_values;

		std::unordered_set<usize> flags;
	};
}
