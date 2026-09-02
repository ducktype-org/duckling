#pragma once

#include "value_parser.hpp"

#include <string>
#include <utility>

namespace clah {
	class PositionalParameter final {
	public:
		PositionalParameter(Box<ValueParser> parser, std::string description):
			  parser(std::move(parser)),
			  description(std::move(description)) {}

		[[nodiscard]] const ValueParser& getValueParser() const { return *parser; }

		[[nodiscard]] const std::string& getDescription() const { return description; }

	private:
		Box<ValueParser> parser;
		std::string      description;
	};
}  // namespace clah
