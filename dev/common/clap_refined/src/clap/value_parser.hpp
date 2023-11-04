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
		std::string raw_source;  // Source chars from which the value was created
		usize position;  // position is an index ONE AFTER the last character of the parsed value
	};

	class ValueParser {
	public:
		virtual ~ValueParser() = default;

		[[nodiscard]]
		// It is assumed that the raw_input[start] is a non-whitespace character.
		virtual ValueParsingResult parse(usize start, const std::string_view& raw_input) const
			= 0;
	};

	class StringParser: public ValueParser {
	public:
		static base::unique_ptr<StringParser> make() { return base::make_unique<StringParser>(); }

		[[nodiscard]]
		ValueParsingResult parse(usize start, const std::string_view& raw_input) const override;
	};

	class IntParser: public ValueParser {
	public:
		static base::unique_ptr<IntParser> make() { return base::make_unique<IntParser>(); }

		[[nodiscard]]
		ValueParsingResult parse(usize start, const std::string_view& raw_input) const override;
	};

}
