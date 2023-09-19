#pragma once

#include "parameter_config.hpp"

#include <base/option.hpp>
#include <base/str_concat.hpp>
#include <base/string_id.hpp>
#include <variant>

namespace clap {
	struct PositionalParametersCountError {
		usize required;
		usize found;

		std::string print() const;
	};

	struct MissingRequiredParameter {
		base::StrId name;

		MissingRequiredParameter(const ParameterConfig& c);
		std::string print() const;
	};

	struct DuplicatedParameter {
		base::StrId name;

		DuplicatedParameter(const ParameterConfig& c);
		std::string print() const;
	};

	struct UnexpectedParameter {
		base::StrId name;

		UnexpectedParameter(byte c);
		UnexpectedParameter(base::StrId c);
		std::string print() const;
	};

	struct MissingParameterArgument {
		base::StrId name;

		MissingParameterArgument(const ParameterConfig& c);
		std::string print() const;
	};

	struct HelpMessage {
		std::string print() const;
	};

	using ClapParsingError = std::variant<PositionalParametersCountError, MissingRequiredParameter,
	                                      DuplicatedParameter, UnexpectedParameter,
	                                      MissingParameterArgument, HelpMessage>;
}
