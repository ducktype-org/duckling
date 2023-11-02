/**
 * @file parsing_result.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "parsing_result.hpp"
#include "clap.hpp"

namespace clap {
	const std::string& ParsingResult::getFilePath() const { return file_path; }

	const std::string& ParsingResult::getArgs() const { return args; }

	base::Optional<usize> ParsingResult::getId(char name) const {}

	base::Optional<usize> ParsingResult::getId(base::RawView name) const {}
}
