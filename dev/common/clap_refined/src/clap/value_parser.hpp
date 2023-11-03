/**
 * @file value_parser.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <any>
#include "base/raw_view.hpp"
#include "base/smart_pointers.hpp"
#include "base/exceptions.hpp"

namespace clap {

	struct ValueParsingException: public base::LogicError {
	public:
		using base::LogicError::LogicError;
	};

	struct ValueParsingResult {
		std::any    value;
		std::string raw_source;
		usize       position;
	};

	class ValueParser {
	public:
		virtual ~ValueParser() = default;

		[[nodiscard]]
		virtual ValueParsingResult parse(usize start, const std::string& raw_input) const
			= 0;  // returns index in the string where it has finished parsing.
	};

	class StringParser: public ValueParser {
	public:
		using ValueParser::ValueParser;

		static base::unique_ptr<StringParser> make() { return base::make_unique<StringParser>(); }

		[[nodiscard]]
		ValueParsingResult parse(usize start, const std::string& raw_input) const override;
	};

}
