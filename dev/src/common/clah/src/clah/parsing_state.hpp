#pragma once

#include "parsing_result.hpp"

#include <string>

namespace clah {
	enum class NameType { EmptyName, ShortName, LongName };

	/**
	 * @brief A helper for the method clah::Clah::parse().
	 * A class containing the parsing state of the command line arguments.
	 */
	class ParsingState {
	public:
		usize                    current_word         = 0;  /// Current word index.
		usize                    inside_word_position = 0;  /// Position inside the current word.
		std::vector<std::string> words;                     /// Arguments.
		ParsingResult            result;                    /// The result of the parsing.

		/**
		 * @brief Construct a ParsingState object from an argument string
		 * @param args Arguments to parse. Assumes the string contains only the arguments. The
		 * string should not begin with the program name.
		 */
		ParsingState(const std::string& args);

		/**
		 * @brief Construct a ParsingState object from command line arguments.
		 * Assumes argv contains the program name at the 0 index.
		 */
		ParsingState(usize argc, const char* const* argv);

		/**
		 * @brief Tries to perform parsing of a positional argument with a parser.
		 * @param parser The parser to be used.
		 */
		void parsePositional(const clah::ValueParser& parser);

		/**
		 * @brief Tires to perform parsing of an extra argument with a parser.
		 * @param parser The parser to be used.
		 */
		void parseExtra(const clah::ValueParser& parser);

		/**
		 * @brief Tries to perform parsing of a named parameter. It could be a flag or a value
		 * parameter.
		 * @param parameters All the available parameters.
		 */
		void parseParameter(const std::vector<clah::Parameter>& params);


		/**
		 * @brief The character at the current position.
		 */
		char frontChar() const;

		/**
		 * @brief The word string from the current position within the word until the end of the word.
		 */
		std::string frontWord() const;

		/**
		 * @brief Checks whether there is more arguments to parse in the args string.
		 * @return Are there any arguments to parse.
		 */
		bool isEnd() const;

		/**
		 * @brief Advance the position by one character. If the end of the current word is reached,
		 * move to the next word.
		 */
		void advanceChar() {
			inside_word_position++;
			if (inside_word_position >= words[current_word].size()) {
				current_word++;
				inside_word_position = 0;
			}
		}

		/**
		 * @brief Advances the position until a character satisfying the condition is reached.
		 */
		size_t advanceUntilInWord(std::predicate<char> auto condition) {
			size_t count      = 0;
			size_t start_word = current_word;
			while (current_word == start_word && !condition(frontChar())) {
				advanceChar();
				count++;
			}
			return count;
		}

		/**
		 * @brief Sets the position to the beginning of the next word.
		 */
		void advanceWord() {
			current_word++;
			inside_word_position = 0;
		}

		/**
		 * @brief Returns the current position in the merged args string. Used for error messages.
		 */
		size_t currentPosition() const;

		/**
		 * @brief Get the merged view of the arguments. Used for error messages.
		 */
		std::string mergedArguments() const;

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
			const std::vector<clah::Parameter>& parameters,
			const std::string&                  param_name,
			NameType                            name_type
		);

		/**
		 * @brief Parses the value or inserts a flag to the ParsingResult if no ValueParer provided.
		 * @param parameter The parameter with mathing name.
		 * @param name The name of the parameter.
		 */
		void parseWithParameter(const clah::Parameter& parameter, const std::string& name);

		/**
		 * @brief Tries to perform parsing with a value parser. Throws an error on exception
		 * @param parser The parser to be used.
		 * @return A parsed value.
		 */
		clah::ParsedValue parseValueWithParser(const clah::ValueParser& parser);
	};
}
