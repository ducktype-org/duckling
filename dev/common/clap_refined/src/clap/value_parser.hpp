/**
 * @file value_parser.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <any>
#include "base/raw_view.hpp"
#include "base/smart_pointers.hpp"
#include "base/exceptions.hpp"
#include "base/type_traits.hpp"

namespace clap {

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
		virtual ValueParsingResult parse(usize start, std::string_view raw_input) const
			= 0;

		virtual std::string getTypeName() const = 0;
	};

	class StringParser: public ValueParser {
	public:
		static base::unique_ptr<StringParser> make() { return base::make_unique<StringParser>(); }

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return "<string>";
		}
	};

	class IntParser: public ValueParser {
	public:
		static base::unique_ptr<IntParser> make() { return base::make_unique<IntParser>(); }

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return "<int>";
		}
	};

	class RangeParser: public ValueParser {
	public:
		struct Range {
			i64 begin, end;
		};

		static base::unique_ptr<RangeParser> make() { return base::make_unique<RangeParser>(); }

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return "<range:int..int>";
		}
	};

}
