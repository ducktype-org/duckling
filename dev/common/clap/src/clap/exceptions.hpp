/**
 * @file exceptions.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once
#include <utility>
#include <filesystem>

#include "base/exceptions.hpp"
#include "parsing_result.hpp"

namespace clap::exceptions {

	/**
	 * Base struct for all the parsing-related exceptions.
	 */
	struct ClapException: public base::LogicError {
		explicit ClapException(const std::string& message);
	};

	/**
	 * Raised when passed ``-h`` / ``--h``.
	 * It is not an exception per se.
	 */
	struct HelpException: public base::LogicError {
		ParsingResult parsing_result;

		explicit HelpException(ParsingResult result);
	};

	/**
	 * Raised by value parsers when input is malformed.
	 */
	struct ValueParsingException: public ClapException {
		ValueParsingException(
			base::RawView    type,
			usize            start,
			usize            end,
			std::string_view source,
			std::string_view reason = ""
		);
	};

	/**
	 * Raised by value parser clap::FileParser when a passed file does not exist.
	 */
	struct FileDoesNotExist: public ClapException {
		explicit FileDoesNotExist(const std::filesystem::path& path);
	};

	/**
	 * Raised when user did not pass a necessary positional argument.
	 */
	struct PositionalParameterExpected: public ClapException {
		PositionalParameterExpected(usize param_index, const std::string& param_type);
	};

	/**
	 * Raised when user passes an unknown parameter.
	 */
	struct InvalidParameterName: public ClapException {
		explicit InvalidParameterName(const std::string& name);
	};

	/**
	 * Raised when parameter was passed twice.
	 */
	struct DuplicatedParameter: public ClapException {
		explicit DuplicatedParameter(const std::string& name);
	};

	/**
	 * Raised when user does not pass a value to a parameter.
	 */
	struct ParameterRequiresValue: public ClapException {
		ParameterRequiresValue(const std::string& name, const std::string& value_type);
	};

	/**
	 * Raised when user does not pass a parameter name after ``-`` or ``--``.
	 */
	struct ExpectedParameterIdentifier: public ClapException {
		ExpectedParameterIdentifier(i32 at, std::string_view source);
	};

	/**
	 * Raised when user does not pass a required parameter.
	 */
	struct MissingRequiredParameter: public ClapException {
		explicit MissingRequiredParameter(const std::string& name);
	};

	/**
	 * Raised when conditional parameter's condition is not met.
	 */
	struct MissingConditionalParameter: public ClapException {
		explicit MissingConditionalParameter(const std::string& name, std::string_view why = "");
	};

	/**
	 * Raised when user has specified extra arguments, but Clap has Clap::default_value_parser
	 * set to nullptr.
	 */
	struct NoDefaultValueParser: public ClapException {
		explicit NoDefaultValueParser(i32 at, std::string_view values);
	};
}
