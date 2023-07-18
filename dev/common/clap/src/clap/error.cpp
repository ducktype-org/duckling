#include "error.hpp"

namespace clap {
	std::string PositionalParametersCountError::print() const {
		return base::strConcat("Wrong number of positional parameters. Found: ", found, ", required: ", required, "\n");
	}
	
	std::string MissingRequiredParameter::print() const {
		return base::strConcat("Required parameter is missing: '", name, "'\n");
	}
	
	std::string DuplicatedParameter::print() const {
		return base::strConcat("Parameter '", name, "' is duplicated\n");
	}
	
	std::string UnexpectedParameter::print() const {
		return base::strConcat("Found unexpected parameter: '", name, "'\n");
	}
	
	std::string MissingParameterArgument::print() const {
		return base::strConcat("Missing argument of parameter: '", name, "'\n");
	}
	
	std::string HelpMessage::print() const {
		return "";
	}

	MissingRequiredParameter::MissingRequiredParameter(const ParameterConfig& c) :
		name(c.to_str_id()) {}

	DuplicatedParameter::DuplicatedParameter(const ParameterConfig& c) :
		name(c.to_str_id()) {}

	UnexpectedParameter::UnexpectedParameter(char c) :
		name(base::StrId("" + c)) {}
	UnexpectedParameter::UnexpectedParameter(base::StrId c) :
		name(c) {}

	MissingParameterArgument::MissingParameterArgument(const ParameterConfig& c) :
		name(c.to_str_id()) {}
}
