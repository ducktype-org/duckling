/**
 * @file value_parser.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief This is an interface class that is used to parse values from a string.
 */

#pragma once

#include <base/collections/optional.hpp>
#include <base/misc/anycast.hpp>
#include <base/pointers/box.hpp>

#include <filesystem/file_fd.hpp>

#include <any>
#include <concepts>
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
	};

	/**
	 * An interface class for all other parsers.
	 * Each parser should be stored inside a Box and should accept
	 * custom name as a constructor parameter.
	 */
	class ValueParser {
	public:
		ValueParser() = default;

		explicit ValueParser(std::string_view custom_value_name):
			  custom_value_name(custom_value_name) {}

		virtual ~ValueParser() = default;

		/**
		 * Performs parsing of a value at the start index.
		 * It is assumed that the argument[start] is a non-whitespace character.
		 */
		[[nodiscard]]
		virtual ValueParsingResult parse(std::string_view argument) const
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
	 * A ValueParser that exposes the type of the value it produces, and thus can be used as
	 * an element parser of ListParser.
	 */
	template<class P>
	concept ElementValueParser
		= std::derived_from<P, ValueParser> && requires { typename P::Result; };

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
		ValueParsingResult parse(std::string_view argument) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("string");
		}

		using Result = std::string;
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
		ValueParsingResult parse(std::string_view argument) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("int");
		}

		using Result = i64;
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
		ValueParsingResult parse(std::string_view argument) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("int..int");
		}

		using Result = Range;
	};

	/**
	 * A value parser used for a file parsing.
	 * Creates values of type fs::File, which are links to valid files.
	 * Additionally, it accepts std::regex to match only given file extensions or anything else.
	 */
	class FileParser final: public ValueParser {
		std::regex file_regex = std::regex(".*");  // The regex - default matches everything.

		// Some commands (e.g. compile_package) take a directory through this parser, the rest
		// accepts regular files only.
		bool accept_directories = false;

	public:
		FileParser() = default;

		explicit FileParser(std::string_view name, bool accept_directories = false):
			  ValueParser(name),
			  accept_directories(accept_directories) {}

		explicit FileParser(std::regex regex, bool accept_directories = false):
			  file_regex(std::move(regex)),
			  accept_directories(accept_directories) {}

		FileParser(std::string_view name, std::regex regex, bool accept_directories = false):
			  ValueParser(name),
			  file_regex(std::move(regex)),
			  accept_directories(accept_directories) {}

		template<class... Args>
		static Box<FileParser> make(Args&&... args) {
			return makeBox<FileParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(std::string_view argument) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("file");
		}

		using Result = fs::File;
	};

	/**
	 * A value parser used for a file path parsing.
	 * Creates values of type fs::FilePath.
	 * Additionally, it accepts std::regex to match only given file extensions or anything else.
	 */
	class FilePathParser final: public ValueParser {
		using ValueParser::ValueParser;

		std::regex filepath_regex = std::regex(".*");

	public:
		explicit FilePathParser(std::regex regex): filepath_regex(std::move(regex)) {}

		FilePathParser(std::string_view name, std::regex regex):
			  ValueParser(name),
			  filepath_regex(std::move(regex)) {}

		template<class... Args>
		static Box<FilePathParser> make(Args&&... args) {
			return makeBox<FilePathParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(std::string_view argument) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("path");
		}

		using Result = fs::FilePath;
	};

	/**
	 * A value parser used for enum-like category parsing.
	 * Creates values of type std::string.
	 */
	class CategoryParser final: public ValueParser {
		using ValueParser::ValueParser;

		std::vector<std::string> categories;

	public:
		explicit CategoryParser(std::vector<std::string> categories):
			  categories(std::move(categories)) {}

		CategoryParser(std::string_view name, std::vector<std::string> categories):
			  ValueParser(name),
			  categories(std::move(categories)) {}

		template<class... Args>
		static Box<CategoryParser> make(Args&&... args) {
			return makeBox<CategoryParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(std::string_view argument) const override;

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr("category");
		}

		static std::string debugPrintCategories(const std::vector<std::string>& categories);

		using Result = std::string;
	};

	namespace utils {
		std::vector<std::string> splitCommaSeparated(std::string_view value);
	}

	/**
	 * A value parser used for a comma separated list of values parsing. I.e. "val1, val2, val3".
	 * Each item is parsed by an element parser of type T.
	 * Creates values of type std::vector<T::Result>.
	 */
	template<ElementValueParser T>
	class ListParser final: public ValueParser {
		using ElemType = T::Result;

		Box<T> element_parser;

	public:
		explicit ListParser(Box<T> element_parser): element_parser(std::move(element_parser)) {}

		ListParser(std::string_view name, Box<T> element_parser):
			  ValueParser(name),
			  element_parser(std::move(element_parser)) {}

		template<class... Args>
		static Box<ListParser> make(Args&&... args) {
			return makeBox<ListParser>(std::forward<Args>(args)...);
		}

		[[nodiscard]]
		ValueParsingResult parse(std::string_view argument) const override {
			std::vector<ElemType> result;

			for (const auto& value: utils::splitCommaSeparated(argument))
				result.emplace_back(base::anyCast<ElemType>(element_parser->parse(value).value));

			return { .value = std::move(result), .raw_source = std::string(argument) };
		}

		[[nodiscard]]
		std::string getTypeName() const override {
			return getCustomValueName().copyValueOr(element_parser->getTypeName() + "-list");
		}

		using Result = std::vector<ElemType>;
	};

	using StringListParser   = ListParser<StringParser>;
	using CategoryListParser = ListParser<CategoryParser>;
	using FilePathListParser = ListParser<FilePathParser>;
	using FileListParser     = ListParser<FileParser>;
}
