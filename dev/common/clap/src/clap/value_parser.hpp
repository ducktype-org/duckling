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
#include <regex>
#include <utility>

namespace clap {

	/**
	 * A result of a single value parsing.
	 */
	struct ValueParsingResult {
		/**
		 * The value. It has to be cast back with base::anyCast.
		 */
		std::any value;
		/**
		 * Source chars from which the value was created.
		 */
		std::string raw_source;
		/**
		 * Position is an index ONE AFTER the last character of the parsed value.
		 */
		usize position;
	};

	/**
	 * An interface class for all other parsers.
	 * Each parser should be stored inside a base::unique_ptr and should accept
	 * custom name as a constructor parameter.
	 */
	class ValueParser {
	public:
		ValueParser() = default;

		explicit ValueParser(const std::string& custom_value_name):
			  custom_value_name(custom_value_name) {}

		virtual ~ValueParser() = default;

		/**
		 * Performs parsing of a value at the start index.
		 * It is assumed that the raw_input[start] is a non-whitespace character.
		 */
		[[nodiscard]]
		virtual ValueParsingResult parse(usize start, std::string_view raw_input) const
			= 0;

		/**
		 * @return name of the type of a parsed value, or a custom name.
		 */
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

	/**
	 * A value parser used for a string parsing.
	 * Creates values of type std::string.
	 */
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

	/**
	 * A value parser used for an integer parsing.
	 * Creates values of type i64.
	 */
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

	/**
	 * A value parser used for an integer range parsing. I.e. "-1..5".
	 * Creates values of type RangeParser::Range.
	 */
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

	/**
	 * A value parser used for a file parsing.
	 * Creates values of type base::FilePath, which are links to valid files.
	 * Additionally, it accepts std::regex to match only given file extensions or anything else.
	 */
	class FileParser: public ValueParser {
		using ValueParser::ValueParser;

		std::regex file_regex = std::regex(".*");  // The regex - default matches everything.

	public:
		explicit FileParser(std::regex regex): file_regex(std::move(regex)) {}

		FileParser(const std::string& name, std::regex regex):
			  ValueParser(name),
			  file_regex(std::move(regex)) {}

		template<class... Args>
		static base::unique_ptr<FileParser> make(Args&&... args) {
			return base::make_unique<FileParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().value_or("file");
		}
	};

}
