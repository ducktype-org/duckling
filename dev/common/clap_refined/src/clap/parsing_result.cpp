//
// Created by mateusz on 11/1/23.
//

#include "parsing_result.hpp"
#include "clap.hpp"

namespace clap {
	const std::string& ParsingResult::getFilePath() const { return file_path; }

	const std::string& ParsingResult::getArgs() const { return args; }

	ParsingResult ParsingResult::parse(
		const std::string& file_path, const std::string& args, const Clap& params
	) {
		ParsingResult result(file_path, args);

		usize position = 0;
		while (position < args.size()) {}

		return result;
	}
}
