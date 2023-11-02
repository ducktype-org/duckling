/**
 * @file parsing_result.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once
#include <string>
#include <utility>
#include <vector>
#include <any>
#include <unordered_set>
#include "base/ints.hpp"
#include "base/optional.hpp"
#include "base/maps.hpp"

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

		base::Optional<usize> getId(char name) const;
		base::Optional<usize> getId(base::RawView name) const;

		base::HashMap<char, usize>          short_names_to_id;
		base::HashMap<base::RawView, usize> long_names_to_id;

		base::HashMap<usize, std::vector<base::RawView>> id_to_source;
		base::HashMap<usize, std::vector<std::any>>      id_to_value;

		std::unordered_set<usize> flags;
	};
}
