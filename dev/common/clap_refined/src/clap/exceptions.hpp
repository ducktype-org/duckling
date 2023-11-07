/**
 * @file exceptions.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once
#include <utility>

#include "base/exceptions.hpp"

namespace clap::exceptions {

	struct ClapException: public base::LogicError {
		explicit ClapException(const std::string& message);
	};

	struct ValueParsingException: public ClapException {
		ValueParsingException(
			base::RawView    type,
			usize            start,
			usize            end,
			std::string_view source,
			std::string_view reason = ""
		);
	};

	struct PositionalParameterExpected: public ClapException {
		PositionalParameterExpected(usize param_index, const std::string& param_type);
	};

	struct InvalidParameterName: public ClapException {
		explicit InvalidParameterName(const std::string& name);
	};

	struct DuplicatedParameter: public ClapException {
		explicit DuplicatedParameter(const std::string& name);
	};

	struct ParameterRequiresValue: public ClapException {
		ParameterRequiresValue(const std::string& name, const std::string& value_type);
	};

	struct ExpectedParameterIdentifier: public ClapException {
		ExpectedParameterIdentifier(i32 at, std::string_view source);
	};

	struct MissingRequiredParameter: public ClapException {
		explicit MissingRequiredParameter(const std::string& name);
	};

	struct MissingConditionalParameter: public ClapException {
		explicit MissingConditionalParameter(const std::string& name, std::string_view why = "");
	};

}
