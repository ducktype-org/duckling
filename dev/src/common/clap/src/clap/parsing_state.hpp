/**
 * ParsingState related.
 * TODOP: Refactor this comment.
 */
#pragma once

#include "parsing_result.hpp"

#include "base/ints.hpp"

#include <string>

namespace {
	enum class NameType { EmptyName, ShortName, LongName };
}

namespace clap {
	/**
	 * A helper class for the method clap::Command::parse().
	 */
	class ParsingState {
	public:
		usize               parsing_position = 0;  /// Position in the args.
		std::string         args;                  /// Merged arguments.
		ParsingResult result;                /// The result of the parsing.

		ParsingState(usize argc, const char* const* argv);

		/**
		 * @brief Tries to perform parsing of a positional argument with a parser.
		 * @param parser The parser to be used.
		 */
		void parsePositional(const clap::ValueParser& parser);

		/**
		 * @brief Tires to perform parsing of an extra argument with a parser.
		 * @param parser The parser to be used.
		 */
		void parseExtra(const clap::ValueParser& parser);

		/**
		 * @brief Tries to perform parsing of a named parameter. It could be a flag or a value
		 * parameter.
		 * @param parameters All the available parameters.
		 * TODOP: Update that comment.
		 */
		void parseParameter(
			const std::vector<clap::Parameter>& local_params,
			const std::vector<clap::Parameter>& global_params
		);

		/**
		 * @brief Peeks at the token starting from `parsing_position` to the first space and returns
		 * it. Doesn't move the `parsing_position`.
		 * @return The next token.
		 */
		std::string peekToken();

		/**
		 * @brief Checks whether there is more arguments to parse in the args string.
		 * @return Are there any arguments to parse.
		 */
		bool hasMoreArgs() const;

		/**
		 * @brief Consumes one token starting from the `parsing_position` until the first space.
		 */
		void consumeToken();

	private:
		/**
		 * @brief Tries to parse a name of the parameter.
		 * @return The name and it's type: EmptyName, ShortName or LongName.
		 */
		std::pair<std::string, NameType> parseName();

		/**
		 * @brief Iterates over a collection of parameters and matches a name to the parameter,
		 * then performs parsing using it's ValueParser if provided.
		 * @param parameters All of the available parameters.
		 * @param param_name The parsed name.
		 * @param name_type Type of the parsed name.
		 */
		bool findParameterAndParse(
			const std::vector<clap::Parameter>& parameters,
			const std::string&                  param_name,
			NameType                            name_type
		);

		/**
		 * @brief Parses the value or inserts a flag to the ParsingResult if no ValueParer provided.
		 * @param parameter The parameter with mathing name.
		 * @param name The name of the parameter.
		 */
		void parseWithParameter(const clap::Parameter& parameter, const std::string& name);

		/**
		 * @brief Tries to perform parsing with a value parser. Returns an empty optional if no more
		 * characters are left.
		 * @param parser The parser to be used.
		 * @return An optionally parsed value.
		 */
		base::Optional<clap::ParsedValue> parseValueWithParser(const clap::ValueParser& parser);
	};
}
