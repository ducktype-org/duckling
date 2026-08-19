/**
 * @file exceptions.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "parsing_result.hpp"

#include <base/except/exceptions.hpp>

#include <filesystem>

namespace clah::exceptions {

	/**
	 * @brief Base struct for all the parsing-related exceptions.
	 */
	struct ClahException: public base::LogicError {
		explicit ClahException(const std::string& message);
	};

	/**
	 * @brief Raised when passed ``-h`` / ``--help``.
	 * It is not an exception per se.
	 */
	struct HelpException: public base::LogicError {
		ParsingResult parsing_result;

		explicit HelpException(ParsingResult result);
	};

	/**
	 * @brief Raised when we want to cleanly exit the program early (ex. Receiving a version flag
	 * and exiting after printing version).
	 * It is not an exception per se.
	 */
	struct SuccessExitException: public base::LogicError {
		ParsingResult parsing_result;

		explicit SuccessExitException(ParsingResult result);
	};

	/**
	 * @brief Raised by value parsers when input is malformed.
	 */
	struct ValueParsingException: public ClahException {
		/**
		 * @note start-end is an inclusive range of the input string where the error occurred.
		 */
		ValueParsingException(
			base::RawView    type,
			usize            start,
			usize            end,
			std::string_view source,
			std::string_view reason = ""
		);
	};

	/**
	 * @brief Raised by value parser clah::FileParser when a passed file does not exist.
	 */
	struct FileDoesNotExist: public ClahException {
		explicit FileDoesNotExist(const std::filesystem::path& path);
	};

	/**
	 * @brief Raised by value parser clah::FileParser when a passed path is not a regular file.
	 */
	struct NotARegularFile: public ClahException {
		explicit NotARegularFile(const std::filesystem::path& path);
	};

	/**
	 * @brief Raised when user did not pass a necessary positional argument.
	 */
	struct PositionalParameterExpected: public ClahException {
		PositionalParameterExpected(usize param_index, const std::string& param_type);
	};

	/**
	 * @brief Raised when user passes an unknown parameter.
	 */
	struct InvalidParameterName: public ClahException {
		explicit InvalidParameterName(const std::string& name);
	};

	/**
	 * @brief Raised when parameter was passed twice.
	 */
	struct DuplicatedParameter: public ClahException {
		explicit DuplicatedParameter(const std::string& name);
	};

	/**
	 * @brief Raised when user does not pass a value to a parameter.
	 */
	struct ParameterRequiresValue: public ClahException {
		ParameterRequiresValue(const std::string& name, const std::string& value_type);
	};

	/**
	 * @brief Raised when user does not pass a parameter name after ``-`` or ``--``.
	 */
	struct ExpectedParameterIdentifier: public ClahException {
		ExpectedParameterIdentifier(u64 at, std::string_view source);
	};

	/**
	 * @brief Raised when user does not pass a required parameter.
	 */
	struct MissingRequiredParameter: public ClahException {
		explicit MissingRequiredParameter(const std::string& name);
	};

	/**
	 * @brief Raised when conditional parameter's condition is not met.
	 */
	struct MissingConditionalParameter: public ClahException {
		explicit MissingConditionalParameter(const std::string& name, std::string_view why = "");
	};

	/**
	 * @brief Raised when a custom verification callback fails.
	 */
	struct CustomVerificationFailed: public ClahException {
		explicit CustomVerificationFailed(const std::string& message);
	};

	/**
	 * @brief Raised when user has specified extra arguments, but Clah has
	 * Clah::default_value_parser set to nullptr.
	 */
	struct NoDefaultValueParser: public ClahException {
		explicit NoDefaultValueParser(u64 at, std::string_view values);
	};

	/**
	 * @brief Raised when the subcommand is not specified.
	 * Fe. we finish parsing in a command which has a non-empty subcommand list.
	 */
	struct SubcommandNotSpecified: public ClahException {
		explicit SubcommandNotSpecified(const std::string& command_name);
	};

	/**
	 * @brief Raised when a subcommand with a duplicate name is added to a command.
	 * Detected at the time of definition, not parsing.
	 */
	struct DuplicateSubcommand: public ClahException {
		explicit DuplicateSubcommand(
			const std::string& duplicate_name, const std::string& parent_command_name
		);
	};

	/**
	 * @brief Raised when trying to declare positional arguments and subcommands in the same
	 * command. Detected at the time of definition, not parsing.
	 */
	struct CoexistingPositionalAndSubcommand: public ClahException {
		explicit CoexistingPositionalAndSubcommand(const std::string& command_name);
	};

	/**
	 * @brief Raised when adding a subcommand with no name.
	 * Detected at the time of definition, not parsing.
	 */
	struct UnnamedSubcommand: public ClahException {
		explicit UnnamedSubcommand(const std::string& super_command_name);
	};

	/**
	 * @brief Raised when performing execute and the matched command lacks definition of the handler
	 * function.
	 */
	struct NoHandlerSpecified: public ClahException {
		explicit NoHandlerSpecified(const std::string& command_name);
	};
}
