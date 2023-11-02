//
// Created by mateusz on 11/1/23.
//

#pragma once
#include <string>
#include <utility>
#include <vector>

namespace clap {
	class Clap;

	class ParsingResult {
	public:
		ParsingResult(std::string file_path, std::string args):
			  file_path(std::move(file_path)),
			  args(std::move(args)) {}

		[[nodiscard]]
		const std::string& getFilePath() const;
		[[nodiscard]]
		const std::string& getArgs() const;

	private:
		std::string file_path;
		std::string args;
	};
}
