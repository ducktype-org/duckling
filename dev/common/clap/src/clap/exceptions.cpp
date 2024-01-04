/**
 * @file exceptions.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "exceptions.hpp"

#include <utility>
#include "parsing_result.hpp"

namespace clap::exceptions {
	ClapException::ClapException(const std::string& message): base::LogicError(message) {}

	PositionalParameterExpected::PositionalParameterExpected(
		usize param_index, const std::string& param_type
	):
		  ClapException(base::strConcat(
			  "Expected positional parameter at position: ",
			  param_index,
			  ", of type: <",
			  param_type,
			  ">"
		  )) {}

	ValueParsingException::ValueParsingException(
		base::RawView type, usize start, usize end, std::string_view source, std::string_view reason
	):
		  ClapException(base::strConcat(
			  "Couldn't parse into <",
			  type,
			  "> from: \"",
			  source.substr(start, end - start + 1),
			  "\"",
			  reason.empty() ? "" : ", reason: ",
			  reason
		  )) {}

	InvalidParameterName::InvalidParameterName(const std::string& name):
		  ClapException("There is no parameter named \'" + name + '\'') {}

	ParameterRequiresValue::ParameterRequiresValue(
		const std::string& name, const std::string& value_type
	):
		  ClapException(base::strConcat(
			  "Parameter \"", name, "\" requires a value of type <", value_type, ">"
		  )) {}

	ExpectedParameterIdentifier::ExpectedParameterIdentifier(i32 at, std::string_view source):
		  ClapException(base::strConcat(
			  source.substr(std::max(0, at - 20), source.size() - std::max(0, at - 20) + 1),
			  "_<- Here expected parameter identifier."
		  )) {}

	MissingRequiredParameter::MissingRequiredParameter(const std::string& name):
		  ClapException(base::strConcat("Missing parameter: ", name)) {}

	MissingConditionalParameter::MissingConditionalParameter(
		const std::string& name, std::string_view why
	):
		  ClapException(base::strConcat("Invalid parameter - ", name, " - ", why)) {}

	DuplicatedParameter::DuplicatedParameter(const std::string& name):
		  ClapException(base::strConcat("Duplicated parameter named: ", name)) {}

	HelpException::HelpException(ParsingResult result):
		  base::LogicError("Help flag was passed, help message should be generated."),
		  parsing_result(std::move(result)) {}

	FileDoesNotExist::FileDoesNotExist(const std::filesystem::path& path):
		  ClapException("File at \"" + absolute(path).string() + "\" does not exist.") {}
}
