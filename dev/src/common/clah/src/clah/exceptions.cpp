/**
 * @file exceptions.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "exceptions.hpp"

#include "parsing_result.hpp"

#include <base/misc/int_conv.hpp>
#include <base/str/str_utils.hpp>

#include <utility>

namespace {
	/**
	 * Shortens the string, leaving the important part and dots if necessary:
	 * "..." + source[at-back..at+front - 1] + "...".
	 * The "..." are inserted if text is longer than the shown part on a corresponding end.
	 * @param at Current position in the source.
	 * @param back Number of characters from the back to show.
	 * @param front Number of characters in the front to show.
	 * @param source The source of characters.
	 * @return The formatted text.
	 */
	std::string shorten(i64 at, i64 back, i64 front, std::string_view source) {
		std::string shortened;
		if (at - back > 0) shortened += "...";

		shortened += source.substr(
			usize(std::max(0L, at - back)), source.size() - usize(std::max(0L, at - front)) + 1
		);

		if (at + front < source.size() - 1) shortened += "...";

		return shortened;
	}
}

namespace clah::exceptions {
	ClahException::ClahException(const std::string& message): base::LogicError(message) {}

	PositionalParameterExpected::PositionalParameterExpected(
		usize param_index, const std::string& param_type
	):
		  ClahException(base::strConcat(
			  "Expected positional parameter at position: ",
			  param_index,
			  ", of type: <",
			  param_type,
			  ">"
		  )) {}

	ValueParsingException::ValueParsingException(
		base::RawView type, usize start, usize end, std::string_view source, std::string_view reason
	):
		  ClahException(base::strConcat(
			  "Couldn't parse into <",
			  type,
			  "> from: \"",
			  source.substr(start, end - start + 1),
			  "\"",
			  reason.empty() ? "" : ", reason: ",
			  reason
		  )) {}

	InvalidParameterName::InvalidParameterName(const std::string& name):
		  ClahException("There is no parameter named \'" + name + '\'') {}

	ParameterRequiresValue::ParameterRequiresValue(
		const std::string& name, const std::string& value_type
	):
		  ClahException(base::strConcat(
			  "Parameter \"", name, "\" requires a value of type <", value_type, ">"
		  )) {}

	ExpectedParameterIdentifier::ExpectedParameterIdentifier(u64 at, std::string_view source):
		  ClahException(base::strConcat(
			  shorten(base::safeIntConv<i64>(at), 20, 20, source),
			  "_<- Here expected parameter identifier."
		  )) {}

	MissingRequiredParameter::MissingRequiredParameter(const std::string& name):
		  ClahException(base::strConcat("Missing parameter: ", name)) {}

	MissingConditionalParameter::MissingConditionalParameter(
		const std::string& name, std::string_view why
	):
		  ClahException(base::strConcat("Invalid parameter - ", name, " - ", why)) {}

	CustomVerificationFailed::CustomVerificationFailed(const std::string& message):
		  ClahException(base::strConcat(message)) {}

	DuplicatedParameter::DuplicatedParameter(const std::string& name):
		  ClahException(base::strConcat("Duplicated parameter named: ", name)) {}

	HelpException::HelpException(ParsingResult result):
		  base::LogicError("Help flag was passed, help message should be generated."),
		  parsing_result(std::move(result)) {}

	SuccessExitException::SuccessExitException(ParsingResult result):
		  base::LogicError("Clean exit requested."),
		  parsing_result(std::move(result)) {}

	FileDoesNotExist::FileDoesNotExist(const std::filesystem::path& path):
		  ClahException("File at \"" + absolute(path).string() + "\" does not exist.") {}

	NoDefaultValueParser::NoDefaultValueParser(u64 at, std::string_view values):
		  ClahException(base::strConcat(
			  "Extra values provided, but no default value specified.\nExtra values: \"",
			  shorten(base::safeIntConv<i64>(at), 0, 20, values),
			  "\""
		  )) {}

	SubcommandNotSpecified::SubcommandNotSpecified(const std::string& command_name):
		  ClahException(base::strConcat(
			  "Command \"",
			  command_name,
			  "\" cannot be executed on its own. Please specify a subcommand."
		  )) {}

	DuplicateSubcommand::DuplicateSubcommand(
		const std::string& duplicate_name, const std::string& parent_command_name
	):
		  ClahException(base::strConcat(
			  "A subcommand with the name \"",
			  duplicate_name,
			  "\" has already been added to the command \"",
			  parent_command_name,
			  "\"."
		  )) {}

	UnnamedSubcommand::UnnamedSubcommand(const std::string& super_command_name):
		  ClahException(base::strConcat(
			  "Tried to add a subcommand with no name to command:\"", super_command_name, "\"."
		  )) {}

	CoexistingPositionalAndSubcommand::CoexistingPositionalAndSubcommand(
		const std::string& command_name
	):
		  ClahException(base::strConcat(
			  "Positional arguments and subcommands can't coexist in the same command: \"",
			  command_name,
			  "\"."
		  )) {}

	NoHandlerSpecified::NoHandlerSpecified(const std::string& command_name):
		  ClahException(base::strConcat(
			  "No handler specified for the executed command: \"", command_name, "\"."
		  )) {}

}
