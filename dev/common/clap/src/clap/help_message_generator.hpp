/**
 * @file help_message_generator.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <string>
#include <clap/clap.hpp>
#include <sstream>
#include <iomanip>

namespace clap {

	class HelpMessageGenerator {
	public:
		HelpMessageGenerator() = delete;

		static std::string generate(const Clap& clap, const ParsingResult& parsing_result);
	};

}  // clap
