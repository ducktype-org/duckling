/**
 * @file value_parser.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief This is an interface class that is used to parse values from a string.
 */

#pragma once

#include <base/pointers/box.hpp>
#include <base/optional.hpp>

#include <any>
#include <regex>
#include <utility>

namespace clah {

	/**
	 * A result of a single value parsing.
	 */
	struct ValueParsingResult final {
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
	 * Each parser should be stored inside a Box and should accept
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
	class StringParser final: public ValueParser {
		using ValueParser::ValueParser;

	public:
		template<class... Args>
		static Box<StringParser> make(Args&&... args) {
			return makeBox<StringParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("string");
		}
	};

	/**
	 * A value parser used for an integer parsing.
	 * Creates values of type i64.
	 */
	class IntParser final: public ValueParser {
		using ValueParser::ValueParser;

	public:
		template<class... Args>
		static Box<IntParser> make(Args&&... args) {
			return makeBox<IntParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("int");
		}
	};

	/**
	 * A value parser used for an integer range parsing. I.e. "-1..5".
	 * Creates values of type RangeParser::Range.
	 */
	class RangeParser final: public ValueParser {
		using ValueParser::ValueParser;

	public:
		struct Range final {
			i64 begin, end;
		};

		template<class... Args>
		static Box<RangeParser> make(Args&&... args) {
			return makeBox<RangeParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("int..int");
		}
	};

	/**
	 * A value parser used for a file parsing.
	 * Creates values of type base::FilePath, which are links to valid files.
	 * Additionally, it accepts std::regex to match only given file extensions or anything else.
	 */
	class FileParser final: public ValueParser {
		using ValueParser::ValueParser;

		std::regex file_regex = std::regex(".*");  // The regex - default matches everything.

	public:
		explicit FileParser(std::regex regex): file_regex(std::move(regex)) {}

		FileParser(const std::string& name, std::regex regex):
			  ValueParser(name),
			  file_regex(std::move(regex)) {}

		template<class... Args>
		static Box<FileParser> make(Args&&... args) {
			return makeBox<FileParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("file");
		}
	};

	class FilePathParser final: public ValueParser {
		using ValueParser::ValueParser;

		std::regex filepath_regex = std::regex(".*");

	public:
		explicit FilePathParser(std::regex regex): filepath_regex(std::move(regex)) {}

		FilePathParser(const std::string& name, std::regex regex):
			  ValueParser(name),
			  filepath_regex(std::move(regex)) {}

		template<class... Args>
		static Box<FilePathParser> make(Args&&... args) {
			return makeBox<FilePathParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(usize start, std::string_view raw_input) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("filepath");
		}
	};

}
