/**
 * @file value_parser.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "base/raw_view.hpp"

namespace clap {

	// This cannot be templated
	class ValueParser {
	public:
		virtual ~ValueParser() = default;

		explicit ValueParser(bool accept_multiple_values = false, base::RawView delimiter = " "):
			  accept_multiple_values(accept_multiple_values),
			  delimiter(delimiter) {}

		[[nodiscard]]
		virtual usize parse(usize start, base::RawView raw_input) const
			= 0;  // returns index in the string where it has finished parsing.

		[[nodiscard]]
		bool doesAcceptMultipleValues() const;
		[[nodiscard]]
		const base::RawView& getDelimiter() const;

	private:
		bool          accept_multiple_values;
		base::RawView delimiter;
	};

	class StringParser: public ValueParser {
	public:
		using ValueParser::ValueParser;

		template<class... Args>
		static base::unique_ptr<StringParser> make(Args&&... args) {
			return std::move(base::make_unique<StringParser>(std::forward<Args>(args)...));
		}

		//		std::any parse() ?
	};

}
