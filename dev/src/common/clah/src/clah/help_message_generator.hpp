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
