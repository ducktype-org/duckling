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
#include <base/optional.hpp>

namespace clap {

	struct ValueParsingResult {
		std::any    value;
		std::string raw_source;  // Source chars from which the value was created
		usize position;  // position is an index ONE AFTER the last character of the parsed value
	};

	class ValueParser {
	public:
		ValueParser() = default;

		explicit ValueParser(const std::string& custom_value_name):
			  custom_value_name(custom_value_name) {}

		virtual ~ValueParser() = default;

		[[nodiscard]]
		// It is assumed that the raw_input[start] is a non-whitespace character.
		virtual ValueParsingResult parse(usize start, std::string_view raw_input) const
			= 0;

		[[nodiscard]]
		virtual std::string getTypeName() const
			= 0;

	protected:
		[[nodiscard]]
		const base::Optional<std::string>& getCustomValueName() const {
			return custom_value_name;
		}

	private:
		base::Optional<std::string> custom_value_name;
	};

	class StringParser: public ValueParser {
		using ValueParser::ValueParser;

	public:
		template<class... Args>
		static base::unique_ptr<StringParser> make(Args&&... args) {
			return base::make_unique<StringParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().value_or("string");
		}
	};

	class IntParser: public ValueParser {
		using ValueParser::ValueParser;

	public:
		template<class... Args>
		static base::unique_ptr<IntParser> make(Args&&... args) {
			return base::make_unique<IntParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().value_or("int");
		}
	};

	class RangeParser: public ValueParser {
		using ValueParser::ValueParser;

	public:
		struct Range {
			i64 begin, end;
		};

		template<class... Args>
		static base::unique_ptr<RangeParser> make(Args&&... args) {
			return base::make_unique<RangeParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().value_or("int..int");
		}
	};

}
