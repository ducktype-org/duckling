// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
