// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file help_message_generator.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief A generic help message generator, that is easily extensible.
 */

#pragma once

#include <clah/clah.hpp>

#include <string>

namespace clah {

	// Class serving as a namespace for generating help messages.
	class HelpMessageGenerator {
	public:
		HelpMessageGenerator() = delete;

		/**
		 * Generates a generic help message.
		 * @param clah The clah object used for parsing.
		 * @param parsing_result Parsing result from help exception.
		 * @return A nicely formatted string with a help message.
		 */
		static std::string generate(const Clah& clah, const ParsingResult& result);
	};

}  // clah
